////////////////////////////////////////////////////////////////////////////////
//
// plugins/optimfrog/input.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include <memory>
#include <string_view>
#include <utility>

#include <OptimFROG/OptimFROG.h>


namespace amp {
namespace ofr {
namespace {

using namespace std::literals;


extern "C" {

static uint8 close(void*)
{
    return 1;
}

static int32 read(void* const opaque, void* const dst, uint32 const n)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return static_cast<int32>(file.try_read(static_cast<uchar*>(dst), n));
    }
    catch (...) {
        return -1;
    }
}

static uint8 eof(void* const opaque)
{
    auto&& file = *static_cast<io::stream*>(opaque);
    return file.eof() ? 1 : 0;
}

static uint8 seekable(void*)
{
    return 1;
}

static int64 length(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_try_cast<int64>(file.size()).value_or(-1);
    }
    catch (...) {
        return -1;
    }
}

static int64 tell(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return numeric_try_cast<int64>(file.tell()).value_or(-1);
    }
    catch (...) {
        return -1;
    }
}

static uint8 seek(void* const opaque, int64 const pos)
{
    AMP_ASSERT(opaque != nullptr);
    AMP_ASSERT(pos >= 0);

    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        file.seek(static_cast<uint64>(pos));
        return 1;
    }
    catch (...) {
        return 0;
    }
}

}     // extern "C"

constexpr ::ReadInterface callbacks {
    ofr::close,
    ofr::read,
    ofr::eof,
    ofr::seekable,
    ofr::length,
    ofr::tell,
    ofr::seek,
};


struct sample_type
{
    uint8 bytes_per_sample;
    bool  sign;
};

inline sample_type get_sample_type(::OptimFROG_Info const& file_info)
{
    std::string_view const sample_type{file_info.sampleType};

    if (sample_type == "SINT8"sv)  { return {1, true}; }
    if (sample_type == "SINT16"sv) { return {2, true}; }
    if (sample_type == "SINT24"sv) { return {3, true}; }
    if (sample_type == "SINT32"sv) { return {4, true}; }

    if (sample_type == "UINT8"sv)  { return {1, false}; }
    if (sample_type == "UINT16"sv) { return {2, false}; }
    if (sample_type == "UINT24"sv) { return {3, false}; }
    if (sample_type == "UINT32"sv) { return {4, false}; }

    raise(errc::unsupported_format, "unsupported OptimFROG sample type: '%s'",
          file_info.sampleType);
}


struct decoder_delete
{
    void operator()(void* const instance) const noexcept
    { ::OptimFROG_destroyInstance(instance); }
};


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
    ref_ptr<io::stream> file_;
    std::unique_ptr<audio::pcm::blitter> blitter_;
    std::unique_ptr<void, ofr::decoder_delete> decoder_;
    io::buffer buf_;
    ::OptimFROG_Info file_info_;
    uint32 bytes_per_frame_;
};

input::input(ref_ptr<io::stream> s, audio::open_mode) :
    file_{std::move(s)},
    decoder_{::OptimFROG_createInstance()}
{
    if (AMP_UNLIKELY(decoder_ == nullptr)) {
        raise_bad_alloc();
    }

    auto const reader = const_cast<::ReadInterface*>(&ofr::callbacks);
    auto ok = ::OptimFROG_openExt(decoder_.get(), reader, file_.get(), false);
    if (AMP_UNLIKELY(!ok)) {
        raise(errc::failure, "failed to open OptimFROG decoder");
    }

    ok = ::OptimFROG_getInfo(decoder_.get(), &file_info_);
    if (AMP_UNLIKELY(!ok)) {
        raise(errc::failure, "failed to obtain OptimFROG file info");
    }

    bytes_per_frame_ = file_info_.channels * (file_info_.bitspersample / 8);
    buf_.resize((file_info_.samplerate / 4) * bytes_per_frame_, uninitialized);

    auto const sample_type = ofr::get_sample_type(file_info_);

    audio::pcm::spec spec;
    spec.bytes_per_sample = sample_type.bytes_per_sample;
    spec.bits_per_sample = spec.bytes_per_sample * 8;
    spec.channels = file_info_.channels;
    spec.flags = audio::pcm::host_endian;
    if (sample_type.sign) {
        spec.flags |= audio::pcm::signed_int;
    }
    blitter_ = audio::pcm::blitter::create(spec);
}

void input::read(audio::packet& pkt)
{
    auto const frames = static_cast<uint32>(buf_.size() / bytes_per_frame_);
    auto const ret = ::OptimFROG_read(decoder_.get(), buf_.data(), frames);

    if (AMP_UNLIKELY(ret <= 0)) {
        if (ret == 0) {
            pkt.clear();
            return;
        }
        raise(errc::failure, "failed to read OptimFROG packet");
    }

    blitter_->convert(buf_.data(), static_cast<uint32>(ret), pkt);
    pkt.set_bit_rate(file_info_.bitrate * 1000);
}

void input::seek(uint64 const pts)
{
    auto const point = numeric_cast<int64>(pts);
    auto const ok = ::OptimFROG_seekPoint(decoder_.get(), point);

    if (AMP_UNLIKELY(!ok)) {
        raise(errc::failure, "failed to seek in OptimFROG file");
    }
}

auto input::get_format() const noexcept
{
    audio::format fmt;
    fmt.sample_rate = file_info_.samplerate;
    fmt.channels = file_info_.channels;
    fmt.channel_layout = audio::guess_channel_layout(fmt.channels);
    return fmt;
}

auto input::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{get_format()};
    info.codec_id         = audio::codec::optimfrog;
    info.bits_per_sample  = file_info_.bitspersample;
    info.average_bit_rate = file_info_.bitrate * 1000;
    info.frames           = numeric_cast<uint64>(file_info_.noPoints);

    if (ape::find(*file_)) {
        ape::read(*file_, info.tags);
    }
    else if (id3v1::find(*file_)) {
        id3v1::read(*file_, info.tags);
    }
    return info;
}

auto input::get_image(media::image::type const type)
{
    return ape::find_image(*file_, type);
}

auto input::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(input, "ofr", "ofs");

}}}   // namespace amp::ofr::<unnamed>

