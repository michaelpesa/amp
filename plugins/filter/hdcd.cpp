////////////////////////////////////////////////////////////////////////////////
//
// plugins/filter/hdcd.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/bitops.hpp>
#include <amp/io/buffer.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include "hdcd_data.hpp"

#include <algorithm>
#include <utility>


namespace amp {
namespace hdcd {
namespace {

AMP_INLINE void apply_gain(int32& x, uint32 const g) noexcept
{
    x = static_cast<int32>((int64{x} * gaintab[g]) >> 23);
}


enum class code_result : uint8 {
    none = 0,
    A,
    A_almost,
    B,
    B_checkfail,
    expect_A,
    expect_B,
};

inline auto code(uint32 const bits, uint8& out) noexcept
{
    if ((bits & 0x0fa00500) == 0x0fa00500) {
        if ((bits & 0xc8) == 0) {
            out = (bits & 0xff) + (bits & 0x7);
            return hdcd::code_result::A;
        }
        return hdcd::code_result::A_almost;
    }
    if ((bits & 0xa0060000) == 0xa0060000) {
        if (((bits ^ (~bits >> 8 & 0xff)) & 0xffff00ff) == 0xa0060000) {
            out = (bits >> 8) & 0xff;
            return hdcd::code_result::B;
        }
        return hdcd::code_result::B_checkfail;
    }
    if (bits == 0x7e0fa005) {
        return hdcd::code_result::expect_A;
    }
    if (bits == 0x7e0fa006) {
        return hdcd::code_result::expect_B;
    }
    return hdcd::code_result::none;
}


struct channel_state
{
    uint64 window{};
    uint8  readahead{32};
    uint8  arg{};
    uint8  control{};
    uint32 sustain{};
    uint32 sustain_init{44100 * 10};
    uint32 running_gain{};
    uint32 code_counterA{};
    uint32 code_counterA_almost{};
    uint32 code_counterB{};
    uint32 code_counterB_checkfails{};
    uint32 code_counterC{};
    uint32 code_counterC_unmatched{};
    uint32 peak_extended_count{};
    uint32 transient_filter_count{};
    uint32 gain_counts[16]{};
    uint32 max_gain{};
    int32  sustain_expired_count{-1};

    void update_info() noexcept
    {
        if (control & 0x10) {
            ++peak_extended_count;
        }
        if (control & 0x20) {
            ++transient_filter_count;
        }
        ++gain_counts[control & 0xf];
        max_gain = std::max(max_gain, (uint32{control} & 0xf));
    }

    void sustain_reset() noexcept
    {
        sustain = sustain_init;
        if (sustain_expired_count == -1) {
            sustain_expired_count = 0;
        }
    }
};


constexpr double gain_to_float(uint32 const g) noexcept
{
    return (g != 0) ? -double(g >> 1) - ((g & 1) ? .5 : 0) : 0;
}


class filter
{
public:
    filter()
    {
        std::fprintf(stderr, "\n[HDCD] gaintab = {");
        for (auto const x : gaintab) {
            std::fprintf(stderr, "\n\t%.6f (%#08x)",
                         static_cast<double>(x) / double{1 << 23}, x);
        }
        std::fprintf(stderr, "\n}\n");
    }

    void calibrate(audio::format&);
    void process(audio::packet<float>&);
    void drain(audio::packet<float>&) noexcept;
    void flush();
    uint64 get_latency() const noexcept;

private:
    void process_(int32*, uint32);
    void control_(bool&, bool&);
    uint32 scan_(int32 const*, uint32);
    uint32 integrate_(int32 const*, uint32, uint32&);
    uint32 envelope_(int32*, uint32, uint32, bool) const noexcept;

