////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_track.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_66387B86_F5CB_4081_811F_CE6B1D9287A7
#define AMP_INCLUDED_66387B86_F5CB_4081_811F_CE6B1D9287A7


#include <amp/audio/decoder.hpp>
#include <amp/io/buffer.hpp>
#include <amp/stddef.hpp>

#include "mp4_box.hpp"

#include <numeric>
#include <utility>
#include <vector>


namespace amp {
namespace mp4 {

class track_fragment
{
public:
    explicit track_fragment(mp4::box const& traf) :
        tfhd(traf.find("tfhd")->tfhd)
    {
        for (auto const& box : traf.equal_range("trun")) {
            auto const& trun = box.trun;
            truns.emplace_back(&trun);

            duration += std::accumulate(
                trun.samples.begin(),
                trun.samples.end(),
                uint64{0},
                [](auto const acc, auto const& sample) noexcept {
                    return acc + sample.duration;
                });
        }
    }

    void set_start_time(uint64 const x) noexcept
    { start_time = x; }

    uint64 get_start_time() const noexcept
    { return start_time; }

    uint64 get_duration() const noexcept
    { return duration; }

    int64 get_base_data_offset() const noexcept
    { return tfhd.base_data_offset; }

    mp4::trun_box_data const& operator[](uint32 const i) const noexcept
    { return *truns[i]; }

    uint32 get_trun_count() const noexcept
    { return static_cast<uint32>(truns.size()); }

private:
    uint64 start_time{-1ULL};
    uint64 duration{};
    std::vector<mp4::trun_box_data const*> truns;
    mp4::tfhd_box_data const& tfhd;
};


class track_segment
{
public:
    void set_start_time(uint64 const x) noexcept
    { start_time = x; }

    void add_fragment(mp4::box const& traf)
    {
        auto fragment = mp4::track_fragment{traf};
        if (auto const found = traf.find("tfdt")) {
            fragment.set_start_time(found->tfdt.base_media_decode_time);
        }
        else if (!trafs.empty()) {
            fragment.set_start_time(trafs.back().get_start_time() +
                                    trafs.back().get_duration());
        }
        else {
            fragment.set_start_time(start_time);
        }
        trafs.push_back(std::move(fragment));
    }

    uint64 get_duration() const noexcept
    {
        return std::accumulate(
            trafs.begin(),
            trafs.end(),
            uint64{0},
            [](auto const acc, auto const& fragment) noexcept {
                return acc + fragment.get_duration();
            });
    }

    bool feed(io::stream& file, io::buffer& dest)
    {
        if (traf_number == trafs.size()) {
            return false;
        }

        auto const& traf = trafs[traf_number];
        auto const& trun = traf[trun_number];

        if (sample_number == 0) {
            auto const offset = traf.get_base_data_offset() + trun.data_offset;
            file.seek(offset, file.beg);
        }

        dest.assign(file, trun.samples[sample_number].size);
        if (++sample_number == trun.samples.size()) {
            if (++trun_number == traf.get_trun_count()) {
                ++traf_number;
                trun_number = 0;
            }
            sample_number = 0;
        }
        return true;
    }

    void seek(io::stream& file, uint64 target, uint64& priming)
    {
        for (; traf_number != trafs.size(); ++traf_number) {
            auto&& traf = trafs[traf_number];
            if (target >= traf.get_duration()) {
                target -= traf.get_duration();
                continue;
            }

            trun_number = 0;
            for (; trun_number != traf.get_trun_count(); ++trun_number) {
                auto&& trun = traf[trun_number];
                auto offset = traf.get_base_data_offset() + trun.data_offset;

                sample_number = 0;
                for (; sample_number != trun.samples.size(); ++sample_number) {
                    auto const sample = trun.samples[sample_number];
                    if (target  < sample.duration) {
                        priming = sample.duration - target;
                        file.seek(offset, file.beg);
                        return;
                    }
                    target -= sample.duration;
                    offset += sample.size;
                }
            }
        }
    }

    void reset() noexcept
    {
        traf_number = 0;
        trun_number = 0;
        sample_number = 0;
    }

private:
    uint64 start_time{-1ULL};
    std::vector<track_fragment> trafs;
    uint32 traf_number{};
    uint32 trun_number{};
    uint32 sample_number{};
};


class track
{
public:
    explicit track(mp4::box const& t) noexcept :
        trak(t),
        stbl(*trak.find("mdia/minf/stbl")),
        tkhd(trak.find("tkhd")->tkhd),
        mdhd(trak.find("mdia/mdhd")->mdhd),
        stco(stbl.find_first_of("co64", "stco")->stco),
        stsz(stbl.find_first_of("stsz", "stz2")->stsz),
        stsc(stbl.find("stsc")->stsc),
        stts(stbl.find("stts")->stts)
    {
        segment.set_start_time(mdhd.duration);
    }

    bool feed(io::stream&, io::buffer&);
    void seek(io::stream&, uint64, uint64&);

    uint32 get_id() const noexcept
    { return tkhd.track_id; }

    uint32 get_sample_duration(uint32 number) const noexcept
    {
        for (auto&& entry : stts.entries) {
            if (number <= entry.sample_count) {
                return entry.sample_delta;
            }
            number -= entry.sample_count;
        }
        return 0;
    }

    uint32 get_sample_count() const noexcept
    { return stsz.sample_count; }

    uint32 get_time_scale() const noexcept
    { return mdhd.time_scale; }

    uint64 get_duration() const noexcept
    { return mdhd.duration + segment.get_duration(); }

    uint32 get_handler_type() const noexcept
    {
        auto const found = trak.find("mdia/hdlr");
        AMP_ASSERT(found);
        return found->hdlr.handler_type;
    }

    uint32 get_average_bit_rate() const noexcept;

    mp4::elst_box_data const* get_edit_list() const noexcept
    {
        if (auto const found = trak.find("edts/elst")) {
            return &found->elst;
        }
        return nullptr;
    }

    auto find(char const* const path) noexcept
    { return trak.find(path); }

    auto find(char const* const path) const noexcept
    { return trak.find(path); }

    void add_fragment(mp4::box const& traf)
    { segment.add_fragment(traf); }

    optional<audio::codec_format> select_first_audio_sample_entry();

private:
    mp4::box const& trak;
    mp4::box const& stbl;
    mp4::tkhd_box_data const& tkhd;
    mp4::mdhd_box_data const& mdhd;
    mp4::stco_box_data const& stco;
    mp4::stsz_box_data const& stsz;
    mp4::stsc_box_data const& stsc;
    mp4::stts_box_data const& stts;
    mp4::track_segment segment;

    uint32 qtff_sample_size{};
    uint32 qtff_samples_per_packet{1};
    uint32 sample_number{};
    uint32 chunk_number{};
    uint32 last_sample_in_chunk{};
};

}}    // namespace amp::mp4


#endif  // AMP_INCLUDED_66387B86_F5CB_4081_811F_CE6B1D9287A7

