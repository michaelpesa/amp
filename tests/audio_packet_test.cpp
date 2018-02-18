////////////////////////////////////////////////////////////////////////////////
//
// tests/audio_packet_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/bitops.hpp>
#include <amp/range.hpp>

#include <algorithm>
#include <iterator>
#include <random>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(audio_packet, resize)
{
    audio::packet pkt;

    ASSERT_EQ(pkt.size(), 0);
    ASSERT_EQ(pkt.capacity(), 0);
    ASSERT_TRUE(pkt.empty());

    pkt.resize(128);
    ASSERT_EQ(pkt.size(), 128);
    ASSERT_EQ(pkt.capacity(), 128);
    ASSERT_FALSE(pkt.empty());
    ASSERT_TRUE(is_aligned(pkt.data(), 32));

    float buf[128] = {};
    ASSERT_TRUE(std::equal(std::begin(buf), std::end(buf), std::begin(pkt)));

    auto rnd = [
        urng = std::mt19937{std::random_device{}()},
        dist = std::uniform_real_distribution<float>{-1.f, +1.f}
    ]() mutable {
        return dist(urng);
    };
    std::generate(std::begin(buf), std::end(buf), rnd);
    std::copy(std::begin(buf), std::end(buf), std::begin(pkt));

    pkt.resize(1024);
    ASSERT_EQ(pkt.size(), 1024);
    ASSERT_EQ(pkt.capacity(), 1024);
    ASSERT_FALSE(pkt.empty());
    ASSERT_TRUE(is_aligned(pkt.data(), 32));
    ASSERT_TRUE(std::equal(std::begin(buf), std::end(buf), std::begin(pkt)));

    pkt.resize(128);
    ASSERT_EQ(pkt.size(), 128);
    ASSERT_EQ(pkt.capacity(), 1024);
    ASSERT_FALSE(pkt.empty());
    ASSERT_TRUE(is_aligned(pkt.data(), 32));
    ASSERT_TRUE(std::equal(std::begin(buf), std::end(buf), std::begin(pkt)));
}

TEST(audio_packet, fill_planar)
{
    auto rnd = [
        urng = std::mt19937{std::random_device{}()},
        dist = std::uniform_real_distribution<float>{-1.f, +1.f}
    ]() mutable {
        return dist(urng);
    };

    constexpr auto N = 128_sz;

    float L[N], R[N];
    std::generate(std::begin(L), std::end(L), rnd);
    std::generate(std::begin(R), std::end(R), rnd);

    audio::packet pkt;
    pkt.set_channel_layout(audio::channel_layout_stereo);
    ASSERT_EQ(pkt.channels(), 2);

    float const* const planes[] { L, R, };
    pkt.fill_planar(planes, N);
    ASSERT_EQ(pkt.frames(), N);
    ASSERT_EQ(pkt.samples(), N * 2);

    for (auto const i : xrange(N)) {
        ASSERT_EQ(pkt[i*2 + 0], L[i]);
        ASSERT_EQ(pkt[i*2 + 1], R[i]);
    }
}

