////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_box.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/cxp/map.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "mp4_box.hpp"
#include "mp4_descriptor.hpp"

#include <cinttypes>
#include <iterator>
#include <memory>
#include <utility>


namespace amp {
namespace mp4 {

template<typename T>
static void type_erased_delete_(void const* const p) noexcept
{
    static_cast<T const*>(p)->~T();
}


template<typename T>
AMP_INLINE auto box::construct()
noexcept(is_nothrow_default_constructible_v<T>) ->
    enable_if_t<!is_trivially_destructible_v<T>, T&>
{
    ::new(static_cast<void*>(&box_data)) T;
    destroy_ = type_erased_delete_<T>;
    return reinterpret_cast<T&>(box_data);
}

template<typename T>
AMP_INLINE auto box::construct()
noexcept(is_nothrow_default_constructible_v<T>) ->
    enable_if_t<is_trivially_destructible_v<T>, T&>
{
    return *(::new(static_cast<void*>(&box_data)) T);
}


namespace {

constexpr uint32 fourcc_(char const* const s) noexcept
{
    return (uint32{static_cast<uint8>(s[0])} << 24)
         | (uint32{static_cast<uint8>(s[1])} << 16)
         | (uint32{static_cast<uint8>(s[2])} <<  8)
         | (uint32{static_cast<uint8>(s[3])} <<  0);
}


constexpr auto required = uint32{0x00000080};
constexpr auto unique   = uint32{0x00008000};

class box_spec
{
public:
    constexpr box_spec(char const* const s, uint32 const flags) noexcept :
        data{fourcc_(s) | flags}
    {
        AMP_ASSERT((fourcc_(s) & (required|unique)) == 0);
    }

    uint32 type() const noexcept
    { return data & ~(required|unique); }

    bool test(uint32 const flag) const noexcept
    { return (data & flag) != 0; }

private:
    uint32 data;
};


struct box_checker
{
    void update(mp4::box const& box) noexcept
    {
        for (auto const i : xrange(count)) {
            if (specs[i].type() == box.type()) {
                state[i] += 1;
                break;
            }
        }
    }

    void finish() const
    {
        char const* what;
        uint32 type;

        for (auto const i : xrange(count)) {
            if (specs[i].test(required)) {
                if (AMP_UNLIKELY(state[i] == 0)) {
                    type = specs[i].type();
                    what = "not present";
                    goto fail;
                }
            }
            if (specs[i].test(unique)) {
                if (AMP_UNLIKELY(state[i] > 1)) {
                    type = specs[i].type();
                    what = "not unique";
                    goto fail;
                }
            }
        }
        return;

fail:
        auto const name = net::to_host<BE>(type);
        raise(errc::invalid_data_format, "MP4: box '%.4s' is %s",
              reinterpret_cast<char const*>(&name), what);
    }

