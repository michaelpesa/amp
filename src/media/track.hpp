////////////////////////////////////////////////////////////////////////////////
//
// media/track.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C7FADA6B_CC4C_45B9_928F_21CCEC34668B
#define AMP_INCLUDED_C7FADA6B_CC4C_45B9_928F_21CCEC34668B


#include <amp/aux/operators.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/net/uri.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <chrono>
#include <utility>


namespace amp {
namespace media {

class track :
    private equality_comparable<track>
{
public:
    net::uri   location;
    dictionary tags;
    dictionary info;
    uint64     start_offset{};
    uint64     frames{};
    uint32     sample_rate{};
    uint32     channel_layout{};
    uint32     chapter{};

    AMP_INLINE void swap(track& x) noexcept
    {
        using std::swap;
        swap(location,     x.location);
        swap(tags,         x.tags);
        swap(info,         x.info);
        swap(start_offset, x.start_offset);
        swap(frames,       x.frames);
        swap(sample_rate,  x.sample_rate);
        swap(chapter,      x.chapter);
    }

    template<typename Duration = std::chrono::milliseconds>
    AMP_INLINE Duration length() const noexcept
    { return Duration{muldiv(frames, Duration::period::den, sample_rate)}; }

private:
    friend bool operator==(track const& x, track const& y) noexcept
    { return (x.location == y.location) && (x.chapter == y.chapter); }
};

AMP_INLINE void swap(track& x, track& y) noexcept
{
    x.swap(y);
}

}}    // namespace amp::media


#endif  // AMP_INCLUDED_C7FADA6B_CC4C_45B9_928F_21CCEC34668B

