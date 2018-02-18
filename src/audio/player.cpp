////////////////////////////////////////////////////////////////////////////////
//
// audio/player.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "audio/filter_chain.hpp"
#include "audio/input_slice.hpp"
#include "audio/player.hpp"
#include "audio/replaygain.hpp"
#include "audio/sink_context.hpp"
#include "core/registry.hpp"
#include "media/track.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <utility>


namespace amp {
namespace audio {

void player::set_output(u8string const& s_id, u8string const& d_id)
{
    if (s_id == session_id_ && d_id == device_id_) {
        if (s_id.empty()) {
            raise(errc::failure, "cannot set output to null");
        }
        return;
    }

    try {
        auto factory = audio::output_factories.find(s_id);
        if (factory == audio::output_factories.end()) {
            raise(errc::failure, "no such output plugin");
        }

        session_ = factory->create();
        stream_ = session_->activate(d_id);
        session_id_ = s_id;
        device_id_ = d_id;
    }
    catch (...) {
        session_.reset();
        stream_.reset();
        session_id_.clear();
        device_id_.clear();
        stop();
        throw;
    }
}

void player::set_preset(std::vector<u8string> x, audio::replaygain_config y)
{
    {
        std::lock_guard<std::mutex> const lk{mtx_};
        preset_    = std::move(x);
        rg_config_ = std::move(y);
    }

    if (!is_stopped()) {
        events_.emplace(event::state);
        cnd_.notify_one();
    }
}

void player::seek(std::chrono::nanoseconds const position)
{
    AMP_ASSERT(!is_stopped() && "cannot seek while stopped");
    events_.emplace(event::seek, static_cast<uint64>(position.count()));
    cnd_.notify_one();
}

void player::start()
{
    AMP_ASSERT(is_stopped() && "cannot play unless stopped");
    thread_ = std::thread([this]() noexcept {
        try         { run_thread_(); }
        catch (...) { delegate_.error_occurred(); }
    });
    state_ = player_state::playing;
}

void player::stop()
{
    if (thread_.joinable()) {
        events_.emplace(event::stop);
        cnd_.notify_one();
        thread_.join();
    }

    events_.clear();
    tracks_.clear();
    position_.store(0, std::memory_order_relaxed);
    bit_rate_.store(0, std::memory_order_relaxed);
    state_ = player_state::stopped;
}

void player::pause()
{
    AMP_ASSERT(!is_stopped() && "cannot pause while stopped");
    events_.emplace(event::pause);
    cnd_.notify_one();
    state_ = is_playing() ? player_state::paused : player_state::playing;
}


namespace {

struct source_context
{
    AMP_INLINE explicit operator bool() const noexcept
    { return static_cast<bool>(input); }

    AMP_INLINE audio::input* operator->() const noexcept
    { return input.get(); }

    AMP_INLINE void reset() noexcept
    { input.reset(); }

    AMP_INLINE void reset(media::track const& x)
    {
        input = audio::input::resolve(x.location, audio::playback);
        if (x.chapter) {
            input = audio::input_slice::make(std::move(input), x);
        }
        frames = x.frames;
        format = input->get_format();
        rg_info.reset(x.info);
    }