    mp4::box_spec const* specs;
    uint32*              state;
    uint32               count;
};


mp4::box_header read_box_header(io::stream& file)
{
    mp4::box_header header;
    header.header_size = 8;
    header.fpos = file.tell();

    file.gather<BE>(io::alias<uint32>(header.size), header.type);

    if (header.size < header.header_size) {
        switch (header.size) {
        case 0:
            header.size = file.size() - header.fpos;
            break;
        case 1:
            header.size = file.read<uint64,BE>();
            header.header_size += 8;
            break;
        }

        if (header.size < header.header_size) {
            raise(errc::invalid_data_format,
                  "MP4 box cannot be smaller than its header");
        }
    }
    return header;
}

AMP_INLINE auto& add_child(mp4::box& parent, mp4::box_header const& header)
{
    return *parent.children.insert(*new mp4::box{header, &parent});
}

void read_container(mp4::box&, io::stream&, mp4::box_checker);

template<uint32 N>
AMP_INLINE void read_container(mp4::box& parent, io::stream& file,
                               mp4::box_spec const(&specs)[N])
{
    uint32 counts[N] = {};
    return read_container(parent, file, box_checker{specs, counts, N});
}

AMP_INLINE void read_container(mp4::box& parent, io::stream& file)
{
    return mp4::read_container(parent, file, box_checker{});
}

void read_ftyp(mp4::box& box, io::stream& file)
{
    if (box.size() < 8) {
        raise(errc::invalid_data_format, "MP4 'ftyp' box is too small");
    }

    auto&& ftyp = box.construct<ftyp_box_data>();
    file.gather<BE>(ftyp.major_brand,
                    ftyp.minor_version);

    auto compatible_brand_count = numeric_cast<uint32>((box.size() - 8) / 4);
    ftyp.compatible_brands.assign(file, compatible_brand_count);
}

void read_mvhd(mp4::box& box, io::stream& file)
{
    auto&& mvhd = box.construct<mvhd_box_data>();
    file.read(mvhd.version_and_flags);

    if (mvhd.version() == 1) {
        file.gather<BE>(mvhd.creation_time,
                        mvhd.modification_time,
                        mvhd.time_scale,
                        mvhd.duration);
    }
    else {
        file.gather<BE>(io::alias<uint32>(mvhd.creation_time),
                        io::alias<uint32>(mvhd.modification_time),
                        mvhd.time_scale,
                        io::alias<uint32>(mvhd.duration));
    }

    file.gather<BE>(mvhd.rate,
                    mvhd.volume,
                    io::ignore<10>,
                    mvhd.matrix,
                    mvhd.preview_time,
                    mvhd.preview_duration,
                    mvhd.poster_time,
                    mvhd.selection_time,
                    mvhd.selection_duration,
                    mvhd.current_time,
                    mvhd.next_track_id);
}

void read_tkhd(mp4::box& box, io::stream& file)
{
    auto&& tkhd = box.construct<tkhd_box_data>();
    file.read(tkhd.version_and_flags);

    if (tkhd.version() == 1) {
        file.gather<BE>(tkhd.creation_time,
                        tkhd.modification_time,
                        tkhd.track_id,
                        io::ignore<4>,
                        tkhd.duration);
    }
    else {
        file.gather<BE>(io::alias<uint32>(tkhd.creation_time),
                        io::alias<uint32>(tkhd.modification_time),
                        tkhd.track_id,
                        io::ignore<4>,              // reserved
                        io::alias<uint32>(tkhd.duration));
    }

    file.gather<BE>(io::ignore<8>,
                    tkhd.layer,
                    tkhd.alternate_group,
                    tkhd.volume,
                    io::ignore<2>,
                    tkhd.matrix,
                    tkhd.width,
                    tkhd.height);
}

void read_mdhd(mp4::box& box, io::stream& file)
{
    auto&& mdhd = box.construct<mdhd_box_data>();
    file.read(mdhd.version_and_flags);

    if (mdhd.version() == 1) {
        file.gather<BE>(mdhd.creation_time,
                        mdhd.modification_time,
                        mdhd.time_scale,
                        mdhd.duration,
                        mdhd.language,
                        mdhd.quality);
    }
    else {
        file.gather<BE>(io::alias<uint32>(mdhd.creation_time),
                        io::alias<uint32>(mdhd.modification_time),
                        mdhd.time_scale,
                        io::alias<uint32>(mdhd.duration),
                        mdhd.language,
                        mdhd.quality);
    }
}

void read_chap(mp4::box& box, io::stream& file)
{
    auto&& chap = box.construct<chap_box_data>();
    auto track_id_count = numeric_cast<uint32>(box.size() / sizeof(uint32));
    chap.track_ids.assign(file, track_id_count);
}

void read_hdlr(mp4::box& box, io::stream& file)
{
    auto&& hdlr = box.construct<hdlr_box_data>();
    file.gather<BE>(hdlr.version_and_flags,
                    hdlr.pre_defined,
                    hdlr.handler_type,
                    io::ignore<12>);
}

void read_stts(mp4::box& box, io::stream& file)
{
    auto&& stts = box.construct<stts_box_data>();
    uint32 stts_entry_count;
    file.gather<BE>(stts.version_and_flags,
                    stts_entry_count);

    stts.entries.resize(stts_entry_count);
    for (auto&& entry : stts.entries) {
        file.gather<BE>(entry.sample_count,
                        entry.sample_delta);
    }
}

void read_soun(mp4::box& box, io::stream& file)
{
    auto&& soun = box.construct<soun_box_data>();
    file.gather<BE>(io::ignore<6>,
                    soun.data_reference_index,
                    soun.version,
                    soun.revision,
                    soun.vendor,
                    soun.channels,
                    soun.sample_size,
                    soun.compression_id,
                    soun.packet_size,
                    soun.sample_rate);

    if (soun.version == 2) {
        file.gather<BE>(soun.v2.size_of_struct_only,
                        soun.v2.audio_sample_rate,
                        soun.v2.audio_channels,
                        soun.v2.always_7F000000,
                        soun.v2.const_bits_per_channel,
                        soun.v2.format_specific_flags,
                        soun.v2.const_bytes_per_audio_packet,
                        soun.v2.const_lpcm_frames_per_audio_packet);
    }
    else if (soun.version == 1) {
        file.gather<BE>(soun.v1.samples_per_packet,
                        soun.v1.bytes_per_packet,
                        soun.v1.bytes_per_frame,
                        soun.v1.bytes_per_sample);
    }
    else if (soun.version != 0) {
        raise(errc::failure,
              "invalid MPEG-4 audio sample entry box version: %" PRId16,
              soun.version);
    }
    mp4::read_container(box, file);
}

void read_stsd(mp4::box& box, io::stream& file)
{
    auto found = box.find("../../../hdlr");
    if (!found) {
        raise(errc::failure,
              "MP4 sample table description box cannot appear "
              "before the media handler reference box");
    }
    if (found->hdlr.handler_type != media_handler::audio) {
        return;
    }

    auto&& stsd = box.construct<stsd_box_data>();
    uint32 stsd_entry_count;
    file.gather<BE>(stsd.version_and_flags,
                    stsd_entry_count);

    while (stsd_entry_count-- != 0) {
        auto const header = mp4::read_box_header(file);
        if (header.type == 0) {
            break;
        }

        auto&& soun = add_child(box, header);
        read_soun(soun, file);
        file.seek(header.fpos + header.size);
    }
}

void read_esds(mp4::box& box, io::stream& file)
{
    auto&& esds = box.construct<esds_box_data>();
    file.read(esds.version_and_flags);

    if (box.size() <= 4) {
        raise(errc::invalid_data_format, "MP4 'esds' box is too small");
    }
    esds.dcd.parse(io::buffer{file, box.size() - 4});
}

void read_alac(mp4::box& box, io::stream& file)
{
    auto&& alac = box.construct<alac_box_data>();
    file.read(alac.version_and_flags);
    alac.extra.assign(file, box.size() - 4);
}

void read_wave(mp4::box& box, io::stream& file)
{
    auto&& wave = box.construct<wave_box_data>();
    wave.extra.assign(file, box.size());
}

void read_frma(mp4::box& box, io::stream& file)
{
    auto&& frma = box.construct<frma_box_data>();
    frma.data_format = file.read<uint32,BE>();
}

void read_enda(mp4::box& box, io::stream& file)
{
    auto&& enda = box.construct<enda_box_data>();
    enda.little_endian = file.read<int16,BE>();
}

void read_stsz(mp4::box& box, io::stream& file)
{
    auto&& stsz = box.construct<stsz_box_data>();
    file.gather<BE>(stsz.version_and_flags,
                    stsz.sample_size,
                    stsz.sample_count);

    if (stsz.sample_size == 0) {
        stsz.entries.assign(file, stsz.sample_count);
    }
}

void read_stz2(mp4::box& box, io::stream& file)
{
    // XXX: intentionally constructing 'stsz_box_data' as there is no
    //      independent 'stz2_box_data' type.

    auto&& stz2 = box.construct<stsz_box_data>();
    uint8  stz2_reserved[3];
    uint8  stz2_field_size;
    file.gather<BE>(stz2.version_and_flags,
                    stz2_reserved,
                    stz2_field_size,
                    stz2.sample_count);

    stz2.sample_size = 0;
    stz2.entries.resize(stz2.sample_count);

    switch (stz2_field_size) {
    case 16:
        for (auto&& entry : stz2.entries) {
            entry = file.read<uint16,BE>();
        }
        break;
    case 8:
        for (auto&& entry : stz2.entries) {
            entry = file.read<uint8>();
        }
        break;
    case 4:
        for (auto i = uint32{0}; (i + 2) <= stz2.sample_count; i += 2) {
            auto const byte = file.read<uint8>();
            stz2.entries[i + 0] = (uint32{byte} >> 4) & 0xf;
            stz2.entries[i + 1] = (uint32{byte} >> 0) & 0xf;
        }
        if (stz2.sample_count & 1) {
            stz2.entries.back() = (uint32{file.read<uint8>()} >> 4) & 0xf;
        }
        break;
    default:
        raise(errc::failure, "[MP4] invalid 'stz2' field size: %" PRIu8,
              stz2_field_size);
    }
}

void read_stsc(mp4::box& box, io::stream& file)
{
    auto&& stsc = box.construct<stsc_box_data>();
    uint32 stsc_entry_count;
    file.gather<BE>(stsc.version_and_flags,
                    stsc_entry_count);

    stsc.entries.resize(stsc_entry_count);
    for (auto&& entry : stsc.entries) {
        file.gather<BE>(entry.first_chunk,
                        entry.samples_per_chunk,
                        entry.sample_description_index);
    }

    auto sample = uint32{0};
    auto const last = stsc.entries.end();
    for (auto first = stsc.entries.begin(); first != last; ++first) {
        first->first_sample = sample;
        if ((first + 1) != last) {
            sample += (first[1].first_chunk - first[0].first_chunk)
                    * (first[0].samples_per_chunk);
        }
    }
}

void read_stco(mp4::box& box, io::stream& file)
{
    auto&& stco = box.construct<stco_box_data>();
    uint32 stco_entry_count;
    file.gather<BE>(stco.version_and_flags,
                    stco_entry_count);

    stco.entries.resize(stco_entry_count);
    for (auto&& entry : stco.entries) {
        entry = file.read<uint32,BE>();
    }
}

void read_co64(mp4::box& box, io::stream& file)
{
    // XXX: intentionally constructing 'stco_box_data' as there is no
    //      independent 'co64_box_data' type.

    auto&& co64 = box.construct<stco_box_data>();
    uint32 co64_entry_count;
    file.gather<BE>(co64.version_and_flags,
                    co64_entry_count);

    co64.entries.assign(file, co64_entry_count);
}

void read_elst(mp4::box& box, io::stream& file)
{
    auto&& elst = box.construct<elst_box_data>();
    uint32 elst_entry_count;
    file.gather<BE>(elst.version_and_flags,
                    elst_entry_count);

    elst.entries.resize(elst_entry_count);
    if (elst.version() == 1) {
        for (auto&& entry : elst.entries) {
            file.gather<BE>(entry.segment_duration,
                            entry.media_time,
                            entry.media_rate);
        }
    }
    else {
        for (auto&& entry : elst.entries) {
            file.gather<BE>(io::alias<uint32>(entry.segment_duration),
                            io::alias<int32>(entry.media_time),
                            entry.media_rate);
        }
    }
}

void read_meta(mp4::box& box, io::stream& file)
{
    auto&& meta = box.construct<meta_box_data>();
    file.read(meta.version_and_flags);
    mp4::read_container(box, file);
}

void read_ilst(mp4::box& box, io::stream& file)
{
    auto&& ilst = box.construct<ilst_box_data>();

    auto const end_pos = box.end_position();
    while (file.tell() < end_pos) {
        auto header = read_box_header(file);
        auto const item_end_pos = header.fpos + header.size;

        ilst_entry item;
        item.type = header.type;

        while (file.tell() < item_end_pos) {
            header = read_box_header(file);
            auto const remain = header.size - header.header_size;

            switch (header.type) {
            case "mean"_4cc:
            case "name"_4cc:
                {
                    u8string_buffer buf(remain - 4, uninitialized);
                    file.skip(4);
                    file.read(buf.data(), buf.size());

                    (header.type == "mean"_4cc
                     ? item.mean : item.name) = buf.promote();
                }
                break;
            case "data"_4cc:
                file.gather<BE>(io::ignore<2>,
                                item.type_set_identifier,
                                item.data_type,
                                item.locale_country,
                                item.locale_language);
                item.data.assign(file, remain - 8);
                break;
            default:
                file.skip(remain);
            }
        }

        file.seek(item_end_pos);
        ilst.push_back(std::move(item));
    }
}

void read_chpl(mp4::box& box, io::stream& file)
{
    auto&& chpl = box.construct<chpl_box_data>();
    file.read(chpl.version_and_flags);

    uint32 chpl_entry_count;
    if (chpl.version() == 1) {
        file.gather<BE>(io::ignore<1>, chpl_entry_count);
    }
    else {
        chpl_entry_count = file.read<uint8>();
    }

    chpl.entries.resize(chpl_entry_count);
    for (auto&& entry : chpl.entries) {
        uint8 title_length;
        file.gather<BE>(entry.start, title_length);

        char title[256];
        file.read(title, title_length);
        entry.title = u8string::from_utf8(title, title_length);
    }
}

void read_mehd(mp4::box& box, io::stream& file)
{
    auto&& mehd = box.construct<mehd_box_data>();
    file.read(mehd.version_and_flags);

    if (mehd.version() == 1) {
        mehd.fragment_duration = file.read<uint64,BE>();
    }
    else {
        mehd.fragment_duration = file.read<uint32,BE>();
    }
}

void read_trex(mp4::box& box, io::stream& file)
{
    auto&& trex = box.construct<trex_box_data>();
    file.gather<BE>(trex.version_and_flags,
                    trex.track_id,
                    trex.default_sample_description_index,
                    trex.default_sample_duration,
                    trex.default_sample_size,
                    trex.default_sample_flags);
}

void read_mfhd(mp4::box& box, io::stream& file)
{
    auto&& mfhd = box.construct<mfhd_box_data>();
    file.gather<BE>(mfhd.version_and_flags,
                    mfhd.sequence_number);
}

void read_tfhd(mp4::box& box, io::stream& file)
{
    auto&& tfhd = box.construct<tfhd_box_data>();
    file.gather<BE>(tfhd.version_and_flags,
                    tfhd.track_id);

    tfhd.base_data_offset         = 0;
    tfhd.sample_description_index = 0;
    tfhd.default_sample_duration  = 0;
    tfhd.default_sample_size      = 0;
    tfhd.default_sample_flags     = 0;

    if (tfhd.base_data_offset_present()) {
        tfhd.base_data_offset = file.read<int64,BE>();
    }
    else if (tfhd.default_base_is_moof()) {
        auto const& moof = *box.up()->up();
        tfhd.base_data_offset = static_cast<int64>(moof.start_position());
    }

    if (tfhd.sample_description_index_present()) {
        tfhd.sample_description_index = file.read<uint32,BE>();
    }
    if (tfhd.default_sample_duration_present()) {
        tfhd.default_sample_duration = file.read<uint32,BE>();
    }
    if (tfhd.default_sample_size_present()) {
        tfhd.default_sample_size = file.read<uint32,BE>();
    }
    if (tfhd.default_sample_flags_present()) {
        tfhd.default_sample_flags = file.read<uint32,BE>();
    }
}

void read_tfdt(mp4::box& box, io::stream& file)
{
    auto&& tfdt = box.construct<tfdt_box_data>();
    file.read(tfdt.version_and_flags);

    if (tfdt.version() != 0) {
        tfdt.base_media_decode_time = file.read<uint64,BE>();
    }
    else {
        tfdt.base_media_decode_time = file.read<uint32,BE>();
    }
}

void read_trun(mp4::box& box, io::stream& file)
{
    auto&& trun = box.construct<trun_box_data>();
    uint32 trun_sample_count;
    file.gather<BE>(trun.version_and_flags,
                    trun_sample_count);

    trun.data_offset        = 0;
    trun.first_sample_flags = 0;

    if (trun.data_offset_present()) {
        trun.data_offset = file.read<int32,BE>();
    }
    if (trun.first_sample_flags_present()) {
        trun.first_sample_flags = file.read<uint32,BE>();
    }

    auto&& tfhd = box["../tfhd"].tfhd;
    auto&& trex = [&] {
        for (auto&& entry : box["/moov/mvex"].equal_range("trex")) {
            if (entry.trex.track_id == tfhd.track_id) {
                return entry.trex;
            }
        }
        raise(errc::invalid_data_format,
              "MP4 'trex' box is missing for track ID %" PRIu32,
              tfhd.track_id);
    }();

    auto const default_sample_duration = tfhd.default_sample_flags_present()
                                       ? tfhd.default_sample_duration
                                       : trex.default_sample_duration;

    auto const default_sample_size = tfhd.default_sample_size_present()
                                   ? tfhd.default_sample_size
                                   : trex.default_sample_size;

    auto const default_sample_flags = tfhd.default_sample_flags_present()
                                    ? tfhd.default_sample_flags
                                    : trex.default_sample_flags;

    auto const default_sample_composition_time_offset = uint32{0};

#define READ_FIELD_OR_DEFAULT(Field)                                        \
    sample.Field = trun.sample_##Field##_present()                          \
                 ? file.read<uint32,BE>()                                   \
                 : default_sample_##Field

    trun.samples.resize(trun_sample_count);
    for (auto& sample : trun.samples) {
        READ_FIELD_OR_DEFAULT(duration);
        READ_FIELD_OR_DEFAULT(size);
        READ_FIELD_OR_DEFAULT(flags);
        READ_FIELD_OR_DEFAULT(composition_time_offset);
    }
}

void read_mfro(mp4::box& box, io::stream& file)
{
    auto&& mfro = box.construct<mfro_box_data>();
    file.gather<BE>(mfro.version_and_flags,
                    mfro.size);
}

void read_tfra(mp4::box& box, io::stream& file)
{
    auto const read_vint = [&file](uint32 n) -> uint32 {
        ++n;
        AMP_ASSERT(n >= 1 && n <= 4);
        AMP_ASSUME(n >= 1 && n <= 4);

        uint32 v{};
        file.read(reinterpret_cast<uchar*>(&v) + (4 - n), n);
        return net::to_host<BE>(v);
    };

    auto&& tfra = box.construct<tfra_box_data>();
    uint32 tfra_entry_sizes;
    uint32 tfra_entry_count;
    file.gather<BE>(tfra.version_and_flags,
                    tfra.track_id,
                    tfra_entry_sizes,
                    tfra_entry_count);

    tfra.entries.resize(tfra_entry_count);
    for (auto&& entry : tfra.entries) {
        if (tfra.version() == 1) {
            file.gather<BE>(entry.time,
                            entry.moof_offset);
        }
        else {
            file.gather<BE>(io::alias<uint32>(entry.time),
                            io::alias<uint32>(entry.moof_offset));
        }

        entry.traf_number   = read_vint((tfra_entry_sizes >> 4) & 0x3);
        entry.trun_number   = read_vint((tfra_entry_sizes >> 2) & 0x3);
        entry.sample_number = read_vint((tfra_entry_sizes >> 0) & 0x3);
    }
}

void read_sidx(mp4::box& box, io::stream& file)
{
    auto&& sidx = box.construct<sidx_box_data>();
    file.gather<BE>(sidx.version_and_flags,
                    sidx.reference_id,
                    sidx.time_scale);

    uint16 sidx_entry_count;
    if (sidx.version() == 1) {
        file.gather<BE>(sidx.earliest_presentation_time,
                        sidx.first_offset,
                        io::ignore<2>,
                        sidx_entry_count);
    }
    else {
        file.gather<BE>(io::alias<uint32>(sidx.earliest_presentation_time),
                        io::alias<uint32>(sidx.first_offset),
                        io::ignore<2>,
                        sidx_entry_count);
    }

    sidx.entries.resize(sidx_entry_count);
    for (auto&& entry : sidx.entries) {
        uint32 tmp[3];
        file.gather<BE>(tmp);

        entry.reference_type      =  tmp[0] >> 31;
        entry.reference_size      =  tmp[0] & 0x7fffffff;
        entry.subsegment_duration =  tmp[1];
        entry.starts_with_SAP     =  tmp[2] >> 31;
        entry.SAP_type            = (tmp[2] >> 28) & 0x7;
        entry.SAP_delta_time      =  tmp[2] & 0x0fffffff;
    }
}

void read_edts(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "elst", required|unique },
    };
    mp4::read_container(box, file, spec);
}

void read_mdia(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "hdlr", required|unique },
        { "mdhd", required|unique },
        { "minf", required|unique },
    };
    mp4::read_container(box, file, spec);
}

