////////////////////////////////////////////////////////////////////////////////
//
// tests/flat_set_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/flat_set.hpp>
#include <amp/string_view.hpp>

#include <algorithm>
#include <functional>

#include <gtest/gtest.h>


using namespace ::amp;


class fs_compare final
{
public:
    fs_compare() noexcept(false) {}
    fs_compare(fs_compare const&) noexcept(false) {}
    fs_compare& operator=(fs_compare const&) noexcept(false) { return *this; }

    bool operator()(string_view const x, string_view const y) const
    {
        auto const x_hidden = !x.empty() && (x.front() == '.');
        auto const y_hidden = !y.empty() && (y.front() == '.');
        if (x_hidden != y_hidden) {
            return x_hidden;
        }
        return (x < y);
    }

    void swap(fs_compare&) noexcept(false) {}

private:
    friend void swap(fs_compare&, fs_compare&) noexcept(false) {}
};


TEST(flat_set_test, emplace_hint)
{
    flat_set<int> s;

    static_assert(is_nothrow_default_constructible_v<decltype(s)>, "");
    static_assert(is_nothrow_move_constructible_v<decltype(s)>, "");
    static_assert(is_nothrow_move_assignable_v<decltype(s)>, "");
    static_assert(is_nothrow_swappable_v<decltype(s)>, "");

    {
        auto const it = s.emplace_hint(s.cend());
        EXPECT_EQ(it, s.begin());
        EXPECT_EQ(*it, 0);
        EXPECT_EQ(s.size(), 1);
    }

    {
        auto const it = s.emplace_hint(s.cend());
        EXPECT_EQ(it, s.begin());
        EXPECT_EQ(*it, 0);
        EXPECT_EQ(s.size(), 1);
    }

    {
        auto const it = s.emplace_hint(s.cbegin(), 22);
        EXPECT_EQ(it, std::next(s.begin()));
        EXPECT_EQ(*it, 22);
        EXPECT_EQ(s.size(), 2);
    }

    s.clear();
    EXPECT_TRUE(s.empty());
}

TEST(flat_set_test, stateful_compare)
{
    flat_set<string_view, fs_compare> s;

    static_assert(!is_nothrow_default_constructible_v<decltype(s)>, "");
    static_assert(!is_nothrow_move_constructible_v<decltype(s)>, "");
    static_assert(!is_nothrow_move_assignable_v<decltype(s)>, "");
    static_assert(!is_nothrow_swappable_v<decltype(s)>, "");

    {
        auto const r = s.insert(".bashrc");
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first, s.begin());
        EXPECT_EQ(*r.first, ".bashrc");
        EXPECT_EQ(s.size(), 1);
    }

    {
        auto const r = s.insert(".bashrc");
        EXPECT_FALSE(r.second);
        EXPECT_EQ(r.first, s.begin());
        EXPECT_EQ(*r.first, ".bashrc");
        EXPECT_EQ(s.size(), 1);
    }

    {
        auto const r = s.insert("Documents");
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first, std::next(s.begin()));
        EXPECT_EQ(*r.first, "Documents");
        EXPECT_EQ(s.size(), 2);
    }

    {
        auto const r = s.insert(".profile");
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first, std::next(s.begin()));
        EXPECT_EQ(*r.first, ".profile");
        EXPECT_EQ(s.size(), 3);
    }

    s.clear();
    EXPECT_TRUE(s.empty());
}

