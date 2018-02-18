////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/format.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/error.hpp>
#include <amp/bitops.hpp>
#include <amp/cxp/map.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <cinttypes>
#include <cstring>


namespace amp {
namespace audio {
namespace codec {

static constexpr cxp::map<uint32, char const*, 87> id_names {{
    { aac_lc,            "AAC LC" },
    { aac_scalable,      "AAC Scalable" },
    { aac_eld,           "AAC ELD" },
    { aac_eld_sbr,       "AAC ELD (with SBR)" },
    { aac_eld_v2,        "AAC ELD v2" },
    { he_aac_v1,         "HE-AAC v1" },
    { aac_ld,            "AAC LD" },
    { aac_main,          "AAC Main" },
    { he_aac_v2,         "HE-AAC v2" },
    { aac_ssr,           "AAC SSR" },
    { aac_ltp,           "AAC LTP" },
    { ac3,               "AC-3" },
    { adpcm_adx,         "ADX ADPCM" },
    { alac,              "Apple Lossless" },
    { alaw,              "A-law" },
    { als,               "ALS" },
    { lpcm_dvda,         "DVD-A Linear PCM" },
    { atrac1,            "ATRAC1" },
    { atrac3,            "ATRAC3" },
    { atrac3_plus,       "ATRAC3+" },
    { lpcm_bd,           "Bluray Linear PCM" },
    { cook,              "Cook" },
    { lpcm_dvd,          "DVD Linear PCM" },
    { dsd,               "DSD" },
    { dts,               "DTS" },
    { dts_express,       "DTS Express" },
    { dts_hd,            "DTS-HD" },
    { dts_hd_ma,         "DTS-HD Master Audio" },
    { dv_audio,          "DV Audio" },
    { eac3,              "E-AC-3" },
    { flac,              "FLAC" },
    { g723_1,            "G.723.1" },
    { adpcm_g722,        "G.722 ADPCM" },
    { adpcm_g726,        "G.726 ADPCM" },
    { g729,              "G.729" },
    { gsm,               "GSM" },
    { ilbc,              "iLBC" },
    { adpcm_ima_qt,      "QuickTime IMA ADPCM" },
    { lpcm,              "Linear PCM" },
    { monkeys_audio,     "Monkey's Audio" },
    { mace3,             "MACE 3:1" },
    { mace6,             "MACE 6:1" },
    { mlp,               "MLP" },
    { musepack_sv7,      "Musepack SV7" },
    { musepack_sv8,      "Musepack SV8" },
    { mpeg_layer1,       "MP1" },
    { mpeg_layer2,       "MP2" },
    { mpeg_layer3,       "MP3" },
    { adpcm_ms,          "Microsoft ADPCM" },
    { adpcm_ima_oki,     "Dialogic OKI IMA ADPCM" },
    { adpcm_ima_ms,      "Microsoft IMA ADPCM" },
    { adpcm_yamaha,      "Yamaha ADPCM" },
    { truespeech,        "TrueSpeech" },
    { gsm_ms,            "Microsoft GSM" },
    { adpcm_ima_dk4,     "DK4 IMA ADPCM" },
    { adpcm_ima_dk3,     "DK3 IMA ADPCM" },
    { adpcm_creative,    "Creative ADPCM" },
    { intel_music_coder, "Intel Music Coder" },
    { indeo_audio,       "Indeo Audio" },
    { adpcm_swf,         "Shockwave Flash ADPCM" },
    { dpcm_xan,          "Xan DPCM" },
    { nellymoser,        "Nellymoser ASAO" },
    { optimfrog,         "OptimFROG" },
    { opus,              "Opus" },
    { qcelp,             "Qualcomm PureVoice" },
    { qdesign2,          "QDesign Music Codec 2" },
    { qdesign,           "QDesign Music Codec" },
    { ra_14_4,           "RealAudio 1.0 (14.4K)" },
    { ra_28_8,           "RealAudio 2.0 (28.8K)" },
    { ra_lossless,       "RealAudio Lossless" },
    { amr_nb,            "AMR-NB" },
    { amr_wb,            "AMR-WB" },
    { amr_wb_plus,       "AMR-WB+" },
    { shorten,           "Shorten" },
    { sipr,              "RealAudio SIPR" },
    { speex,             "Speex" },
    { tak,               "TAK" },
    { truehd,            "TrueHD" },
    { tta,               "TTA" },
    { ulaw,              "µ-law" },
    { vorbis,            "Vorbis" },
    { wma_v1,            "WMA Standard v1" },
    { wma_v2,            "WMA Standard v2" },
    { wma_lossless,      "WMA Lossless" },
    { wma_pro,           "WMA Professional" },
    { wma_voice,         "WMA Voice" },
    { wavpack,           "WavPack" },
}};
static_assert(cxp::is_sorted(id_names), "");


u8string name(uint32 const id)
{
    auto const pos = id_names.find(id);
    if (pos != id_names.end()) {
        return u8string::from_utf8_unchecked(pos->second);
    }
    return u8format("%#x", id);
}

}     // namespace codec


void format::validate() const
{
    if (channels != popcnt(channel_layout)) {
        raise(errc::unsupported_format,
              "channel count (%" PRIu32 ") does not match the computed "
              "channel count (%" PRIu32 ")",
              channels, popcnt(channel_layout));
    }
    if (channels < min_channels || channels > max_channels) {
        raise(errc::unsupported_format,
              "invalid channel count: %" PRIu32
              " (valid channel counts: [%" PRIu32 ", %" PRIu32 "]",
              channels, min_channels, max_channels);
    }
    if (sample_rate < min_sample_rate || sample_rate > max_sample_rate) {
        raise(errc::unsupported_format,
              "invalid sample rate: %" PRIu32
              " (valid sample rates: [%" PRIu32 ", %" PRIu32 "]",
              sample_rate, min_sample_rate, max_sample_rate);
    }
}

}}    // namespace amp::audio

