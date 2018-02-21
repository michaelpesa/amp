////////////////////////////////////////////////////////////////////////////////
//
// tests/intrusive_slist_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/aux/operators.hpp>
#include <amp/intrusive/slist.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>

#include <gtest/gtest.h>


using namespace ::amp;


class record :
    public intrusive::slist_link<>,
    private totally_ordered<record>,
    private totally_ordered<record, std::string_view>
{
public:
    std::string_view name;

    explicit record(std::string_view const s) noexcept :
        name(s)
    {}

private:
    friend bool operator==(record const& x, record const& y) noexcept
    { return (x.name == y.name); }

    friend bool operator<(record const& x, record const& y) noexcept
    { return (x.name < y.name); }

    friend bool operator==(record const& x, std::string_view const& y) noexcept
    { return (x.name == y); }

    friend bool operator<(record const& x, std::string_view const& y) noexcept
    { return (x.name < y); }

    friend bool operator>(record const& x, std::string_view const& y) noexcept
    { return (x.name > y); }
};


TEST(intrusive_slist_test, push_front_and_push_back)
{
    record a{"A"};
    record b{"B"};
    record c{"C"};
    record d{"D"};
    record e{"E"};

    intrusive::slist<record> s;

    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.end(), s.begin());
    ASSERT_EQ(s.cend(), s.cbegin());
    ASSERT_EQ(s.end(), std::next(s.before_begin()));
    ASSERT_EQ(s.cend(), std::next(s.cbefore_begin()));

    s.push_front(c);
    ASSERT_EQ(c, s.front());
    ASSERT_EQ(c, s.back());
    ASSERT_EQ(1, s.size());

    s.push_back(d);
    ASSERT_EQ(c, s.front());
    ASSERT_EQ(d, s.back());
    ASSERT_EQ(2, s.size());

    s.push_front(b);
    ASSERT_EQ(b, s.front());
    ASSERT_EQ(d, s.back());
    ASSERT_EQ(3, s.size());
    ASSERT_EQ(b, *std::next(s.cbegin(), 0));
    ASSERT_EQ(c, *std::next(s.cbegin(), 1));
    ASSERT_EQ(d, *std::next(s.cbegin(), 2));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 3));

    s.push_back(e);
    ASSERT_EQ(b, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(4, s.size());
    ASSERT_EQ(b, *std::next(s.cbegin(), 0));
    ASSERT_EQ(c, *std::next(s.cbegin(), 1));
    ASSERT_EQ(d, *std::next(s.cbegin(), 2));
    ASSERT_EQ(e, *std::next(s.cbegin(), 3));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 4));

    s.push_front(a);
    ASSERT_EQ(a, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(5, s.size());
    ASSERT_EQ(a, *std::next(s.cbegin(), 0));
    ASSERT_EQ(b, *std::next(s.cbegin(), 1));
    ASSERT_EQ(c, *std::next(s.cbegin(), 2));
    ASSERT_EQ(d, *std::next(s.cbegin(), 3));
    ASSERT_EQ(e, *std::next(s.cbegin(), 4));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 5));

    s.pop_front();
    ASSERT_EQ(b, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(4, s.size());
    ASSERT_EQ(b, *std::next(s.cbegin(), 0));
    ASSERT_EQ(c, *std::next(s.cbegin(), 1));
    ASSERT_EQ(d, *std::next(s.cbegin(), 2));
    ASSERT_EQ(e, *std::next(s.cbegin(), 3));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 4));

    s.pop_front();
    ASSERT_EQ(c, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(3, s.size());
    ASSERT_EQ(c, *std::next(s.cbegin(), 0));
    ASSERT_EQ(d, *std::next(s.cbegin(), 1));
    ASSERT_EQ(e, *std::next(s.cbegin(), 2));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 3));

    s.pop_front();
    ASSERT_EQ(d, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(2, s.size());
    ASSERT_EQ(d, *std::next(s.cbegin(), 0));
    ASSERT_EQ(e, *std::next(s.cbegin(), 1));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 2));

    s.pop_front();
    ASSERT_EQ(e, s.front());
    ASSERT_EQ(e, s.back());
    ASSERT_EQ(1, s.size());
    ASSERT_EQ(e, *std::next(s.cbegin(), 0));
    ASSERT_EQ(s.cend(), std::next(s.cbegin(), 1));

    s.pop_front();
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(0, s.size());
    ASSERT_EQ(s.cend(), s.cbegin());
}

TEST(intrusive_slist_test, erase_after_and_dispose)
{
    record a{"A"};
    record b{"B"};
    record c{"C"};
    record d{"D"};
    record e{"E"};

    intrusive::slist<record> s;

    s.push_back(a);
    s.push_back(b);
    s.push_back(c);
    s.push_back(d);
    s.push_back(e);
    ASSERT_EQ(s.size(), 5);

    s.erase_after_and_dispose(
        std::next(s.cbefore_begin(), 0),
        [](record* const p) { ASSERT_EQ(*p, "A"); });

    s.erase_after_and_dispose(
        std::next(s.cbefore_begin(), 2),
        [](record* const p) { ASSERT_EQ(*p, "D"); });

    s.erase_after_and_dispose(
        std::next(s.cbefore_begin(), 2),
        [](record* const p) { ASSERT_EQ(*p, "E"); });

    s.erase_after_and_dispose(
        std::next(s.cbefore_begin(), 1),
        [](record* const p) { ASSERT_EQ(*p, "C"); });

    s.erase_after_and_dispose(
        std::next(s.cbefore_begin(), 0),
        [](record* const p) { ASSERT_EQ(*p, "B"); });
}

TEST(intrusive_slist_test, erase_after)
{
    record a{"A"};
    record b{"B"};
    record c{"C"};
    record d{"D"};
    record e{"E"};

    intrusive::slist<record> s;

    s.push_back(a);
    s.push_back(b);
    s.push_back(c);
    s.push_back(d);
    s.push_back(e);
    ASSERT_EQ(s.size(), 5);

    auto prev = s.begin();
    auto last = std::next(prev, 3);
    auto ret = s.erase_after(prev, last, 2);
    ASSERT_EQ(*ret++, d);
    ASSERT_EQ(*ret++, e);
    ASSERT_EQ(ret, s.end());
    ASSERT_EQ(s.front(), a);
    ASSERT_EQ(s.size(), 3);

    prev = s.before_begin();
    last = s.end();
    ret = s.erase_after(prev, last, 3);
    ASSERT_EQ(ret, s.end());
    ASSERT_EQ(s.size(), 0);
    ASSERT_TRUE(s.empty());
}

