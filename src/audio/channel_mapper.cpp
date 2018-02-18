////////////////////////////////////////////////////////////////////////////////
//
// audio/channel_mapper.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/channel_mapper.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>

#include <algorithm>
#include <array>
#include <cinttypes>


namespace amp {
namespace audio {
namespace {

class channel_mapper_impl final :
    public channel_mapper
{
public:
    explicit channel_mapper_impl(string_view const mapping) noexcept :
        channels_{static_cast<uint32>(mapping.size())}
    {
        std::array<uint8, max_channels> ordered;
        std::copy(mapping.begin(), mapping.end(), ordered.begin());
        std::sort(ordered.begin(), ordered.begin() + channels_);

        for (auto const i : xrange(channels_)) {
            auto const ch = static_cast<uint8>(mapping[i]);

            auto const found = std::find(ordered.begin(), ordered.end(), ch);
            AMP_ASSERT(found != ordered.cend());

            offsets_[i] = static_cast<std::size_t>(found - ordered.begin());
            layout_ |= (uint32{1} << (ch - 1));
        }
    }

    void process(audio::packet& pkt) override
    {
        if (AMP_UNLIKELY(pkt.channels() != channels_)) {
            raise(errc::unsupported_format,
                  "channel mapper is not configured for packet");
        }

        std::array<float, max_channels> tmp;
        auto const last = pkt.end();
        for (auto first = pkt.begin(); first != last; ) {
            for (auto const i : xrange(channels_)) {
                tmp[offsets_[i]] = first[i];
            }
            for (auto const i : xrange(channels_)) {
                *first++ = tmp[i];
            }
        }
    }

    uint32 get_channel_layout() const noexcept override
    {
        return layout_;
    }

private:
    std::array<std::size_t, max_channels> offsets_;
    uint32 channels_;
    uint32 layout_{};
};


constexpr std::array<char const*, 55> channel_mappings {{
    "\x3",                              // mono
    "\x1\x2",                           // stereo
    "\x1\x2",                           // stereo_headphones
    "\x1\x2",                           // matrix_stereo
    "\x1\x2",                           // mid_side
    "\x1\x2",                           // xy
    "\x1\x2",                           // binaural
    nullptr,                            // ambisonic_b_format
    "\x1\x2\x5\x6",                     // quadraphonic
    "\x1\x2\x5\x6\x3",                  // pentagonal
    "\x1\x2\x5\x6\x3\x9",               // hexagonal
    "\x1\x2\x5\x6\x3\x9\xa\xb",         // octagonal
    "\x1\x2\x5\x6\xd\xe\x10\x11",       // cube
    "\x1\x2\x3",                        // mpeg_3_0_a
    "\x3\x1\x2",                        // mpeg_3_0_b
    "\x1\x2\x3\x9",                     // mpeg_4_0_a
    "\x3\x1\x2\x9",                     // mpeg_4_0_b
    "\x1\x2\x3\x5\x6",                  // mpeg_5_0_a
    "\x1\x2\x5\x6\x3",                  // mpeg_5_0_b
    "\x1\x3\x2\x5\x6",                  // mpeg_5_0_c
    "\x3\x1\x2\x5\x6",                  // mpeg_5_0_d
    "\x1\x2\x3\x4\x5\x6",               // mpeg_5_1_a
    "\x1\x2\x5\x6\x3\x4",               // mpeg_5_1_b
    "\x1\x3\x2\x5\x6\x4",               // mpeg_5_1_c
    "\x3\x1\x2\x5\x6\x4",               // mpeg_5_1_d
    "\x1\x2\x3\x4\x5\x6\x9",            // mpeg_6_1_a
    "\x1\x2\x3\x4\x5\x6\x7\x8",         // mpeg_7_1_a
    "\x3\x7\x8\x1\x2\x5\x6\x4",         // mpeg_7_1_b
    "\x1\x2\x3\x4\x5\x6\x21\x22",       // mpeg_7_1_c
    "\x1\x2\x5\x6\x3\x4\x7\x8",         // emagic_default_7_1
    nullptr,                            // smpte_dtv
    "\x1\x2\x9",                        // itu_2_1
    "\x1\x2\x5\x6",                     // itu_2_2
    "\x1\x2\x4",                        // dvd_4
    "\x1\x2\x4\x9",                     // dvd_5
    "\x1\x2\x4\x5\x6",                  // dvd_6
    "\x1\x2\x3\x4",                     // dvd_10
    "\x1\x2\x3\x4\x9",                  // dvd_11
    "\x1\x2\x5\x6\x4",                  // dvd_18
    "\x1\x2\x5\x6\x3\x9",               // audiounit_6_0
    "\x1\x2\x5\x6\x3\x21\x22",          // audiounit_7_0
    "\x3\x1\x2\x5\x6\x9",               // aac_6_0
    "\x3\x1\x2\x5\x6\x9\x4",            // aac_6_1
    "\x3\x1\x2\x5\x6\x21\x22",          // aac_7_0
    "\x3\x1\x2\x5\x6\x21\x22\x9",       // aac_octagonal
    nullptr,                            // tmh_10_2_standard,
    nullptr,                            // tmh_10_2_full,
    nullptr,                            // discrete_in_order,
    "\x1\x2\x5\x6\x3\x7\x8",            // audiounit_7_0_front
    "\x3\x4",                           // ac3_1_0_1
    "\x1\x3\x2",                        // ac3_3_0
    "\x1\x3\x2\x9",                     // ac3_3_1
    "\x1\x3\x2\x4",                     // ac3_3_0_1
    "\x1\x2\x9\x4",                     // ac3_2_1_1
    "\x1\x3\x2\x9\x4",                  // ac3_3_1_1
}};

}     // namespace <unnamed>


std::unique_ptr<channel_mapper> channel_mapper::create(uint32 const tag)
{
    if (tag >= audio::channel_layout_tag::mono &&
        tag <= audio::channel_layout_tag::ac3_3_1_1) {
        auto found = channel_mappings[(tag >> 16) - 100];
        if (found != nullptr) {
            auto const map = string_view{found, tag & 0xff};
            return std::make_unique<channel_mapper_impl>(map);
        }
    }
    raise(errc::not_implemented,
          "unsupported channel layout tag: 0x%08" PRIx32, tag);
}

std::unique_ptr<channel_mapper> channel_mapper::create(string_view const map)
{
    if (map.size() <= audio::max_channels) {
        return std::make_unique<channel_mapper_impl>(map);
    }
    raise(errc::unsupported_format,
          "unsupported channel count: %zu", map.size());
}

}}    // namespace amp::audio

