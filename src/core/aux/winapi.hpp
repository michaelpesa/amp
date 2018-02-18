////////////////////////////////////////////////////////////////////////////////
//
// core/aux/winapi.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_7695E8D6_968C_4FA9_AF91_6D27D6D7EE61
#define AMP_INCLUDED_7695E8D6_968C_4FA9_AF91_6D27D6D7EE61


#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>


namespace amp {

inline auto widen(char const* const s, std::size_t const len)
{
    if (len == 0) {
        return std::wstring{};
    }

    auto const wlen = ::MultiByteToWideChar(CP_UTF8,
                                            MB_ERR_INVALID_CHARS,
                                            s, static_cast<int>(len),
                                            nullptr, 0);
    if (wlen > 0) {
        std::wstring ws(static_cast<std::size_t>(wlen), L'\0');
        ::MultiByteToWideChar(CP_UTF8,
                              MB_ERR_INVALID_CHARS,
                              s, static_cast<int>(len),
                              &ws[0], wlen);
        return ws;
    }
    raise(static_cast<int32>(::GetLastError()), win32_category());
}

inline auto widen(string_view const s)
{
    return widen(s.data(), s.size());
}

}     // namespace amp


#endif  // AMP_INCLUDED_7695E8D6_968C_4FA9_AF91_6D27D6D7EE61

