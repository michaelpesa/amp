////////////////////////////////////////////////////////////////////////////////
//
// plugins/coreaudio/decoder.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include "error.hpp"

#include <algorithm>
#include <memory>
#include <type_traits>

#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFormat.h>
#include <CoreAudio/CoreAudioTypes.h>


namespace std {

template<>
struct default_delete<std::remove_pointer_t<AudioConverterRef>>
{
    void operator()(AudioConverterRef const conv) const noexcept
    { AudioConverterDispose(conv); }
};

}     // namespace std


namespace amp {
namespace coreaudio {
namespace {

inline io::buffer reconstruct_es_descriptor(io::buffer const& asc)
{
    io::buffer esd{asc.size() + 5 + 3 + 5 + 13 + 5, uninitialized};
    auto dst = esd.data();

    auto append_descr = [&](uint8 const tag, std::size_t const size) {
        *dst++ = tag;
        *dst++ = static_cast<uint8>((size >> 21) | 0x80);
        *dst++ = static_cast<uint8>((size >> 14) | 0x80);
        *dst++ = static_cast<uint8>((size >>  7) | 0x80);
        *dst++ = static_cast<uint8>((size >>  0) & 0x7f);
    };
    auto append_bytes = [&](void const* const src, std::size_t const n) {
        dst = std::copy_n(static_cast<uchar const*>(src), n, dst);
    };

    // ES descriptor
    append_descr(0x03, asc.size() + 3 + 5 + 13 + 5);
    append_bytes("\x00\x00\x00", 3);

    // DecoderConfig descriptor
    append_descr(0x04, asc.size() + 13 + 5);
    append_bytes("\x40\x15", 2);           // object type + flags
    append_bytes("\x00\x00\x00", 3);       // buffer size DB
    append_bytes("\x00\x00\x00\x00", 4);   // maximum bit rate
    append_bytes("\x00\x00\x00\x00", 4);   // average bit rate

    // DecoderSpecificConfig descriptor
    append_descr(0x05, asc.size());
    append_bytes(asc.data(), asc.size());
    return esd;
}


class decoder
{
public:
    explicit decoder(audio::codec_format&);

    void send(io::buffer&) noexcept;
    auto recv(audio::packet&);
    void flush();

    uint32 get_decoder_delay() const noexcept;

private:
    static remove_pointer_t<AudioConverterComplexInputDataProc> callback;

