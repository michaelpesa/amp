////////////////////////////////////////////////////////////////////////////////
//
// plugins/vorbis/decoder.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>

#include "error.hpp"

#include <utility>

#include <ogg/ogg.h>
#include <vorbis/codec.h>


namespace amp {
namespace vorbis {
namespace {

static constexpr uint8 channel_mappings[8][8] {
    { 0 },
    { 0, 1 },
    { 0, 2, 1 },
    { 0, 1, 2, 3 },
    { 0, 2, 1, 3, 4 },
    { 0, 2, 1, 5, 3, 4 },
    { 0, 2, 1, 6, 5, 3, 4 },
    { 0, 2, 1, 7, 5, 6, 3, 4 },
};

class decoder
{
public:
    explicit decoder(audio::codec_format&);
    ~decoder();

    void send(io::buffer&);
    auto recv(audio::packet&);
    void flush();
    uint32 get_decoder_delay() const noexcept;

private:
    ::vorbis_info       info{};
    ::vorbis_comment    comm{};
    ::vorbis_dsp_state  dsp{};
    ::vorbis_block      block{};
    ::ogg_packet        op{};
    uint8 const*        remap;
};

int split_xiph_headers(io::buffer& extra,
                       uint8*(&headers)[3],
                       uint32(&lengths)[3]) noexcept
{
    if (extra.size() <= 2) {
        return OV_EBADHEADER;
    }

    auto const first_header_size = io::load<uint16,BE>(&extra[0]);

    if ((extra.size() >= 6) && (first_header_size == 30)) {
        auto total_size = 6U;
        auto first = extra.begin();
        for (auto const i : xrange(3)) {
            auto const length = io::load<uint16,BE>(first);
            first += 2;

            lengths[i] = length;
            headers[i] = std::exchange(first, first + length);

            if ((total_size + length) > extra.size()) {
                return OV_EBADHEADER;
            }
            total_size += length;
        }
        return 0;
    }

    if (extra[0] != 2) {
        return OV_EBADHEADER;
    }

    auto total_size = uint32{3};
    auto first = extra.begin() + 1;

    for (auto const i : xrange(2)) {
        lengths[i] = 0;
        for (; (total_size < extra.size()) && (*first == 0xff); ++first) {
            lengths[i] += 0xff;
            total_size += 0xff + 1;
        }
        lengths[i] += *first;
        total_size += *first;
        if (total_size > extra.size()) {
            return OV_EBADHEADER;
        }
        ++first;
    }

    lengths[2] = static_cast<uint32>(extra.size() - total_size);
    headers[0] = first;
    headers[1] = first + lengths[0];
    headers[2] = first + lengths[0] + lengths[1];
    return 0;
}


decoder::decoder(audio::codec_format& fmt)
{
    ::vorbis_info_init(&info);
    ::vorbis_comment_init(&comm);
    auto guard1 = scope_exit ^ [&]{
        ::vorbis_comment_clear(&comm);
        ::vorbis_info_clear(&info);
    };

    uint8* headers[3];
    uint32 lengths[3];
    verify(split_xiph_headers(fmt.extra, headers, lengths));

    for (auto const i : xrange(3)) {
        op.b_o_s  = (i == 0);
        op.bytes  = lengths[i];
        op.packet = headers[i];
        verify(::vorbis_synthesis_headerin(&info, &comm, &op));
    }

    if (AMP_UNLIKELY(info.rate <= 0)) {
        raise(errc::unsupported_format, "invalid Vorbis sample rate: %ld",
              info.rate);
    }
    if (AMP_UNLIKELY(info.channels <= 0)) {
        raise(errc::unsupported_format, "invalid Vorbis channel count: %d",
              info.channels);
    }

    fmt.sample_rate    = static_cast<uint32>(info.rate);
    fmt.channels       = static_cast<uint32>(info.channels);
    fmt.channel_layout = audio::xiph_channel_layout(fmt.channels);
    remap = vorbis::channel_mappings[fmt.channels - 1];

    verify(::vorbis_synthesis_init(&dsp, &info));
    auto guard2 = scope_exit ^ [&]{
        ::vorbis_dsp_clear(&dsp);
    };

    verify(::vorbis_block_init(&dsp, &block));
    guard2.dismiss();
    guard1.dismiss();
}

decoder::~decoder()
{
    ::vorbis_block_clear(&block);
    ::vorbis_dsp_clear(&dsp);
    ::vorbis_comment_clear(&comm);
    ::vorbis_info_clear(&info);
}

void decoder::send(io::buffer& buf)
{
    if (!buf.empty()) {
        op.packet = buf.data();
        op.bytes  = numeric_cast<long>(buf.size());

        verify(::vorbis_synthesis(&block, &op));
        verify(::vorbis_synthesis_blockin(&dsp, &block));
    }
}

auto decoder::recv(audio::packet& pkt)
{
    float** tmp;
    auto frames = verify(::vorbis_synthesis_pcmout(&dsp, &tmp));
    if (!frames) {
        return audio::decode_status::none;
    }

    float* planes[8];
    for (auto const i : xrange(info.channels)) {
        planes[i] = tmp[remap[i]];
    }

    pkt.fill_planar(planes, frames);
    verify(::vorbis_synthesis_read(&dsp, as_signed(frames)));
    return audio::decode_status::incomplete;
}

void decoder::flush()
{
    verify(::vorbis_synthesis_restart(&dsp));
}

uint32 decoder::get_decoder_delay() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::vorbis);

}}}   // namespace amp::vorbis::<unnamed>

