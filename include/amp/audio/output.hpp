////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/output.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_83A49C6D_B49D_4F13_A8C7_C452E7B661F0
#define AMP_INCLUDED_83A49C6D_B49D_4F13_A8C7_C452E7B661F0


#include <amp/aux/operators.hpp>
#include <amp/audio/format.hpp>
#include <amp/functional.hpp>
#include <amp/intrusive/set.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include <utility>


namespace amp {
namespace audio {

struct output_device
{
    u8string uid;
    u8string name;
};


class output_session_delegate
{
public:
    virtual void device_added(output_device const&) = 0;
    virtual void device_removed(u8string const&) = 0;
    virtual void default_device_changed() = 0;

protected:
    output_session_delegate() = default;
    ~output_session_delegate() = default;
};

class output_device_list
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual uint32 get_count() = 0;
    virtual output_device get_device(uint32) = 0;
    virtual output_device get_default_device() = 0;

protected:
    output_device_list() = default;
    ~output_device_list() = default;
};

class output_stream
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual void start(function_view<void(float*, uint32)>) = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void flush() = 0;

    virtual void set_volume(float) = 0;
    virtual float get_volume() = 0;
    virtual audio::format get_format() = 0;

protected:
    output_stream() = default;
    ~output_stream() = default;
};

class output_session
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual void set_delegate(output_session_delegate*) = 0;
    virtual ref_ptr<output_device_list> get_devices() = 0;
    virtual ref_ptr<output_stream> activate(u8string const&) = 0;

protected:
    output_session() = default;
    ~output_session() = default;
};


class output_session_factory :
    public intrusive::set_link<>,
    private less_than_comparable<output_session_factory>,
    private less_than_comparable<output_session_factory, string_view>
{
public:
    virtual ref_ptr<output_session> create() const = 0;

    char const* const id;
    char const* const display_name;

protected:
    explicit output_session_factory(char const* const i,
                                    char const* const n) noexcept :
        id(i),
        display_name(n)
    {}

    ~output_session_factory() = default;

    AMP_EXPORT
    void register_() noexcept;

private:
    friend bool operator<(output_session_factory const& x,
                          output_session_factory const& y) noexcept
    { return (x.id < y.id); }

    friend bool operator<(output_session_factory const& x,
                          string_view const& y) noexcept
    { return (x.id < y); }

    friend bool operator>(output_session_factory const& x,
                          string_view const& y) noexcept
    { return (x.id > y); }
};

template<typename T>
class register_output final :
    public output_session_factory
{
public:
    explicit register_output(char const* const id,
                             char const* const name) noexcept :
        output_session_factory{id, name}
    {
        output_session_factory::register_();
    }

    ref_ptr<output_session> create() const override
    {
        return ref_ptr<output_session>::consume(new T());
    }
};


#define AMP_REGISTER_OUTPUT(T, ...) \
    static ::amp::audio::register_output<T> AMP_PP_ANON(reg){__VA_ARGS__}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_83A49C6D_B49D_4F13_A8C7_C452E7B661F0

