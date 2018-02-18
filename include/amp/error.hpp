////////////////////////////////////////////////////////////////////////////////
//
// amp/error.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C9E39240_7A82_488E_A1EF_98F6D3092EC4
#define AMP_INCLUDED_C9E39240_7A82_488E_A1EF_98F6D3092EC4


#include <amp/stddef.hpp>

#include <cstddef>


namespace amp {

enum class errc : uint32 {
    unexpected             = 0x8000ffff,
    out_of_bounds          = 0x8000000b,
    object_disposed        = 0x80000013,
    not_implemented        = 0x80004001,
    invalid_cast           = 0x80004002,
    invalid_pointer        = 0x80004003,
    failure                = 0x80004005,
    protocol_not_supported = 0x8001273b,
    file_not_found         = 0x80070002,
    too_many_open_files    = 0x80070004,
    access_denied          = 0x80070005,
    seek_error             = 0x80070019,
    write_fault            = 0x8007001d,
    read_fault             = 0x8007001e,
    end_of_file            = 0x80070026,
    invalid_argument       = 0x80070057,
    arithmetic_overflow    = 0x80070216,
    invalid_unicode        = 0x80070459,
    invalid_data_format    = 0x83760002,
    unsupported_format     = 0x88890008,
};


[[noreturn]] AMP_EXPORT
void raise(errc);
[[noreturn]] AMP_EXPORT AMP_PRINTF_FORMAT(2, 3)
void raise(errc, AMP_PRINTF_FORMAT_STRING char const*, ...);
[[noreturn]] AMP_EXPORT
void raise_bad_alloc();

[[noreturn]] void raise_system_error(int);
[[noreturn]] void raise_current_system_error();

}     // namespace amp


#endif  // AMP_INCLUDED_C9E39240_7A82_488E_A1EF_98F6D3092EC4