void read_mfra(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "mfro", required|unique },
    };
    mp4::read_container(box, file, spec);
}

void read_minf(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "stbl", required|unique },
    };
    mp4::read_container(box, file, spec);
}

void read_moof(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "mfhd", required|unique },
        { "traf", required        },
    };
    mp4::read_container(box, file, spec);
}

void read_moov(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "mvhd", required|unique },
        { "trak", required        },
        { "udta",          unique },
    };
    mp4::read_container(box, file, spec);
}

void read_mvex(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "mehd",          unique },
        { "trex", required        },
    };
    mp4::read_container(box, file, spec);
}

void read_stbl(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "co64",          unique },
        { "stco",          unique },
        { "stsz",          unique },
        { "stz2",          unique },
        { "stsc", required|unique },
        { "stsd", required|unique },
        { "stts", required|unique },
        { "stss",          unique },
        { "stsh",          unique },
        { "ctts",          unique },
    };
    mp4::read_container(box, file, spec);

    if (!box.find_first_of("stco", "co64")) {
        raise(errc::invalid_data_format, "MP4 'stbl.stco' box is missing");
    }
    if (!box.find_first_of("stsz", "stz2")) {
        raise(errc::invalid_data_format, "MP4 'stbl.stsz' box is missing");
    }
}

