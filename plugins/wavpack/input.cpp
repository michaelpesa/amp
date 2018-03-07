////////////////////////////////////////////////////////////////////////////////
//
// plugins/wavpack/input.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/ape.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <memory>
#include <utility>

#include <wavpack/wavpack.h>


namespace amp {
namespace wavpack {
namespace {

struct context_delete
{
    void operator()(::WavpackContext* const ctx) const noexcept
    { ::WavpackCloseFile(ctx); }
};

using context = std::unique_ptr<::WavpackContext, context_delete>;


extern "C" {

static int32 read(void* const opaque, void* const dst, int32 const n)
{
    AMP_ASSERT(n >= 0);
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return static_cast<int32>(file.try_read(dst, as_unsigned(n)));
    }
    catch (...) {
        return -1;
    }
}

static int32 write(void*, void*, int32)
{
    return -1;
}

static int64 tell(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_cast<int64>(file.tell());
    }
    catch (...) {
        return -1;
    }
}

static int seek_relative(void* const opaque, int64 const off, int const whence)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        file.seek(off, static_cast<io::seekdir>(whence));
        return 0;
    }
    catch (...) {
        return -1;
    }
}

static int seek_absolute(void* const opaque, int64 const pos)
{
    return seek_relative(opaque, pos, as_underlying(io::seekdir::beg));
}

static int unget(void* const opaque, int const c)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        file.seek(-1, io::seekdir::cur);
        return c;
    }
    catch (...) {
        return EOF;
    }
}

static int64 length(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_cast<int64>(file.size());
    }
    catch (...) {
        return -1;
    }
}

static int can_seek(void*)
{
    return 1;
}

static int truncate(void*)
{
    return -1;
}

static int close(void*)
{
    return 0;
}

}     // extern "C"


auto open_correction_file(net::uri const& location)
{
    ref_ptr<io::stream> file;
    if (location.scheme() == "file") {
        try {
            auto const path = location.get_file_path() + 'c';
            file = io::open(net::uri::from_file_path(path), io::in|io::binary);
        }
        catch (...) {
            file = nullptr;
        }
    }
    return file;
}

auto open_context(io::stream* const file, io::stream* const correction_file)
{
    static constexpr ::WavpackStreamReader64 reader {
        &wavpack::read,
        &wavpack::write,
        &wavpack::tell,
        &wavpack::seek_absolute,
        &wavpack::seek_relative,
        &wavpack::unget,
        &wavpack::length,
        &wavpack::can_seek,
        &wavpack::truncate,
        &wavpack::close,
    };

    char error[128];
    auto const ctx = ::WavpackOpenFileInputEx64(
        const_cast<::WavpackStreamReader64*>(&reader),
        file,
        correction_file,
        error,
        OPEN_WVC|OPEN_DSD_AS_PCM,
        0);

    if (ctx != nullptr) {
        return wavpack::context{ctx};
    }
    raise(errc::failure, "failed to open WavPack input file: %s", error);
}


inline auto get_sample_rate(wavpack::context const& ctx)
{ return numeric_cast<uint32>(::WavpackGetSampleRate(ctx.get())); }

inline auto get_channels(wavpack::context const& ctx)
{ return numeric_cast<uint32>(::WavpackGetNumChannels(ctx.get())); }

inline auto get_channel_mask(wavpack::context const& ctx)
{ return numeric_cast<uint32>(::WavpackGetChannelMask(ctx.get())); }

inline auto get_bits_per_sample(wavpack::context const& ctx)
{ return numeric_cast<uint32>(::WavpackGetBitsPerSample(ctx.get())); }

inline auto get_average_bit_rate(wavpack::context const& ctx)
{ return numeric_cast<uint32>(::WavpackGetAverageBitrate(ctx.get(), 1)); }

inline auto get_num_samples(wavpack::context const& ctx)
{ return numeric_cast<uint64>(::WavpackGetNumSamples(ctx.get())); }


