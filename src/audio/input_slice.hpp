////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/input_slice.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_69EE2A5A_711B_401F_AB7C_F6B40F6E3BFB
#define AMP_INCLUDED_69EE2A5A_711B_401F_AB7C_F6B40F6E3BFB


#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/media/image.hpp>
#include <amp/stddef.hpp>

#include "media/track.hpp"

#include <utility>


namespace amp {
namespace audio {
namespace {

class input_slice final :
    public implement_ref_count<input_slice, audio::input>
{
public:
    input_slice(ref_ptr<audio::input> in, media::track const& t) :
        base_{std::move(in)},
        start_offset_{t.start_offset},
        length_{t.frames},
        cursor_{0}
    {
        if (start_offset_ != 0) {
            base_->seek(start_offset_);
        }
    }

    void read(audio::packet& pkt) override
    {
        if (AMP_UNLIKELY(cursor_ >= length_)) {
            pkt.clear();
            return;
        }

        base_->read(pkt);
        cursor_ += pkt.frames();

        if (AMP_UNLIKELY(cursor_ > length_)) {
            auto const trim = static_cast<uint32>(cursor_ - length_);
            pkt.pop_back(trim * pkt.channels());
            cursor_ = length_;
        }
    }

    void seek(uint64 const pts) override
    {
        base_->seek(start_offset_ + pts);
        cursor_ = pts;
    }

    audio::format get_format() override
    { return base_->get_format(); }

    audio::stream_info get_info(uint32 const number) override
    { return base_->get_info(number); }

    media::image get_image(media::image_type const type) override
    { return base_->get_image(type); }

    uint32 get_chapter_count() override
    { return base_->get_chapter_count(); }

private:
    ref_ptr<audio::input> const base_;
    uint64 const start_offset_;
    uint64 const length_;
    uint64 cursor_;
};

}}}   // namespace amp::audio::<unnamed>


#endif  // AMP_INCLUDED_69EE2A5A_711B_401F_AB7C_F6B40F6E3BFB

