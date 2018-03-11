////////////////////////////////////////////////////////////////////////////////
//
// media/title_format.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/cxp/char.hpp>
#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "media/tags.hpp"
#include "media/title_format.hpp"
#include "media/track.hpp"

#include <cstddef>
#include <memory>
#include <utility>


namespace amp {
namespace media {
namespace {

class expression
{
private:
    enum class type {
        meta = 0,
        meta_exact,
        literal,
        if_then,
        if_exists,
        concat,
    };

public:
    explicit expression(expression::type const t, u8string s,
                        std::unique_ptr<expression> x = nullptr,
                        std::unique_ptr<expression> y = nullptr) noexcept :
        lhs_(std::move(x)),
        rhs_(std::move(y)),
        str_(std::move(s)),
        type_(t)
    {}

    static auto meta(u8string key)
    {
        return std::make_unique<expression>(type::meta, std::move(key));
    }

    static auto meta_exact(u8string key)
    {
        return std::make_unique<expression>(type::meta_exact, std::move(key));
    }

    static auto literal(u8string text)
    {
        return std::make_unique<expression>(type::literal, std::move(text));
    }

    static auto if_then(u8string key, std::unique_ptr<expression> then)
    {
        return std::make_unique<expression>(type::if_then, std::move(key),
                                            std::move(then));
    }

    static auto if_exists(u8string key, std::unique_ptr<expression> lhs,
                          std::unique_ptr<expression> rhs)
    {
        return std::make_unique<expression>(type::if_exists, std::move(key),
                                            std::move(lhs), std::move(rhs));
    }

    static auto concat(std::unique_ptr<expression> lhs,
                       std::unique_ptr<expression> rhs)
    {
        return std::make_unique<expression>(type::concat, u8string{},
                                            std::move(lhs), std::move(rhs));
    }

    void eval(media::track const& track, u8string_buffer& out) const
    {
        switch (type_) {
        case type::meta_exact:
        case type::meta:
            out += tags::find(track, str_, static_cast<tags::scope>(type_));
            break;
        case type::literal:
            out += str_;
            break;
        case type::if_then:
            if (tags::contains(track, str_)) {
                lhs_->eval(track, out);
            }
            break;
        case type::if_exists:
            if (auto found = tags::find(track, str_)) {
                if (lhs_) { lhs_->eval(track, out); }
                out += found;
                if (rhs_) { rhs_->eval(track, out); }
            }
            break;
        case type::concat:
            lhs_->eval(track, out);
            rhs_->eval(track, out);
            break;
        }
    }

private:
    std::unique_ptr<expression> lhs_;
    std::unique_ptr<expression> rhs_;
    u8string str_;
    expression::type type_;
};


auto parse_literal_string(char const*& src)
{
    auto len = 0_sz;
    auto const end = [&]{
        for (auto s = src;; ++s, ++len) {
            switch (*s) {
            case '\\':
                if (*(++s) == '\0') {
                    raise(errc::invalid_argument,
                          "incomplete escape sequence in title format string");
                }
                break;
            case '\0':
            case '%':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
                return s;
            }
        }
    }();

    u8string_buffer buf(len, uninitialized);
    for (auto dst = buf.begin(); src != end; ++src, ++dst) {
        if (*src == '\\') {
            ++src;
        }
        *dst = *src;
    }
    return buf.promote();
}

auto parse_key_string(char const*& src, char const delim)
{
    auto const end = [&, p = src]() mutable {
        for (; *p != delim; ++p) {
            if (!cxp::isalnum(*p) && (*p != ' ')) {
                raise(errc::failure, "statement contains illegal character");
            }
        }
        return p;
    }();
    if (src == end) {
        raise(errc::failure, "empty statement");
    }

    u8string_buffer buf(static_cast<std::size_t>(end - src), uninitialized);
    std::transform(src, end, buf.begin(), cxp::tolower);
    src = end + 1;
    return buf.promote();
}

std::unique_ptr<expression> parse_expression(char const*& src,
                                             char const delim)
{
    auto parse_function = [&](u8string const& fn) {
        if (fn == "if") {
            auto key  = parse_key_string(src, ',');
            auto then = parse_expression(src, ')');
            if (!then) {
                raise(errc::invalid_argument,
                      "'if' statement requires two parameters");
            }
            return expression::if_then(std::move(key), std::move(then));
        }
        if (fn == "meta") {
            return expression::meta_exact(parse_key_string(src, ')'));
        }
        raise(errc::failure, "unknown function: \"%s\"", fn.c_str());
    };

    std::unique_ptr<expression> tree;
    while (*src != delim) {
        auto expr = [&]() {
            switch (*src) {
            case '\0':
            case '(':
            case ')':
            case ']':
                raise(errc::failure, "incomplete statement");
            case '[':
                {
                    ++src;
                    auto lhs = parse_expression(src, '%');
                    auto key = parse_key_string(src, '%');
                    auto rhs = parse_expression(src, ']');
                    return expression::if_exists(std::move(key),
                                                 std::move(lhs),
                                                 std::move(rhs));
                }
            case '%':
                ++src;
                return expression::meta(parse_key_string(src, '%'));
            case '$':
                ++src;
                return parse_function(parse_key_string(src, '('));
            default:
                return expression::literal(parse_literal_string(src));
            }
        }();
        AMP_ASSERT(expr != nullptr);

        if (tree == nullptr) {
            tree = std::move(expr);
        }
        else {
            tree = expression::concat(std::move(tree), std::move(expr));
        }
    }

    if (*src != '\0') {
        ++src;
    }
    return tree;
}

}     // namespace <unnamed>


void title_format::destroy_state_(void* const opaque) noexcept
{
    delete static_cast<expression*>(opaque);
}

u8string title_format::operator()(media::track const& track) const
{
    u8string_buffer buf;
    if (state_ != nullptr) {
        static_cast<expression*>(state_)->eval(track, buf);
    }
    return buf.promote();
}

void title_format::compile(u8string const& format)
{
    auto src = format.c_str();
    auto tmp = parse_expression(src, '\0');
    destroy_state_(std::exchange(state_, tmp.release()));
}

}}    // namespace amp::media

