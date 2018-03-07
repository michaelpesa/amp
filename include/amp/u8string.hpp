////////////////////////////////////////////////////////////////////////////////
//
// amp/u8string.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_AB4EE4FD_D59E_4DBE_BAC0_D948185C1A83
#define AMP_INCLUDED_AB4EE4FD_D59E_4DBE_BAC0_D948185C1A83


#include <amp/aux/operators.hpp>
#include <amp/error.hpp>
#include <amp/memory.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>


namespace amp {
namespace io {
    class stream;
}


enum class string_encoding : uint32 {
    utf8    = 0,
    utf16   = 1,
    utf16be = 2,
    utf16le = 3,
    utf32   = 4,
    utf32be = 5,
    utf32le = 6,
    cp1252  = 7,
    latin1  = cp1252,
};


AMP_EXPORT AMP_READONLY
char const* is_valid_utf8_until(char const*, char const*) noexcept;

AMP_INLINE auto is_valid_utf8_until(char const* const s,
                                    std::size_t const len) noexcept
{ return is_valid_utf8_until(s, s + len); }


AMP_INLINE bool is_valid_utf8(char const* const first,
                              char const* const last) noexcept
{ return (is_valid_utf8_until(first, last) == last); }

AMP_INLINE bool is_valid_utf8(char const* const s,
                              std::size_t const len) noexcept
{ return is_valid_utf8(s, s + len); }

AMP_INLINE bool is_valid_utf8(std::string_view const s) noexcept
{ return is_valid_utf8(s.begin(), s.end()); }


struct u8string_rep
{
    static constexpr auto interned_bit = uint32{1} << 31;

    union {
        mutable u8string_rep const* intern_next;
        std::size_t capacity;
    };
    std::size_t size;
    uint32 hash_code;
    mutable std::atomic<uint32> ref_count;

    AMP_INLINE char* data() noexcept
    { return reinterpret_cast<char*>(this + 1); }

    AMP_INLINE char const* data() const noexcept
    { return reinterpret_cast<char const*>(this + 1); }

    AMP_INLINE void add_ref() const noexcept
    { ref_count.fetch_add(1, std::memory_order_relaxed); }

    AMP_INLINE uint32 use_count() const noexcept
    { return ref_count.load(std::memory_order_relaxed) & ~interned_bit; }

    AMP_INLINE bool is_interned() const noexcept
    { return !!(ref_count.load(std::memory_order_relaxed) & interned_bit); }

    AMP_EXPORT
    void release() const noexcept;

    AMP_EXPORT
    static u8string_rep* from_utf8_unchecked(char const*, std::size_t);

    AMP_EXPORT
    static u8string_rep* from_encoding(void const*, std::size_t, uint32);

