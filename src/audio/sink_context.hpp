////////////////////////////////////////////////////////////////////////////////
//
// audio/sink_context.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_889035C3_BF5C_4FEA_97F1_2719C5626A20
#define AMP_INCLUDED_889035C3_BF5C_4FEA_97F1_2719C5626A20


#include <amp/audio/format.hpp>
#include <amp/audio/output.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>

#include "audio/circular_buffer.hpp"
#include "core/event.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <utility>


namespace amp {
namespace audio {
namespace {

class sink_context
{
public:
    explicit sink_context(auto_reset_event& ev,
                          ref_ptr<audio::output_stream> s) :
        format(s->get_format()),
        ready_(ev),
        buffer_(format.sample_rate * format.channels),
        stream_(std::move(s))
    {}

    ~sink_context()
    { stream_->stop(); }

    void start()
    {
        stream_->start({&sink_context::read, this});
        paused_ = false;
    }

    void pause()
    {
        stream_->pause();
        paused_ = true;
    }

    void flush()
    {
        if (!paused_) {
            stream_->stop();
        }
        buffer_.read_flush();
        if (!paused_) {
            stream_->start({&sink_context::read, this});
        }
    }

    std::size_t write(float const* const src, std::size_t n) noexcept
    {
        n = std::min(n, buffer_.write_prepare());
        if (AMP_LIKELY(n != 0)) {
            std::copy_n(src, n, buffer_.write_cursor());
            buffer_.write_commit(n);
        }
        return n;
    }

    uint64 delay() const noexcept
    {
        return buffer_.read_avail();
    }

    audio::format const format;

private:
    static void read(void*, float*, uint32) noexcept;

    auto_reset_event& ready_;
    audio::circular_buffer<float> buffer_;
    ref_ptr<audio::output_stream> stream_;
    bool paused_{false};
};


void sink_context::read(void*  const opaque,
                        float* const dst,
                        uint32 const frames) noexcept
{
    if (AMP_UNLIKELY(frames == 0)) {
        return;
    }

    auto&& self = *static_cast<sink_context*>(opaque);
    auto n = std::size_t{frames} * self.format.channels;

    auto const avail = self.buffer_.read_acquire();
    if (AMP_UNLIKELY(avail < n)) {
        std::fill_n(dst + avail, n - avail, 0.f);
        n = avail;
    }
    std::copy_n(self.buffer_.read_cursor(), n, dst);
    self.buffer_.read_release(n);
    self.ready_.post();
}

}}}   // namespace amp::audio::<unnamed>


#endif  // AMP_INCLUDED_889035C3_BF5C_4FEA_97F1_2719C5626A20

