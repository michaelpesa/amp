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
    std::unique_ptr<::OpusMSDecoder> handle;
    uint8 const* source_data{};
    int32        source_size{};
    uint32 const channels;
};

decoder::decoder(audio::codec_format& fmt) :
    channels{fmt.channels}
{
    fmt.sample_rate = 48000;

    auto gain_dB = int16{0};
    auto chanmap = uint8{0};
    auto preroll = uint16{0};
    auto& config = fmt.extra;

    if (config.size() >= opus::head_size) {
        preroll = io::load<uint16,LE>(&config[10]);
        gain_dB = io::load< int16,LE>(&config[16]);
        chanmap = config[18];
    }

    uint8  mapping_array[] { 0, 1, 0, 0, 0, 0, 0, 0, };
    uint8* mapping{};

    auto coupled = uint8{0};
    auto streams = uint8{0};

    if (config.size() >= opus::head_size + 2 + channels) {
        streams = config[opus::head_size + 0];
        coupled = config[opus::head_size + 1];
        if (streams + coupled != channels) {
            raise(errc::invalid_data_format, "invalid Opus config");
        }
        mapping = &config[opus::head_size + 2];
    }
    else {
        if (channels > 2 || chanmap != 0) {
            raise(errc::invalid_data_format, "invalid Opus config");
        }
        streams = 1;
        coupled = (channels > 1);
        mapping = mapping_array;
    }

    if (channels > 2 && channels <= 8) {
        auto const offsets = channel_layout_offsets[channels - 1];
        for (auto const ch : xrange(channels)) {
            mapping_array[ch] = mapping[offsets[ch]];
        }
        mapping = mapping_array;
    }

    int error{};
    handle.reset(::opus_multistream_decoder_create(
            48000, static_cast<int>(channels),
            streams, coupled,
            mapping, &error));

    if (AMP_UNLIKELY(handle == nullptr)) {
        raise(errc::failure, "failed to create Opus decoder: %s",
              ::opus_strerror(error));
    }

    std::fprintf(stderr,
                 "\n[opus.decoder] {"
                 "\n\t   gain : %d"
                 "\n\tstreams : %u"
                 "\n\tcoupled : %u"
                 "\n\tchanmap : %u"
                 "\n\tpreroll : %u"
                 "\n}"
                 "\n",
                 gain_dB, streams, coupled, chanmap, preroll);
}

void decoder::send(io::buffer& buf)
{
    source_data = buf.data();
    source_size = numeric_cast<int32>(buf.size());
}

auto decoder::recv(audio::packet& pkt)
{
    auto const max_packet_size = uint16{960 * 6};
    pkt.resize(max_packet_size, uninitialized);

    auto const ret = ::opus_multistream_decode_float(
        handle.get(),
        std::exchange(source_data, nullptr),
        std::exchange(source_size, 0),
        pkt.data(), max_packet_size, 0);

    if (ret < 0) {
        raise(errc::failure, "failed to decoder Opus packet: %s",
              ::opus_strerror(ret));
    }

    pkt.resize(static_cast<std::size_t>(ret) * channels);
    return audio::decode_status::none;
}

void decoder::flush()
{
    ::opus_multistream_decoder_ctl(handle.get(), OPUS_RESET_STATE);

    source_data = nullptr;
    source_size = 0;
}

uint32 decoder::get_decoder_delay() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::opus);

}}}   // namespace amp::opus::<unnamed>

