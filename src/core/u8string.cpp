////////////////////////////////////////////////////////////////////////////////
//
// core/u8string.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/crc.hpp>
#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/numeric.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "core/aux/unicode.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>


namespace amp {
namespace {

constexpr auto intern_hash_size = 32768_sz;

u8string_rep const* intern_hash[intern_hash_size];
std::mutex          intern_hash_mtx;


u8string_rep* allocate_u8string_rep(std::size_t const n, uint32 const rc = 1)
{
    AMP_ASSERT(n != 0);

    auto const bytes = sizeof(u8string_rep) + n + 1;
    auto const rep = static_cast<u8string_rep*>(std::malloc(bytes));
    if (AMP_UNLIKELY(rep == nullptr)) {
        raise_bad_alloc();
    }

    ::new(&rep->intern_next) decltype(rep->intern_next){};
    ::new(&rep->ref_count)   decltype(rep->ref_count){rc};
    ::new(&rep->hash_code)   decltype(rep->hash_code);      // uninitialized
    ::new(&rep->size)        decltype(rep->size){n};
    ::new(rep->data())       char[n + 1];                   // uninitialized
    rep->data()[n] = '\0';
    return rep;
}

}     // namespace <unnamed>


AMP_NOINLINE
char const* is_valid_utf8_until(char const* s, char const* const end) noexcept
{
    while (s != end) {
        s = unicode::is_valid_ascii_until(s, end);
        if (s == end) {
            break;
        }
        if (!unicode::check_utf8_sequence(s, end)) {
            break;
        }
    }
    return s;
}


void u8string_rep::release() const noexcept
{
    auto remain = ref_count.fetch_sub(1, std::memory_order_release) - 1;
    if ((remain & ~interned_bit) == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);

        if (remain == interned_bit) {
            std::lock_guard<std::mutex> const lk{intern_hash_mtx};
            if (ref_count.load(std::memory_order_relaxed) != remain) {
                return;
            }

            auto p = &intern_hash[hash_code % intern_hash_size];
            for (; *p != this; p = &(**p).intern_next) {}
            *p = intern_next;
        }
        std::free(const_cast<u8string_rep*>(this));
    }
}

char* u8string_rep::from_utf8_unchecked(char const* const str,
                                        std::size_t const len)
{
    if (AMP_UNLIKELY(len == 0)) {
        return nullptr;
    }
    AMP_ASSERT(str != nullptr);
    AMP_ASSERT(is_valid_utf8(str, len));

    auto const rep = allocate_u8string_rep(len);
    std::copy_n(str, len, rep->data());
    rep->hash_code = crc32c::compute(rep->data(), rep->size);
    return rep->data();
}

char* u8string_rep::from_encoding(void const* const buf, std::size_t len,
                                  uint32 enc)
{
    auto src = static_cast<char const*>(buf);
    auto const lossy = !!(enc & (uint32{1} << 31));
    enc &= ~(uint32{1} << 31);

    if (AMP_LIKELY(len != 0)) {
        AMP_ASSERT(src != nullptr);

        if (enc == as_underlying(string_encoding::utf16)) {
            enc = (unicode::get_byte_order<char16>(src, len) == BE)
                ? as_underlying(string_encoding::utf16be)
                : as_underlying(string_encoding::utf16le);
        }
        else if (enc == as_underlying(string_encoding::utf32)) {
            enc = (unicode::get_byte_order<char32>(src, len) == BE)
                ? as_underlying(string_encoding::utf32be)
                : as_underlying(string_encoding::utf32le);
        }
    }

    if (AMP_UNLIKELY(len == 0)) {
        return nullptr;
    }

#define UTF8_CONV_CASE(E, ...)                                              \
    case as_underlying(string_encoding::E):                                 \
        return unicode::utf8_conv<string_encoding::E>(__VA_ARGS__)

#define UTF8_CONV_SWITCH(...)                                               \
    ([&]{                                                                   \
        switch (enc) {                                                      \
        UTF8_CONV_CASE(utf8,    __VA_ARGS__);                               \
        UTF8_CONV_CASE(utf16be, __VA_ARGS__);                               \
        UTF8_CONV_CASE(utf16le, __VA_ARGS__);                               \
        UTF8_CONV_CASE(utf32be, __VA_ARGS__);                               \
        UTF8_CONV_CASE(utf32le, __VA_ARGS__);                               \
        UTF8_CONV_CASE(cp1252,  __VA_ARGS__);                               \
        }                                                                   \
        AMP_UNREACHABLE();                                                  \
    }())

    auto const bytes = UTF8_CONV_SWITCH(src, len, lossy);
    AMP_ASSERT(bytes != 0);
    auto const rep = allocate_u8string_rep(bytes);

    UTF8_CONV_SWITCH(src, len, lossy, rep->data());
    rep->hash_code = crc32c::compute(rep->data(), rep->size);
    return rep->data();
}

char* u8string_rep::from_text_file(io::stream& file)
{
    auto len = numeric_cast<std::size_t>(file.size());
    if (len >= 3) {
        uint8 bom[3];
        file.read(bom);

        if (bom[0] == 0xef && bom[1] == 0xbb && bom[2] == 0xbf) {
            len -= 3;
        }
        else {
            file.rewind();
        }
    }

    if (AMP_UNLIKELY(len == 0)) {
        return nullptr;
    }

    auto const rep = allocate_u8string_rep(len);
    file.read(rep->data(), len);

    if (AMP_UNLIKELY(!is_valid_utf8(rep->data(), len))) {
        rep->release();
        raise(errc::invalid_unicode);
    }

    rep->hash_code = crc32c::compute(rep->data(), len);
    return rep->data();
}