    hdcd::channel_state  state[2];
    uint32               target_gain{};
    io::buffer           buf;
};


void filter::calibrate(audio::format& fmt)
{
    if (fmt.channels != 2 || fmt.sample_rate != 44100) {
        raise(errc::unsupported_format,
              "HDCD only present in CD audio (stereo, 44100 Hz)");
    }
}

void filter::process(audio::packet<float>& pkt)
{
    auto const frames = pkt.size() / 2;
    buf.resize(frames * 2 * sizeof(int32), uninitialized);
    auto data = reinterpret_cast<int32*>(buf.data());

    constexpr auto scale = float{1L << 15};
    for (auto const i : xrange(pkt.size())) {
        data[i] = static_cast<int32>(pkt[i] * scale);
    }

    process_(data, frames);

    constexpr char const status[2][20] = {
        "\033[00;31m" "NO"  "\033[00;00m",
        "\033[00;34m" "YES" "\033[00;00m",
    };

    std::fprintf(stderr, "[HDCD] sustain = {%s, %s}\n",
                 status[!!state[0].sustain],
                 status[!!state[1].sustain]);
    std::fprintf(stderr, "[HDCD] transient filter = {%s, %s}\n",
                 status[!!state[0].transient_filter_count],
                 status[!!state[1].transient_filter_count]);

    constexpr auto inv_scale = 1.f / float{1L << 31};
    for (auto const i : xrange(pkt.size())) {
        pkt[i] = static_cast<float>(data[i]) * inv_scale;
    }
}

void filter::drain(audio::packet<float>&) noexcept
{
}

void filter::flush()
{
    state[0] = {};
    state[1] = {};
    target_gain = 0;
}

uint64 filter::get_latency() const noexcept
{
    return 0;
}


uint32 filter::envelope_(int32* data, uint32 frames, uint32 gain,
                         bool const extend) const noexcept
{
    if (extend) {
        for (auto const i : xrange(frames)) {
            auto const x = data[i*2];
            auto const a = std::abs(x) - 0x5981;
            data[i*2] = (a >= 0)
                      ? (x >= 0 ? int32(peaktab[a]) : -int32(peaktab[a]))
                      : (x << 15);
        }
    }
    else {
        for (auto const i : xrange(frames)) {
            data[i*2] <<= 15;
        }
    }

    if (gain <= target_gain) {
        auto n = std::min(frames, target_gain - gain);
        frames -= n;

        while (n-- != 0) {
            apply_gain(*data, ++gain);
            data += 2;
        }
    }
    else {
        auto n = std::min(frames, (gain - target_gain) >> 3);
        frames -= n;

        while (n-- != 0) {
            gain -= 8;
            apply_gain(*data, gain);
            data += 2;
        }
        if (gain < target_gain + 8) {
            gain = target_gain;
        }
    }

    if (gain == 0) {
        data += (frames * 2);
    }
    else {
        while (--frames != 0) {
            apply_gain(*data, gain);
            data += 2;
        }
    }

    return gain;
}

void filter::process_(int32* data, uint32 frames)
{
    auto gain0 = state[0].running_gain;
    auto gain1 = state[1].running_gain;

    bool peak_extend0, peak_extend1;
    control_(peak_extend0, peak_extend1);

    auto lead = uint32{0};
    while (frames > lead) {
        auto const run = scan_(data + (lead * 2), frames - lead) + lead;
        auto const envelope_run = run - 1;
        if (envelope_run != 0) {
            gain0 = envelope_(data + 0, envelope_run, gain0, peak_extend0);
            gain1 = envelope_(data + 1, envelope_run, gain1, peak_extend1);
        }

        data   += envelope_run * 2;
        frames -= envelope_run;
        lead    = run - envelope_run;

        control_(peak_extend0, peak_extend1);
    }

    if (lead != 0) {
        gain0 = envelope_(data + 0, lead, gain0, peak_extend0);
        gain1 = envelope_(data + 1, lead, gain1, peak_extend1);
    }

    state[0].running_gain = gain0;
    state[1].running_gain = gain1;
}

void filter::control_(bool& peak_extend0, bool& peak_extend1)
{
    peak_extend0 = !!(state[0].control & 0x10);
    peak_extend1 = !!(state[1].control & 0x10);

    auto const target_gain0 = (uint32{state[0].control} & 0xf) << 7;
    auto const target_gain1 = (uint32{state[1].control} & 0xf) << 7;

    if (target_gain0 == target_gain1) {
        target_gain = target_gain0;
    }
    else {
        raise(errc::failure,
              "[HDCD] unmatched target gain (L=%0.1f R=%01.f avg=%0.1f)",
              gain_to_float(target_gain0 >> 7),
              gain_to_float(target_gain1 >> 7),
              gain_to_float(target_gain  >> 7));
    }
}

uint32 filter::scan_(int32 const* data, uint32 max)
{
    bool cdt_active[2] = {false, false};

    for (auto i = 0U; i != 2; ++i) {
        if (state[i].sustain > 0) {
            cdt_active[i] = true;
            if (state[i].sustain <= max) {
                state[i].control = 0;
                max = state[i].sustain;
            }
            state[i].sustain -= max;
        }
    }

    auto ret = uint32{};
    while (ret < max) {
        uint32 flag;
        auto const consumed = integrate_(data, max - ret, flag);
        ret  += consumed;
        data += consumed * 2;

        if (flag & 0x1) { state[0].sustain_reset(); }
        if (flag & 0x2) { state[1].sustain_reset(); }
    }

    for (auto i = 0U; i != 2; ++i) {
        if (cdt_active[i] && state[i].sustain == 0) {
            ++state[i].sustain_expired_count;
        }
    }
    return ret;
}

uint32 filter::integrate_(int32 const* data, uint32 const frames, uint32& flag)
{
    flag = 0;

    auto ret = frames;
    ret = std::min(ret, uint32{state[0].readahead});
    ret = std::min(ret, uint32{state[1].readahead});

    uint32 bits[2] = {0, 0};
    for (auto i = ret; i != 0; --i) {
        bits[0] |= static_cast<uint32>(*(data++) & 0x1) << (i - 1);
        bits[1] |= static_cast<uint32>(*(data++) & 0x1) << (i - 1);
    }

    for (auto i = 0U; i != 2; ++i) {
        auto&& s = state[i];

        s.window = (s.window << ret) | bits[i];
        s.readahead -= ret;
        if (s.readahead != 0) {
            continue;
        }

        auto const w = s.window;
        auto const wbits = static_cast<uint32>(w ^ (w >> 5) ^ (w >> 23));
        if (s.arg) {
            switch (hdcd::code(wbits, s.control)) {
            case hdcd::code_result::A:
                ++s.code_counterA;
                flag |= (1 << i);
                break;
            case hdcd::code_result::B:
                ++s.code_counterB;
                flag |= (1 << i);
                break;
            case hdcd::code_result::A_almost:
                ++s.code_counterA_almost;
                std::fprintf(stderr,
                             "[HDCD] {%u} control A almost: %#02x\n",
                             i, wbits & 0xff);
                break;
            case hdcd::code_result::B_checkfail:
                ++s.code_counterB_checkfails;
                std::fprintf(stderr,
                             "[HDCD] {%u} control B check failed: %#04x "
                             "(%#02x vs %#02x)\n",
                             i, wbits & 0xffff,
                             (wbits >> 8) & 0xff, ~wbits & 0xff);
                break;
            case hdcd::code_result::none:
                ++s.code_counterC_unmatched;
                std::fprintf(stderr,
                             "[HDCD] {%u} unmatched code: %#08x\n",
                             i, wbits);
                break;
            case hdcd::code_result::expect_A:
            case hdcd::code_result::expect_B:
                std::fprintf(stderr, "[HDCD] unexpected code result!\n");
                std::terminate();
            }

            if (flag & (1 << i)) {
                s.update_info();
            }
            s.arg = 0;
        }
        if (wbits == 0x7e0fa005 || wbits == 0x7e0fa006){
            s.readahead = static_cast<uint8>((wbits & 3) * 8);
            s.arg = 1;
            ++s.code_counterC;
        }
        else if (wbits != 0) {
            s.readahead = readaheadtab[wbits & 0xff];
        }
        else {
            s.readahead = 31;
        }
    }
    return ret;
}

AMP_REGISTER_FILTER(
    filter,
    "amp.filter.hdcddecoder",
    "HDCD decoder");

}}}   // namespace amp::hdcd::<unnamed>


