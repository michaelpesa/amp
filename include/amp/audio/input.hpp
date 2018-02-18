////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/input.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_76116F60_C6C2_4CAD_9404_1E63F570E523
#define AMP_INCLUDED_76116F60_C6C2_4CAD_9404_1E63F570E523


#include <amp/audio/format.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/image.hpp>
#include <amp/net/uri.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <initializer_list>
#include <utility>


namespace amp {
namespace io {
    class stream;
}
namespace net {
    class uri;
}

namespace audio {

class packet;

enum open_mode : uint32 {
    playback = (1 << 0),
    metadata = (1 << 1),
    pictures = (1 << 2),
};
AMP_DEFINE_ENUM_FLAG_OPERATORS(open_mode);


struct stream_info
{
    stream_info() = default;

    explicit stream_info(audio::format const& fmt) noexcept :
        channels{fmt.channels},
        channel_layout{fmt.channel_layout},
        sample_rate{fmt.sample_rate}
    {}

    uint64 start_offset{};
    uint64 frames{};
    uint32 channels{};
    uint32 channel_layout{};
    uint32 sample_rate{};
    uint32 bits_per_sample{};
    uint32 average_bit_rate{};
    uint32 codec_id{};
    media::dictionary tags;
    media::dictionary props;

    AMP_INLINE void validate() const
    {
        audio::format fmt;
        fmt.channels       = channels;
        fmt.channel_layout = channel_layout;
        fmt.sample_rate    = sample_rate;
        fmt.validate();
    }
};


class input
{
public:
    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual void read(audio::packet&) = 0;
    virtual void seek(uint64) = 0;

    virtual audio::format get_format() = 0;
    virtual audio::stream_info get_info(uint32) = 0;
    virtual media::image get_image(media::image_type) = 0;
    virtual uint32 get_chapter_count() = 0;

    AMP_EXPORT
    static ref_ptr<input> resolve(net::uri const&, audio::open_mode);
    AMP_EXPORT
    static ref_ptr<input> resolve(ref_ptr<io::stream>, audio::open_mode);

protected:
    input() = default;
    ~input() = default;
};

class input_factory
{
public:
    virtual ref_ptr<input> create(ref_ptr<io::stream>,
                                  audio::open_mode) const = 0;

protected:
    input_factory() = default;
    ~input_factory() = default;

    AMP_EXPORT
    void register_(char const* const*, char const* const*) noexcept;
};


template<typename T>
class input_bridge final :
    public implement_ref_count<input_bridge<T>, input>
{
public:
    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    AMP_INLINE explicit input_bridge(Args&&... args) :
        base_(std::forward<Args>(args)...)
    {}

    void read(audio::packet& pkt) override
    { base_.read(pkt); }

    void seek(uint64 const pts) override
    { base_.seek(pts); }

    audio::format get_format() override
    { return base_.get_format(); }

    audio::stream_info get_info(uint32 const chapter_number) override
    { return base_.get_info(chapter_number); }

    media::image get_image(media::image_type const type) override
    { return base_.get_image(type); }

    uint32 get_chapter_count() override
    { return base_.get_chapter_count(); }

private:
    T base_;
};

template<typename T>
class register_input final :
    public input_factory
{
public:
    explicit register_input(std::initializer_list<char const*> il) noexcept
    {
        input_factory::register_(il.begin(), il.end());
    }

    ref_ptr<input> create(ref_ptr<io::stream> file,
                          audio::open_mode const mode) const override
    {
        return ref_ptr<input>::consume(
            new input_bridge<T>(std::move(file), mode));
    }
};

#define AMP_REGISTER_INPUT(T, ...) \
    static ::amp::audio::register_input<T> AMP_PP_ANON(reg){__VA_ARGS__}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_76116F60_C6C2_4CAD_9404_1E63F570E523