void read_traf(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        {"tfhd", required|unique},
    };
    mp4::read_container(box, file, spec);
}

void read_trak(mp4::box& box, io::stream& file)
{
    static constexpr mp4::box_spec const spec[] {
        { "edts",          unique },
        { "mdia", required|unique },
        { "tkhd", required|unique },
        { "tref",          unique },
        { "udta",          unique },
    };
    mp4::read_container(box, file, spec);
}


struct box_path
{
public:
    constexpr box_path(uint32 const hi, uint32 const lo) noexcept :
        id{(uint64{hi} << 32) | lo}
    {}

    constexpr box_path(char const(&s)[5]) noexcept :
        box_path{0, fourcc_(s)}
    {}

    constexpr box_path(char const(&s)[10]) noexcept :
        box_path{fourcc_(s), fourcc_(s + 5)}
    {}

private:
    uint64 id;

    friend constexpr bool operator<(box_path const& x,
                                    box_path const& y) noexcept
    { return (x.id < y.id); }
};


using box_reader = void (*)(mp4::box&, io::stream&);

constexpr cxp::map<mp4::box_path, mp4::box_reader, 58> box_readers {{
    {      "ftyp", read_ftyp      },
    {      "mfra", read_mfra      },
    {      "moof", read_moof      },
    {      "moov", read_moov      },
    {      "sidx", read_sidx      },
    {      "styp", read_ftyp      },
    { "QDM2/wave", read_wave      },
    { "QDMC/wave", read_wave      },
    { "alac/alac", read_alac      },
    { "alac/wave", read_container },
    { "edts/elst", read_elst      },
    { "fl32/wave", read_container },
    { "fl64/wave", read_container },
    { "in24/wave", read_container },
    { "in32/wave", read_container },
    { "mdia/hdlr", read_hdlr      },
    { "mdia/mdhd", read_mdhd      },
    { "mdia/minf", read_minf      },
    { "meta/hdlr", read_hdlr      },
    { "meta/ilst", read_ilst      },
    { "mfra/mfro", read_mfro      },
    { "mfra/tfra", read_tfra      },
    { "minf/hdlr", read_hdlr      },
    { "minf/stbl", read_stbl      },
    { "moof/mfhd", read_mfhd      },
    { "moof/traf", read_traf      },
    { "moov/meta", read_meta      },
    { "moov/mvex", read_mvex      },
    { "moov/mvhd", read_mvhd      },
    { "moov/trak", read_trak      },
    { "moov/udta", read_container },
    { "mp4a/esds", read_esds      },
    { "mp4a/wave", read_container },
    { "mvex/mehd", read_mehd      },
    { "mvex/trex", read_trex      },
    { "stbl/co64", read_co64      },
    { "stbl/stco", read_stco      },
    { "stbl/stsc", read_stsc      },
    { "stbl/stsd", read_stsd      },
    { "stbl/stsz", read_stsz      },
    { "stbl/stts", read_stts      },
    { "stbl/stz2", read_stz2      },
    { "traf/tfdt", read_tfdt      },
    { "traf/tfhd", read_tfhd      },
    { "traf/trun", read_trun      },
    { "trak/edts", read_edts      },
    { "trak/mdia", read_mdia      },
    { "trak/meta", read_meta      },
    { "trak/tkhd", read_tkhd      },
    { "trak/tref", read_container },
    { "trak/udta", read_container },
    { "tref/chap", read_chap      },
    { "udta/chpl", read_chpl      },
    { "udta/meta", read_meta      },
    { "wave/alac", read_alac      },
    { "wave/enda", read_enda      },
    { "wave/esds", read_esds      },
    { "wave/frma", read_frma      },
}};
static_assert(cxp::is_sorted(box_readers), "");


