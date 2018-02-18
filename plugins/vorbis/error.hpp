////////////////////////////////////////////////////////////////////////////////
//
// plugins/vorbis/error.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_1BE01AB8_367D_459D_B843_A648863448CB
#define AMP_INCLUDED_1BE01AB8_367D_459D_B843_A648863448CB


#include <amp/error.hpp>
#include <amp/utility.hpp>

#include <vorbis/codec.h>


namespace amp {
namespace vorbis {

[[noreturn]] inline void raise(int const ret)
{
    raise([&]{
        switch (ret) {
        case OV_EOF:
            return errc::end_of_file;
        case OV_EREAD:
            return errc::read_fault;
        case OV_EFAULT:
            return errc::invalid_pointer;
        case OV_EINVAL:
            return errc::invalid_argument;
        case OV_EIMPL:
        case OV_EVERSION:
            return errc::not_implemented;
        case OV_HOLE:
        case OV_ENOTAUDIO:
        case OV_ENOTVORBIS:
        case OV_EBADHEADER:
        case OV_EBADPACKET:
        case OV_EBADLINK:
            return errc::invalid_data_format;
        case OV_ENOSEEK:
            return errc::seek_error;
        case OV_FALSE:
        default:
            return errc::failure;
        }
    }());
}

template<typename Ret>
AMP_INLINE auto verify(Ret const ret)
{
    if (AMP_LIKELY(ret >= 0)) {
        return as_unsigned(ret);
    }
    vorbis::raise(static_cast<int>(ret));
}

}}    // namespace amp::vorbis


#endif  // AMP_INCLUDED_1BE01AB8_367D_459D_B843_A648863448CB

