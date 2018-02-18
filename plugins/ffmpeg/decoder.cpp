////////////////////////////////////////////////////////////////////////////////
//
// plugins/ffmpeg/decoder.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/memory.hpp>
#include <amp/numeric.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstring>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}


namespace std {

template<>
struct default_delete<::AVFrame>
{
    void operator()(::AVFrame* frame) const noexcept
    {
        ::av_frame_free(&frame);
    }
};

template<>
struct default_delete<::AVCodecContext>
{
    void operator()(::AVCodecContext* const context) const noexcept
    {
        ::avcodec_close(context);
        ::av_free(context);
    }
};

}     // namespace std


namespace amp {
namespace ffmpeg {
namespace {

[[noreturn]] void raise(int const ret)
{
    char str[256];
    if (::av_strerror(ret, str, sizeof(str)) < 0) {
        std::strcpy(str, "FFmpeg error code");
    }
    auto const ec = (ret == AVERROR_EOF) ? errc::end_of_file : errc::failure;
    raise(ec, "FFmpeg: %s", str);
}

AMP_INLINE uint verify(int const ret)
{
    if (AMP_LIKELY(ret >= 0)) {
        return static_cast<uint>(ret);
    }
    ffmpeg::raise(ret);
}

inline ::AVCodecID map_codec_id(uint32 const codec_id) noexcept
{
    namespace ac = audio::codec;

    switch (codec_id) {
    case ac::ac3:               return ::AV_CODEC_ID_AC3;
    case ac::adpcm_adx:         return ::AV_CODEC_ID_ADPCM_ADX;
    case ac::alac:              return ::AV_CODEC_ID_ALAC;
    case ac::als:               return ::AV_CODEC_ID_MP4ALS;
    case ac::atrac1:            return ::AV_CODEC_ID_ATRAC1;
    case ac::atrac3:            return ::AV_CODEC_ID_ATRAC3;
    case ac::atrac3_plus:       return ::AV_CODEC_ID_ATRAC3P;
    case ac::cook:              return ::AV_CODEC_ID_COOK;
    case ac::dts:               return ::AV_CODEC_ID_DTS;
    case ac::dts_express:       return ::AV_CODEC_ID_DTS;
    case ac::dts_hd:            return ::AV_CODEC_ID_DTS;
    case ac::dts_hd_ma:         return ::AV_CODEC_ID_DTS;
    case ac::eac3:              return ::AV_CODEC_ID_EAC3;
    case ac::flac:              return ::AV_CODEC_ID_FLAC;
    case ac::g723_1:            return ::AV_CODEC_ID_G723_1;
    case ac::adpcm_g722:        return ::AV_CODEC_ID_ADPCM_G722;
    case ac::adpcm_g726:        return ::AV_CODEC_ID_ADPCM_G726;
    case ac::g729:              return ::AV_CODEC_ID_G729;
    case ac::gsm:               return ::AV_CODEC_ID_GSM;
    case ac::gsm_ms:            return ::AV_CODEC_ID_GSM_MS;
    case ac::adpcm_ima_qt:      return ::AV_CODEC_ID_ADPCM_IMA_QT;
    case ac::monkeys_audio:     return ::AV_CODEC_ID_APE;
    case ac::mpeg_layer1:       return ::AV_CODEC_ID_MP1;
    case ac::mpeg_layer2:       return ::AV_CODEC_ID_MP2;
    case ac::mpeg_layer3:       return ::AV_CODEC_ID_MP3;
    case ac::mace3:             return ::AV_CODEC_ID_MACE3;
    case ac::mace6:             return ::AV_CODEC_ID_MACE6;
    case ac::mlp:               return ::AV_CODEC_ID_MLP;
    case ac::adpcm_ms:          return ::AV_CODEC_ID_ADPCM_MS;
    case ac::adpcm_ima_oki:     return ::AV_CODEC_ID_ADPCM_IMA_OKI;
    case ac::adpcm_ima_ms:      return ::AV_CODEC_ID_ADPCM_IMA_WAV;
    case ac::adpcm_yamaha:      return ::AV_CODEC_ID_ADPCM_YAMAHA;
    case ac::truespeech:        return ::AV_CODEC_ID_TRUESPEECH;
    case ac::adpcm_ima_dk4:     return ::AV_CODEC_ID_ADPCM_IMA_DK4;
    case ac::adpcm_ima_dk3:     return ::AV_CODEC_ID_ADPCM_IMA_DK3;
    case ac::adpcm_creative:    return ::AV_CODEC_ID_ADPCM_CT;
    case ac::intel_music_coder: return ::AV_CODEC_ID_IMC;
    case ac::indeo_audio:       return ::AV_CODEC_ID_IAC;
    case ac::adpcm_swf:         return ::AV_CODEC_ID_ADPCM_SWF;
    case ac::dpcm_xan:          return ::AV_CODEC_ID_XAN_DPCM;
    case ac::nellymoser:        return ::AV_CODEC_ID_NELLYMOSER;
    case ac::qcelp:             return ::AV_CODEC_ID_QCELP;
    case ac::qdesign2:          return ::AV_CODEC_ID_QDM2;
    case ac::ra_14_4:           return ::AV_CODEC_ID_RA_144;
    case ac::ra_28_8:           return ::AV_CODEC_ID_RA_288;
    case ac::ra_lossless:       return ::AV_CODEC_ID_RALF;
    case ac::amr_nb:            return ::AV_CODEC_ID_AMR_NB;
    case ac::amr_wb:            return ::AV_CODEC_ID_AMR_WB;
    case ac::sipr:              return ::AV_CODEC_ID_SIPR;
    case ac::tak:               return ::AV_CODEC_ID_TAK;
    case ac::truehd:            return ::AV_CODEC_ID_TRUEHD;
    case ac::tta:               return ::AV_CODEC_ID_TTA;
    case ac::wma_v1:            return ::AV_CODEC_ID_WMAV1;
    case ac::wma_v2:            return ::AV_CODEC_ID_WMAV2;
    case ac::wma_lossless:      return ::AV_CODEC_ID_WMALOSSLESS;
    case ac::wma_pro:           return ::AV_CODEC_ID_WMAPRO;
    case ac::wma_voice:         return ::AV_CODEC_ID_WMAVOICE;
    case ac::wavpack:           return ::AV_CODEC_ID_WAVPACK;
    }
    AMP_UNREACHABLE();
}

inline auto open_codec_context(audio::codec_format& fmt)
{
    auto const codec = ::avcodec_find_decoder(map_codec_id(fmt.codec_id));
    if (AMP_UNLIKELY(codec == nullptr)) {
        raise(errc::protocol_not_supported);
    }

    std::unique_ptr<::AVCodecContext> context{::avcodec_alloc_context3(codec)};
    if (AMP_UNLIKELY(context == nullptr)) {
        raise_bad_alloc();
    }

    if (fmt.codec_id == audio::codec::alac) {
        constexpr uint8 const preamble[12] {
            0x00, 0x00, 0x00, 0x24,
            0x61, 0x6c, 0x61, 0x63,
            0x00, 0x00, 0x00, 0x00,
        };
        fmt.extra.insert(fmt.extra.cbegin(), preamble, sizeof(preamble));
    }
    fmt.extra.reserve(fmt.extra.size() + AV_INPUT_BUFFER_PADDING_SIZE);

    context->extradata             = fmt.extra.data();
    context->extradata_size        = numeric_cast<int>(fmt.extra.size());
    context->sample_rate           = numeric_cast<int>(fmt.sample_rate);
    context->block_align           = numeric_cast<int>(fmt.bytes_per_packet);
    context->bits_per_coded_sample = numeric_cast<int>(fmt.bits_per_sample);
    context->bit_rate              = numeric_cast<int>(fmt.bit_rate);
    context->channels              = numeric_cast<int>(fmt.channels);
    context->channel_layout        = fmt.channel_layout;

    verify(::avcodec_open2(context.get(), codec, nullptr));
    return context;
}


class decoder
{
public:
    explicit decoder(audio::codec_format&);

