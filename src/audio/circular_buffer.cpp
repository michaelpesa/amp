////////////////////////////////////////////////////////////////////////////////
//
// audio/circular_buffer.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/stddef.hpp>

#include "audio/circular_buffer.hpp"

#include <cstddef>

#if defined(__APPLE__) && defined(__MACH__)
# include <mach/kern_return.h>
# include <mach/mach_init.h>
# include <mach/vm_inherit.h>
# include <mach/vm_map.h>
# include <mach/vm_prot.h>
# include <mach/vm_statistics.h>
# include <mach/vm_types.h>
#elif defined(AMP_HAS_POSIX)
# include <amp/scope_guard.hpp>
# include <stdlib.h>
# include <sys/mman.h>
# include <unistd.h>
#endif


namespace amp {
namespace audio {
namespace aux {

#if defined(__APPLE__) && defined(__MACH__)

void* allocate_mirrored(std::size_t const size)
{
    auto const task = mach_task_self();

    for (auto attempts = 3; attempts != 0; --attempts) {
        ::vm_address_t buffer, mirror;

        auto ret = ::vm_allocate(task, &buffer, size * 2, VM_FLAGS_ANYWHERE);
        if (AMP_UNLIKELY(ret != KERN_SUCCESS)) {
            continue;
        }

        mirror = buffer + size;
        ret = ::vm_deallocate(task, mirror, size);
        if (AMP_UNLIKELY(ret != KERN_SUCCESS)) {
            ::vm_deallocate(task, buffer, size);
            continue;
        }

        ::vm_prot_t cur_protection;
        ::vm_prot_t max_protection;
        ret = ::vm_remap(task,                  // target process (this one)
                         &mirror,               // target address
                         size,                  // target size
                         0,                     // target alignment (any)
                         false,                 // flags (none)
                         task,                  // source process (this one)
                         buffer,                // source address
                         0,                     // read-write mode
                         &cur_protection,       // current protection (unused)
                         &max_protection,       // maximum protection (unused)
                         VM_INHERIT_DEFAULT);   // inheritance

        if (AMP_UNLIKELY(ret != KERN_SUCCESS)) {
            ::vm_deallocate(task, buffer, size);
            continue;
        }
        if (AMP_UNLIKELY(mirror != (buffer + size))) {
            ::vm_deallocate(task, mirror, size);
            ::vm_deallocate(task, buffer, size);
            continue;
        }
        return reinterpret_cast<void*>(buffer);
    }
    raise_bad_alloc();
}

void deallocate_mirrored(void* const ptr, std::size_t const size) noexcept
{
    auto const addr = reinterpret_cast<::vm_address_t>(ptr);
    ::vm_deallocate(mach_task_self(), addr, size * 2);
}

#elif defined(AMP_HAS_POSIX)

void* allocate_mirrored(std::size_t const size)
{
    char path[] = "/tmp/amp-circular-buffer-XXXXXX";
    auto const fd = ::mkstemp(path);
    if (fd < 0) {
        raise(errc::failure, "failed to create temporary file");
    }

    AMP_SCOPE_EXIT { ::close(fd); };
    ::unlink(path);

    if (::ftruncate(fd, static_cast<::off_t>(size * 2)) < 0) {
        raise(errc::failure, "failed to grow temporary file's size");
    }

    auto const buffer = static_cast<uchar*>(::mmap(nullptr, size * 2,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED,
                                                   fd, 0));
    if (reinterpret_cast<intptr>(buffer) == -1) {
        raise_bad_alloc();
    }

    auto const mirror = static_cast<uchar*>(::mmap(buffer + size, size,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_FIXED | MAP_SHARED,
                                                   fd, 0));
    if (mirror != (buffer + size)) {
        ::munmap(buffer, size * 2);
        raise_bad_alloc();
    }
    return buffer;
}

void deallocate_mirrored(void* const ptr, std::size_t const size) noexcept
{
    ::munmap(ptr, size * 2);
}

#endif  // __APPLE__ && __MACH__

}}}   // namespace amp::audio::aux

