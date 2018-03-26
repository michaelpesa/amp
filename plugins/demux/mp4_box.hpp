////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_box.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_45A007B2_EC5C_40E9_9CED_E39A490624CD
#define AMP_INCLUDED_45A007B2_EC5C_40E9_9CED_E39A490624CD


#include <amp/error.hpp>
#include <amp/intrusive/set.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/net/endian.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include "mp4_descriptor.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>


namespace amp {
namespace mp4 {

template<typename T>
class array
{
public:
    static_assert(is_pod_v<T>, "");

    using iterator       = T*;
    using const_iterator = T const*;

    array() = default;

    array(array&& x) noexcept :
        buf_{std::exchange(x.buf_, nullptr)},
        len_{std::exchange(x.len_, 0)}
    {}

    array& operator=(array&& x) & noexcept
    {
        array{std::move(x)}.swap(*this);
        return *this;
    }

    ~array()
    {
        std::free(buf_);
    }

    void swap(array& x) noexcept
    {
        using std::swap;
        swap(buf_, x.buf_);
        swap(len_, x.len_);
    }

    uint32 size() const noexcept
    { return len_; }

    bool empty() const noexcept
    { return (size() == 0); }

    T* data() noexcept
    { return buf_; }

    T const* data() const noexcept
    { return buf_; }

    iterator begin() noexcept
    { return data(); }

    iterator end() noexcept
    { return data() + size(); }

    const_iterator begin() const noexcept
    { return cbegin(); }

    const_iterator end() const noexcept
    { return cend(); }

    const_iterator cbegin() const noexcept
    { return data(); }

    const_iterator cend() const noexcept
    { return data() + size(); }

    T& operator[](uint32 const i) noexcept
    { return data()[i]; }

    T const& operator[](uint32 const i) const noexcept
    { return data()[i]; }

    T& back() noexcept
    { return data()[size() - 1]; }

    T const& back() const noexcept
    { return data()[size() - 1]; }

    // FIXME: this should not be called resize
    void resize(uint32 const n)
    {
        static_assert(is_trivially_constructible_v<T>, "");

        if (AMP_LIKELY(n != 0)) {
            buf_ = static_cast<T*>(std::malloc(n * sizeof(T)));
            if (AMP_UNLIKELY(buf_ == nullptr)) {
                raise_bad_alloc();
            }
            len_ = n;
        }
    }

    void assign(io::stream& file, uint32 const n)
    {
        resize(n);
        file.read(reinterpret_cast<uchar*>(data()), n * sizeof(T));
        std::transform(begin(), end(), begin(), net::to_host<BE>);
    }

private:
    T*     buf_{};
    uint32 len_{};
};


namespace brand {
    constexpr auto qt = "qt  "_4cc;
}
namespace metadata_handler {
    constexpr auto itunes = "mdir"_4cc;
}
namespace media_handler {
    constexpr auto audio = "soun"_4cc;
    constexpr auto video = "vide"_4cc;
    constexpr auto text  = "text"_4cc;
    constexpr auto hint  = "hint"_4cc;
}


struct full_box_data
{
    AMP_INLINE constexpr uint8 version() const noexcept
    {
        return version_and_flags[0];
    }

    AMP_INLINE constexpr uint32 flags() const noexcept
    {
        return (uint32{version_and_flags[1]} << 16)
             | (uint32{version_and_flags[2]} <<  8)
             | (uint32{version_and_flags[3]} <<  0);
    }

    uint8 version_and_flags[4];
};


struct ftyp_box_data
{
    uint32             major_brand;
    uint32             minor_version;
    mp4::array<uint32> compatible_brands;

