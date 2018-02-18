////////////////////////////////////////////////////////////////////////////////
//
// media/title_format.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_CAA37AA9_811F_4951_9509_2867482A828B
#define AMP_INCLUDED_CAA37AA9_811F_4951_9509_2867482A828B


#include <amp/stddef.hpp>
#include <amp/u8string.hpp>


namespace amp {
namespace media {

// '•' is U+2022
constexpr char const default_window_title_format[] =
    u8"[%artist%  •  ][%album%  •  ]%title%";


class track;

class title_format
{
public:
    constexpr title_format() noexcept :
        state_{nullptr}
    {}

    title_format(title_format&& x) noexcept :
        state_{std::exchange(x.state_, nullptr)}
    {}

    title_format& operator=(title_format&& x) & noexcept
    {
        title_format{std::move(x)}.swap(*this);
        return *this;
    }

    ~title_format()
    {
        title_format::destroy_state_(state_);
    }

    void swap(title_format& x) noexcept
    {
        using std::swap;
        swap(state_, x.state_);
    }

    void compile(u8string const&);
    u8string operator()(media::track const&) const;

private:
    void* state_;

    static void destroy_state_(void*) noexcept;
};

}}    // namespace amp::media


#endif  // AMP_INCLUDED_CAA37AA9_811F_4951_9509_2867482A828B

