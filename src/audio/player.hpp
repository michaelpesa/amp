////////////////////////////////////////////////////////////////////////////////
//
// audio/player.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3E32E46D_34BA_4E82_B93B_5316FA22085E
#define AMP_INCLUDED_3E32E46D_34BA_4E82_B93B_5316FA22085E


#include <amp/audio/output.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "audio/replaygain.hpp"
#include "core/spsc_queue.hpp"
#include "media/track.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>


namespace amp {
namespace audio {

enum class player_state : uint8 {
    playing,
    paused,
    stopped,
};

class player_delegate
{
public:
    virtual void track_complete() = 0;
    virtual void error_occurred() = 0;

protected:
    player_delegate() = default;
   ~player_delegate() = default;
};

class player
{
public:
    explicit player(audio::player_delegate& d) :
        delegate_{d}
    {}

    void start();
    void pause();
    void stop();
    void seek(std::chrono::nanoseconds);

    void set_output(u8string const&, u8string const&);
    void set_preset(std::vector<u8string>, audio::replaygain_config);

    void set_volume(float const level)
    {
        stream_->set_volume(level);
    }

    float get_volume() const
    {
        return stream_->get_volume();
    }

    void insert_track(media::track const& x)
    {
        tracks_.push(x);
    }

    auto get_position() const noexcept
    {
        return std::chrono::nanoseconds(
            position_.load(std::memory_order_relaxed));
    }

    auto get_bit_rate() const noexcept
    {
        return bit_rate_.load(std::memory_order_relaxed);
    }

    auto get_state() const noexcept
    { return state_; }

    bool is_playing() const noexcept
    { return (get_state() == audio::player_state::playing); }

    bool is_paused() const noexcept
    { return (get_state() == audio::player_state::paused); }

    bool is_stopped() const noexcept
    { return (get_state() == audio::player_state::stopped); }

private:
    struct event
    {
        enum type : uint32 {
            seek  = (1 << 0),
            state = (1 << 1),
            stop  = (1 << 2),
            pause = (1 << 3),
        };

        explicit event(enum type const t, uint64 const d = 0) noexcept :
            type{t},
            data{d}
        {}

        enum type type;
        uint64    data;
    };

    AMP_INTERNAL_LINKAGE void run_thread_();

    audio::player_delegate& delegate_;
    spsc::queue<media::track> tracks_;
    spsc::queue<event> events_;
    std::condition_variable cnd_;
    std::mutex mtx_;
    std::thread thread_;
    std::atomic<uint64> position_{};
    std::atomic<uint32> bit_rate_{};
    std::vector<u8string> preset_;
    audio::replaygain_config rg_config_;

    u8string session_id_;
    u8string device_id_;
    ref_ptr<audio::output_session> session_;
    ref_ptr<audio::output_stream> stream_;
    audio::player_state state_{audio::player_state::stopped};
};

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_3E32E46D_34BA_4E82_B93B_5316FA22085E

