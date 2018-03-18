////////////////////////////////////////////////////////////////////////////////
//
// ui/media_scanner.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/input.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/tags.hpp>
#include <amp/net/uri.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>

#include "core/filesystem.hpp"
#include "core/registry.hpp"
#include "media/cue_sheet.hpp"
#include "media/tags.hpp"
#include "media/track.hpp"
#include "ui/media_scanner.hpp"
#include "ui/string.hpp"

#include <cinttypes>
#include <exception>
#include <utility>
#include <vector>

#include <QtWidgets/QMessageBox>


namespace amp {
namespace ui {
namespace {

inline auto openTagReader(net::uri const& location)
{
    return audio::input::resolve(location, audio::metadata);
}

void scanDirectory(u8string const& directory, std::vector<net::uri>& uris)
{
    for (auto&& path : fs::directory_range{directory}) {
        auto const type = fs::status(path).type();
        if (type == fs::file_type::regular && audio::have_input_for(path)) {
            uris.push_back(net::uri::from_file_path(path));
        }
        else if (type == fs::file_type::directory) {
            scanDirectory(path, uris);
        }
    }
}

void finalizeTrack(media::track& track, audio::stream_info const& info)
{
    auto separate = [&](auto const key_part, auto const key_total) {
        auto pos = track.tags.find(key_part);
        if (pos != track.tags.end() && !track.tags.contains(key_total)) {
            auto const str = pos->second;
            auto const sep = str.find('/');
            if (sep != u8string::npos) {
                pos->second = str.substr(0, sep);
                track.tags.emplace(key_total, str.substr(sep + 1));
            }
        }
    };
    separate(tags::disc_number,  tags::disc_total);
    separate(tags::track_number, tags::track_total);

    auto transfer = [&](auto const key) {
        auto pos = track.tags.find(key);
        if (pos != track.tags.end()) {
            track.info.try_emplace(key, std::move(pos->second));
            track.tags.erase(pos);
        }
    };
    transfer(tags::rg_album_gain);
    transfer(tags::rg_album_peak);
    transfer(tags::rg_track_gain);
    transfer(tags::rg_track_peak);
    transfer(tags::tag_type);

    track.channel_layout = info.channel_layout;
    if (info.bits_per_sample != 0) {
        track.info.emplace(tags::bits_per_sample,
                           to_u8string(info.bits_per_sample));
    }
    if (info.codec_id != 0) {
        track.info.emplace(tags::codec,
                           audio::codec::name(info.codec_id));
    }
    if (auto const kbps = info.average_bit_rate / 1000) {
        track.info.emplace(tags::bit_rate,
                           u8format("%" PRIu32 " kbps", kbps));
    }
}

}     // namespace <unnamed>


void MediaScanner::loadCueSheet(net::uri const& cue_location,
                                u8string cue_text)
{
    auto const embedded = !cue_text.empty();
    if (!embedded) {
        auto const file = io::open(cue_location, io::in);
        cue_text = u8string::from_text_file(*file);
    }

    audio::stream_info info;

    auto cs = cue::parse(std::move(cue_text));
    for (auto const i : xrange(cs.size())) {
        media::track track;
        if (embedded) {
            track.location = cue_location;
        }
        else {
            track.location = net::uri::from_file_path(cs[i].file);
            track.location = track.location.resolve(cue_location);
        }

        if (i == 0 || cs[i].file != cs[i-1].file) {
            info = openTagReader(track.location)->get_info(0);
            info.validate();
        }

        auto to_frames = [&](cue::frames const& f) noexcept {
            return muldiv(f.count(), info.sample_rate, 75);
        };

        track.sample_rate = info.sample_rate;
        track.start_offset = to_frames(cs[i].start);

        if ((i + 1) == cs.size() || cs[i].file != cs[i+1].file) {
            track.frames = info.frames - track.start_offset;
        }
        else {
            track.frames = to_frames(cs[i+1].start - cs[i].start);
        }

        track.chapter = static_cast<uint32>(i + 1);
        track.info = info.props;
        track.tags = std::move(cs[i].tags);
        track.tags.merge(info.tags);
        finalizeTrack(track, info);
        Q_EMIT resultReady(std::move(track));
    }
}

void MediaScanner::loadTracks(net::uri const& location)
{
    if (stricmpeq(fs::extension(location.path()), "cue")) {
        return loadCueSheet(location);
    }

    auto const input = openTagReader(location);
    auto get_chapter = [&](uint32 const chapter) {
        auto info = input->get_info(chapter);
        info.validate();

        media::track track;
        track.location = location;
        track.info = std::move(info.props);
        track.tags = std::move(info.tags);
        track.start_offset = info.start_offset;
        track.frames = info.frames;
        track.sample_rate = info.sample_rate;
        track.chapter = chapter;
        finalizeTrack(track, info);
        return track;
    };

    auto const chapter_count = input->get_chapter_count();
    if (chapter_count <= 1) {
        auto track = get_chapter(0);
        if (auto text = tags::find(track, tags::cue_sheet)) {
            loadCueSheet(location, std::move(text));
        }
        else {
            Q_EMIT resultReady(std::move(track));
        }
    }
    else {
        for (auto const index : xrange(chapter_count)) {
            Q_EMIT resultReady(get_chapter(index + 1));
        }
    }
}


MediaScanner::MediaScanner(QObject* const parent) :
    QObject(parent)
{
    connect(this, &MediaScanner::finished,
            this, [this]{
                if (!errors_.empty()) {
                    QMessageBox::warning(nullptr,
                                         tr("Failed to load tracks"),
                                         errors_.join(u'\n'));
                }
            },
            Qt::QueuedConnection);
}

MediaScanner::~MediaScanner()
{
    cancel();
}

void MediaScanner::cancel()
{
    if (thread_.joinable()) {
        cancel_flag_.store(true, std::memory_order_relaxed);
        thread_.join();
    }
    cancel_flag_.store(false, std::memory_order_relaxed);
}

void MediaScanner::run(std::vector<net::uri> uris, u8string directory)
{
    cancel();
    errors_.clear();

    thread_ = std::thread([
        this,
        uris(std::move(uris)),
        directory(std::move(directory))
    ]() mutable {
        if (!directory.empty()) {
            scanDirectory(directory, uris);
        }
        Q_EMIT progressRangeChanged(0, static_cast<int>(uris.size()));

        for (auto const i : xrange(uris.size())) {
            if (cancel_flag_.load(std::memory_order_relaxed)) {
                Q_EMIT canceled();
                break;
            }

            auto const& location = uris[i];
            auto const progress_text = [&]{
                if (location.scheme() == "file") {
                    return to_qstring(fs::filename(location.get_file_path()));
                }
                return to_qstring(location.data(), location.size());
            }();

            Q_EMIT progressTextChanged(progress_text);
            try {
                loadTracks(location);
            }
            catch (std::exception const& ex) {
                errors_.push_back(tr("Failed to load \"%1\": %2")
                                  .arg(progress_text)
                                  .arg(to_qstring(ex.what())));
            }
            Q_EMIT progressValueChanged(static_cast<int>(i));
        }

        Q_EMIT progressValueChanged(static_cast<int>(uris.size()));
        Q_EMIT finished();
    });
}

}}    // namespace amp::ui