void read_container(mp4::box& parent, io::stream& file, mp4::box_checker check)
{
    auto const container_end = parent.end_position();

    for (auto pos = file.tell(); pos < container_end; ) {
        auto const header = mp4::read_box_header(file);
        auto&& box = add_child(parent, header);

        auto query = mp4::box_path{parent.type(), header.type};
        auto found = mp4::box_readers.find(query);
        if (found != mp4::box_readers.end()) {
            found->second(box, file);
        }

        check.update(box);
        pos = file.tell();

        auto const box_end = box.end_position();
        if (pos != box_end) {
            if (pos > box_end) {
                raise(errc::failure,
                      "MP4: read outside of box boundaries ("
                      "expected file offset=%" PRIu64
                      ", actual file offset=%" PRIu64 ")",
                      box_end, pos);
            }
            pos = box_end;
            file.seek(pos);
        }
    }

    check.finish();
    file.seek(container_end);
}

}     // namespace <unnamed>


box const* box::find(char const* p) const noexcept
{
    auto x = this;
    if (p[0] == '/') {
        p++;
        while (x->up() != nullptr) {
            x = x->up();
        }
    }

    while (p[0] != '\0') {
        if (p[0] == '.' && (p[1] == '/' || p[1] == '\0')) {
            p += 1;
        }
        else if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) {
            if (!x->up()) {
                return nullptr;
            }
            x = x->up();
            p += 2;
        }
        else {
            AMP_ASSERT(p[4] == '/' || p[4] == '\0');

            auto found = x->children.find(io::load<uint32,BE>(p));
            if (found == x->children.end()) {
                return nullptr;
            }

            x = &(*found);
            p += 4;
        }
        p += (p[0] == '/');
    }
    return x;
}