    bool compatible_with(uint32 const brand) const noexcept
    {
        if (brand == major_brand) {
            return true;
        }

        auto const first = compatible_brands.cbegin();
        auto const last  = compatible_brands.cend();
        return std::find(first, last, brand) != last;
    }
};

struct mvhd_box_data : full_box_data
{
    uint64 creation_time;
    uint64 modification_time;
    uint32 time_scale;
    uint64 duration;            // longest track's duration (in movie TS)
    int32  rate;                // preferred playback rate (fixed point 16.16)
    int16  volume;              // preferred playback volume (fixed point 8.8)
    int32  matrix[9];
    int32  preview_time;
    int32  preview_duration;
    int32  poster_time;
    int32  selection_time;
    int32  selection_duration;
    int32  current_time;
    uint32 next_track_id;
};

struct tkhd_box_data : full_box_data
{
    uint64 creation_time;
    uint64 modification_time;
    uint32 track_id;            // unique and non-zero
    uint64 duration;
    int16  layer;
    int16  alternate_group;
    int16  volume;              // preferred playback volume (fixed point 8.8)
    int32  matrix[9];
    uint32 width;
    uint32 height;
};

struct mdhd_box_data : full_box_data
{
    uint64 creation_time;
    uint64 modification_time;
    uint32 time_scale;
    uint64 duration;
    uint16 language;
    int16  quality;
};

struct chap_box_data
{
    mp4::array<uint32> track_ids;
};

struct hdlr_box_data : full_box_data
{
    uint32 pre_defined;
    uint32 handler_type;
};

struct stts_box_data : full_box_data
{
    struct entry_type
    {
        uint32 sample_count;
        uint32 sample_delta;
    };

    mp4::array<entry_type> entries;
};

struct stsd_box_data : full_box_data
{
};

struct soun_box_data
{
    uint16 data_reference_index;
    int16  version;
    int16  revision;
    uint32 vendor;
    uint16 channels;
    uint16 sample_size;
    int16  compression_id;
    uint16 packet_size;
    uint32 sample_rate;

    struct sound_description_v1
    {
        uint32 samples_per_packet;
        uint32 bytes_per_packet;
        uint32 bytes_per_frame;
        uint32 bytes_per_sample;
    };

    struct sound_description_v2
    {
        uint32 size_of_struct_only;
        double audio_sample_rate;
        uint32 audio_channels;
        uint32 always_7F000000;
        uint32 const_bits_per_channel;
        uint32 format_specific_flags;
        uint32 const_bytes_per_audio_packet;
        uint32 const_lpcm_frames_per_audio_packet;
    };

    union {
        sound_description_v1 v1;
        sound_description_v2 v2;
    };
};

struct esds_box_data : full_box_data
{
    mp4::decoder_config_descriptor dcd;
};

struct alac_box_data : full_box_data
{
    io::buffer extra;
};

struct wave_box_data
{
    io::buffer extra;
};

struct frma_box_data
{
    uint32 data_format;
};

struct enda_box_data
{
    int16 little_endian;
};

struct stsz_box_data : full_box_data
{
    uint32             sample_size;
    uint32             sample_count;
    mp4::array<uint32> entries;
};

struct stsc_box_data : full_box_data
{
    struct entry_type
    {
        uint32 first_sample;
        uint32 first_chunk;
        uint32 samples_per_chunk;
        uint32 sample_description_index;
    };

    mp4::array<entry_type> entries;
};

struct stco_box_data : full_box_data
{
    mp4::array<uint64> entries;
};

struct elst_box_data : full_box_data
{
    struct entry_type
    {
        uint64 segment_duration;    // in movie time scale
        int64  media_time;          // in media time scale (-1 == empty)
        int32  media_rate;          // 16.16 fixed point
    };

    mp4::array<entry_type> entries;
};

struct meta_box_data : full_box_data
{
};

struct ilst_entry
{
    uint32     type;
    uint8      type_set_identifier;
    uint8      data_type;
    uint16     locale_country;
    uint16     locale_language;
    u8string   mean;
    u8string   name;
    io::buffer data;
};

using ilst_box_data = std::vector<ilst_entry>;

struct chpl_box_data : full_box_data
{
    struct entry_type
    {
        // version 0: 'start' is in movie time scale units
        // version 1: 'start' is in 100 nanosecond units
        uint64   start;
        u8string title;
    };

