////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/demuxer.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_1F722665_56BC_4D19_A38C_E71C5815B2BE
#define AMP_INCLUDED_1F722665_56BC_4D19_A38C_E71C5815B2BE


#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/io/buffer.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <memory>
#include <utility>


namespace amp {
namespace audio {

template<typename Derived>
class basic_demuxer
{
private:
    enum class state : uint32 {
        send = 0x0,
        recv = 0x1,
        eos  = 0x2,
    };

public:
    void read(audio::packet& pkt)
    {
        do {
            switch (state_) {
            case state::eos:
                return;
            case state::send:
                if (static_cast<Derived&>(*this).feed(rdbuf_)) {
                    state_ = state::recv;
                }
                else {
                    rdbuf_.clear();
                    state_ = state::eos;
                    instant_bit_rate = average_bit_rate;
                }
                decoder->send(rdbuf_);
                [[fallthrough]];
            case state::recv:
                if (!(decoder->recv(pkt) & decode_status::incomplete)) {
                    state_ = static_cast<state>(as_underlying(state_) & 0x2);
                }

                if (priming_ != 0) {
                    auto const n = std::min(priming_, uint64{pkt.frames()});
                    priming_ -= n;
                    pkt.pop_front(static_cast<uint32>(n) * pkt.channels());
                }

                pts_ += pkt.frames();
                if (pts_ > total_frames) {
                    auto const n = (pts_ - total_frames) * pkt.channels();
                    pkt.pop_back(static_cast<uint32>(n));
                    pts_ = total_frames;
                    state_ = state::eos;
                }
            }
        }
        while (pkt.empty());
        pkt.set_bit_rate(instant_bit_rate);
    }

    audio::format get_format() const noexcept
    {
        audio::format out;
        out.channels       = format.channels;
        out.channel_layout = format.channel_layout;
        out.sample_rate    = format.sample_rate;
        return out;
    }

protected:
    basic_demuxer() = default;
   ~basic_demuxer() = default;

    void set_seek_target_and_offset(uint64 const target, uint64 const offset)
    {
        decoder->flush();
        state_ = state::send;
        pts_ = target;
        reset_priming_(offset);
    }

    void set_encoder_delay(uint32 const x) noexcept
    {
        encoder_delay_ = x;
        reset_priming_(0);
    }

    void set_total_frames(uint64 const x) noexcept
    {
        total_frames = x;
    }

    [[nodiscard]] bool try_resolve_decoder()
    {
        try {
            decoder = audio::decoder::resolve(format);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    [[nodiscard]] bool try_resolve_decoder(audio::codec_format fmt)
    {
        try {
            decoder = audio::decoder::resolve(fmt);
            format = std::move(fmt);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    void resolve_decoder()
    {
        decoder = audio::decoder::resolve(format);
        AMP_ASSERT(decoder != nullptr);
    }

    ref_ptr<audio::decoder> decoder;
    audio::codec_format format;
    uint64 total_frames{};
    uint32 instant_bit_rate{};
    uint32 average_bit_rate{};

private:
    void reset_priming_(uint64 const x) noexcept
    {
        priming_ = decoder->get_decoder_delay() + encoder_delay_ + x;
    }

    io::buffer rdbuf_;
    uint64 priming_{};
    uint64 pts_{};
    uint32 encoder_delay_{};
    state state_{state::send};
};

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_1F722665_56BC_4D19_A38C_E71C5815B2BE