    AMP_EXPORT
    static u8string_rep* from_text_file(io::stream&);
};


namespace aux {

template<typename D>
class u8string_common_ :
    private totally_ordered<D, D>,
    private totally_ordered<D, std::string_view>,
    private totally_ordered<D, char const*>
{
public:
    static auto from_encoding(void const*     const s,
                              std::size_t     const n,
                              string_encoding const enc)
    {
        auto const flags = static_cast<uint32>(enc);
        return D::consume(u8string_rep::from_encoding(s, n, flags));
    }

    static auto from_encoding_lossy(void const*     const s,
                                    std::size_t     const n,
                                    string_encoding const enc)
    {
        auto const flags = static_cast<uint32>(enc) | (uint32{1} << 31);
        return D::consume(u8string_rep::from_encoding(s, n, flags));
    }

    static auto from_utf8_unchecked(char const* const s, std::size_t const n)
    { return D::consume(u8string_rep::from_utf8_unchecked(s, n)); }

    static auto from_utf8_unchecked(std::string_view const s)
    { return from_utf8_unchecked(s.data(), s.size()); }

#define AMP_U8STRING_FROM_ENCODING(Name, E, C)                              \
    static auto from_##Name(C const* const s, std::size_t const n)          \
    { return from_encoding(s, n, string_encoding::E); }                     \
    static auto from_##Name(std::basic_string_view<C> const s)              \
    { return from_##Name(s.data(), s.size()); }                             \
    static auto from_##Name##_lossy(C const* const s, std::size_t const n)  \
    { return from_encoding_lossy(s, n, string_encoding::E); }               \
    static auto from_##Name##_lossy(std::basic_string_view<C> const s)      \
    { return from_##Name##_lossy(s.data(), s.size()); }

    AMP_U8STRING_FROM_ENCODING(utf8,    utf8,    char)
    AMP_U8STRING_FROM_ENCODING(utf16,   utf16,   char16)
    AMP_U8STRING_FROM_ENCODING(utf16be, utf16be, char16)
    AMP_U8STRING_FROM_ENCODING(utf16le, utf16le, char16)
    AMP_U8STRING_FROM_ENCODING(utf32,   utf32,   char32)
    AMP_U8STRING_FROM_ENCODING(utf32be, utf32be, char32)
    AMP_U8STRING_FROM_ENCODING(utf32le, utf32le, char32)
    AMP_U8STRING_FROM_ENCODING(cp1252,  cp1252,  char)
    AMP_U8STRING_FROM_ENCODING(latin1,  latin1,  char)
#ifdef _WIN32
    AMP_U8STRING_FROM_ENCODING(wide,    utf16le, wchar_t)
#endif

#undef AMP_U8STRING_FROM_ENCODING

    static auto from_text_file(io::stream& file)
    { return D::consume(u8string_rep::from_text_file(file)); }

private:
    friend bool operator==(D const& x, D const& y) noexcept
    { return x.compare(y) == 0; }
    friend bool operator<(D const& x, D const& y) noexcept
    { return x.compare(y) < 0; }

    friend bool operator==(D const& x, std::string_view const& y) noexcept
    { return x.compare(y) == 0; }
    friend bool operator<(D const& x, std::string_view const& y) noexcept
    { return x.compare(y) < 0; }
    friend bool operator>(D const& x, std::string_view const& y) noexcept
    { return x.compare(y) > 0; }

    friend bool operator==(D const& x, char const* const y) noexcept
    { return x.compare(y) == 0; }
    friend bool operator<(D const& x, char const* const y) noexcept
    { return x.compare(y) < 0; }
    friend bool operator>(D const& x, char const* const y) noexcept
    { return x.compare(y) > 0; }
};

}     // namespace aux


class u8string;

class u8string_buffer :
    public aux::u8string_common_<u8string_buffer>
{
public:
    using value_type             = char;
    using pointer                = char*;
    using const_pointer          = char const*;
    using iterator               = char*;
    using const_iterator         = char const*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    static constexpr auto npos = static_cast<size_type>(-1);

    constexpr u8string_buffer() noexcept :
        rep_{nullptr}
    {}

    u8string_buffer(u8string_buffer&& x) noexcept :
        rep_{std::exchange(x.rep_, nullptr)}
    {}

    u8string_buffer& operator=(u8string_buffer&& x) & noexcept
    {
        u8string_buffer{std::move(x)}.swap(*this);
        return *this;
    }

    AMP_EXPORT
    explicit u8string_buffer(size_type const n, uninitialized_t);

    explicit u8string_buffer(size_type const n, char const c = '\0') :
        u8string_buffer{n, uninitialized}
    {
        std::fill(begin(), end(), c);
    }

    u8string_buffer(const_pointer const s, size_type const n) :
        u8string_buffer{from_utf8(s, n)}
    {}

    u8string_buffer(const_pointer const s) :
        u8string_buffer{s, s ? std::strlen(s) : 0}
    {}

    u8string_buffer(std::string_view const s) :
        u8string_buffer{s.data(), s.size()}
    {}

    ~u8string_buffer()
    {
        if (rep_) {
            rep_->release();
        }
    }

    u8string_buffer(u8string_buffer const&) = delete;
    u8string_buffer& operator=(u8string_buffer const&) = delete;

    void swap(u8string_buffer& x) noexcept
    {
        using std::swap;
        swap(rep_, x.rep_);
    }

    iterator erase(const_iterator first, const_iterator const last) noexcept
    {
        AMP_ASSERT((first <= last)     && "invalid iterator range");
        AMP_ASSERT((first >= cbegin()) && "foregin begin iterator");
        AMP_ASSERT((last  <= cend())   && "foregin end iterator");

        if (first == cbegin() && last == cend()) {
            clear();
        }
        else {
            std::copy(last, cend(), const_cast<iterator>(first));
            rep_->size -= static_cast<size_type>(last - first);
            rep_->data()[rep_->size] = '\0';
        }
        return const_cast<iterator>(first);
    }

    iterator erase(const_iterator const pos) noexcept
    {
        AMP_ASSERT((pos != cend()) && "cannot erase end iterator");
        return erase(pos, pos + 1);
    }

    u8string_buffer& erase(size_type const pos, size_type n)
    {
        if (pos > size()) {
            raise(errc::out_of_bounds, "invalid string position");
        }

        n = std::min(n, size() - pos);
        erase(cbegin() + pos, cbegin() + pos + n);
        return *this;
    }

    value_type& operator[](size_type const pos) noexcept
    {
        AMP_ASSERT((size() >= pos) && "invalid string position");
        return data()[pos];
    }

    value_type const& operator[](size_type const pos) const noexcept
    {
        AMP_ASSERT((size() >= pos) && "invalid string position");
        return data()[pos];
    }

    value_type& front() noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the front of an empty string");
        return data()[0];
    }

    value_type& back() noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the front of an empty string");
        return data()[size() - 1];
    }

