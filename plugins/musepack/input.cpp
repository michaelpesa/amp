////////////////////////////////////////////////////////////////////////////////
//
// plugins/musepack/input.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/utility.hpp>
#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/ape.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include <mpc/mpcdec.h>
#include <mpc/reader.h>
#include <mpc/streaminfo.h>


namespace std {

template<>
struct default_delete<::mpc_demux_t>
{
    void operator()(::mpc_demux* const d) const noexcept
    { ::mpc_demux_exit(d); }
};

}     // namespace std


namespace amp {
namespace mpc {
namespace {

extern "C" {

static int32 read(::mpc_reader* const r, void* const dst, int32 const n)
{
    try {
        auto&& file = *static_cast<io::stream*>(r->data);
        return static_cast<int32>(file.try_read(static_cast<uchar*>(dst),
                                                static_cast<std::size_t>(n)));
    }
    catch (...) {
        return -1;
    }
}

static uint8 seek(::mpc_reader* const r, int32 const pos)
{
    try {
        auto&& file = *static_cast<io::stream*>(r->data);
        file.seek(pos, io::seekdir::beg);
        return 1;
    }
    catch (...) {
        return 0;
    }
}

static int32 tell(::mpc_reader* const r)
{
    try {
        auto&& file = *static_cast<io::stream*>(r->data);
        return numeric_cast<int32>(file.tell());
    }
    catch (...) {
        return -1;
    }
}

static int32 get_size(::mpc_reader* const r)
{
    try {
        auto&& file = *static_cast<io::stream*>(r->data);
        return numeric_cast<int32>(file.size());
    }
    catch (...) {
        return -1;
    }
}

static uint8 can_seek(::mpc_reader*)
{
    return 1;
}

}     // extern "C"


class input
{
public:
    explicit input(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const noexcept;
    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const;

private:
    ref_ptr<io::stream> file;
    ::mpc_reader reader;
    ::mpc_streaminfo si;
    std::unique_ptr<::mpc_demux_t> handle;
};

input::input(ref_ptr<io::stream> s, audio::open_mode) :
    file(std::move(s)),
    reader{
        mpc::read,
        mpc::seek,
        mpc::tell,
        mpc::get_size,
        mpc::can_seek,
        file.get(),
    },
    handle(::mpc_demux_init(&reader))
{
    if (AMP_UNLIKELY(handle == nullptr)) {
        raise_bad_alloc();
    }

    ::mpc_demux_get_info(handle.get(), &si);
    if (si.beg_silence >= si.samples) {
        raise(errc::out_of_bounds,
              "Musepack: beginning silence (%" PRIu64 ") cannot equal "
              "or exceed the total samples (%" PRIu64 ")",
              si.beg_silence, si.samples);
    }
}

void input::read(audio::packet& pkt)
{
    pkt.resize(MPC_DECODER_BUFFER_LENGTH, uninitialized);

    ::mpc_frame_info frame;
    frame.buffer = pkt.data();

    do {
        auto const ret = ::mpc_demux_decode(handle.get(), &frame);
        if (AMP_UNLIKELY(ret != 0)) {
            raise(errc::failure,
                  "failed to decode Musepack frame (code=0x%08x)", ret);
        }
        if (frame.bits <= 0) {
            pkt.clear();
            return;
        }
    }
    while (frame.samples == 0);

    auto const bits = static_cast<uint32>(frame.bits);
    pkt.set_bit_rate(muldiv(bits, si.sample_freq, frame.samples));
    pkt.resize(frame.samples * si.channels);
}

void input::seek(uint64 const frame)
{
    auto const ret = ::mpc_demux_seek_sample(handle.get(), frame);
    if (AMP_UNLIKELY(ret != 0)) {
        raise(errc::failure,
              "failed to seek in Musepack stream (code=0x%08x)", ret);
    }
}

auto input::get_format() const noexcept
{
    audio::format format;
    format.sample_rate    = si.sample_freq;
    format.channels       = si.channels;
    format.channel_layout = audio::guess_channel_layout(format.channels);
    return format;
}

auto input::get_info(uint32 const number)
{
    audio::stream_info info{get_format()};
    info.average_bit_rate = numeric_cast<uint32>(si.average_bitrate);
    info.codec_id = (si.stream_version == 8)
                  ? audio::codec::musepack_sv8
                  : audio::codec::musepack_sv7;

    info.props.emplace(tags::codec_profile, si.profile_name);
    info.props.emplace(tags::encoder, si.encoder);

    if (ape::find(*file)) {
        ape::read(*file, info.tags);
    }
    else if (id3v1::find(*file)) {
        id3v1::read(*file, info.tags);
    }

    auto insert_gain = [&](std::string_view const key, uint16 const x) {
        if (x != 0) {
            auto const gain = 64.82 - (x / 256.0);
            info.tags.insert_or_assign(key, u8format("%1.2f dB", gain));
        }
    };
    auto insert_peak = [&](std::string_view const key, uint16 const x) {
        if (x != 0) {
            auto const peak = audio::to_amplitude(x / 256.0) / (1 << 15);
            info.tags.insert_or_assign(key, u8format("%.6f", peak));
        }
    };

    insert_gain(tags::rg_album_gain, si.gain_album);
    insert_peak(tags::rg_album_peak, si.peak_album);

    if (number == 0) {
        info.frames = si.samples - si.beg_silence;
        insert_gain(tags::rg_track_gain, si.gain_title);
        insert_peak(tags::rg_track_peak, si.peak_title);
    }
    else {
        auto const index = static_cast<int>(number - 1);
        auto const chap = ::mpc_demux_chap(handle.get(), index);

        if (auto const next = ::mpc_demux_chap(handle.get(), index + 1)) {
            info.frames = next->sample;
        }
        else {
            info.frames = si.samples - si.beg_silence;
        }
        info.frames -= chap->sample;
        info.start_offset = chap->sample;

        if (chap->tag_size != 0) {
            media::dictionary tmp;
            ape::read_no_preamble(chap->tag, chap->tag_size, tmp);
            info.tags.merge(tmp);
        }
        insert_gain(tags::rg_track_gain, chap->gain);
        insert_peak(tags::rg_track_peak, chap->peak);
    }
    return info;
}

auto input::get_image(media::image_type const type)
{
    return ape::find_image(*file, type);
}

auto input::get_chapter_count() const
{
    return numeric_cast<uint32>(::mpc_demux_chap_nb(handle.get()));
}

AMP_REGISTER_INPUT(input, "mp+", "mpc", "mpp");

}}}   // namespace amp::mpc::<unnamed>

