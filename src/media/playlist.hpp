////////////////////////////////////////////////////////////////////////////////
//
// media/playlist.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_6D040B4C_69A0_4793_BDBB_E8DB1F8E39AF
#define AMP_INCLUDED_6D040B4C_69A0_4793_BDBB_E8DB1F8E39AF


#include <amp/error.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "media/track.hpp"

#include <atomic>
#include <string_view>
#include <utility>
#include <vector>


namespace amp {
namespace media {

enum class sort_order : int {
    ascending = -1,
    descending = 1,
};

enum class playback_order : int {
    linear,
    random,
    repeat,
};


class playlist
{
private:
    template<typename>
    friend class amp::ref_ptr;

public:
    using iterator        = std::vector<media::track>::iterator;
    using const_iterator  = std::vector<media::track>::const_iterator;
    using difference_type = std::vector<media::track>::difference_type;
    using size_type       = std::vector<media::track>::size_type;

    auto empty() const noexcept { return tracks_.empty(); }
    auto size()  const noexcept { return tracks_.size(); }

    auto begin()        noexcept { return tracks_.begin(); }
    auto end()          noexcept { return tracks_.end(); }
    auto begin()  const noexcept { return tracks_.begin(); }
    auto end()    const noexcept { return tracks_.end(); }
    auto cbegin() const noexcept { return tracks_.cbegin(); }
    auto cend()   const noexcept { return tracks_.cend(); }

    auto& at(size_type const n) const
    { return tracks_.at(n); }

    auto& playing() const
    { return at(position()); }

    void set_playback_order(media::playback_order const x) noexcept
    { gen_order_ = x; }

    size_type next(size_type const pos)
    { return gen_position_(pos, true); }

    size_type prev(size_type const pos)
    { return gen_position_(pos, false); }

    size_type position() const noexcept
    { return position_; }

    void position(size_type const pos)
    {
        if (size() > pos) {
            position_ = pos;
            return;
        }
        raise(errc::out_of_bounds,
              "target position (%zu) equals or exceeds size (%zu)",
              pos, size());
    }

    void reserve(size_type const n)
    {
        tracks_.reserve(n);
    }

    template<typename InIt>
    void insert(const_iterator const pos, InIt first, InIt last)
    {
        tracks_.insert(pos, first, last);
        unsaved_changes_ = true;
    }

    void push_back(media::track&& t)
    {
        tracks_.push_back(std::move(t));
        unsaved_changes_ = true;
    }

    void erase(const_iterator const first, const_iterator const last)
    {
        if (first != last) {
            tracks_.erase(first, last);
            if (position_ >= size()) {
                position_ = (size() != 0 ? size() - 1 : 0);
            }
            unsaved_changes_ = true;
        }
    }

    void erase(difference_type const first, difference_type const last)
    {
        AMP_ASSERT(first >= 0);
        AMP_ASSERT(last >= first);
        AMP_ASSERT(last <= as_signed(size()));

        erase(cbegin() + first, cbegin() + last);
    }

    void clear() noexcept
    {
        tracks_.clear();
        unsaved_changes_ = true;
    }

    void sort(std::string_view, media::sort_order);

    void save();
    void remove();

    u8string const& path() const noexcept
    { return path_; }

    uint32 id() const noexcept
    { return id_; }

    static ref_ptr<playlist> make(u8string, uint32);

private:
    AMP_INTERNAL_LINKAGE explicit playlist(u8string, uint32);

    size_type gen_position_(size_type, bool);

    void add_ref() const noexcept
    {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const noexcept
    {
        auto remain = ref_count_.fetch_sub(1, std::memory_order_release) - 1;
        if (!remain) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }

    mutable std::atomic<uint32> ref_count_;
    uint32                      id_;
    media::playback_order       gen_order_;
    std::vector<media::track>   tracks_;
    size_type                   position_;
    u8string                    path_;
    bool                        unsaved_changes_;
};


struct playlist_index_entry
{
    uint32   uid;
    uint32   pos;
    u8string name;
};

struct playlist_index
{
    void load(u8string const&);
    void save(u8string const&) const;

    std::vector<playlist_index_entry> entries;
    uint32                            selection{};
};

}}    // namespace amp::media


#endif  // AMP_INCLUDED_6D040B4C_69A0_4793_BDBB_E8DB1F8E39AF

