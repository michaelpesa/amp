////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/aiff.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>


namespace amp {
namespace aiff {
namespace {

class demuxer final :
    public audio::basic_demuxer<aiff::demuxer>
{
    using Base = audio::basic_demuxer<aiff::demuxer>;
    friend class audio::basic_demuxer<aiff::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    void read_chunk_comm(uint32);
    void read_chunk_ssnd(uint32);

    ref_ptr<io::stream> file;
    uint64 data_chunk_start{};
    uint64 data_chunk_end{};
    uint64 id3_chunk_start{};
    uint32 packet_count;
    uint32 packet_step{1};
    bool is_aifc{};
};


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    uint32 chunk_id, chunk_size, form_type;
    file->gather<BE>(chunk_id, chunk_size, form_type);

    if (chunk_id != "FORM"_4cc ||
        (form_type != "AIFF"_4cc && form_type != "AIFC"_4cc)) {
        raise(errc::invalid_data_format, "invalid AIFF 'FORM' chunk");
    }
    is_aifc = (form_type == "AIFC"_4cc);

    auto file_pos = file->tell();
    for (auto const file_len = file->size(); (file_pos + 8) < file_len; ) {
        file->gather<BE>(chunk_id, chunk_size);

        switch (chunk_id) {
        case "COMM"_4cc:
            read_chunk_comm(chunk_size);
            break;
        case "SSND"_4cc:
            read_chunk_ssnd(chunk_size);
            break;
        case "ID3 "_4cc:
            id3_chunk_start = file->tell();
            break;
        case "wave"_4cc:
            format.extra.assign(*file, chunk_size);
            break;
        }
        file_pos = align_up(file_pos + chunk_size + 8, 2);
        file->seek(file_pos);
    }

    if (!(mode & (audio::playback|audio::metadata))) {
        return;
    }

    switch (format.codec_id) {
    case audio::codec::qdesign:
    case audio::codec::qdesign2:
        if (format.extra.size() >= 48) {
            format.bit_rate          = io::load<uint32,BE>(&format.extra[32]);
            format.frames_per_packet = io::load<uint32,BE>(&format.extra[36]);
            format.bytes_per_packet  = io::load<uint32,BE>(&format.extra[44]);
        }
        break;
    case audio::codec::qcelp:
        if (format.extra.size() >= 25) {
            switch (format.extra[24]) {
            case 'H': format.bytes_per_packet = 17; break;  // half rate
            case 'F': format.bytes_per_packet = 35; break;  // full rate
            }
        }
        break;
    }

    if (format.bit_rate == 0 && format.frames_per_packet != 0) {
        format.bit_rate = muldiv(format.bytes_per_packet,
                                 format.sample_rate * 8,
                                 format.frames_per_packet);
    }