    value_type const& front() const noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the front of an empty string");
        return data()[0];
    }

    value_type const& back() const noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the back of an empty string");
        return data()[size() - 1];
    }

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

    reverse_iterator rbegin() noexcept
    { return reverse_iterator(begin()); }

    reverse_iterator rend() noexcept
    { return reverse_iterator(end()); }

    const_reverse_iterator rbegin() const noexcept
    { return crbegin(); }

    const_reverse_iterator rend() const noexcept
    { return crend(); }

    const_reverse_iterator crbegin() const noexcept
    { return const_reverse_iterator(cend()); }

    const_reverse_iterator crend() const noexcept
    { return const_reverse_iterator(cbegin()); }

    AMP_INLINE pointer data() noexcept
    { return rep_ ? rep_->data() : nullptr; }

    AMP_INLINE const_pointer data() const noexcept
    { return rep_ ? rep_->data() : nullptr; }

    AMP_INLINE const_pointer c_str() const noexcept
    { return data(); }

    AMP_INLINE size_type max_size() const noexcept
    { return static_cast<size_type>(-1); }

    AMP_INLINE size_type size() const noexcept
    { return rep_ ? rep_->size : 0; }

    AMP_INLINE size_type length() const noexcept
    { return size(); }

    AMP_INLINE bool empty() const noexcept
    { return (rep_ == nullptr); }

    void clear() noexcept
    {
        if (rep_) {
            rep_->release();
            rep_ = nullptr;
        }
    }

    AMP_EXPORT
    void resize(size_type const new_size);

    AMP_EXPORT
    u8string promote();

    int compare(const_pointer const s, size_type const n) const noexcept
    { return mem::compare(data(), size(), s, n); }

    int compare(const_pointer const s) const noexcept
    { return compare(s, s ? std::strlen(s) : 0); }

    int compare(u8string_buffer const& x) const noexcept
    { return compare(x.data(), x.size()); }

    int compare(std::string_view const x) const noexcept
    { return compare(x.data(), x.size()); }

    std::string_view to_string_view() const noexcept
    { return {data(), size()}; }

    operator std::string_view() const noexcept
    { return to_string_view(); }

    void remove_prefix(size_type const n) noexcept
    {
        if (n >= size()) {
            clear();
        }
        else {
            erase(cbegin(), cbegin() + static_cast<size_type>(n));
        }
    }

    void remove_suffix(size_type const n) noexcept
    {
        if (n >= size()) {
            clear();
        }
        else {
            AMP_ASSERT(rep_ != nullptr);
            rep_->size -= n;
            rep_->data()[rep_->size] = '\0';
        }
    }

    u8string_buffer& append(char const c)
    {
        resize(size() + 1);

        AMP_ASSERT(rep_ != nullptr);
        rep_->data()[size() - 1] = c;
        return *this;
    }

    u8string_buffer& append(char const* const s, size_type const n)
    {
        if (n != 0) {
            auto const start = size();
            resize(n + start);
            std::copy_n(s, n, data() + start);
        }
        return *this;
    }

    u8string_buffer& append(std::string_view const s)
    { return append(s.data(), s.size()); }

    u8string_buffer& append(char const* const s)
    { return append(s, s ? std::strlen(s) : 0_sz); }

    u8string_buffer& append(u8string_buffer const& s)
    { return append(s.data(), s.size()); }

    u8string_buffer& append(u8string const&);

    AMP_EXPORT AMP_PRINTF_FORMAT(2, 3)
    u8string_buffer& appendf(char const* AMP_PRINTF_FORMAT_STRING, ...);

