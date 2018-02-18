////////////////////////////////////////////////////////////////////////////////
//
// plugins/coreaudio/output.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/format.hpp>
#include <amp/audio/output.hpp>
#include <amp/error.hpp>
#include <amp/flat_set.hpp>
#include <amp/functional.hpp>
#include <amp/range.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include "error.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>


namespace std {

template<>
struct default_delete<remove_pointer_t<AudioUnit>>
{
    void operator()(AudioUnit const unit) const noexcept
    {
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
    }
};

}     // namespace std


namespace amp {
namespace coreaudio {
namespace {

class output_stream final :
    public implement_ref_count<output_stream, audio::output_stream>
{
public:
    explicit output_stream(AudioDeviceID const device_id) :
        unit_([]{
            AudioComponentDescription acd{};
            acd.componentType         = kAudioUnitType_Output;
            acd.componentSubType      = kAudioUnitSubType_HALOutput;
            acd.componentManufacturer = kAudioUnitManufacturer_Apple;

            auto const component = AudioComponentFindNext(nullptr, &acd);
            if (AMP_UNLIKELY(component == nullptr)) {
                raise(errc::unexpected,
                      "failed to find HAL audio output component");
            }

            AudioUnit instance;
            verify(AudioComponentInstanceNew(component, &instance));
            return instance;
        }())
    {
        AURenderCallbackStruct rcbs{};
        rcbs.inputProc       = &output_stream::render;
        rcbs.inputProcRefCon = this;

        verify(AudioUnitSetProperty(unit_.get(),
                                    kAudioUnitProperty_SetRenderCallback,
                                    kAudioUnitScope_Input,
                                    0, &rcbs, sizeof(rcbs)));

        verify(AudioUnitSetProperty(unit_.get(),
                                    kAudioOutputUnitProperty_CurrentDevice,
                                    kAudioUnitScope_Global,
                                    0, &device_id, sizeof(device_id)));

        verify(AudioUnitInitialize(unit_.get()));
    }

    void start(function_view<void(float*, uint32)> const f) override
    {
        feed_ = f;
        set_stream_format();
        verify(AudioOutputUnitStart(unit_.get()));
    }

    void stop() override
    {
        pause();
        feed_ = {};
    }

    void pause() override
    {
        verify(AudioOutputUnitStop(unit_.get()));
    }

    void flush() override
    {
    }

    void set_volume(float const volume) override
    {
        verify(AudioUnitSetParameter(unit_.get(),
                                     kHALOutputParam_Volume,
                                     kAudioUnitScope_Global,
                                     0, volume, 0));
    }

    float get_volume() override
    {
        float volume;
        verify(AudioUnitGetParameter(unit_.get(),
                                     kHALOutputParam_Volume,
                                     kAudioUnitScope_Global,
                                     0, &volume));
        return volume;
    }

    audio::format get_format() override
    {
        auto const asbd = get_stream_format();

        audio::format format;
        format.channels       = asbd.mChannelsPerFrame;
        format.channel_layout = audio::guess_channel_layout(format.channels);
        format.sample_rate    = static_cast<uint32>(asbd.mSampleRate);
        return format;
    }

private:
    static remove_pointer_t<AURenderCallback> render;

    AudioStreamBasicDescription get_stream_format() const
    {
        AudioStreamBasicDescription asbd;
        auto size = uint32{sizeof(asbd)};

        verify(AudioUnitGetProperty(unit_.get(),
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input,
                                    0, &asbd, &size));
        return asbd;
    }

    void set_stream_format()
    {
        auto asbd = get_stream_format();
        asbd.mFormatID        = kAudioFormatLinearPCM;
        asbd.mFormatFlags     = kAudioFormatFlagIsFloat;
        asbd.mFramesPerPacket = 1;
        asbd.mBitsPerChannel  = sizeof(float) * 8;
        asbd.mBytesPerFrame   = sizeof(float) * asbd.mChannelsPerFrame;
        asbd.mBytesPerPacket  = asbd.mBytesPerFrame * asbd.mFramesPerPacket;

        verify(AudioUnitSetProperty(unit_.get(),
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input,
                                    0, &asbd, sizeof(asbd)));
    }

    std::unique_ptr<remove_pointer_t<AudioUnit>> unit_;
    function_view<void(float*, uint32)> feed_;
};

OSStatus output_stream::render(
    void*                       const inRefCon,
    AudioUnitRenderActionFlags* const /* ioActionFlags */,
    AudioTimeStamp const*       const /* inTimeStamp */,
    uint32                      const /* inBusNumber */,
    uint32                      const inNumberFrames,
    AudioBufferList*            const ioData)
{
    if (AMP_LIKELY(ioData != nullptr)) {
        auto&& self = *static_cast<output_stream*>(inRefCon);
        self.feed_(static_cast<float*>(ioData->mBuffers[0].mData),
                   inNumberFrames);
    }
    return noErr;
}


inline auto get_string_property(AudioDeviceID const device_id,
                                AudioObjectPropertySelector const selector)
{
    CFStringRef data;
    auto size = uint32{sizeof(data)};
    auto addr = AudioObjectPropertyAddress {
        selector,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster,
    };

    verify(AudioObjectGetPropertyData(device_id, &addr, 0, nullptr,
                                      &size, &data));

    AMP_SCOPE_EXIT { CFRelease(data); };
    auto const len = CFStringGetLength(data);
    if (len <= 0) {
        return u8string{};
    }

    auto const bytes = static_cast<std::size_t>(len) * sizeof(UniChar);
    auto const buf = static_cast<UniChar*>(std::malloc(bytes));
    if (AMP_UNLIKELY(buf == nullptr)) {
        raise_bad_alloc();
    }

    AMP_SCOPE_EXIT { std::free(buf); };
    CFStringGetCharacters(data, CFRange{0, len}, buf);
    return u8string::from_utf16(reinterpret_cast<char16 const*>(buf),
                                static_cast<std::size_t>(len));
}

inline auto get_device_uid(AudioDeviceID const device_id)
{
    return get_string_property(device_id, kAudioDevicePropertyDeviceUID);
}

inline auto get_device_name(AudioDeviceID const device_id)
{
    return get_string_property(device_id, kAudioObjectPropertyName);
}

inline auto get_device_id(u8string const& uid)
{
    AudioDeviceID data;
    auto size = uint32{sizeof(data)};
    auto addr = AudioObjectPropertyAddress {
        kAudioHardwarePropertyTranslateUIDToDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster,
    };

    auto const qualifier = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<uint8 const*>(uid.data()),
        static_cast<CFIndex>(uid.size()),
        kCFStringEncodingUTF8,
        false);

