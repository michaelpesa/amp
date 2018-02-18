////////////////////////////////////////////////////////////////////////////////
//
// tests/io_reader_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/io/reader.hpp>
#include <amp/memory.hpp>
#include <amp/stddef.hpp>

#include <stdexcept>

#include <gtest/gtest.h>


using namespace ::amp;


constexpr uint8 subtype_pcm[] {
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,
};

constexpr uint8 packed_format[] {
    0x01, 0x00,                                         // wFormatTag
    0x04, 0x00,                                         // nChannels
    0x44, 0xac, 0x00, 0x00,                             // nSamplesPerSec
    0x40, 0xc4, 0x0a, 0x00,                             // nAvgBytesPerSec
    0x10, 0x00,                                         // nBlockAlign
    0x20, 0x00,                                         // wBitsPerSample
    0x16, 0x00,                                         // cbSize
    0x18, 0x00,                                         // wValidBitsPerSample
    0x33, 0x00, 0x00, 0x00,                             // dwChannelMask
    0x01, 0x00, 0x00, 0x00,                             // SubFormat.Data1
    0x00, 0x00,                                         // SubFormat.Data2
    0x10, 0x00,                                         // SubFormat.Data3
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,     // SubFormat.Data4
};


TEST(io_reader_test, read)
{
    io::reader r{packed_format};

    for (auto repeat = 0; repeat < 2; ++repeat, r.rewind()) {
        r.rewind();
        ASSERT_EQ(r.tell(), 0);

        ASSERT_EQ((r.read<uint16,LE>()), 0x0001);
        ASSERT_EQ((r.read<uint16,LE>()), 4);
        ASSERT_EQ((r.read<uint32,LE>()), 44100);
        ASSERT_EQ((r.read<uint32,LE>()), 44100 * 4 * 4);
        ASSERT_EQ((r.read<uint16,LE>()), 16);
        ASSERT_EQ((r.read<uint16,LE>()), 32);
        ASSERT_EQ((r.read<uint16,LE>()), 22);
        ASSERT_EQ((r.read<uint16,LE>()), 24);
        ASSERT_EQ((r.read<uint32,LE>()), 0x33);

        uint8 sub_format[16];
        r.read(sub_format);
        ASSERT_TRUE(mem::equal(sub_format, subtype_pcm));

        ASSERT_EQ(r.tell(), r.size());
        ASSERT_THROW(r.read_n(1), std::runtime_error);
        ASSERT_EQ(r.tell(), r.size());
    }
}

TEST(io_reader_test, gather)
{
    uint16 format_tag;
    uint16 channels;
    uint32 samples_per_sec;
    uint32 avg_bytes_per_sec;
    uint32 block_align;
    uint16 bits_per_sample;
    uint16 valid_bits_per_sample;
    uint32 channel_mask;
    uint8  sub_format[16];

    io::reader r{packed_format};

    for (auto repeat = 0; repeat < 2; ++repeat, r.rewind()) {
        r.rewind();
        ASSERT_EQ(r.tell(), 0);

        r.gather<LE>(format_tag,
                     channels,
                     samples_per_sec,
                     avg_bytes_per_sec,
                     io::alias<uint16>(block_align),
                     bits_per_sample,
                     io::ignore<2>,
                     valid_bits_per_sample,
                     channel_mask,
                     sub_format);

        ASSERT_EQ(format_tag,            0x0001);
        ASSERT_EQ(channels,              4);
        ASSERT_EQ(samples_per_sec,       44100);
        ASSERT_EQ(avg_bytes_per_sec,     44100 * 4 * 4);
        ASSERT_EQ(block_align,           16);
        ASSERT_EQ(bits_per_sample,       32);
        ASSERT_EQ(valid_bits_per_sample, 24);
        ASSERT_EQ(channel_mask,          0x33);
        ASSERT_TRUE(mem::equal(sub_format, subtype_pcm));

        ASSERT_EQ(r.tell(), r.size());
        ASSERT_THROW(r.read_n(1), std::runtime_error);
        ASSERT_EQ(r.tell(), r.size());
    }
}

