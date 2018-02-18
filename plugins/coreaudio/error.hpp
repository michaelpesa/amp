////////////////////////////////////////////////////////////////////////////////
//
// plugins/coreaudio/error.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_616F8B72_5822_4D74_B271_0768ED308D9C
#define AMP_INCLUDED_616F8B72_5822_4D74_B271_0768ED308D9C


#include <amp/error.hpp>
#include <amp/stddef.hpp>

#include <CoreAudio/AudioHardwareBase.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>


namespace amp {
namespace coreaudio {

[[noreturn]] inline void raise(OSStatus const ret)
{
    if (ret == kAudio_MemFullError) {
        raise_bad_alloc();
    }

    auto const ec = [ret]{
        switch (ret) {
        case kAudioDeviceUnsupportedFormatError:
        case kAudioUnitErr_FormatNotSupported:
        case kAudioConverterErr_FormatNotSupported:
        case kAudioConverterErr_InputSampleRateOutOfRange:
        case kAudioConverterErr_OutputSampleRateOutOfRange:
            return errc::unsupported_format;
        case kAudioConverterErr_OperationNotSupported:
        case kAudioDevicePermissionsError:
        case kAudioUnitErr_Unauthorized:
            return errc::access_denied;
        case kAudio_ParamError:
        case kAudioConverterErr_InvalidInputSize:
        case kAudioConverterErr_InvalidOutputSize:
        case kAudioConverterErr_PropertyNotSupported:
        case kAudioConverterErr_RequiresPacketDescriptionsError:
        case kAudioHardwareBadDeviceError:
        case kAudioHardwareBadObjectError:
        case kAudioHardwareBadPropertySizeError:
        case kAudioHardwareBadStreamError:
        case kAudioHardwareUnknownPropertyError:
        case kAudioUnitErr_InvalidElement:
        case kAudioUnitErr_InvalidParameter:
        case kAudioUnitErr_InvalidProperty:
        case kAudioUnitErr_InvalidPropertyValue:
        case kAudioUnitErr_InvalidScope:
        case kAudioUnitErr_PropertyNotWritable:
        case kAudioUnitErr_TooManyFramesToProcess:
            return errc::invalid_argument;
        case kAudio_UnimplementedError:
            return errc::not_implemented;
        case kAudio_BadFilePathError:
        case kAudio_FileNotFoundError:
            return errc::file_not_found;
        case kAudio_TooManyFilesOpenError:
            return errc::too_many_open_files;
        default:
            return errc::failure;
        }
    }();
    raise(ec, "CoreAudio status: %#x", ret);
}

AMP_INLINE void verify(OSStatus const ret)
{
    if (AMP_UNLIKELY(ret != noErr)) {
        coreaudio::raise(ret);
    }
}

}}    // namespace amp::coreaudio


#endif  // AMP_INCLUDED_616F8B72_5822_4D74_B271_0768ED308D9C

