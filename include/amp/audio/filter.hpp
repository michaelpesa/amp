////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/filter.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_08889B58_0A45_4D3C_B7C6_9CDDA39E3C5F
#define AMP_INCLUDED_08889B58_0A45_4D3C_B7C6_9CDDA39E3C5F


#include <amp/aux/operators.hpp>
#include <amp/intrusive/set.hpp>
#include <amp/intrusive/slist.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/type_traits.hpp>

#include <utility>


namespace amp {
namespace audio {

enum : uint8 {
    quality_minimum = 1,
    quality_low     = 2,
    quality_medium  = 3,
    quality_high    = 4,
    quality_maximum = 5,
};


struct format;
class packet;


class filter
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual void calibrate(audio::format&) = 0;
    virtual void process(audio::packet&) = 0;
    virtual void drain(audio::packet&) = 0;
    virtual void flush() = 0;
    virtual uint64 get_latency() = 0;

protected:
    filter() = default;
    ~filter() = default;
};

class resampler :
    public filter
{
public:
    virtual void set_sample_rate(uint32) = 0;
    virtual void set_quality(uint8) = 0;

protected:
    resampler() = default;
    ~resampler() = default;
};


class filter_factory :
    public intrusive::set_link<>,
    private less_than_comparable<filter_factory>,
    private less_than_comparable<filter_factory, string_view>
{
public:
    virtual ref_ptr<filter> create() const = 0;

    char const* const id;
    char const* const display_name;

protected:
    explicit filter_factory(char const* const i,
                            char const* const n) noexcept :
        id(i),
        display_name(n)
    {}

    ~filter_factory() = default;

    AMP_EXPORT
    void register_() noexcept;

private:
    friend bool operator<(filter_factory const& x,
                          filter_factory const& y) noexcept
    { return (x.id < y.id); }

    friend bool operator<(filter_factory const& x,
                          string_view const& y) noexcept
    { return (x.id < y); }

    friend bool operator>(filter_factory const& x,
                          string_view const& y) noexcept
    { return (x.id > y); }
};

class resampler_factory :
    public intrusive::slist_link<>
{
public:
    virtual ref_ptr<resampler> create() const = 0;

protected:
    resampler_factory() = default;
    ~resampler_factory() = default;

    AMP_EXPORT
    void register_() noexcept;
};


template<typename T>
class filter_bridge final :
    public implement_ref_count<filter_bridge<T>, filter>
{
public:
    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    explicit filter_bridge(Args&&... args) :
        base_(std::forward<Args>(args)...)
    {}

    void calibrate(audio::format& fmt) override
    { base_.calibrate(fmt); }

    void process(audio::packet& pkt) override
    { base_.process(pkt); }

    void drain(audio::packet& pkt) override
    { base_.drain(pkt); }

    void flush() override
    { base_.flush(); }

    uint64 get_latency() override
    { return base_.get_latency(); }

private:
    T base_;
};

template<typename T>
class resampler_bridge final :
    public implement_ref_count<resampler_bridge<T>, resampler>
{
public:
    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    explicit resampler_bridge(Args&&... args) :
        base_(std::forward<Args>(args)...)
    {}

    void calibrate(audio::format& fmt) override
    { base_.calibrate(fmt); }

    void process(audio::packet& pkt) override
    { base_.process(pkt); }

    void drain(audio::packet& pkt) override
    { base_.drain(pkt); }

    void flush() override
    { base_.flush(); }

    uint64 get_latency() override
    { return base_.get_latency(); }

    void set_sample_rate(uint32 const sample_rate) override
    { base_.set_sample_rate(sample_rate); }

    void set_quality(uint8 const quality) override
    { base_.set_quality(quality); }

private:
    T base_;
};

template<typename T>
class register_filter final :
    public filter_factory
{
public:
    explicit register_filter(char const* const id,
                             char const* const name) noexcept :
        filter_factory{id, name}
    {
        filter_factory::register_();
    }

    ref_ptr<filter> create() const override
    { return filter_bridge<T>::make(); }
};

template<typename T>
class register_resampler final :
    public resampler_factory
{
public:
    register_resampler() noexcept
    {
        resampler_factory::register_();
    }

    ref_ptr<resampler> create() const override
    { return resampler_bridge<T>::make(); }
};


#define AMP_REGISTER_FILTER(T, ...) \
    static ::amp::audio::register_filter<T> AMP_PP_ANON(reg){__VA_ARGS__}

#define AMP_REGISTER_RESAMPLER(T) \
    static ::amp::audio::register_resampler<T> AMP_PP_ANON(reg)

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_08889B58_0A45_4D3C_B7C6_9CDDA39E3C5F