    void send(io::buffer&);
    auto recv(audio::packet&);
    void flush();

    uint32 get_decoder_delay() const noexcept;

private:
    void create_blitter();

    std::unique_ptr<audio::pcm::blitter> blitter;
    std::unique_ptr<::AVCodecContext> context;
    std::unique_ptr<::AVFrame> frame;
    ::AVPacket packet;
    bool planar{false};
};


decoder::decoder(audio::codec_format& fmt) :
    context{open_codec_context(fmt)},
    frame{::av_frame_alloc()}
{
    if (AMP_UNLIKELY(frame == nullptr)) {
        raise_bad_alloc();
    }

    if (context->sample_fmt != ::AV_SAMPLE_FMT_NONE) {
        create_blitter();
    }
    if (fmt.channel_layout == 0) {
        fmt.channel_layout = numeric_cast<uint32>(context->channel_layout);
    }
}

void decoder::send(io::buffer& buf)
{
    ::av_init_packet(&packet);
    if (!buf.empty()) {
        buf.reserve(buf.size() + AV_INPUT_BUFFER_PADDING_SIZE);
        packet.data = buf.data();
        packet.size = numeric_cast<int>(buf.size());
    }
    else {
        packet.data = nullptr;
        packet.size = 0;
    }
    verify(::avcodec_send_packet(context.get(), &packet));
}

auto decoder::recv(audio::packet& pkt)
{
    auto const ret = ::avcodec_receive_frame(context.get(), frame.get());
    if (ret != 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            pkt.clear();
            return audio::decode_status::none;
        }
        ffmpeg::raise(ret);
    }