box::~box()
{
    if (destroy_) {
        destroy_(&box_data);
    }
}

root_box::root_box(io::stream& file) :
    box(mp4::box_header{0, file.size(), 0, 0}, nullptr)
{
    static constexpr mp4::box_spec const spec[] {
        { "ftyp",          unique },
        { "moov", required|unique },
        { "mvex",          unique },
        { "pdin",          unique },
    };
    mp4::read_container(*this, file, spec);

    optional<uint32> last_sequence_number;
    for (auto&& moof : equal_range("moof")) {
        auto&& mfhd = moof.find("mfhd")->mfhd;
        if (mfhd.sequence_number <= last_sequence_number) {
            raise(errc::failure,
                  "MP4 movie fragments must be in increasing order");
        }
        last_sequence_number = mfhd.sequence_number;
    }
}


#if 0
static void print_box(mp4::box const& box, int const indent)
{
    char name[4];
    io::store<BE>(name, box.type());

    std::fprintf(stderr,
                 "\n"
                 "%*s[" "\033[00;35m" "%.4s" "\033[00;00m"
                 "] @"  "\033[00;34m" "%llu" "\033[00;00m"
                 ":"    "\033[00;32m" "%llu" "\033[00;00m",
                 indent, "| ", name,
                 box.start_position(),
                 box.end_position());

    for (auto const& child : box.children) {
        print_box(child, indent + 4);
    }
}

void root_box::print() const
{
    std::fputs("\n[MP4] {", stderr);
    for (auto const& child : children) {
        print_box(child, 4);
    }
    std::fputs("\n}\n", stderr);
}
#endif

}}    // namespace amp::mp4

