////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/decoder.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_537861EB_D30C_48B5_B0AB_971DA7F216AB
#define AMP_INCLUDED_537861EB_D30C_48B5_B0AB_971DA7F216AB


#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <initializer_list>
#include <utility>


namespace amp {
namespace io {
    class buffer;
}


namespace audio {

struct codec_format;
class packet;

enum class decode_status : uint32 {
    none       = 0x0,
    incomplete = 0x1,
};
AMP_DEFINE_ENUM_FLAG_OPERATORS(decode_status);

AMP_INLINE constexpr bool operator!(decode_status const x) noexcept
{
    return (x == decode_status::none);
}


class decoder
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual void send(io::buffer&) = 0;
    virtual audio::decode_status recv(audio::packet&) = 0;
    virtual void flush() = 0;
    virtual uint32 get_decoder_delay() const noexcept = 0;

    AMP_EXPORT
    static ref_ptr<decoder> resolve(audio::codec_format&);

protected:
    decoder() = default;
    ~decoder() = default;
};

class decoder_factory
{
public:
    virtual ref_ptr<decoder> create(audio::codec_format&) const = 0;

protected:
    decoder_factory() = default;
    ~decoder_factory() = default;

    AMP_EXPORT
    void register_(uint32 const*, uint32 const*) noexcept;
};


template<typename T>
class decoder_bridge final :
    public implement_ref_count<decoder_bridge<T>, decoder>
{
public:
    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    explicit decoder_bridge(Args&&... args) :
        base_(std::forward<Args>(args)...)
    {}

    void send(io::buffer& buf) override
    { base_.send(buf); }

    audio::decode_status recv(audio::packet& pkt) override
    { return base_.recv(pkt); }

    void flush() override
    { base_.flush(); }

    uint32 get_decoder_delay() const noexcept override
    { return base_.get_decoder_delay(); }

private:
    T base_;
};

template<typename T>
class register_decoder final :
    public decoder_factory
{
public:
    explicit register_decoder(std::initializer_list<uint32> il) noexcept
    { decoder_factory::register_(il.begin(), il.end()); }

    ref_ptr<decoder> create(audio::codec_format& fmt) const override
    { return decoder_bridge<T>::make(fmt); }
};

#define AMP_REGISTER_DECODER(T, ...) \
    static ::amp::audio::register_decoder<T> AMP_PP_ANON(reg){__VA_ARGS__}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_537861EB_D30C_48B5_B0AB_971DA7F216AB

