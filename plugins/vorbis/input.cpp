////////////////////////////////////////////////////////////////////////////////
//
// plugins/vorbis/input.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/base64.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/utility.hpp>

#include "error.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>


namespace amp {
namespace vorbis {
namespace {

extern "C" {

static std::size_t read(void* const dst, std::size_t const size,
                        std::size_t const n, void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return file.try_read(static_cast<uchar*>(dst), size * n);
    }
    catch (...) {
        return -1_sz;
    }
}

static int seek(void* const opaque, int64 const off, int const whence)
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

static long tell(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_try_cast<long>(file.tell()).value_or(-1);
    }
    catch (...) {
        return -1;
    }
}

}     // extern "C"


struct file_handle : OggVorbis_File
{
    explicit file_handle(void* const opaque)
    {
        static constexpr ::ov_callbacks callbacks {
            vorbis::read,
            vorbis::seek,
            nullptr,
            vorbis::tell,
        };
        verify(::ov_open_callbacks(opaque, this, nullptr, 0, callbacks));
    }

    ~file_handle()
    {
        ::ov_clear(this);
    }
};


class input
{
public:
    explicit input(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const noexcept;
    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    ref_ptr<io::stream> file;
    vorbis::file_handle handle;
    audio::format format;
    uint8 const* mapping;
};

input::input(ref_ptr<io::stream> s, audio::open_mode) :
    file{std::move(s)},
    handle{file.get()}
{
    auto const info = ::ov_info(&handle, -1);
    if (AMP_UNLIKELY(info == nullptr)) {
        raise(errc::failure, "Vorbis file info cannot be null");
    }
    if (info->channels < 1 || info->channels > 8) {
        raise(errc::unsupported_format, "invalid Vorbis channel count: %d",
              info->channels);
    }

    format.sample_rate = numeric_cast<uint32>(info->rate);
    format.channels = static_cast<uint32>(info->channels);
    format.channel_layout = audio::xiph_channel_layout(format.channels);

    static constexpr uint8 channel_mappings[8][8] {
        { 0 },
        { 0, 1 },
        { 0, 2, 1 },
        { 0, 1, 2, 3 },
        { 0, 2, 1, 3, 4 },
        { 0, 2, 1, 5, 3, 4 },
        { 0, 2, 1, 6, 5, 3, 4 },
        { 0, 2, 1, 7, 5, 6, 3, 4 },
    };
    mapping = channel_mappings[format.channels - 1];
}

void input::read(audio::packet& pkt)
{
    float** tmp;
    long ret;
    do { ret = ::ov_read_float(&handle, &tmp, 5760, nullptr); }
    while (AMP_UNLIKELY(ret == OV_HOLE));

    auto const frames = verify(ret);
    if (AMP_UNLIKELY(ret == 0)) {
        return;
    }
    AMP_ASSERT(tmp != nullptr);

    float* planes[8];
    for (auto const i : xrange(format.channels)) {
        planes[i] = tmp[mapping[i]];
    }

    pkt.set_bit_rate(static_cast<uint32>(::ov_bitrate_instant(&handle)));
    pkt.set_channel_layout(format.channel_layout);
    pkt.fill_planar(planes, frames);
}

void input::seek(uint64 const pts)
{
    verify(::ov_pcm_seek(&handle, as_signed(pts)));
}

auto input::get_format() const noexcept
{
    return format;
}

auto input::get_info(uint32 const number)
{
    audio::stream_info info{get_format()};
    info.codec_id = audio::codec::vorbis;
    info.props.emplace(tags::container, "Ogg");

    auto const link = static_cast<int32>(number) - 1;
    auto const bit_rate = verify(::ov_bitrate(&handle, link));

    info.average_bit_rate = numeric_cast<uint32>(bit_rate);
    info.frames = verify(::ov_pcm_total(&handle, link));

    if (link != -1) {
        for (auto const prev : xrange(link)) {
            info.start_offset += verify(::ov_pcm_total(&handle, prev));
        }
    }

    auto const vc = ::ov_comment(&handle, link);
    if (vc && (vc->comments > 0)) {
        for (auto const i : xrange(as_unsigned(vc->comments))) {
            if (vc->comment_lengths[i] <= 0) {
                continue;
            }

            auto const len = as_unsigned(vc->comment_lengths[i]);
            auto const str = vc->user_comments[i];
            auto const sep = std::string_view{str, len}.find('=');
            if (sep >= len) {
                continue;
            }

            // Don't store embedded cover art in the dictionary.
            std::string_view const key{str, sep};
            if (stricmpeq(key, "COVERART")     ||
                stricmpeq(key, "COVERARTMIME") ||
                stricmpeq(key, "METADATA_BLOCK_PICTURE")) {
                continue;
            }

            std::string_view const value{str + (sep + 1), len - (sep + 1)};
            info.tags.emplace(tags::map_common_key(key),
                              u8string::from_utf8_lossy(value));
        }
    }
    return info;
}

auto input::get_image(media::image_type const type)
{
    auto const vc = ::ov_comment(&handle, -1);
    if (vc == nullptr || vc->comments <= 0) {
        return media::image{};
    }

    auto const key = "METADATA_BLOCK_PICTURE";
    auto const n = ::vorbis_comment_query_count(vc, key);
    if (n <= 0) {
        return media::image{};
    }

    media::image image;
    for (auto const i : xrange(n)) {
        std::string_view const block{::vorbis_comment_query(vc, key, i)};
        io::buffer buf{base64::decoded_size(block), uninitialized};
        base64::decode(block.data(), block.size(), buf.data());

        io::reader r{buf};
        if (r.read<uint32,BE>() == as_underlying(type)) {
            image.set_mime_type(r.read_pascal_string<uint32,BE>());
            image.set_description(r.read_pascal_string<uint32,BE>());

            r.skip(sizeof(uint32) * 5); // width, height, depth, colors, size
            buf.pop_front(r.tell());
            image.set_data(std::move(buf));
            break;
        }
    }
    return image;
}

auto input::get_chapter_count() const noexcept
{
    return (handle.links > 1) ? static_cast<uint32>(handle.links) : 0;
}

AMP_REGISTER_INPUT(input, "oga", "ogg");

}}}   // namespace amp::vorbis::<unnamed>