    AMP_SCOPE_EXIT { CFRelease(qualifier); };
    verify(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                      sizeof(qualifier), &qualifier,
                                      &size, &data));
    return data;
}

inline auto get_default_device_id()
{
    AudioDeviceID data;
    auto size = uint32{sizeof(data)};
    auto addr = AudioObjectPropertyAddress {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster,
    };

    verify(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &addr, 0, nullptr, &size, &data));
    return data;
}

inline auto make_device(AudioDeviceID const device_id)
{
    audio::output_device ret;
    ret.uid = get_device_uid(device_id);
    ret.name = get_device_name(device_id);
    return ret;
}

inline auto make_device(u8string const& uid)
{
    audio::output_device ret;
    ret.uid = uid;
    ret.name = get_device_name(get_device_id(uid));
    return ret;
}


class output_device_list final :
    public implement_ref_count<output_device_list, audio::output_device_list>
{
public:
    explicit output_device_list(flat_set<u8string> const& uids) :
        devices_(new AudioDeviceID[uids.size()]),
        count_(static_cast<uint32>(uids.size()))
    {
        std::transform(uids.begin(), uids.end(),
                       devices_.get(), get_device_id);
    }

    uint32 get_count() noexcept override
    { return count_; }

    audio::output_device get_device(uint32 const index) override
    { return make_device(devices_[index]); }

    audio::output_device get_default_device() override
    { return make_device(get_default_device_id()); }

private:
    std::unique_ptr<AudioDeviceID[]> devices_;
    uint32 count_;
};


