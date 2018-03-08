////////////////////////////////////////////////////////////////////////////////
//
// plugins/demuxer/mp4.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "mp4_box.hpp"
#include "mp4_movie.hpp"
#include "mp4_track.hpp"

#include <algorithm>
#include <memory>
#include <utility>


namespace amp {
namespace mp4 {
namespace {

inline void fix_sbr_time_line(mp4::movie const& movie,
                              mp4::track const& track,
                              audio::codec_format const& format,
                              uint64& frames,
                              uint32& priming) noexcept
{
    auto const movie_time_scale = movie.get_time_scale();
    auto const media_time_scale = track.get_time_scale();

    if (media_time_scale == (format.sample_rate / 2)) {
        frames  *= 2;
        priming *= 2;
    }

    auto const is_fhg_aac = (movie_time_scale == format.sample_rate)
                         && (media_time_scale == format.sample_rate);

    auto const is_nero_aac = (movie.find("udta/chpl") != nullptr)
                          && (movie_time_scale == 90000)
                          && (media_time_scale == (format.sample_rate / 2));

    if (is_fhg_aac || is_nero_aac) {
        constexpr auto sbr_decoder_delay = uint32{(480 + 1) * 2};
        if (priming >= sbr_decoder_delay) {
            priming -= sbr_decoder_delay;
        }
    }
}

inline void get_time_line(mp4::movie const& movie,
                          mp4::track const& track,
                          audio::codec_format const& format,
                          uint64& frames,
                          uint32& priming)
{
    if (auto edit_list = track.get_edit_list()) {
        auto edit = edit_list->entries[0];
        if (edit.segment_duration == 0) {
            edit.segment_duration = track.get_duration();
            edit.segment_duration -= static_cast<uint64>(edit.media_time);
        }
        else {
            edit.segment_duration = muldiv(edit.segment_duration,
                                           track.get_time_scale(),
                                           movie.get_time_scale());
        }
        priming = numeric_cast<uint32>(edit.media_time);
        frames  = edit.segment_duration;
    }
    else if (auto found = movie.get_iTunSMPB()) {
        frames  = found->frames;
        priming = found->priming;
    }
    else {
        frames = track.get_duration();
        if (format.codec_id == audio::codec::aac_lc) {
            if (frames >= 1024) {
                frames -= 1024;
                priming = 1024;
            }
        }
    }

    if (format.codec_id == audio::codec::he_aac_v1 ||
        format.codec_id == audio::codec::he_aac_v2) {
        mp4::fix_sbr_time_line(movie, track, format, frames, priming);
    }
}

using chapter_list = decltype(mp4::chpl_box_data::entries);


class demuxer final :
    public audio::basic_demuxer<mp4::demuxer>
{
    using Base = audio::basic_demuxer<mp4::demuxer>;
    friend class audio::basic_demuxer<mp4::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    void find_audio_track();
    void find_chapters();
    void find_nero_chapters();

    ref_ptr<io::stream> file;
    mp4::root_box       root;
    mp4::movie          movie;
    mp4::track*         track{};
    mp4::chapter_list   chapters;
};


void demuxer::find_audio_track()
{
    for (auto&& t : movie) {
        if (t.get_handler_type() == media_handler::audio) {
            auto fmt = t.select_first_audio_sample_entry();
            if (fmt && Base::try_resolve_decoder(std::move(*fmt))) {
                track = &t;
                return;
            }
        }
    }
    raise(errc::failure, "no audio track(s) found in MP4 file");
}

void demuxer::find_chapters()
{
    auto found = track->find("tref/chap");
    if (!found) {
        return find_nero_chapters();
    }

    auto text = movie.find_chapter_track(found->chap);
    if (!text) {
        return find_nero_chapters();
    }

    auto const count = text->get_sample_count();
    chapters.resize(count);

    io::buffer sample;
    for (auto const i : xrange(count)) {
        text->feed(*file, sample);
        io::reader r{sample};

        auto const len = r.read<uint16,BE>();
        auto const str = r.read_n(len);

        chapters[i].title = {reinterpret_cast<char const*>(str), len};
        chapters[i].start = text->get_sample_duration(i);
        if (i != 0) {
            chapters[i].start += chapters[i - 1].start;
        }
    }
}

void demuxer::find_nero_chapters()
{
    if (auto const found = movie.find("udta/chpl")) {
        chapters = found->chpl.entries;
        for (auto&& entry : chapters) {
            entry.start = muldiv(entry.start, format.sample_rate, 10000000);
        }
    }
}

demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file(std::move(s)),
    root(*file),
    movie(*root.find("moov"))
{
    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    find_audio_track();
    for (auto const& moof : root.equal_range("moof")) {
        for (auto const& traf : moof.equal_range("traf")) {
            if (track->get_id() == traf.find("tfhd")->tfhd.track_id) {
                track->add_fragment(traf);
            }
        }
    }

    uint64 frames;
    uint32 priming;
    mp4::get_time_line(movie, *track, format, frames, priming);

    Base::set_total_frames(frames);
    Base::set_encoder_delay(priming);
    Base::average_bit_rate = track->get_average_bit_rate();
    if (Base::average_bit_rate == 0) {
        Base::average_bit_rate = format.bit_rate;
    }

    if (mode & audio::metadata) {
        find_chapters();
    }
    // root.print();
}

bool demuxer::feed(io::buffer& dest)
{
    if (track->feed(*file, dest)) {
        if (format.bit_rate != 0) {
            instant_bit_rate = format.bit_rate;
        }
        else if (format.frames_per_packet != 0) {
            instant_bit_rate = muldiv(static_cast<uint32>(dest.size()),
                                      format.sample_rate * 8,
                                      format.frames_per_packet);
        }
        else {
            instant_bit_rate = average_bit_rate;
        }
        return true;
    }
    return false;
}

void demuxer::seek(uint64 const pts)
{
    auto const srate = format.sample_rate;
    auto const shift = uint32{srate == (2 * track->get_time_scale())};
    auto const preroll = shift ? std::min(pts, uint64{srate}) : 0;

    auto priming = uint64{0};
    track->seek(*file, (pts - preroll) >> shift, priming);

    priming = (priming << shift) + preroll + (pts & shift);
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const number)
{
    audio::stream_info info{Base::get_format()};
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;

    info.tags = movie.get_tags();
    info.props.emplace(tags::container, "MP4");

    auto found = info.tags.find(tags::encoded_by);
    if (found != info.tags.end()) {
        info.props.emplace(tags::encoder, found->second);
        info.tags.erase(found);
    }

    if (number == 0) {
        info.frames = Base::total_frames;
    }
    else {
        info.tags.try_emplace(tags::track_number, to_u8string(number));
        info.tags.try_emplace(tags::track_total, to_u8string(chapters.size()));
        info.tags.try_emplace(tags::title, chapters[number - 1].title);
        info.start_offset = chapters[number - 1].start - chapters[0].start;
        if (number == chapters.size()) {
            info.frames = Base::total_frames;
        }
        else {
            info.frames = chapters[number].start - chapters[0].start;
        }
        info.frames -= info.start_offset;
    }
    return info;
}

auto demuxer::get_image(media::image::type)
{
    return movie.get_cover_art();
}

auto demuxer::get_chapter_count() const noexcept
{
    return static_cast<uint32>(chapters.size());
}

AMP_REGISTER_INPUT(
    demuxer,
    "3gp",              // 3GPP File Format
    "3g2",              // 3GPP2 File Format
    "f4a",              // Adobe Flash audio
    "f4b",              // Adobe Flash audio book
    "f4v",              // Adobe Flash video
    "m4a",              // Apple iTunes audio
    "m4b",              // Apple iTunes audio book / podcast
    "m4r",              // Apple iTunes ringtone
    "m4v", "mp4v",      // Apple iTunes movie
    "mp4",              // MPEG-4 Part 14
    "mov", "mqv",       // QuickTime File Format
    "m21", "mp21",      // MPEG-21 Part 9
    "mj2", "mjp2");     // Motion JPEG 2000

}}}   // namespace amp::mp4::<unnamed>

