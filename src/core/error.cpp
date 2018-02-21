////////////////////////////////////////////////////////////////////////////////
//
// core/error.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <system_error>

#if defined(AMP_HAS_POSIX)
# include <cerrno>
#elif defined(_WIN32)
# include "core/aux/winapi.hpp"
#endif


namespace amp {

static char const* error_message_(errc const e) noexcept
{
    switch (e) {
    case errc::unexpected:
        return "unexpected error occurred";
    case errc::out_of_bounds:
        return "attempted to access out-of-bounds data";
    case errc::object_disposed:
        return "the object has been disposed of";
    case errc::not_implemented:
        return "function not implemented";
    case errc::invalid_cast:
        return "cannot cast object to an unsupported interface";
    case errc::invalid_pointer:
        return "attempted to dereference an invalid pointer";
    case errc::failure:
        return "unspecified error";
    case errc::protocol_not_supported:
        return "requested protocol is not implemented";
    case errc::file_not_found:
        return "file not found";
    case errc::too_many_open_files:
        return "too many open files";
    case errc::access_denied:
        return "access denied";
    case errc::seek_error:
        return "cannot seek to position";
    case errc::write_fault:
        return "cannot write to device";
    case errc::read_fault:
        return "cannot read from device";
    case errc::end_of_file:
        return "reached the end of the file";
    case errc::invalid_argument:
        return "function received invalid argument(s)";
    case errc::arithmetic_overflow:
        return "conversion would cause arithmetic overflow";
    case errc::invalid_unicode:
        return "string contains invalid Unicode character(s)";
    case errc::invalid_data_format:
        return "invalid data format for operation";
    case errc::unsupported_format:
        return "unsupported format";
    }
}

void raise(errc const e)
{
    throw std::runtime_error{error_message_(e)};
}

void raise(errc const e, char const* const format, ...)
{
    va_list ap1;
    va_start(ap1, format);
    AMP_SCOPE_EXIT { va_end(ap1); };

    va_list ap2;
    va_copy(ap2, ap1);
    AMP_SCOPE_EXIT { va_end(ap2); };

    auto const ret = std::vsnprintf(nullptr, 0, format, ap1);
    if (ret <= 0) {
        raise((ret < 0) ? errc::invalid_argument : e);
    }

    auto const n = static_cast<std::size_t>(ret);
    auto const msg = std::string_view{error_message_(e)};
    auto const buf = std::make_unique<char[]>(msg.size() + 2 + n);

    auto dst = std::copy(msg.begin(), msg.end(), buf.get());
    *dst++ = ':';
    *dst++ = ' ';
    std::vsnprintf(dst, n + 1, format, ap2);
    throw std::runtime_error(buf.get());
}

void raise_bad_alloc()
{
    throw std::bad_alloc();
}

void raise_system_error(int const ev)
{
    throw std::system_error{ev, std::system_category()};
}

void raise_current_system_error()
{
#if defined(_WIN32)
    raise_system_error(static_cast<int>(::GetLastError()));
#else
    raise_system_error(errno);
#endif
}

}     // namespace amp

