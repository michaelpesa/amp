////////////////////////////////////////////////////////////////////////////////
//
// plugins/opus/input.cpp
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
#include <amp/optional.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <opus/opusfile.h>


namespace std {

template<>
struct default_delete<::OggOpusFile>
{
    void operator()(::OggOpusFile* const handle) const noexcept
    { ::op_free(handle); }
};

}     // namespace std


namespace amp {
namespace opus {
namespace {

inline errc translate_error(int const ret) noexcept
{
    switch (ret) {
    case OP_ENOTFORMAT:
        return errc::invalid_data_format;
    case OP_EINVAL:
        return errc::invalid_argument;
    case OP_ENOSEEK:
        return errc::seek_error;
    case OP_EREAD:
        return errc::read_fault;
    case OP_EIMPL:
        return errc::not_implemented;
    case OP_EFAULT:
        return errc::invalid_pointer;
    case OP_FALSE:
    case OP_EBADPACKET:
    case OP_EBADLINK:
    case OP_EBADTIMESTAMP:
    case OP_EVERSION:
    default:
        return errc::failure;
    }
}

template<typename Ret>
AMP_INLINE auto check(Ret const ret)
{
    if (AMP_LIKELY(ret >= 0)) {
        return as_unsigned(ret);
    }
    raise(opus::translate_error(static_cast<int>(ret)));
}


extern "C" {

static int read(void* const opaque, uchar* const dst, int const n)
{
    AMP_ASSERT(opaque != nullptr);
    AMP_ASSERT(n >= 0);

    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return static_cast<int>(file.try_read(dst, as_unsigned(n)));
    }
    catch (...) {
        return -1;
    }
}

static int seek(void* const opaque, long long const off, int const whence)
{
    AMP_ASSERT(opaque != nullptr);
    AMP_ASSERT(whence >= 0 && whence <= 2);

    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        file.seek(off, static_cast<io::seekdir>(whence));
        return 0;
    }
    catch (...) {
        return -1;
    }
}

static long long tell(void* const opaque)
{
    AMP_ASSERT(opaque != nullptr);

    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_try_cast<long long>(file.tell()).value_or(-1);
    }
    catch (...) {
        return -1;
    }
}

}     // extern "C"


inline auto open(io::stream& file)
{
    static constexpr ::OpusFileCallbacks callbacks {
        opus::read,
        opus::seek,
        opus::tell,
        nullptr,
    };

    int error = 0;
    auto handle = ::op_open_callbacks(&file, &callbacks, nullptr, 0, &error);
    opus::check(error);
    return std::unique_ptr<::OggOpusFile>{handle};
}


inline optional<int32> parse_fixed_point_gain(std::string_view s) noexcept
{
    if (s.empty()) {
        return nullopt;
    }

    auto const sign = (s[0] == '-') ? -1 : +1;
    if (s[0] == '+' || s[0] == '-') {
        s.remove_prefix(1);
        if (s.empty()) {
            return nullopt;
        }
    }

    auto gain = int32{0};
    for (auto const c : s) {
        if (c < '0' || c > '9') {
            return nullopt;
        }

        gain *= 10;
        gain += (c - '0');

        if (gain > (std::numeric_limits<int16>::max() - sign)) {
            return nullopt;
        }
    }

    return make_optional(gain * sign);
}

inline u8string format_gain(int const gain_q8)
{
    constexpr auto headroom_RG   = -18.0;
    constexpr auto headroom_R128 = -23.0;
    constexpr auto headroom_diff = headroom_R128 - headroom_RG;

    auto const gain = static_cast<double>(gain_q8) / 256.0;
    return u8format("%.02f dB", gain - headroom_diff);
}


