//////////////////////////////////////////////////////////////////////////////// //
// core/filesystem.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C39CFF1D_5C4F_4D7C_A8B2_B40AB7487785
#define AMP_INCLUDED_C39CFF1D_5C4F_4D7C_A8B2_B40AB7487785


#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>


namespace amp {
namespace fs {

enum class file_type {
    not_found = -1,
    none      = 0,
    regular   = 1,
    directory = 2,
    symlink   = 3,
    block     = 4,
    character = 5,
    fifo      = 6,
    socket    = 7,
    unknown   = 8,
};

enum class perms : uint32 {
    none             =    00,
    owner_read       =  0400,
    owner_write      =  0200,
    owner_exec       =  0100,
    owner_all        =  0700,
    group_read       =   040,
    group_write      =   020,
    group_exec       =   010,
    group_all        =   070,
    others_read      =    04,
    others_write     =    02,
    others_exec      =    01,
    others_all       =    07,
    all              =  0777,
    set_uid          = 04000,
    set_gid          = 02000,
    sticky_bit       = 01000,
    mask             = 07777,

    unknown          = 0x0ffff,
    add_perms        = 0x10000,
    remove_perms     = 0x20000,
    resolve_symlinks = 0x40000,
};
AMP_DEFINE_ENUM_FLAG_OPERATORS(perms);


class file_status
{
public:
    explicit file_status(fs::file_type const t = fs::file_type::none,
                         fs::perms const m = fs::perms::unknown) noexcept :
        type_{t},
        mode_{m}
    {}

    fs::file_type type() const noexcept
    { return type_; }

    fs::perms permissions() const noexcept
    { return mode_; }

    void type(fs::file_type const t) noexcept
    { type_ = t; }

    void permissions(fs::perms const p) noexcept
    { mode_ = p; }

private:
    fs::file_type type_;
    fs::perms mode_;
};


class directory_range
{
public:
    class iterator :
        private input_iteratable<iterator>
    {
    public:
        using value_type        = u8string;
        using reference         = u8string const&;
        using pointer           = u8string const*;
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;

        iterator& operator++();

        reference operator*() const noexcept
        { return entry_; }

        pointer operator->() const noexcept
        { return std::addressof(**this); }

    private:
        friend class directory_range;

        friend bool operator==(iterator const& x, iterator const& y) noexcept
        { return (x.range_ == y.range_); }

        explicit iterator(directory_range const* const p) noexcept :
            range_{p}
        {}

        directory_range const* range_{nullptr};
        u8string entry_;
    };

    explicit directory_range(u8string);
    ~directory_range();

    directory_range(directory_range const&) = delete;
    directory_range& operator=(directory_range const&) = delete;

    auto begin() const noexcept { return iter_; }
    auto end()   const noexcept { return iterator{nullptr}; }

private:
    void* handle_;
    u8string root_;
    iterator iter_;
};


enum class user_directory {
    config,
    cache,
    data,
};

extern u8string get_user_directory(fs::user_directory);

extern bool remove(u8string const&);
extern bool create_directory(u8string const&);

extern file_status status(u8string const&);
extern u8string extension(u8string const&);
extern u8string parent_path(u8string const&);
extern u8string filename(u8string const&);
extern u8string stem(u8string const&);

inline bool status_known(fs::file_status const s) noexcept
{ return (s.type() != fs::file_type::none); }

inline bool exists(fs::file_status const s) noexcept
{ return fs::status_known(s) && (s.type() != fs::file_type::not_found); }

inline bool exists(u8string const& p)
{ return fs::exists(fs::status(p)); }

inline bool is_directory(u8string const& p)
{ return (fs::status(p).type() == fs::file_type::directory); }

inline bool is_regular_file(u8string const& p)
{ return (fs::status(p).type() == fs::file_type::regular); }

inline bool is_symlink(u8string const& p)
{ return (fs::status(p).type() == fs::file_type::symlink); }

}}    // namespace amp::fs


#endif  // AMP_INCLUDED_C39CFF1D_5C4F_4D7C_A8B2_B40AB7487785