class output_session final :
    public implement_ref_count<output_session, audio::output_session>
{
public:
    output_session() :
        uids_{build_device_list()}
    {
        set_power_saving_hint();

        auto addr = AudioObjectPropertyAddress {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMaster,
        };
        verify(AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr,
                                              devices_listener, this));

        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        verify(AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr,
                                              default_device_listener, this));
    }

    ~output_session()
    {
        auto addr = AudioObjectPropertyAddress {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMaster,
        };
        AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr,
                                          devices_listener, this);

        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr,
                                          default_device_listener, this);
    }

    ref_ptr<audio::output_device_list> get_devices() override
    {
        std::lock_guard<std::mutex> const lk{mtx_};
        return output_device_list::make(uids_);
    }

    ref_ptr<audio::output_stream> activate(u8string const& uid) override
    {
        auto const device_id = !uid.empty()
                             ? get_device_id(uid)
                             : get_default_device_id();

        return output_stream::make(device_id);
    }

    void set_delegate(audio::output_session_delegate* const d) override
    {
        delegate_ = d;
    }

private:
    static void set_power_saving_hint()
    {
        auto addr = AudioObjectPropertyAddress {
            kAudioHardwarePropertyPowerHint,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMaster,
        };
        auto hint = kAudioHardwarePowerHintFavorSavingPower;
        verify(AudioObjectSetPropertyData(kAudioObjectSystemObject, &addr,
                                          0, nullptr, sizeof(hint), &hint));
    }

    static flat_set<u8string> build_device_list()
    {
        auto size = uint32{};
        auto addr = AudioObjectPropertyAddress {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMaster,
        };
        verify(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                              &addr, 0, nullptr, &size));

        auto const count = size / uint32{sizeof(AudioDeviceID)};
        auto const device_ids = std::make_unique<AudioDeviceID[]>(count);

        verify(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                          0, nullptr, &size, device_ids.get()));

        flat_set<u8string> uids;
        for (auto const device_id : make_range(device_ids.get(), count)) {
            if (is_output_device(device_id)) {
                uids.insert(get_device_uid(device_id));
            }
        }
        return uids;
    }

    static bool is_output_device(AudioDeviceID const device_id)
    {
        auto size = uint32{};
        auto addr = AudioObjectPropertyAddress {
            kAudioDevicePropertyStreams,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster,
        };

        auto const ret = AudioObjectGetPropertyDataSize(device_id, &addr, 0,
                                                        nullptr, &size);
        return (ret == noErr) && (size != 0);
    }

    using ListenerProc = remove_pointer_t<AudioObjectPropertyListenerProc>;
    static ListenerProc devices_listener;
    static ListenerProc default_device_listener;

    audio::output_session_delegate* delegate_{};
    flat_set<u8string> uids_;
    mutable std::mutex mtx_;
};


OSStatus output_session::devices_listener(
    AudioObjectID                     const /* inObjectID */,
    uint32                            const /* inNumberAddresss */,
    AudioObjectPropertyAddress const* const /* inAddresses */,
    void*                             const inClientData)
{
    auto&& self = *static_cast<output_session*>(inClientData);

    auto const new_uids = build_device_list();
    auto const old_uids = [&]{
        std::lock_guard<std::mutex> const lk{self.mtx_};
        return std::exchange(self.uids_, new_uids);
    }();

    if (self.delegate_) {
        for (auto&& uid : new_uids) {
            if (old_uids.find(uid) == old_uids.end()) {
                self.delegate_->device_removed(uid);
            }
        }
        for (auto&& uid : old_uids) {
            if (new_uids.find(uid) == new_uids.end()) {
                self.delegate_->device_added(make_device(uid));
            }
        }
    }
    return noErr;
}

OSStatus output_session::default_device_listener(
    AudioObjectID                     const /* inObjectID */,
    uint32                            const /* inNumberAddresss */,
    AudioObjectPropertyAddress const* const /* inAddresses */,
    void*                             const inClientData)
{
    auto&& self = *static_cast<output_session*>(inClientData);
    if (self.delegate_) {
        self.delegate_->default_device_changed();
    }
    return noErr;
}


AMP_REGISTER_OUTPUT(
    output_session,
    "amp.output.coreaudio",
    "Core Audio");

}}}   // namespace amp::coreaudio::<unnamed>

