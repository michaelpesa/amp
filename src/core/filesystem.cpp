////////////////////////////////////////////////////////////////////////////////
//
// core/filesystem.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "core/filesystem.hpp"

#include <cstddef>
#include <cstdlib>

#if defined(_WIN32)
# include <amp/scope_guard.hpp>
# include "core/aux/winapi.hpp"
#else
# include <cerrno>
# include <dirent.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <unistd.h>
#endif


namespace amp {
namespace fs {
namespace {

template<typename Char>
constexpr bool is_current_or_parent_directory(Char const* const p) noexcept
{
    return p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0'));
}

constexpr bool is_sep(char const c) noexcept
{
#if defined(_WIN32)
    return (c == '/') || (c == '\\');
#else
    return (c == '/');
#endif
}

#if defined(_WIN32)
constexpr auto separators = "/\\"_sv;
constexpr auto preferred_separator = '\\';
#else
constexpr auto separators = "/"_sv;
constexpr auto preferred_separator = '/';
#endif


auto& path_append(u8string_buffer& lhs, string_view const rhs)
{
    if (!rhs.empty() && !is_sep(rhs.front()) &&
        !lhs.empty() && !is_sep(lhs.back())) {
        lhs.append(preferred_separator);
    }
    lhs.append(rhs);
    return lhs;
}

auto path_append(u8string const& lhs, string_view const rhs)
{
    auto buf = lhs.detach();
    return path_append(buf, rhs).promote();
}

auto root_directory_start(u8string const& p, std::size_t const size) noexcept
{
#if defined(_WIN32)
    if (size > 2 && p[1] == ':' && is_sep(p[2])) {
        return 2;
    }
#endif
    if (size == 2 && is_sep(p[0]) && is_sep(p[1])) {
        return u8string::npos;
    }

#if defined(_WIN32)
    if (size > 4     &&
        is_sep(p[0]) &&
        is_sep(p[1]) &&
        p[2] == '?'  &&
        is_sep(p[3])) {
        auto const pos = p.find_first_of(separators, 4);
        return (pos < size) ? pos : u8string::npos;
    }
#endif
    if (size > 3     &&
        is_sep(p[0]) &&
        is_sep(p[1]) &&
        !is_sep(p[2])) {
        auto const pos = p.find_first_of(separators, 2);
        return (pos < size) ? pos : u8string::npos;
    }

    if (size > 0 && is_sep(p[0])) {
        return 0_sz;
    }
    return u8string::npos;
}

auto filename_position(u8string const& p, std::size_t const end) noexcept
{
    if (end == 2 && is_sep(p[0]) && is_sep(p[1])) {
        return 0_sz;
    }
    if (end != 0 && is_sep(p[end - 1])) {
        return end - 1;
    }

    auto pos = p.find_last_of(separators, end - 1);
#if defined(_WIN32)
    if (pos == u8string::npos && end > 1) {
        pos = p.find_last_of(':', end - 2);
    }
#endif
    if (pos == u8string::npos || (pos == 1 && is_sep(p[0]))) {
        return 0_sz;
    }
    return pos + 1;
}

std::size_t parent_path_end(u8string const& p) noexcept
{
    auto end = filename_position(p, p.size());
    auto const filename_was_separator = (!p.empty() && is_sep(p[end]));

    auto const start = root_directory_start(p, end);
    for (; end > 0 && (end - 1) != start && is_sep(p[end - 1]); --end) {}

    if (end == 1 && start == 0 && filename_was_separator) {
        end = u8string::npos;
    }
    return end;
}

}     // namespace <unnamed>

u8string extension(u8string const& p)
{
    auto const pos = p.rfind('.');
    if (pos != u8string::npos) {
        return p.substr(pos + 1);
    }
    return {};
}

u8string parent_path(u8string const& p)
{
    auto const end = fs::parent_path_end(p);
    if (end != u8string::npos) {
        return p.substr(0, end);
    }
    return {};
}

u8string filename(u8string const& p)
{
    if (!p.empty()) {
        auto const end = fs::parent_path_end(p);
        if (end != 0) {
            return p.substr(end + 1);
        }
    }
    return {};
}

u8string stem(u8string const& p)
{
    auto start = fs::parent_path_end(p);
    if (start != 0) {
        start += 1;
    }
    auto const end = p.rfind('.');
    auto const n = (start < end) ? (end - start) : u8string::npos;
    return p.substr(start, n);
}


directory_range::iterator& directory_range::iterator::operator++()
{
#if defined(_WIN32)
    ::WIN32_FIND_DATAW entry;
    while (::FindNextFileW(range_->handle_, &entry)) {
        if (!is_current_or_parent_directory(entry.cFileName)) {
            entry_ = path_append(range_->root_, to_u8string(entry.cFileName));
            return *this;
        }
    }
    auto const ev = static_cast<int>(::GetLastError());
#else
    auto const handle = static_cast<::DIR*>(range_->handle_);
    while (auto entry = (errno = 0, ::readdir(handle))) {
        if (!is_current_or_parent_directory(entry->d_name)) {
            entry_ = path_append(range_->root_, entry->d_name);
            return *this;
        }
    }
    auto const ev = errno;
#endif
    range_ = nullptr;
    if (AMP_UNLIKELY(ev != 0)) {
        raise_system_error(ev);
    }
    return *this;
}

directory_range::directory_range(u8string p) :
    root_{std::move(p)},
    iter_{this}
{
#if defined(_WIN32)
    auto wildname = widen(root_);
    if (!wildname.empty()) {
        wildname.append(L"\\*");
    }

    ::WIN32_FIND_DATAW entry;
    handle_ = ::FindFirstFileExW(wildname.c_str(),
                                 FindExInfoStandard,
                                 &entry,
                                 FindExSearchNameMatch,
                                 nullptr,
                                 0);

    if (AMP_UNLIKELY(handle_ == INVALID_HANDLE_VALUE)) {
        raise_current_system_error();
    }

    try {
        if (!is_current_or_parent_directory(entry.cFileName)) {
            iter_.path_ = path_append(root_, to_u8string(entry.cFileName));
        }
        else {
            ++iter_;
        }
    }
    catch (...) {
        ::FindClose(handle_);
        throw;
    }
#else
    handle_ = ::opendir(root_.c_str());
    if (AMP_UNLIKELY(handle_ == nullptr)) {
        raise_current_system_error();
    }

    try {
        ++iter_;
    }
    catch (...) {
        ::closedir(static_cast<::DIR*>(handle_));
        throw;
    }
#endif
}

directory_range::~directory_range()
{
#if defined(_WIN32)
    ::FindClose(handle_);
#else
    ::closedir(static_cast<::DIR*>(handle_));
#endif
}


bool remove(u8string const& p)
{
#if defined(_WIN32)
    auto removed = (::DeleteFileW(widen(p).c_str()) != 0);
    if (!removed) {
        auto const ev = ::GetLastError();
        if (ev != ERROR_FILE_NOT_FOUND) {
            raise_system_error(static_cast<int>(ev));
        }
    }
#else
    auto removed = (::unlink(p.c_str()) == 0);
    if (!removed) {
        auto const ev = errno;
        if (ev != ENOENT) {
            raise_system_error(ev);
        }
    }
#endif
    return removed;
}

bool create_directory(u8string const& p)
{
#if defined(_WIN32)
    auto created = (::CreateDirectoryW(widen(p).c_str(), nullptr) != 0);
    if (!created) {
        auto const ev = ::GetLastError();
        if (ev != ERROR_ALREADY_EXISTS) {
            raise_system_error(static_cast<int>(ev));
        }
    }
#else
    auto created = (::mkdir(p.c_str(), as_underlying(perms::all)) == 0);
    if (!created) {
        auto const ev = errno;
        if (ev != EEXIST || !is_directory(p)) {
            raise_system_error(ev);
        }
    }
#endif
    return created;
}

file_status status(u8string const& p)
{
#if defined(_WIN32)
    auto const attr = ::GetFileAttributesW(widen(p).c_str());
    if (attr == static_cast<::DWORD>(-1)) {
        switch (auto const ev = ::GetLastError()) {
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_PATHNAME:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_INVALID_NAME:
        case ERROR_INVALID_PARAMETER:
            return file_status{file_type::not_found};
        default:
            raise_system_error(static_cast<int>(ev));
        }
    }

    auto const type = (attr & FILE_ATTRIBUTE_DIRECTORY) ? file_type::directory
                    : (attr & FILE_ATTRIBUTE_REGULAR)   ? file_type::regular
                    :                                     file_type::unknown;
    auto mode = perms::all;
    if (attr & FILE_ATTRIBUTE_READONLY) {
        mode &= ~(perms::owner_write|perms::group_write|perms::others_write);
    }
    return file_status{type, mode};
#else
    struct ::stat st;
    if (::stat(p.c_str(), &st) != 0) {
        switch (auto const ev = errno) {
        case ENOTDIR:
        case ENOENT:
            return file_status{file_type::not_found};
        default:
            raise_system_error(ev);
        }
    }

    auto const type = S_ISREG(st.st_mode)  ? file_type::regular
                    : S_ISDIR(st.st_mode)  ? file_type::directory
                    : S_ISCHR(st.st_mode)  ? file_type::character
                    : S_ISBLK(st.st_mode)  ? file_type::block
                    : S_ISLNK(st.st_mode)  ? file_type::symlink
                    : S_ISFIFO(st.st_mode) ? file_type::fifo
                    : S_ISSOCK(st.st_mode) ? file_type::socket
                    :                        file_type::unknown;
    auto const mode = static_cast<perms>(st.st_mode) & perms::mask;
    return file_status{type, mode};
#endif
}

u8string get_user_directory(fs::user_directory const x)
{
#if defined(__APPLE__) && defined(__MACH__)
    auto const name = [&]{
        switch (x) {
        case fs::user_directory::config: return "Preferences";
        case fs::user_directory::cache:  return "Caches";
        case fs::user_directory::data:   return "Application Support";
        }
    }();
    return u8format("%s/Library/%s/amp", std::getenv("HOME"), name);
#elif defined(AMP_HAS_POSIX)
    auto const var = [&]{
        switch (x) {
        case fs::user_directory::config: return "XDG_CONFIG_HOME";
        case fs::user_directory::cache:  return "XDG_CACHE_HOME";
        case fs::user_directory::data:   return "XDG_DATA_HOME";
        }
    }();
    return u8format("%s/amp", std::getenv(var));
#elif defined(_WIN32)
    wchar_t* path;
    auto ret = ::SHGetKnownFolderPath(::FOLDERID_RoamingAppData,
                                      ::KF_FLAG_CREATE, nullptr, &path);
    if (ret != S_OK) {
        raise_system_error(static_cast<int>(ret));
    }

    AMP_SCOPE_EXIT { ::CoTaskMemFree(path); };
    return u8string::from_wide(path) + "\\amp";
#endif
}

}}    // namespace amp::fs