    std::vector<entry_type> entries;
};

struct mehd_box_data : full_box_data
{
    uint64 fragment_duration;
};

struct trex_box_data : full_box_data
{
    uint32 track_id;
    uint32 default_sample_description_index;
    uint32 default_sample_duration;
    uint32 default_sample_size;
    uint32 default_sample_flags;
};

struct mfhd_box_data : full_box_data
{
    uint32 sequence_number;
};

struct tfhd_box_data : full_box_data
{
    uint32 track_id;
    int64  base_data_offset;
    uint32 sample_description_index;
    uint32 default_sample_duration;
    uint32 default_sample_size;
    uint32 default_sample_flags;

    bool base_data_offset_present() const noexcept
    { return (flags() & 0x000001) != 0; }

    bool sample_description_index_present() const noexcept
    { return (flags() & 0x000002) != 0; }

    bool default_sample_duration_present() const noexcept
    { return (flags() & 0x000008) != 0; }

    bool default_sample_size_present() const noexcept
    { return (flags() & 0x000010) != 0; }

    bool default_sample_flags_present() const noexcept
    { return (flags() & 0x000020) != 0; }

    bool duration_is_empty() const noexcept
    { return (flags() & 0x010000) != 0; }

    bool default_base_is_moof() const noexcept
    { return (flags() & 0x020000) != 0; }
};

struct tfdt_box_data : full_box_data
{
    uint64 base_media_decode_time;
};

struct trun_box_data : full_box_data
{
    struct entry_type
    {
        uint32 duration;
        uint32 size;
        uint32 flags;
        uint32 composition_time_offset;
    };

    int32                  data_offset;
    uint32                 first_sample_flags;
    mp4::array<entry_type> samples;

    bool data_offset_present() const noexcept
    { return (flags() & 0x000001) != 0; }

    bool first_sample_flags_present() const noexcept
    { return (flags() & 0x000004) != 0; }

    bool sample_duration_present() const noexcept
    { return (flags() & 0x000100) != 0; }

    bool sample_size_present() const noexcept
    { return (flags() & 0x000200) != 0; }

    bool sample_flags_present() const noexcept
    { return (flags() & 0x000400) != 0; }

    bool sample_composition_time_offset_present() const noexcept
    { return (flags() & 0x000800) != 0; }
};

struct mfro_box_data : full_box_data
{
    uint32 size;
};

struct tfra_box_data : full_box_data
{
    struct entry_type
    {
        uint64 time;
        uint64 moof_offset;
        uint32 traf_number;
        uint32 trun_number;
        uint32 sample_number;
    };

    uint32                 track_id;
    mp4::array<entry_type> entries;
};

struct sidx_box_data : full_box_data
{
    struct entry_type
    {
        uint32 reference_type       :  1;
        uint32 reference_size       : 31;
        uint32 subsegment_duration;
        uint32 starts_with_SAP      :  1;
        uint32 SAP_type             :  3;
        uint32 SAP_delta_time       : 28;
    };

