////////////////////////////////////////////////////////////////////////////////
//
// audio/replaygain.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_80FF4690_9EF9_4573_89D4_D9C136988BD9
#define AMP_INCLUDED_80FF4690_9EF9_4573_89D4_D9C136988BD9


#include <amp/audio/utility.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>

#include <cmath>


namespace amp {
namespace media {
    class dictionary;
}


namespace audio {

class packet;


enum class replaygain_mode : uint8 {
    none  = 0,
    track = 1,
    album = 2,
};

class replaygain_info
{
public:
    void reset(media::dictionary const&);

    float album_gain() const noexcept
    { return access_(album_gain_, track_gain_, 0.f); }

    float album_peak() const noexcept
    { return access_(album_peak_, track_peak_, 1.f); }

    float track_gain() const noexcept
    { return access_(track_gain_, album_gain_, 0.f); }

    float track_peak() const noexcept
    { return access_(track_peak_, album_peak_, 1.f); }

private:
    static float access_(float const a, float const b, float const c) noexcept
    { return !std::isinf(a) ? a : !std::isinf(b) ? b : c; }

    float album_gain_{inf<float>};
    float album_peak_{inf<float>};
    float track_gain_{inf<float>};
    float track_peak_{inf<float>};
};

class replaygain_config
{
public:
    explicit replaygain_config(replaygain_mode const m = {},
                               float           const p = {}) noexcept :
        preamp_(p),
        mode_(m)
    {}

    float compute_scale(replaygain_info const& info) const noexcept
    {
        float gain;
        float peak;

        if (mode_ == replaygain_mode::track) {
            gain = info.track_gain();
            peak = info.track_peak();
        }
        else if (mode_ == replaygain_mode::album) {
            gain = info.album_gain();
            peak = info.album_peak();
        }
        else {
            return 1.f;
        }

        auto scale = audio::to_amplitude(gain + preamp_);
        if (scale * peak > 1.f) {
            scale = 1.f / peak;
        }
        return scale;
    }

private:
    float preamp_;
    replaygain_mode mode_;
};

class replaygain_filter
{
public:
    void process(audio::packet&) const noexcept;

    void calibrate(replaygain_info const& info) noexcept
    { scale_ = config_.compute_scale(info); }

    void reset(replaygain_config const& x) noexcept
    { config_ = x; }

private:
    float scale_{1.f};
    replaygain_config config_;
};

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_80FF4690_9EF9_4573_89D4_D9C136988BD9

