////////////////////////////////////////////////////////////////////////////////
//
// core/aux/dynamic_library.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3C25DE1C_0E88_43DF_9D1E_EF5E92B89345
#define AMP_INCLUDED_3C25DE1C_0E88_43DF_9D1E_EF5E92B89345


#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include <memory>

#if defined(AMP_HAS_POSIX)
# include <dlfcn.h>
#elif defined(_WIN32)
# include "core/aux/winapi.hpp"
#else
# error "dynamic library loading not implemented on this platform"
#endif


namespace amp {

class dynamic_library
{
public:
#ifdef AMP_HAS_POSIX
    using native_handle_type = void*;

    explicit dynamic_library(u8string const& path) :
        handle{::dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW)}
    {
        if (AMP_UNLIKELY(handle == nullptr)) {
            auto const error = ::dlerror();
            raise(errc::failure, "::dlopen failed: %s",
                  error ? error : "unspecified error");
        }
    }

    uintptr resolve(char const* const name) const
    {
        auto const symbol = ::dlsym(handle.get(), name);
        if (AMP_LIKELY(symbol != nullptr)) {
            return reinterpret_cast<uintptr>(symbol);
        }
        auto const error = ::dlerror();
        raise(errc::failure, "::dlsym failed: %s",
              error ? error : "unspecified error");
    }

#else   // AMP_HAS_POSIX

    using native_handle_type = ::HMODULE;

    explicit dynamic_library(u8string const& path)
    {
        auto mode = ::DWORD{SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX};
        if (!::SetThreadErrorMode(mode, &mode)) {
            raise_current_system_error();
        }
# ifdef __cplusplus_winrt
        handle.reset(::LoadPackagedLibrary(widen(path).c_str(), 0));
# else
        handle.reset(::LoadLibraryW(widen(path).c_str()));
# endif
        ::SetThreadErrorMode(mode, nullptr);

        if (AMP_UNLIKELY(handle == nullptr)) {
            raise_current_system_error();
        }
    }

    uintptr resolve(char const* const name) const
    {
        if (auto const symbol = ::GetProcAddress(handle.get(), name)) {
            return reinterpret_cast<uintptr>(symbol);
        }
        raise_current_system_error();
    }

#endif  // AMP_HAS_POSIX

    dynamic_library(dynamic_library&&) = default;
    dynamic_library& operator=(dynamic_library&&) & = default;

    void detach() noexcept
    { handle.release(); }

    native_handle_type native_handle() const noexcept
    { return handle.get(); }

    static constexpr char const* file_extension() noexcept
    {
#if defined(_WIN32)
        return "dll";
#elif defined(__APPLE__) && defined(__MACH__)
        return "dylib";
#else
        return "so";
#endif
    }

private:
    struct deleter_type
    {
        void operator()(native_handle_type const h) const noexcept
        {
#ifdef AMP_HAS_POSIX
            ::dlclose(h);
#else
            ::FreeLibrary(h);
#endif
        }
    };

    std::unique_ptr<remove_pointer_t<native_handle_type>, deleter_type> handle;
};

}     // namespace amp


#endif  // AMP_INCLUDED_3C25DE1C_0E88_43DF_9D1E_EF5E92B89345