private:
    friend class u8string;
    friend class aux::u8string_common_<u8string_buffer>;

    struct consume_arg_t {};

    explicit u8string_buffer(u8string_rep* const p, consume_arg_t) noexcept :
        rep_{p}
    {}

    static u8string_buffer consume(u8string_rep* const p) noexcept
    { return u8string_buffer{p, consume_arg_t()}; }

    u8string_rep* rep_;
};


inline void swap(u8string_buffer& x, u8string_buffer& y) noexcept
{ x.swap(y); }


inline auto& operator+=(u8string_buffer& x, char const y)
{ return x.append(y); }

inline auto& operator+=(u8string_buffer& x, char const* const y)
{ return x.append(y); }

inline auto& operator+=(u8string_buffer& x, std::string_view const y)
{ return x.append(y); }

inline auto& operator+=(u8string_buffer& x, u8string const& y)
{ return x.append(y); }

inline auto& operator+=(u8string_buffer& x, u8string_buffer const& y)
{ return x.append(y); }


inline auto operator+(u8string_buffer x, char const y)
{ x += y; return x; }

inline auto operator+(u8string_buffer x, u8string_buffer const& y)
{ x += y; return x; }

inline auto operator+(u8string_buffer x, std::string_view const y)
{ x += y; return x; }

inline auto operator+(u8string_buffer x, char const* const y)
{ x += y; return x; }

inline auto operator+(u8string_buffer x, u8string const& y)
{ x += y; return x; }

inline auto operator+(std::string_view const x, u8string_buffer const& y)
{ return u8string_buffer{x} + y; }


inline auto to_string_view(u8string_buffer const& s) noexcept
{ return s.to_string_view(); }

inline auto to_string(u8string_buffer const& s)
{ return std::string{s.data(), s.size()}; }


