////////////////////////////////////////////////////////////////////////////////
//
// plugins/opus/decoder.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include <cstdio>
#include <memory>
#include <utility>

#include <opus/opus.h>
#include <opus/opus_multistream.h>


namespace std {

template<>
struct default_delete<::OpusMSDecoder>
{
    void operator()(::OpusMSDecoder* const decoder) const noexcept
    { ::opus_multistream_decoder_destroy(decoder); }
};

}     // namespace std


namespace amp {
namespace opus {
namespace {

constexpr auto head_size = 19_sz;

constexpr uint8 channel_layout_offsets[8][8] {
    { 0, },
    { 0, 1, },
    { 0, 2, 1, },
    { 0, 1, 2, 3, },
    { 0, 2, 1, 3, 4, },
    { 0, 2, 1, 5, 3, 4, },
    { 0, 2, 1, 6, 5, 3, 4, },
    { 0, 2, 1, 7, 5, 6, 3, 4, },
};


class decoder
{
public:
    explicit decoder(audio::codec_format&);

    void send(io::buffer&);
    auto recv(audio::packet&);
    void flush();
    uint32 get_decoder_delay() const noexcept;

private:
    std::unique_ptr<::OpusMSDecoder> handle_;
    uint8 const* source_data_{};
    int32 source_size_{};
    uint32 channels_;
    uint16 preroll_{};
};

decoder::decoder(audio::codec_format& fmt) :
    channels_{fmt.channels}
{
    fmt.sample_rate = 48000;

    uint8 coupled;
    uint8 streams;
    uint8 channel_mapping = 0;
    uint8 mapping_array[] = { 0, 1, 0, 0, 0, 0, 0, 0, };
    uint8* mapping{};

    if (fmt.extra.size() >= opus::head_size) {
        preroll_ = io::load<uint16,LE>(&fmt.extra[10]);
        channel_mapping = fmt.extra[18];
    }

    if (fmt.extra.size() >= opus::head_size + 2 + channels_) {
        streams = fmt.extra[opus::head_size + 0];
        coupled = fmt.extra[opus::head_size + 1];
        if (streams + coupled != channels_) {
            raise(errc::invalid_data_format, "invalid Opus config");
        }
        mapping = &fmt.extra[opus::head_size + 2];
    }
    else {
        if (channels_ > 2 || channel_mapping != 0) {
            raise(errc::invalid_data_format, "invalid Opus config");
        }
        streams = 1;
        coupled = (channels_ > 1);
        mapping = mapping_array;
    }

    if (channels_ > 2 && channels_ <= 8) {
        auto const offsets = channel_layout_offsets[channels_ - 1];
        for (auto const i : xrange(channels_)) {
            mapping_array[i] = mapping[offsets[i]];
        }
        mapping = mapping_array;
    }

    int error{};
    handle_.reset(::opus_multistream_decoder_create(
            48000, static_cast<int>(channels_),
            streams, coupled,
            mapping, &error));

    if (AMP_UNLIKELY(handle_ == nullptr)) {
        raise(errc::failure, "failed to create Opus decoder: %s",
              ::opus_strerror(error));
    }
}

void decoder::send(io::buffer& buf)
{
    source_data_ = buf.data();
    source_size_ = numeric_cast<int32>(buf.size());
}

auto decoder::recv(audio::packet& pkt)
{
    auto const max_packet_size = uint16{960 * 6};
    pkt.resize(max_packet_size, uninitialized);

    auto const ret = ::opus_multistream_decode_float(
        handle_.get(),
        std::exchange(source_data_, nullptr),
        std::exchange(source_size_, 0),
        pkt.data(), max_packet_size, 0);

    if (ret < 0) {
        raise(errc::failure, "failed to decoder Opus packet: %s",
              ::opus_strerror(ret));
    }

    pkt.resize(static_cast<std::size_t>(ret) * channels_);
    return audio::decode_status::none;
}

void decoder::flush()
{
    ::opus_multistream_decoder_ctl(handle_.get(), OPUS_RESET_STATE);

    source_data_ = nullptr;
    source_size_ = 0;
}

uint32 decoder::get_decoder_delay() const noexcept
{
    return preroll_;
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::opus);

}}}   // namespace amp::opus::<unnamed>

