////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/hls.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>

#include "m3u.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <utility>


namespace amp {
namespace hls {
namespace {

class demuxer final
{
public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const noexcept;
    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    void open_input_for_current_segment();

    m3u::variant_playlist  master_playlist;
    m3u::media_playlist*   playlist;
    ref_ptr<audio::input>  input;
    uint32                 segment{};
    audio::format          format;
    audio::open_mode const mode;
};


void demuxer::open_input_for_current_segment()
{
    if (segment >= playlist->segments.size()) {
        input = nullptr;
        return;
    }
    input = audio::input::resolve(playlist->segments[segment].location, mode);
}

demuxer::demuxer(ref_ptr<io::stream> file, audio::open_mode const m) :
    master_playlist{std::move(file)},
    playlist{master_playlist.find_by_codec("mp4a")},
    mode{m}
{
    if (!playlist) {
        raise(errc::failure, "failed to select playlist");
    }

    playlist->load();
    open_input_for_current_segment();
    format = input->get_format();
}

void demuxer::read(audio::packet& pkt)
{
    for (;;) {
        if (AMP_UNLIKELY(input == nullptr)) {
            return;
        }

        input->read(pkt);
        if (AMP_LIKELY(!pkt.empty())) {
            return;
        }

        ++segment;
        open_input_for_current_segment();
    }
}

void demuxer::seek(uint64 const frame)
{
    segment = 0;
    auto target = muldiv(frame, std::nano::den, format.sample_rate);

    for (; segment != playlist->segments.size(); ++segment) {
        if (target >= playlist->segments[segment].duration) {
            target -= playlist->segments[segment].duration;
        }
        else {
            break;
        }
    }

    open_input_for_current_segment();
    if (input != nullptr) {
        input->seek(muldiv(target, format.sample_rate, std::nano::den));
    }
}

auto demuxer::get_format() const noexcept
{
    return format;
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    auto const total = std::accumulate(
        playlist->segments.begin(),
        playlist->segments.end(),
        uint64{0},
        [](auto const acc, auto const& s) noexcept {
            return acc + s.duration;
        });

    auto info = input ? input->get_info(0) : audio::stream_info{format};
    info.frames = muldiv(total, format.sample_rate, std::nano::den);
    return info;
}

auto demuxer::get_image(media::image_type)
{
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "m3u", "m3u8");

}}}   // namespace amp::hls::<unnamed>