class u8string :
    public aux::u8string_common_<u8string>
{
public:
    using value_type             = char;
    using pointer                = char const*;
    using const_pointer          = char const*;
    using iterator               = char const*;
    using const_iterator         = char const*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    static constexpr auto npos = static_cast<size_type>(-1);

    constexpr u8string() noexcept :
        rep_{nullptr}
    {}

    u8string(const_pointer const s, size_type const n) :
        u8string{from_utf8(s, n)}
    {}

    u8string(const_pointer const s) :
        u8string{s, s ? std::strlen(s) : 0}
    {}

    u8string(std::string_view const s) :
        u8string{s.data(), s.size()}
    {}

    u8string(u8string_buffer buf) :
        u8string{buf.promote()}
    {}

    u8string(u8string const& x) noexcept :
        rep_{x.rep_}
    {
        if (rep_) {
            rep_->add_ref();
        }
    }

    u8string(u8string&& x) noexcept :
        rep_{std::exchange(x.rep_, nullptr)}
    {}

    u8string& operator=(u8string const& x) & noexcept
    {
        u8string{x}.swap(*this);
        return *this;
    }

    u8string& operator=(u8string&& x) & noexcept
    {
        u8string{std::move(x)}.swap(*this);
        return *this;
    }

    ~u8string()
    {
        if (rep_) {
            rep_->release();
        }
    }

    void swap(u8string& x) noexcept
    {
        using std::swap;
        swap(rep_, x.rep_);
    }

    value_type const& operator[](size_type const pos) const noexcept
    {
        AMP_ASSERT((size() >= pos) && "string index is out of bounds");
        return data()[pos];
    }

    value_type const& front() const noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the front of an empty string");
        return data()[0];
    }

    value_type const& back() const noexcept
    {
        AMP_ASSERT(!empty() && "dereferenced the back of an empty string");
        return data()[size() - 1];
    }

    const_iterator begin() const noexcept
    { return cbegin(); }

    const_iterator end() const noexcept
    { return cend(); }

    const_iterator cbegin() const noexcept
    { return data(); }

    const_iterator cend() const noexcept
    { return data() + size(); }

    const_reverse_iterator rbegin() const noexcept
    { return crbegin(); }

    const_reverse_iterator rend() const noexcept
    { return crend(); }

    const_reverse_iterator crbegin() const noexcept
    { return const_reverse_iterator(end()); }

    const_reverse_iterator crend() const noexcept
    { return const_reverse_iterator(begin()); }

    AMP_INLINE const_pointer data() const noexcept
    { return rep_ ? rep_->data() : nullptr; }

    AMP_INLINE const_pointer c_str() const noexcept
    { return data(); }

    AMP_INLINE size_type max_size() const noexcept
    { return static_cast<size_type>(-1); }

    AMP_INLINE size_type size() const noexcept
    { return rep_ ? rep_->size : 0; }

    AMP_INLINE size_type length() const noexcept
    { return size(); }

    AMP_INLINE bool empty() const noexcept
    { return (rep_ == nullptr); }

    AMP_INLINE explicit operator bool() const noexcept
    { return (rep_ != nullptr); }

    void clear() noexcept
    {
        if (rep_) {
            rep_->release();
            rep_ = nullptr;
        }
    }

    AMP_EXPORT
    void intern();

    AMP_EXPORT
    u8string substr(size_type const start, size_type const n = npos) const;

    AMP_EXPORT
    u8string_buffer detach() const&;

    u8string_buffer detach() &&
    {
        if (rep_ && (rep_->ref_count.load(std::memory_order_relaxed) == 1)) {
            return u8string_buffer::consume(
                const_cast<u8string_rep*>(std::exchange(rep_, nullptr)));
        }
        return const_cast<u8string const&>(*this).detach();
    }

    int compare(const_pointer const s, size_type const n) const noexcept
    { return mem::compare(data(), size(), s, n); }

    int compare(const_pointer const s) const noexcept
    { return compare(s, s ? std::strlen(s) : 0); }

    int compare(u8string const& x) const noexcept
    { return (rep_ == x.rep_) ? 0 : compare(x.data(), x.size()); }

    int compare(std::string_view const x) const noexcept
    { return compare(x.data(), x.size()); }


    size_type find(std::string_view const s,
                   size_type const pos = 0) const noexcept
    {
        return to_string_view().find(s, pos);
    }

    size_type find(value_type const c, size_type const pos = 0) const noexcept
    {
        return to_string_view().find(c, pos);
    }


    size_type rfind(std::string_view const s,
                    size_type const pos = npos) const noexcept
    {
        return to_string_view().rfind(s, pos);
    }

    size_type rfind(value_type const c,
                    size_type const pos = npos) const noexcept
    {
        return to_string_view().rfind(c, pos);
    }


    size_type find_first_of(std::string_view const s,
                            size_type const pos = 0) const noexcept
    {
        return to_string_view().find_first_of(s, pos);
    }

    size_type find_first_of(value_type const c,
                            size_type const pos = 0) const noexcept
    {
        return find(c, pos);
    }


    size_type find_last_of(std::string_view const s,
                           size_type const pos = npos) const noexcept
    {
        return to_string_view().find_last_of(s, pos);
    }

    size_type find_last_of(value_type const c,
                           size_type const pos = npos) const noexcept
    {
        return rfind(c, pos);
    }


    size_type find_first_not_of(std::string_view const s,
                                size_type const pos = 0) const noexcept
    {
        return to_string_view().find_first_not_of(s, pos);
    }

    size_type find_first_not_of(value_type const c,
                                size_type const pos = 0) const noexcept
    {
        return to_string_view().find_first_not_of(c, pos);
    }


    size_type find_last_not_of(std::string_view const s,
                               size_type const pos = npos) const noexcept
    {
        return to_string_view().find_last_not_of(s, pos);
    }

    size_type find_last_not_of(value_type const c,
                               size_type const pos = npos) const noexcept
    {
        return to_string_view().find_last_not_of(c, pos);
    }


    std::string_view to_string_view() const noexcept
    { return {data(), size()}; }

    operator std::string_view() const noexcept
    { return to_string_view(); }

    explicit operator u8string_buffer() const&
    { return detach(); }

    explicit operator u8string_buffer() &&
    { return static_cast<u8string&&>(*this).detach(); }

    uint32 hash_code() const noexcept
    { return rep_ ? rep_->hash_code : 0; }

    uint32 use_count() const noexcept
    { return rep_ ? rep_->use_count() : 0; }

    AMP_EXPORT
    static u8string intern(char const*, size_type);

    static u8string intern(std::string_view const s)
    { return u8string::intern(s.data(), s.size()); }