    uint64 frames;
    audio::format format;
    audio::replaygain_info rg_info;
    ref_ptr<audio::input> input;
};

}     // namespace <unnamed>


void player::run_thread_()
{
    uint64 sample;
    audio::packet pkt;
    audio::source_context source, pending_source;
    audio::filter_chain chain;
    audio::sink_context sink([&]{
        std::lock_guard<std::mutex> const lk{mtx_};
        chain.rebuild(preset_, rg_config_);
        return stream_;
    }());

    auto calibrate = [&]{
        chain.calibrate(source.format, sink.format, source.rg_info);
    };

    auto commit_track_change = [&]{
        pending_source.reset();
        tracks_.pop();
        delegate_.track_complete();
    };

    auto cancel_track_change = [&]{
        if (pending_source) {
            source = std::move(pending_source);
            calibrate();
        }
    };

    auto const sink_rate = sink.format.sample_rate * sink.format.channels;

    auto sync_clock = [&](uint64 const delta) {
        sample += delta;

        uint64 pos;
        if (AMP_LIKELY(!pending_source)) {
            pos = muldiv(sample - sink.delay(), std::nano::den, sink_rate);
        }
        else if (sample >= sink.delay()) {
            commit_track_change();
            pos = muldiv(sample - sink.delay(), std::nano::den, sink_rate);
        }
        else {
            pos = position_.load(std::memory_order_relaxed);
            pos += muldiv(delta, std::nano::den, sink_rate);
        }
        position_.store(pos, std::memory_order_relaxed);
    };

    auto process_events = [&]{
        uint32 ret{};
        uint64 pos{};

        events_.for_each([&](event const& e) noexcept {
            switch (e.type) {
            case event::seek:
                pos = e.data;
                [[fallthrough]];
            case event::state:
            case event::stop:
                ret |= e.type;
                break;
            case event::pause:
                ret ^= event::pause;
                break;
            }
        });

        if (ret & event::stop) {
            return ret;
        }
        if (ret & event::state) {
            {
                std::lock_guard<std::mutex> const lk{mtx_};
                chain.rebuild(preset_, rg_config_);
            }
            calibrate();
        }
        if (ret & event::seek) {
            cancel_track_change();

            pos = muldiv(pos, source.format.sample_rate, std::nano::den);
            pos = std::min(pos, source.frames - 1);

            sample = muldiv(pos, sink_rate, source.format.sample_rate);
            position_.store(muldiv(sample, std::nano::den, sink_rate),
                            std::memory_order_relaxed);

            source->seek(pos);
            chain.flush();
            sink.flush();
            pkt.clear();
        }
        return ret;
    };

    auto poll = [&](std::chrono::nanoseconds const timeout) {
        auto const status = [&]{
            std::unique_lock<std::mutex> lk{mtx_};
            return cnd_.wait_for(lk, timeout);
        }();
        return (status == std::cv_status::no_timeout) ? process_events() : 0;
    };

    auto prepare_track_change = [&]{
        if (pending_source) {
            commit_track_change();
        }

        auto next = tracks_.front();
        for (; !next; next = tracks_.front()) {
            if (auto const ret = process_events()) {
                return ret;
            }
        }

        pending_source = std::move(source);
        source.reset(*next);
        calibrate();
        sample = 0;
        return uint32{0};
    };

    auto receive_packet = [&]{
        pkt.clear();
        pkt.set_channel_layout(source.format.channel_layout);
        source->read(pkt);

        if (AMP_UNLIKELY(pkt.empty())) {
            chain.drain(pkt);
            return prepare_track_change();
        }

        bit_rate_.store(pkt.bit_rate(), std::memory_order_relaxed);
        chain.process(pkt);
        return process_events();
    };

    auto process_packet = [&, timeout = sink.get_wait_timeout()]{
        while (pkt.empty()) {
            if (auto const ret = receive_packet()) {
                return ret;
            }
        }

        auto const end = pkt.data() + pkt.size();
        auto remain = pkt.size();

        for (;;) {
            auto const samples = sink.write(end - remain, remain);
            sync_clock(samples);

            remain -= samples;
            if (remain == 0) {
                pkt.clear();
                return uint32{0};
            }
            if (auto const ret = poll(timeout)) {
                pkt.pop_front(pkt.size() - remain);
                return ret;
            }
        }
    };

    prepare_track_change();
    commit_track_change();

play:
    sink.start();
    for (;;) {
        auto const ret = process_packet();
        if (ret & event::pause) { goto pause; }
        if (ret & event::stop) { return; }
    }

pause:
    sink.pause();
    for (;;) {
        auto const ret = poll(std::chrono::nanoseconds::max());
        if (ret & event::pause) { goto play; }
        if (ret & event::stop) { return; }
    }
}

}}    // namespace amp::audio

