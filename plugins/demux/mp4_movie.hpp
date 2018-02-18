////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_movie.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_A4A8CA5F_3B07_4027_A2AA_D98A206F82B2
#define AMP_INCLUDED_A4A8CA5F_3B07_4027_A2AA_D98A206F82B2


#include <amp/optional.hpp>
#include <amp/stddef.hpp>

#include "mp4_box.hpp"
#include "mp4_track.hpp"

#include <utility>
#include <vector>


namespace amp {
namespace media {
    class dictionary;
    class image;
}


namespace mp4 {

struct iTunSMPB
{
    uint32 priming;
    uint32 padding;
    uint64 frames;
};


class movie
{
public:
    explicit movie(mp4::box const& m) :
        moov(m),
        mvhd(moov.find("mvhd")->mvhd)
    {
        for (auto&& trak : moov.equal_range("trak")) {
            tracks.emplace_back(trak);
        }

        auto found = moov.find("udta/meta/hdlr");
        if (found && (found->hdlr.handler_type == metadata_handler::itunes)) {
            found = found->find("../ilst");
            if (found) {
                ilst = &found->ilst;
            }
        }
    }

    uint64 get_duration() const noexcept
    { return mvhd.duration; }

    uint32 get_time_scale() const noexcept
    { return mvhd.time_scale; }

    mp4::track* find_track_with_id(uint32 const id) noexcept
    {
        for (auto&& track : tracks) {
            if (track.get_id() == id) {
                return &track;
            }
        }
        return nullptr;
    }

    mp4::track* find_chapter_track(mp4::chap_box_data const& chap) noexcept
    {
        for (auto const id : chap.track_ids) {
            auto const track = find_track_with_id(id);
            if (track && (track->get_handler_type() == media_handler::text)) {
                return track;
            }
        }
        return nullptr;
    }

    auto find(char const* const path) noexcept
    { return moov.find(path); }

    auto find(char const* const path) const noexcept
    { return moov.find(path); }

    template<typename... Args>
    auto find_first_of(Args&&... args) noexcept
    { return moov.find(std::forward<Args>(args)...); }

    template<typename... Args>
    auto find_first_of(Args&&... args) const noexcept
    { return moov.find(std::forward<Args>(args)...); }

    auto begin() noexcept
    { return tracks.begin(); }

    auto end() noexcept
    { return tracks.end(); }

    auto begin() const noexcept
    { return tracks.begin(); }

    auto end() const noexcept
    { return tracks.end(); }

    auto cbegin() const noexcept
    { return tracks.cbegin(); }

    auto cend() const noexcept
    { return tracks.cend(); }

    media::dictionary get_tags() const;
    media::image get_cover_art() const;
    optional<iTunSMPB> get_iTunSMPB() const;

private:
    mp4::box const& moov;
    mp4::mvhd_box_data const& mvhd;
    mp4::ilst_box_data const* ilst{};
    std::vector<mp4::track> tracks;
};

}}    // namespace amp::mp4


#endif  // AMP_INCLUDED_A4A8CA5F_3B07_4027_A2AA_D98A206F82B2