    std::unique_ptr<remove_pointer_t<AudioConverterRef>> conv_;
    io::buffer buf_;
    AudioStreamBasicDescription iasbd_{};
    AudioStreamBasicDescription oasbd_{};
    AudioStreamPacketDescription aspd_{};
};


OSStatus decoder::callback(
    AudioConverterRef              const /* inAudioConverter */,
    UInt32*                        const ioNumberDataPackets,
    AudioBufferList*               const ioData,
    AudioStreamPacketDescription** const outDataPacketDescription,
    void*                          const inUserData)
{
    auto&& self = *static_cast<decoder*>(inUserData);

    auto const bytes = static_cast<uint32>(self.buf_.size());
    if (bytes != 0) {
        ioData->mBuffers[0].mData         = self.buf_.data();
        ioData->mBuffers[0].mDataByteSize = bytes;
        *ioNumberDataPackets              = 1;
    }
    else {
        ioData->mBuffers[0].mData         = nullptr;
        ioData->mBuffers[0].mDataByteSize = 0;
        *ioNumberDataPackets              = 0;
    }

    if (outDataPacketDescription != nullptr) {
        self.aspd_.mStartOffset            = 0;
        self.aspd_.mVariableFramesInPacket = 0;
        self.aspd_.mDataByteSize           = bytes;
        *outDataPacketDescription          = &self.aspd_;
    }
    return noErr;
}


decoder::decoder(audio::codec_format& fmt)
{
    iasbd_.mFramesPerPacket  = fmt.frames_per_packet;
    iasbd_.mSampleRate       = fmt.sample_rate;
    iasbd_.mChannelsPerFrame = fmt.channels;
    iasbd_.mFormatID         = [&]{
        switch (fmt.codec_id) {
        case audio::codec::aac_lc:      return kAudioFormatMPEG4AAC;
        case audio::codec::he_aac_v1:   return kAudioFormatMPEG4AAC_HE;
        case audio::codec::he_aac_v2:   return kAudioFormatMPEG4AAC_HE_V2;
        case audio::codec::aac_ld:      return kAudioFormatMPEG4AAC_LD;
        case audio::codec::aac_eld:     return kAudioFormatMPEG4AAC_ELD;
        case audio::codec::aac_eld_sbr: return kAudioFormatMPEG4AAC_ELD_SBR;
        case audio::codec::aac_eld_v2:  return kAudioFormatMPEG4AAC_ELD_V2;
        case audio::codec::qcelp:       return kAudioFormatQUALCOMM;
        case audio::codec::qdesign:     return kAudioFormatQDesign;
        case audio::codec::qdesign2:    return kAudioFormatQDesign2;
        }
        AMP_UNREACHABLE();
    }();

    oasbd_.mFramesPerPacket  = 1;
    oasbd_.mSampleRate       = iasbd_.mSampleRate;
    oasbd_.mChannelsPerFrame = iasbd_.mChannelsPerFrame;
    oasbd_.mBitsPerChannel   = sizeof(float) * 8;
    oasbd_.mBytesPerFrame    = sizeof(float) * oasbd_.mChannelsPerFrame;
    oasbd_.mBytesPerPacket   = oasbd_.mBytesPerFrame * oasbd_.mFramesPerPacket;
    oasbd_.mFormatID         = kAudioFormatLinearPCM;
    oasbd_.mFormatFlags      = kAudioFormatFlagIsFloat;

    conv_.reset([&]{
        AudioConverterRef out;
        verify(AudioConverterNew(&iasbd_, &oasbd_, &out));
        return out;
    }());

    auto priming = AudioConverterPrimeInfo{};
    verify(AudioConverterSetProperty(
            conv_.get(), kAudioConverterPrimeInfo,
            sizeof(priming), &priming));

    if (!fmt.extra.empty()) {
        io::buffer esd;
        auto cookie = &fmt.extra;

        if (fmt.codec_id == audio::codec::aac_lc      ||
            fmt.codec_id == audio::codec::he_aac_v1   ||
            fmt.codec_id == audio::codec::he_aac_v2   ||
            fmt.codec_id == audio::codec::aac_ld      ||
            fmt.codec_id == audio::codec::aac_eld     ||
            fmt.codec_id == audio::codec::aac_eld_sbr ||
            fmt.codec_id == audio::codec::aac_eld_v2) {
            esd = reconstruct_es_descriptor(*cookie);
            cookie = &esd;
        }
        verify(AudioConverterSetProperty(
                conv_.get(), kAudioConverterDecompressionMagicCookie,
                numeric_cast<uint32>(cookie->size()), cookie->data()));
    }
}

void decoder::send(io::buffer& buf) noexcept
{
    buf_.swap(buf);
}

auto decoder::recv(audio::packet& pkt)
{
    auto frames = iasbd_.mFramesPerPacket;
    pkt.resize(frames * oasbd_.mChannelsPerFrame, uninitialized);

    AudioBufferList buffers;
    buffers.mNumberBuffers = 1;
    buffers.mBuffers[0].mNumberChannels = oasbd_.mChannelsPerFrame;
    buffers.mBuffers[0].mDataByteSize   = oasbd_.mBytesPerFrame * frames;
    buffers.mBuffers[0].mData           = pkt.data();

    verify(AudioConverterFillComplexBuffer(
            conv_.get(), decoder::callback,
            this, &frames, &buffers, nullptr));

    pkt.resize(frames * oasbd_.mChannelsPerFrame, uninitialized);
    return audio::decode_status::none;
}

void decoder::flush()
{
    verify(AudioConverterReset(conv_.get()));
    buf_.clear();
}

uint32 decoder::get_decoder_delay() const noexcept
{
    if (iasbd_.mFormatID == kAudioFormatMPEG4AAC_HE ||
        iasbd_.mFormatID == kAudioFormatMPEG4AAC_HE_V2) {
        return iasbd_.mFramesPerPacket;
    }
    return 0;
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::aac_lc,
    audio::codec::he_aac_v1,
    audio::codec::he_aac_v2,
    audio::codec::aac_ld,
    audio::codec::aac_eld,
    audio::codec::aac_eld_sbr,
    audio::codec::aac_eld_v2,
    audio::codec::qcelp,
    audio::codec::qdesign,
    audio::codec::qdesign2);

}}}   // namespace amp::coreaudio::<unnamed>

