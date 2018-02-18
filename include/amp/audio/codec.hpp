////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/codec.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_43BE5038_A3CD_422A_A6B3_B1FBF3CBA6ED
#define AMP_INCLUDED_43BE5038_A3CD_422A_A6B3_B1FBF3CBA6ED


#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>


namespace amp {
namespace audio {
namespace codec {

AMP_EXPORT
u8string name(uint32 const id);


constexpr auto aac_lc            = "aac "_4cc;
constexpr auto aac_ld            = "aacl"_4cc;
constexpr auto aac_ltp           = "aact"_4cc;
constexpr auto aac_scalable      = "aacc"_4cc;
constexpr auto aac_ssr           = "aacr"_4cc;
constexpr auto aac_main          = "aacm"_4cc;
constexpr auto aac_eld           = "aace"_4cc;
constexpr auto aac_eld_sbr       = "aacf"_4cc;
constexpr auto aac_eld_v2        = "aacg"_4cc;
constexpr auto he_aac_v1         = "aach"_4cc;
constexpr auto he_aac_v2         = "aacp"_4cc;
constexpr auto ac3               = "ac3 "_4cc;
constexpr auto alac              = "alac"_4cc;
constexpr auto als               = "als "_4cc;
constexpr auto amr_nb            = "samr"_4cc;
constexpr auto amr_wb            = "sawb"_4cc;
constexpr auto amr_wb_plus       = "sawp"_4cc;
constexpr auto atrac1            = "atr1"_4cc;
constexpr auto atrac3            = "atrc"_4cc;
constexpr auto atrac3_plus       = "atrp"_4cc;
constexpr auto cook              = "cook"_4cc;
constexpr auto dsd               = "dsd "_4cc;
constexpr auto dts               = "dts "_4cc;
constexpr auto dts_express       = "dtse"_4cc;
constexpr auto dts_hd            = "dtsh"_4cc;
constexpr auto dts_hd_ma         = "dtsl"_4cc;
constexpr auto dv_audio          = "dvau"_4cc;
constexpr auto eac3              = "eac3"_4cc;
constexpr auto flac              = "flac"_4cc;
constexpr auto g723_1            = "g72\x31"_4cc;
constexpr auto g729              = "g729"_4cc;
constexpr auto gsm               = "gsm "_4cc;
constexpr auto gsm_ms            = "ms\x00\x31"_4cc;
constexpr auto ilbc              = "ilbc"_4cc;
constexpr auto indeo_audio       = "ms\x04\x02"_4cc;
constexpr auto intel_music_coder = "ms\x04\x01"_4cc;
constexpr auto mace3             = "mac3"_4cc;
constexpr auto mace6             = "mac6"_4cc;
constexpr auto mlp               = "mlp "_4cc;
constexpr auto monkeys_audio     = "mac "_4cc;
constexpr auto mpeg_layer1       = "mpg1"_4cc;
constexpr auto mpeg_layer2       = "mpg2"_4cc;
constexpr auto mpeg_layer3       = "mpg3"_4cc;
constexpr auto musepack_sv7      = "mpc7"_4cc;
constexpr auto musepack_sv8      = "mpc8"_4cc;
constexpr auto nellymoser        = "nmos"_4cc;
constexpr auto optimfrog         = "ofr "_4cc;
constexpr auto opus              = "opus"_4cc;
constexpr auto qcelp             = "qclp"_4cc;
constexpr auto qdesign           = "qdmc"_4cc;
constexpr auto qdesign2          = "qdm2"_4cc;
constexpr auto ra_14_4           = "ra\x14\x04"_4cc;
constexpr auto ra_28_8           = "ra\x28\x08"_4cc;
constexpr auto ra_lossless       = "ralf"_4cc;
constexpr auto shorten           = "shn "_4cc;
constexpr auto sipr              = "sipr"_4cc;
constexpr auto speex             = "spx "_4cc;
constexpr auto tak               = "tak "_4cc;
constexpr auto truehd            = "trhd"_4cc;
constexpr auto truespeech        = "ms\x00\x22"_4cc;
constexpr auto tta               = "tta "_4cc;
constexpr auto vorbis            = "vorb"_4cc;
constexpr auto wavpack           = "wvpk"_4cc;
constexpr auto wma_v1            = "wma1"_4cc;
constexpr auto wma_v2            = "wma2"_4cc;
constexpr auto wma_lossless      = "wmal"_4cc;
constexpr auto wma_pro           = "wmap"_4cc;
constexpr auto wma_voice         = "wmav"_4cc;

constexpr auto alaw              = "alaw"_4cc;
constexpr auto ulaw              = "ulaw"_4cc;
constexpr auto lpcm              = "lpcm"_4cc;
constexpr auto lpcm_bd           = "bpcm"_4cc;
constexpr auto lpcm_dvd          = "dpcm"_4cc;
constexpr auto lpcm_dvda         = "apcm"_4cc;

constexpr auto adpcm_adx         = "adx "_4cc;
constexpr auto adpcm_creative    = "ms\x00\xc0"_4cc;
constexpr auto adpcm_g722        = "g722"_4cc;
constexpr auto adpcm_g726        = "g726"_4cc;
constexpr auto adpcm_ima_dk3     = "ms\x00\x62"_4cc;
constexpr auto adpcm_ima_dk4     = "ms\x00\x61"_4cc;
constexpr auto adpcm_ima_ms      = "ms\x00\x11"_4cc;
constexpr auto adpcm_ima_oki     = "ms\x00\x10"_4cc;
constexpr auto adpcm_ima_qt      = "ima4"_4cc;
constexpr auto adpcm_ms          = "ms\x00\x02"_4cc;
constexpr auto adpcm_swf         = "ms\x53\x46"_4cc;
constexpr auto adpcm_yamaha      = "ms\x00\x20"_4cc;

constexpr auto dpcm_xan          = "ms\x59\x4a"_4cc;

}}}   // namespace amp::audio::codec


#endif  // AMP_INCLUDED_43BE5038_A3CD_422A_A6B3_B1FBF3CBA6ED