    uint32                 reference_id;
    uint32                 time_scale;
    uint64                 earliest_presentation_time;
    uint64                 first_offset;
    mp4::array<entry_type> entries;
};


struct box_header
{
    uint64 fpos;
    uint64 size;
    uint32 type;
    uint8  header_size;
};


class box :
    public intrusive::set_link<>
{
private:
    struct type_less
    {
        bool operator()(box const& x, box const& y) const noexcept
        { return (x.type() < y.type()); }

        bool operator()(box const& x, uint32 const y) const noexcept
        { return (x.type() < y); }

        bool operator()(uint32 const x, box const& y) const noexcept
        { return (x < y.type()); }
    };

public:
    intrusive::multiset<box, box::type_less> children;

    box(box const&) = delete;
    box& operator=(box const&) = delete;

    explicit box(mp4::box_header const& h, mp4::box* const p) noexcept :
        parent_(p),
        header_(h)
    {}

    AMP_NOINLINE
    ~box();

    AMP_NOINLINE AMP_READONLY
    box const* find(char const*) const noexcept;

    auto find(char const* const path) noexcept
    { return const_cast<box*>(const_cast<box const&>(*this).find(path)); }

    auto find_first_of(char const* const first) const noexcept
    { return find(first); }

    template<typename... Rest>
    auto find_first_of(char const* const first,
                       Rest const... rest) const noexcept
    {
        auto found = find(first);
        if (found == nullptr) {
            found = find_first_of(rest...);
        }
        return found;
    }

    auto find_first_of(char const* const first) noexcept
    { return find(first); }

    template<typename... Rest>
    auto find_first_of(char const* const first,
                       Rest const... rest) noexcept
    {
        auto found = find(first);
        if (found == nullptr) {
            found = find_first_of(rest...);
        }
        return found;
    }

    mp4::box& operator[](char const* const path) noexcept
    {
        if (auto found = find(path)) {
            return *found;
        }
        raise(errc::failure, "MP4 box='%s' is not present", path);
    }

    mp4::box const& operator[](char const* const path) const noexcept
    {
        if (auto found = find(path)) {
            return *found;
        }
        raise(errc::failure, "MP4 box='%s' is not present", path);
    }

    auto equal_range(char const* const path) noexcept
    { return make_range(children.equal_range(io::load<uint32,BE>(path))); }

    auto equal_range(char const* const path) const noexcept
    { return make_range(children.equal_range(io::load<uint32,BE>(path))); }

    AMP_INLINE uint32 type() const noexcept
    { return header_.type; }

    AMP_INLINE uint32 header_size() const noexcept
    { return header_.header_size; }

    AMP_INLINE uint64 size() const noexcept
    { return header_.size - header_size(); }

    AMP_INLINE uint64 start_position() const noexcept
    { return header_.fpos; }

    AMP_INLINE uint64 end_position() const noexcept
    { return start_position() + size() + header_size(); }

    AMP_INLINE mp4::box* up() noexcept
    { return parent_; }

    AMP_INLINE mp4::box const* up() const noexcept
    { return parent_; }

    template<typename T>
    AMP_INLINE auto construct()
    noexcept(is_nothrow_default_constructible_v<T>) ->
        enable_if_t<!is_trivially_destructible_v<T>, T&>;

    template<typename T>
    AMP_INLINE auto construct()
    noexcept(is_nothrow_default_constructible_v<T>) ->
        enable_if_t<is_trivially_destructible_v<T>, T&>;

    union {
        char box_data = {};
        ftyp_box_data ftyp;
        mvhd_box_data mvhd;
        tkhd_box_data tkhd;
        mdhd_box_data mdhd;
        chap_box_data chap;
        hdlr_box_data hdlr;
        stts_box_data stts;
        stsd_box_data stsd;
        soun_box_data soun;
        esds_box_data esds;
        alac_box_data alac;
        wave_box_data wave;
        frma_box_data frma;
        enda_box_data enda;
        stsz_box_data stsz;
        stsc_box_data stsc;
        stco_box_data stco;
        elst_box_data elst;
        meta_box_data meta;
        ilst_box_data ilst;
        chpl_box_data chpl;
        mehd_box_data mehd;
        trex_box_data trex;
        mfhd_box_data mfhd;
        tfhd_box_data tfhd;
        tfdt_box_data tfdt;
        trun_box_data trun;
        mfro_box_data mfro;
        tfra_box_data tfra;
        sidx_box_data sidx;
    };

private:
    void (*destroy_)(void const*) noexcept = nullptr;
    mp4::box*       const parent_;
    mp4::box_header const header_;
};


class root_box final :
    public box
{
public:
    explicit root_box(io::stream&);

    void print() const;
};

}}    // namespace amp::mp4


#endif  // AMP_INCLUDED_45A007B2_EC5C_40E9_9CED_E39A490624CD