    if (AMP_UNLIKELY(blitter == nullptr)) {
        create_blitter();
    }

    pkt.set_channel_layout(static_cast<uint32>(frame->channel_layout),
                           static_cast<uint32>(frame->channels));

    auto const frames = static_cast<uint>(frame->nb_samples);
    auto const source = planar
                      ? static_cast<void const*>(frame->data)
                      : static_cast<void const*>(frame->data[0]);

    blitter->convert(source, frames, pkt);
    return audio::decode_status::incomplete;
}

void decoder::flush()
{
    ::avcodec_flush_buffers(context.get());
}

uint32 decoder::get_decoder_delay() const noexcept
{
    switch (as_underlying(context->codec_id)) {
    case ::AV_CODEC_ID_MP1:
    case ::AV_CODEC_ID_MP2:
        return 240 + 1;
    case ::AV_CODEC_ID_MP3:
        return 528 + 1;
    default:
        return 0;
    }
}

void decoder::create_blitter()
{
    audio::pcm::spec spec;
    spec.channels = numeric_cast<uint32>(context->channels);
    spec.flags = audio::pcm::host_endian;

    switch (context->sample_fmt) {
    case ::AV_SAMPLE_FMT_U8P:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_U8:
        spec.bits_per_sample = 8;
        break;
    case ::AV_SAMPLE_FMT_S16P:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_S16:
        spec.flags |= audio::pcm::signed_int;
        spec.bits_per_sample = 16;
        break;
    case ::AV_SAMPLE_FMT_S32P:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_S32:
        spec.flags |= audio::pcm::signed_int;
        spec.bits_per_sample = 32;
        break;
    case ::AV_SAMPLE_FMT_S64P:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_S64:
        spec.flags |= audio::pcm::signed_int;
        spec.bits_per_sample = 64;
        break;
    case ::AV_SAMPLE_FMT_FLTP:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_FLT:
        spec.flags |= audio::pcm::ieee_float;
        spec.bits_per_sample = 32;
        break;
    case ::AV_SAMPLE_FMT_DBLP:
        planar = true;
        [[fallthrough]];
    case ::AV_SAMPLE_FMT_DBL:
        spec.flags |= audio::pcm::ieee_float;
        spec.bits_per_sample = 64;
        break;
    case ::AV_SAMPLE_FMT_NONE:
    case ::AV_SAMPLE_FMT_NB:
        raise(errc::unsupported_format, "invalid LibAV sample format: %#x",
              context->sample_fmt);
    }

    if (planar) {
        spec.flags |= audio::pcm::non_interleaved;
    }
    spec.bytes_per_sample = spec.bits_per_sample / 8;
    blitter = audio::pcm::blitter::create(spec);
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::ac3,
    audio::codec::alac,
    audio::codec::als,
    audio::codec::amr_nb,
    audio::codec::amr_wb,
    audio::codec::atrac1,
    audio::codec::atrac3,
    audio::codec::atrac3_plus,
    audio::codec::cook,
    audio::codec::dts,
    audio::codec::dts_express,
    audio::codec::dts_hd,
    audio::codec::dts_hd_ma,
    audio::codec::eac3,
    audio::codec::flac,
    audio::codec::g723_1,
    audio::codec::g729,
    audio::codec::gsm,
    audio::codec::gsm_ms,
    audio::codec::indeo_audio,
    audio::codec::intel_music_coder,
    audio::codec::mace3,
    audio::codec::mace6,
    audio::codec::mlp,
    audio::codec::monkeys_audio,
    audio::codec::mpeg_layer1,
    audio::codec::mpeg_layer2,
    audio::codec::mpeg_layer3,
    audio::codec::nellymoser,
    audio::codec::qcelp,
    audio::codec::qdesign2,
    audio::codec::ra_14_4,
    audio::codec::ra_28_8,
    audio::codec::ra_lossless,
    audio::codec::sipr,
    audio::codec::tak,
    audio::codec::tta,
    audio::codec::truehd,
    audio::codec::truespeech,
    audio::codec::wavpack,
    audio::codec::wma_v1,
    audio::codec::wma_v2,
    audio::codec::wma_pro,
    audio::codec::wma_lossless,
    audio::codec::wma_voice,
    audio::codec::adpcm_adx,
    audio::codec::adpcm_creative,
    audio::codec::adpcm_ima_dk3,
    audio::codec::adpcm_ima_dk4,
    audio::codec::adpcm_g722,
    audio::codec::adpcm_g726,
    audio::codec::adpcm_ima_ms,
    audio::codec::adpcm_ima_oki,
    audio::codec::adpcm_ima_qt,
    audio::codec::adpcm_ms,
    audio::codec::adpcm_swf,
    audio::codec::adpcm_yamaha,
    audio::codec::dpcm_xan);

}}}   // namespace amp::ffmpeg::<unnamed>