private:
    friend class u8string_buffer;
    friend class aux::u8string_common_<u8string>;

    struct consume_arg_t {};

    explicit u8string(u8string_rep const* const p, consume_arg_t) noexcept :
        rep_{p}
    {}

    static u8string consume(u8string_rep const* const p) noexcept
    { return u8string{p, consume_arg_t()}; }

    u8string_rep const* rep_;
};


inline void swap(u8string& x, u8string& y) noexcept
{ x.swap(y); }


inline auto operator+(u8string const& x, char const y)
{ return (x.detach() + y).promote(); }

inline auto operator+(u8string const& x, char const* const y)
{ return (x.detach() + y).promote(); }

inline auto operator+(u8string const& x, std::string_view const y)
{ return (x.detach() + y).promote(); }

inline auto operator+(u8string const& x, u8string const& y)
{ return (x.detach() + y).promote(); }

inline auto operator+(u8string const& x, u8string_buffer const& y)
{ return (x.detach() + y).promote(); }


inline auto& operator+=(u8string& x, char const y)
{ return x = (std::move(x).detach() + y).promote(); }

inline auto& operator+=(u8string& x, char const* const y)
{ return x = (std::move(x).detach() + y).promote(); }

inline auto& operator+=(u8string& x, std::string_view const y)
{ return x = (std::move(x).detach() + y).promote(); }

inline auto& operator+=(u8string& x, u8string const& y)
{ return x = (std::move(x).detach() + y).promote(); }

inline auto& operator+=(u8string& x, u8string_buffer const& y)
{ return x = (std::move(x).detach() + y).promote(); }

inline auto operator+(char const* const x, u8string const& y)
{ return u8string{x} + y; }

inline auto operator+(std::string_view const x, u8string const& y)
{ return u8string{x} + y; }


inline auto to_string_view(u8string const& s) noexcept
{ return s.to_string_view(); }

inline auto to_string(u8string const& s)
{ return std::string{s.data(), s.size()}; }

inline u8string_buffer& u8string_buffer::append(u8string const& s)
{ return append(s.to_string_view()); }


AMP_EXPORT AMP_PRINTF_FORMAT(1, 2)
u8string u8format(AMP_PRINTF_FORMAT_STRING char const*, ...);


AMP_INLINE auto to_u8string(u8string_buffer s)
{ return s.promote(); }

AMP_INLINE auto to_u8string(char const* const s)
{ return u8string{s}; }

AMP_INLINE auto to_u8string(std::string_view const s)
{ return u8string{s}; }

AMP_INLINE auto to_u8string(std::u16string_view const s)
{ return u8string::from_utf16(s); }

AMP_INLINE auto to_u8string(std::u32string_view const s)
{ return u8string::from_utf32(s); }

#ifdef _WIN32
AMP_INLINE auto to_u8string(std::wstring_view const s)
{ return u8string::from_wide(s); }
#endif


template<typename T>
AMP_INLINE auto to_u8string(T const x) ->
    enable_if_t<is_integral_v<T> && is_signed_v<T>, u8string>
{
    return u8format("%jd", static_cast<intmax>(x));
}

template<typename T>
AMP_INLINE auto to_u8string(T const x) ->
    enable_if_t<is_integral_v<T> && is_unsigned_v<T>, u8string>
{
    return u8format("%ju", static_cast<uintmax>(x));
}

template<typename T>
AMP_INLINE auto to_u8string(T const x) ->
    enable_if_t<is_floating_point_v<T>, u8string>
{
    return u8format("%lf", static_cast<double>(x));
}

}     // namespace amp


namespace std {

template<>
struct hash<::amp::u8string>
{
    using argument_type = ::amp::u8string;
    using result_type   = ::amp::uint32;

    result_type operator()(argument_type const& x) const noexcept
    { return x.hash_code(); }
};

}     // namespace std


#endif  // AMP_INCLUDED_AB4EE4FD_D59E_4DBE_BAC0_D948185C1A83

