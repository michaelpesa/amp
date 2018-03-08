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
#include <amp/range.hpp>
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
    ::vorbis_info vi_;
    ::vorbis_comment vc_;
    ::vorbis_dsp_state vd_;
    ::vorbis_block vb_;
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
    ::vorbis_info_init(&vi_);
    ::vorbis_comment_init(&vc_);
    try {
        uint8* headers[3];
        uint32 lengths[3];
        verify(split_xiph_headers(fmt.extra, headers, lengths));

        for (auto const i : xrange(3)) {
            ::ogg_packet op;
            op.b_o_s  = (i == 0);
            op.bytes  = lengths[i];
            op.packet = headers[i];
            verify(::vorbis_synthesis_headerin(&vi_, &vc_, &op));
        }

        if (AMP_UNLIKELY(vi_.rate <= 0)) {
            raise(errc::unsupported_format,
                  "invalid Vorbis sample rate: %ld", vi_.rate);
        }
        if (AMP_UNLIKELY(vi_.channels <= 0 || vi_.channels > 8)) {
            raise(errc::unsupported_format,
                  "invalid Vorbis channel count: %d", vi_.channels);
        }

        verify(::vorbis_synthesis_init(&vd_, &vi_));
        verify(::vorbis_block_init(&vd_, &vb_));
    }
    catch (...) {
        ::vorbis_comment_clear(&vc_);
        ::vorbis_info_clear(&vi_);
        throw;
    }

    fmt.sample_rate = static_cast<uint32>(vi_.rate);
    fmt.channels = static_cast<uint32>(vi_.channels);
    fmt.channel_layout = audio::xiph_channel_layout(fmt.channels);
}

decoder::~decoder()
{
    ::vorbis_block_clear(&vb_);
    ::vorbis_dsp_clear(&vd_);
    ::vorbis_comment_clear(&vc_);
    ::vorbis_info_clear(&vi_);
}

void decoder::send(io::buffer& buf)
{
    if (!buf.empty()) {
        ::ogg_packet op;
        op.packet = buf.data();
        op.bytes = static_cast<long>(buf.size());

        verify(::vorbis_synthesis(&vb_, &op));
        verify(::vorbis_synthesis_blockin(&vd_, &vb_));
    }
}

auto decoder::recv(audio::packet& pkt)
{
    float** pcm;
    auto frames = verify(::vorbis_synthesis_pcmout(&vd_, &pcm));
    if (!frames) {
        return audio::decode_status::none;
    }

    auto const mapping = vorbis::channel_mappings[vi_.channels - 1];
    float const* planes[8];
    for (auto const i : xrange(vi_.channels)) {
        planes[i] = pcm[mapping[i]];
    }

    pkt.fill_planar(planes, frames);
    verify(::vorbis_synthesis_read(&vd_, as_signed(frames)));
    return audio::decode_status::incomplete;
}

void decoder::flush()
{
    verify(::vorbis_synthesis_restart(&vd_));
}

uint32 decoder::get_decoder_delay() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_DECODER(
    decoder,
    audio::codec::vorbis);

}}}   // namespace amp::vorbis::<unnamed>

