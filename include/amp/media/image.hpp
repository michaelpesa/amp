////////////////////////////////////////////////////////////////////////////////
//
// amp/media/image.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_B158DF5D_9BA7_474E_A356_55D015CD1946
#define AMP_INCLUDED_B158DF5D_9BA7_474E_A356_55D015CD1946


#include <amp/io/buffer.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <cstddef>


namespace amp {
namespace media {

enum class image_type : uint8 {
    other = 0,
    file_icon_32x32,
    file_icon_other,
    front_cover,
    back_cover,
    leaflet_page,
    media,
    lead_artist,
    artist,
    conductor,
    band,
    composer,
    lyricist,
    recording_location,
    during_recording,
    during_performance,
    video_screen_capture,
    bright_colored_fish,
    illustration,
    band_logotype,
    publisher_logotype,
};

class image
{
public:
    image() = default;

    explicit image(io::buffer x) noexcept :
        buf_{std::move(x)}
    {}

    explicit image(void const* const src, std::size_t const n) :
        buf_{src, n}
    {}

    void set_data(io::buffer x) noexcept
    { buf_ = std::move(x); }

    void set_data(void const* const src, std::size_t const len)
    { buf_.assign(src, len); }

    void set_mime_type(u8string x) noexcept
    { mime_type_ = std::move(x); }

    void set_description(u8string x) noexcept
    { description_ = std::move(x); }

    u8string const& mime_type() const noexcept
    { return mime_type_; }

    u8string const& description() const noexcept
    { return description_; }

    uint8 const* data() const noexcept
    { return buf_.data(); }

    std::size_t size() const noexcept
    { return buf_.size(); }

    bool empty() const noexcept
    { return buf_.empty(); }

private:
    u8string   mime_type_;
    u8string   description_;
    io::buffer buf_;
};

}}    // namespace amp::media


#endif  // AMP_INCLUDED_B158DF5D_9BA7_474E_A356_55D015CD1946