    Base::resolve_decoder();
    Base::set_total_frames(packet_count * format.frames_per_packet);
    Base::average_bit_rate = format.bit_rate;
    Base::instant_bit_rate = format.bit_rate;
    file->seek(data_chunk_start);
}

void demuxer::read_chunk_comm(uint32 const chunk_size)
{
    uint8 sample_rate[10];
    file->gather<BE>(io::alias<uint16>(format.channels),
                     packet_count,
                     io::alias<uint16>(format.bits_per_sample),
                     sample_rate);

    format.frames_per_packet = 1;
    format.channel_layout = audio::guess_channel_layout(format.channels);
    format.sample_rate = [&]{
        auto const sign = sample_rate[0] & 0x80;
        auto const exp = io::load<uint16,BE>(&sample_rate[0]) & 0x7fff;
        auto const mant_hi = io::load<uint32,BE>(&sample_rate[2]);
        auto const mant_lo = io::load<uint32,BE>(&sample_rate[6]);

        double ret;
        if (exp == 0 && mant_hi == 0 && mant_lo == 0) {
            ret = 0.0;
        }
        else if (exp == 0x7fff) {
            ret = inf<double>;
        }
        else {
            ret = std::ldexp(mant_hi, exp - 16383 - 31)
                + std::ldexp(mant_lo, exp - 16383 - 63);
        }
        return numeric_cast<uint32>(sign ? -ret : +ret);
    }();

    auto codec_tag = "NONE"_4cc;
    if (chunk_size >= 22 && is_aifc) {
        codec_tag = file->read<uint32,BE>();
    }

    switch (codec_tag) {
    case "GSM "_4cc:
    case "agsm"_4cc:
        format.codec_id = audio::codec::gsm;
        format.bytes_per_packet = 33;
        format.frames_per_packet = 160;
        break;
    case "ima4"_4cc:
        format.codec_id = audio::codec::adpcm_ima_qt;
        format.bytes_per_packet = 34 * format.channels;
        format.frames_per_packet = 64;
        break;
    case "G722"_4cc:
        format.codec_id = audio::codec::adpcm_g722;
        format.bytes_per_packet = 1 * format.channels;
        format.frames_per_packet = 2;
        break;
    case "MAC6"_4cc:
        format.codec_id = audio::codec::mace6;
        format.bytes_per_packet = 1 * format.channels;
        format.frames_per_packet = 6;
        break;
    case "MAC3"_4cc:
        format.codec_id = audio::codec::mace3;
        format.bytes_per_packet = 2 * format.channels;
        format.frames_per_packet = 6;
        break;
    case "Qclp"_4cc:
        format.codec_id = audio::codec::qcelp;
        format.frames_per_packet = 160;
        break;
    case "QDMC"_4cc:
        format.codec_id = audio::codec::qdesign;
        break;
    case "QDM2"_4cc:
        format.codec_id = audio::codec::qdesign2;
        break;
    case "alaw"_4cc:
        format.codec_id = audio::codec::alaw;
        format.bits_per_sample = 8;
        break;
    case "ulaw"_4cc:
        format.codec_id = audio::codec::ulaw;
        format.bits_per_sample = 8;
        break;
    case "FL64"_4cc:
    case "fl64"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::ieee_float | audio::pcm::big_endian;
        format.bits_per_sample = 64;
        break;
    case "FL32"_4cc:
    case "fl32"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::ieee_float | audio::pcm::big_endian;
        format.bits_per_sample = 32;
        break;
    case "in32"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        format.bits_per_sample = 32;
        break;
    case "in24"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        format.bits_per_sample = 24;
        break;
    case "twos"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        format.bits_per_sample = 16;
        break;
    case "sowt"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::signed_int;
        format.bits_per_sample = 16;
        break;
    case "raw "_4cc:
        format.codec_id = audio::codec::lpcm;
        format.bits_per_sample = 8;
        break;
    case "NONE"_4cc:
        format.codec_id = audio::codec::lpcm;
        format.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        break;
    default:
        raise(errc::unsupported_format, "unknown AIFF codec tag: %#x",
              codec_tag);
    }

    switch (format.codec_id) {
    case audio::codec::lpcm:
    case audio::codec::alaw:
    case audio::codec::ulaw:
        packet_step = std::max(format.sample_rate / 10, uint32{1});
    }

    if (format.bytes_per_packet == 0) {
        format.bytes_per_packet = format.bits_per_sample * format.channels / 8;
    }
}

void demuxer::read_chunk_ssnd(uint32 const chunk_size)
{
    if (chunk_size <= 8) {
        raise(errc::invalid_data_format, "AIFF 'SSND' chunk is too small");
    }

    uint32 offset, block_size;
    file->gather<BE>(offset, block_size);

    data_chunk_start = file->tell() + offset;
    data_chunk_end = data_chunk_start + (chunk_size - 8);
}

bool demuxer::feed(io::buffer& dest)
{
    auto const remain = data_chunk_end - file->tell();
    if (AMP_UNLIKELY(remain < format.bytes_per_packet)) {
        return false;
    }

    auto packet_size = format.bytes_per_packet * packet_step;
    if (AMP_UNLIKELY(packet_size > remain)) {
        packet_size = static_cast<uint32>(remain);
        packet_size = packet_size - (packet_size % format.bytes_per_packet);
    }

    dest.assign(*file, packet_size);
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto nearest = pts / format.frames_per_packet;
    auto priming = pts % format.frames_per_packet;

    if (nearest >= packet_count) {
        nearest  = packet_count;
        priming  = 0;
    }

    file->seek(data_chunk_start + (nearest * format.bytes_per_packet));
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container, (is_aifc ? "AIFF-C" : "AIFF"));

    if (id3_chunk_start != 0) {
        file->seek(id3_chunk_start);
        id3v2::read(*file, info.tags);
    }
    return info;
}

auto demuxer::get_image(media::image::type const type)
{
    if (id3_chunk_start != 0) {
        file->seek(id3_chunk_start);
        return id3v2::find_image(*file, type);
    }
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "aif", "aifc", "aiff", "aiffc");

}}}   // namespace amp::aiff::<unnamed>