class input
{
public:
    explicit input(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const noexcept;
    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    void on_link_changed_(int);

    ref_ptr<io::stream> file_;
    std::unique_ptr<::OggOpusFile> handle_;
    uint8 const* mapping_;
    uint32 channels_;
    int link_;
};

input::input(ref_ptr<io::stream> s, audio::open_mode) :
    file_(std::move(s)),
    handle_(opus::open(*file_))
{
    opus::check(::op_set_gain_offset(handle_.get(), OP_ABSOLUTE_GAIN, 0));
    on_link_changed_(-1);
}

void input::read(audio::packet& pkt)
{
    // The documentation for ::op_read_float (in <opus/opusfile.h>) recommends
    // a buffer size of at least 120 ms (5760 samples) at 48 kHz per channel.
    pkt.resize(5760 * channels_);
    auto const samples = static_cast<int>(pkt.samples());

    int ret, li;
    do { ret = ::op_read_float(handle_.get(), pkt.data(), samples, &li); }
    while (AMP_UNLIKELY(ret == OP_HOLE));

    if (link_ != li) {
        on_link_changed_(li);
    }

    auto const frames = opus::check(ret);
    pkt.resize(frames * channels_);

    if (mapping_ != nullptr) {
        float tmp[8];
        auto const last = pkt.end();
        for (auto first = pkt.begin(); first != last; ) {
            for (auto const i : xrange(channels_)) {
                tmp[mapping_[i]] = first[i];
            }
            for (auto const i : xrange(channels_)) {
                *first++ = tmp[i];
            }
        }
    }

    auto const instant_bit_rate = ::op_bitrate_instant(handle_.get());
    if (instant_bit_rate > 0) {
        pkt.set_bit_rate(static_cast<uint32>(instant_bit_rate));
    }
}

void input::seek(uint64 const frame)
{
    opus::check(::op_pcm_seek(handle_.get(), numeric_cast<int64>(frame)));
    on_link_changed_(::op_current_link(handle_.get()));
}

auto input::get_format() const noexcept
{
    audio::format format{};
    format.sample_rate = 48000;
    format.channels = channels_;
    format.channel_layout = audio::xiph_channel_layout(channels_);
    return format;
}

auto input::get_info(uint32 const number)
{
    audio::stream_info info{get_format()};
    info.codec_id = audio::codec::opus;
    info.props.emplace(tags::container, "Ogg");

    on_link_changed_(static_cast<int>(number - 1));
    info.average_bit_rate = check(::op_bitrate(handle_.get(), link_));
    info.frames = check(::op_pcm_total(handle_.get(), link_));

    if (link_ != -1) {
        for (auto const i : xrange(link_)) {
            info.start_offset += check(::op_pcm_total(handle_.get(), i));
        }
    }

    auto const tags = ::op_tags(handle_.get(), link_);
    auto const head = ::op_head(handle_.get(), link_);
    if (tags != nullptr && head != nullptr) {
        optional<int32> track_gain;
        for (auto const i : xrange(as_unsigned(tags->comments))) {
            if (tags->comment_lengths[i] <= 0) {
                continue;
            }

            auto const len = as_unsigned(tags->comment_lengths[i]);
            auto const str = std::string_view{tags->user_comments[i], len};
            auto const sep = str.find('=');
            if (sep >= str.size()) {
                continue;
            }

            auto const key   = str.substr(0, sep);
            auto const value = str.substr(sep + 1);

            // Handle R128 track gain separately.
            if (stricmpeq(key, "R128_TRACK_GAIN")) {
                track_gain = parse_fixed_point_gain(value);
            }
            else if (!stricmpeq(key, "METADATA_BLOCK_PICTURE")) {
                info.tags.emplace(tags::map_common_key(key),
                                  u8string::from_utf8_lossy(value));
            }
        }

        auto insert_gain = [&](auto const key, int gain = 0) {
            gain += head->output_gain;
            info.tags.emplace(key, opus::format_gain(gain));
        };

        if (!info.tags.contains(tags::rg_album_gain) &&
            !info.tags.contains(tags::rg_track_gain)) {
            insert_gain(tags::rg_album_gain);
            if (track_gain) {
                insert_gain(tags::rg_track_gain, *track_gain);
            }
        }
    }
    return info;
}

auto input::get_image(media::image::type const type)
{
    auto const tags = ::op_tags(handle_.get(), -1);
    if (tags == nullptr || tags->comments <= 0) {
        return media::image{};
    }

    auto const key = "METADATA_BLOCK_PICTURE";
    auto const n = ::opus_tags_query_count(tags, key);
    if (n <= 0) {
        return media::image{};
    }

    media::image image;
    for (auto const i : xrange(n)) {
        std::string_view const block{::opus_tags_query(tags, key, i)};
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
    auto const count = ::op_link_count(handle_.get());
    return (count > 1) ? static_cast<uint32>(count) : 0;
}

void input::on_link_changed_(int const li)
{
    link_ = li;
    channels_ = opus::check(::op_channel_count(handle_.get(), link_));

    static constexpr uint8 channel_mappings[8][8] {
        { 0 },
        { 0, 1 },
        { 0, 2, 1 },
        { 0, 1, 2, 3 },
        { 0, 2, 1, 3, 4 },
        { 0, 2, 1, 4, 5, 3 },
        { 0, 2, 1, 5, 6, 4, 3 },
        { 0, 2, 1, 6, 7, 4, 5, 3 },
    };

    if (channels_ >= 3 && channels_ <= 8 && channels_ != 4) {
        mapping_ = channel_mappings[channels_ - 1];
    }
    else {
        mapping_ = nullptr;
    }
}

AMP_REGISTER_INPUT(input, "ogg", "opus");

}}}   // namespace amp::opus::<unnamed>