u8string u8string::substr(size_type const start, size_type n) const
{
    if (AMP_UNLIKELY(start > size())) {
        raise(errc::out_of_bounds,
              "invalid substring start index: %zu", start);
    }

    n = std::min(n, size() - start);
    if (n == 0) {
        return u8string{};
    }
    AMP_ASSERT(str_ != nullptr);
    auto const rep = rep_();

    if (unicode::is_utf8_continuation_byte(rep->data()[start]) ||
        unicode::is_utf8_continuation_byte(rep->data()[start + n])) {
        raise(errc::invalid_unicode,
              "substring range [%zu, %zu) crosses a UTF-8 byte sequence",
              start, start + n);
    }
    return u8string::from_utf8_unchecked(rep->data() + start, n);
}

u8string_buffer u8string::detach() const&
{
    if (!str_) {
        return {};
    }

    auto const rep = rep_();
    auto const tmp = allocate_u8string_rep(rep->size);
    std::copy_n(rep->data(), rep->size, tmp->data());
    tmp->hash_code = rep->hash_code;
    return u8string_buffer::consume(tmp->data());
}

void u8string::intern()
{
    if (!str_) {
        return;
    }

    std::lock_guard<std::mutex> const lk{intern_hash_mtx};
    auto const rep = rep_();
    if (rep->is_interned()) {
        return;
    }

    auto const len = rep->size;
    auto&& head = intern_hash[rep->hash_code % intern_hash_size];
    for (auto x = head; x != nullptr; x = x->intern_next) {
        if (x->size == len && std::memcmp(rep->data(), x->data(), len) == 0) {
            rep->release();
            x->add_ref();
            str_ = x->data();
            return;
        }
    }

    rep->ref_count.fetch_or(rep->interned_bit, std::memory_order_relaxed);
    rep->intern_next = std::exchange(head, rep);
}

u8string u8string::intern(char const* const s, std::size_t const len)
{
    if (AMP_UNLIKELY(len == 0)) {
        return u8string{};
    }
    AMP_ASSERT(s != nullptr);

    auto const hash_code = crc32c::compute(s, len);

    std::lock_guard<std::mutex> const lk{intern_hash_mtx};
    auto& head = intern_hash[hash_code % intern_hash_size];

    for (auto x = head; x != nullptr; x = x->intern_next) {
        if (x->size == len && std::memcmp(s, x->data(), len) == 0) {
            x->add_ref();
            return u8string::consume(x->data());
        }
    }

    if (AMP_UNLIKELY(!is_valid_utf8(s, len))) {
        raise(errc::invalid_unicode);
    }

    auto const rep = allocate_u8string_rep(len, 0x80000001);
    AMP_ASSERT(rep != nullptr);

    std::copy_n(s, len, rep->data());
    rep->hash_code = hash_code;
    rep->intern_next = std::exchange(head, rep);
    return u8string::consume(rep->data());
}


u8string_buffer::u8string_buffer(size_type const n, uninitialized_t) :
    str_{n ? allocate_u8string_rep(n)->data() : nullptr}
{
}

void u8string_buffer::resize(size_type const new_size)
{
    auto const old_size = size();
    if (old_size == new_size) {
        return;
    }
    if (new_size == 0) {
        clear();
        return;
    }

    auto rep = str_ ? rep_() : static_cast<u8string_rep*>(nullptr);
    auto const tmp = std::realloc(rep, sizeof(*rep) + new_size + 1);
    if (AMP_UNLIKELY(tmp == nullptr)) {
        raise_bad_alloc();
    }
    rep = static_cast<u8string_rep*>(tmp);
    str_ = rep->data();

    ::new(rep->data()) char[new_size + 1];
    rep->data()[new_size] = '\0';
    rep->size = new_size;

    if (new_size > old_size) {
        std::fill_n(rep->data() + old_size, new_size - old_size, '\0');
    }
}

u8string u8string_buffer::promote()
{
    if (str_ != nullptr) {
        auto const rep = rep_();
        AMP_ASSERT(rep->use_count() == 1);

        if (rep->size == 0) {
            rep->release();
            str_ = nullptr;
        }
        else if (is_valid_utf8(cbegin(), cend())) {
            rep->hash_code = crc32c::compute(rep->data(), rep->size);
        }
        else {
            rep->release();
            str_ = nullptr;
            raise(errc::invalid_unicode);
        }
    }
    return u8string::consume(std::exchange(str_, nullptr));
}

u8string_buffer& u8string_buffer::appendf(char const* const format, ...)
{
    va_list ap1;
    va_start(ap1, format);
    AMP_SCOPE_EXIT { va_end(ap1); };

    va_list ap2;
    va_copy(ap2, ap1);
    AMP_SCOPE_EXIT { va_end(ap2); };

    auto const ret = std::vsnprintf(nullptr, 0, format, ap1);
    if (ret < 0) {
        raise(errc::invalid_argument);
    }
    if (ret > 0) {
        auto const start = size();
        auto const n = static_cast<std::size_t>(ret);
        resize(start + n);
        std::vsnprintf(data() + start, n + 1, format, ap2);
    }
    return *this;
}

u8string u8format(char const* const format, ...)
{
    va_list ap1;
    va_start(ap1, format);
    AMP_SCOPE_EXIT { va_end(ap1); };

    va_list ap2;
    va_copy(ap2, ap1);
    AMP_SCOPE_EXIT { va_end(ap2); };

    auto const ret = std::vsnprintf(nullptr, 0, format, ap1);
    if (ret < 0) {
        raise(errc::invalid_argument);
    }

    auto const n = static_cast<std::size_t>(ret);
    u8string_buffer buf{n, uninitialized};
    if (n != 0) {
        std::vsnprintf(buf.data(), n + 1, format, ap2);
    }
    return buf.promote();
}

}     // namespace amp