class input
{
public:
    explicit input(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const;
    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    ref_ptr<io::stream> const wv_file;
    ref_ptr<io::stream> const wvc_file;
    wavpack::context const context;
    std::unique_ptr<audio::pcm::blitter> blitter;
    std::unique_ptr<int32[]> readbuf;
    uint32 const channels;
    uint32 const frames_per_packet;
};


input::input(ref_ptr<io::stream> s, audio::open_mode const mode) :
    wv_file(std::move(s)),
    wvc_file(wavpack::open_correction_file(wv_file->location())),
    context(wavpack::open_context(wv_file.get(), wvc_file.get())),
    channels(wavpack::get_channels(context)),
    frames_per_packet(wavpack::get_sample_rate(context) / 10)
{
    if (!(mode & audio::playback)) {
        return;
    }

    audio::pcm::spec spec;
    spec.channels = channels;
    spec.flags = audio::pcm::host_endian;
    if (::WavpackGetMode(context.get()) & MODE_FLOAT) {
        spec.flags |= audio::pcm::ieee_float;
    }
    else {
        spec.flags |= audio::pcm::signed_int;
    }

    spec.bits_per_sample = align_up(wavpack::get_bits_per_sample(context), 8);
    spec.bytes_per_sample = 4;
    blitter = audio::pcm::blitter::create(spec);
    readbuf.reset(new int32[frames_per_packet * channels]);
}

void input::read(audio::packet& pkt)
{
    auto const frames = ::WavpackUnpackSamples(context.get(), readbuf.get(),
                                               frames_per_packet);
    if (frames != 0) {
        blitter->convert(readbuf.get(), frames, pkt);
    }
    else if (::WavpackGetNumErrors(context.get()) == 0) {
        pkt.clear();
    }
    else {
        raise(errc::failure, "WavPack read failed: %s",
              ::WavpackGetErrorMessage(context.get()));
    }

    auto const bit_rate = ::WavpackGetInstantBitrate(context.get());
    pkt.set_bit_rate(numeric_try_cast<uint32>(bit_rate).value_or(0));
}

void input::seek(uint64 const pts)
{
    auto const sample = static_cast<uint32>(pts);
    auto const ret = ::WavpackSeekSample(context.get(), sample);

    if (AMP_UNLIKELY(ret < 0)) {
        raise(errc::failure, "WavPack seek failed: %s",
              ::WavpackGetErrorMessage(context.get()));
    }
}

auto input::get_format() const
{
    audio::format fmt;
    fmt.channels       = channels;
    fmt.channel_layout = wavpack::get_channel_mask(context);
    fmt.sample_rate    = wavpack::get_sample_rate(context);
    return fmt;
}

auto input::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{get_format()};
    info.codec_id         = audio::codec::wavpack;
    info.frames           = wavpack::get_num_samples(context);
    info.bits_per_sample  = wavpack::get_bits_per_sample(context);
    info.average_bit_rate = wavpack::get_average_bit_rate(context);

    auto const mode = ::WavpackGetMode(context.get());
    auto const encoding = (mode & MODE_WVC)      ? "hybrid"
                        : (mode & MODE_LOSSLESS) ? "lossless"
                        :                          "lossy";

    auto const compression = (mode & MODE_VERY_HIGH) ? "very high"
                           : (mode & MODE_HIGH)      ? "high"
                           : (mode & MODE_FAST)      ? "fast"
                           :                           "normal";

    u8string profile;
    if (mode & MODE_EXTRA) {
        profile = u8format("%s, %s compression (extra processing: %d)",
                           encoding, compression, (mode & MODE_XMODE) >> 12);
    }
    else {
        profile = u8format("%s, %s compression", encoding, compression);
    }
    info.props.emplace(tags::codec_profile, std::move(profile));
    info.props.emplace(tags::container, "WavPack");

    if (ape::find(*wv_file)) {
        ape::read(*wv_file, info.tags);
    }
    else if (id3v1::find(*wv_file)) {
        id3v1::read(*wv_file, info.tags);
    }
    return info;
}

auto input::get_image(media::image_type const type)
{
    return ape::find_image(*wv_file, type);
}

auto input::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(input, "wv");

}}}   // namespace amp::wavpack::<unnamed>

