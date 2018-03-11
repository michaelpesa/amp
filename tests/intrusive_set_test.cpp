////////////////////////////////////////////////////////////////////////////////
//
// tests/intrusive_set_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/aux/operators.hpp>
#include <amp/intrusive/set.hpp>

#include <algorithm>
#include <array>
#include <climits>
#include <iterator>
#include <random>
#include <utility>

#include <gtest/gtest.h>


using namespace ::amp;


class record :
    public intrusive::set_link<>,
    private totally_ordered<record>,
    private totally_ordered<record, char>
{
public:
    char id;

    record(char const c = 0) noexcept :
        id(c)
    {}

private:
    friend bool operator==(record const& x, record const& y) noexcept
    { return (x.id == y.id); }

    friend bool operator<(record const& x, record const& y) noexcept
    { return (x.id < y.id); }

    friend bool operator==(record const& x, char const y) noexcept
    { return (x.id == y); }

    friend bool operator<(record const& x, char const y) noexcept
    { return (x.id < y); }

    friend bool operator>(record const& x, char const y) noexcept
    { return (x.id > y); }
};


class stateful_compare
{
public:
    int id;

    explicit stateful_compare(int const x) noexcept :
        id(x)
    {}

    template<typename T1, typename T2>
    constexpr bool operator()(T1&& x, T2&& y) const noexcept
    { return (std::forward<T1>(x) < std::forward<T2>(y)); }
};


TEST(intrusive_set_test, stateful_compare)
{
    stateful_compare comp{42};
    intrusive::set<record, stateful_compare> s1{comp};
    ASSERT_EQ(s1.value_comp().id, 42);

    comp = stateful_compare{11};
    intrusive::set<record, stateful_compare> s2{comp};
    ASSERT_EQ(s1.value_comp().id, 42);
    ASSERT_EQ(s2.value_comp().id, 11);

    swap(s1, s2);
    ASSERT_EQ(s1.value_comp().id, 11);
    ASSERT_EQ(s2.value_comp().id, 42);
}

TEST(intrusive_set_test, insert)
{
    record a1{'A'};
    record b1{'B'};
    record c1{'C'};
    record d1{'D'};
    record e1{'E'};

    intrusive::set<record> set;
    ASSERT_TRUE(set.insert(a1).second);
    ASSERT_TRUE(set.insert(e1).second);
    ASSERT_TRUE(set.insert(c1).second);
    ASSERT_TRUE(set.insert(d1).second);
    ASSERT_TRUE(set.insert(b1).second);

    record a2{'A'};
    record b2{'B'};
    record c2{'C'};
    record d2{'D'};
    record e2{'E'};

    auto r = set.insert(a2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(&*r.first, &a1);

    r = set.insert(b2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(&*r.first, &b1);

    r = set.insert(c2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(&*r.first, &c1);

    r = set.insert(d2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(&*r.first, &d1);

    r = set.insert(e2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(&*r.first, &e1);
    ASSERT_EQ(set.size(), 5);
}

TEST(intrusive_set_test, insert_hint)
{
    record a{'A'};
    record b{'B'};
    record c{'C'};
    record f{'F'};
    record g{'G'};

    intrusive::set<record> set;

    auto pos = set.insert(set.cbegin(), a);
    ASSERT_EQ(set.cbegin(), pos);
    ASSERT_EQ(*pos, a);
    ASSERT_EQ(set.size(), 1);

    pos = set.insert(set.cbegin(), b);
    ASSERT_EQ(pos, std::next(set.cbegin()));
    ASSERT_EQ(*pos, b);
    ASSERT_EQ(set.size(), 2);

    pos = set.insert(set.cbegin(), g);
    ASSERT_EQ(pos, std::next(set.cbegin(), 2));
    ASSERT_EQ(*pos, g);
    ASSERT_EQ(set.size(), 3);

    pos = set.insert(std::prev(set.cend()), f);
    ASSERT_EQ(pos, std::next(set.cbegin(), 2));
    ASSERT_EQ(*pos, f);
    ASSERT_EQ(set.size(), 4);

    pos = set.insert(set.cend(), c);
    ASSERT_EQ(pos, std::next(set.cbegin(), 2));
    ASSERT_EQ(*pos, c);
    ASSERT_EQ(set.size(), 5);
}

TEST(intrusive_set_test, count)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    record b{'B'};
    record h{'H'};
    record z{'Z'};

    intrusive::set<record> set;
    ASSERT_TRUE(set.insert(a).second);
    ASSERT_TRUE(set.insert(e).second);
    ASSERT_TRUE(set.insert(i).second);
    ASSERT_TRUE(set.insert(o).second);
    ASSERT_TRUE(set.insert(u).second);

    EXPECT_EQ(set.count(a), 1);
    EXPECT_EQ(set.count(e), 1);
    EXPECT_EQ(set.count(i), 1);
    EXPECT_EQ(set.count(o), 1);
    EXPECT_EQ(set.count(u), 1);
    EXPECT_EQ(set.count(b), 0);
    EXPECT_EQ(set.count(z), 0);
    EXPECT_EQ(set.count(h), 0);

    EXPECT_EQ(set.count('A'), 1);
    EXPECT_EQ(set.count('E'), 1);
    EXPECT_EQ(set.count('I'), 1);
    EXPECT_EQ(set.count('O'), 1);
    EXPECT_EQ(set.count('U'), 1);
    EXPECT_EQ(set.count('B'), 0);
    EXPECT_EQ(set.count('Z'), 0);
    EXPECT_EQ(set.count('H'), 0);
}

TEST(intrusive_set_test, lower_bound)
{
    record a{'A'};
    record b{'B'};
    record c{'C'};
    record d{'D'};
    record f{'F'};

    intrusive::set<record> set;

    ASSERT_TRUE(set.insert(b).second);
    ASSERT_TRUE(set.insert(f).second);
    ASSERT_TRUE(set.insert(c).second);
    ASSERT_TRUE(set.insert(a).second);
    ASSERT_TRUE(set.insert(d).second);

    auto pos = set.cbegin();
    ASSERT_EQ(pos++, set.lower_bound('A'));
    ASSERT_EQ(pos++, set.lower_bound('B'));
    ASSERT_EQ(pos++, set.lower_bound('C'));
    ASSERT_EQ(pos++, set.lower_bound('D'));
    ASSERT_EQ(pos,   set.lower_bound('E'));
    ASSERT_EQ(pos++, set.lower_bound('F'));
    ASSERT_EQ(pos,   set.lower_bound('G'));
    ASSERT_EQ(pos,   set.cend());
}

TEST(intrusive_set_test, upper_bound)
{
    record a{'A'};
    record b{'B'};
    record c{'C'};
    record d{'D'};
    record f{'F'};

    intrusive::set<record> set;

    ASSERT_TRUE(set.insert(b).second);
    ASSERT_TRUE(set.insert(f).second);
    ASSERT_TRUE(set.insert(c).second);
    ASSERT_TRUE(set.insert(a).second);
    ASSERT_TRUE(set.insert(d).second);

    auto pos = std::next(set.cbegin());
    ASSERT_EQ(pos++, set.upper_bound('A'));
    ASSERT_EQ(pos++, set.upper_bound('B'));
    ASSERT_EQ(pos++, set.upper_bound('C'));
    ASSERT_EQ(pos,   set.upper_bound('D'));
    ASSERT_EQ(pos++, set.upper_bound('E'));
    ASSERT_EQ(pos,   set.upper_bound('F'));
    ASSERT_EQ(pos,   set.upper_bound('G'));
    ASSERT_EQ(pos,   set.cend());
}

TEST(intrusive_set_test, move_constructor_and_assignment_operator)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    intrusive::set<record> s1;

    ASSERT_TRUE(s1.insert(u).second);
    ASSERT_TRUE(s1.insert(o).second);
    ASSERT_TRUE(s1.insert(i).second);
    ASSERT_TRUE(s1.insert(e).second);
    ASSERT_TRUE(s1.insert(a).second);

    intrusive::set<record> s2{std::move(s1)};
    auto pos = s2.cbegin();

    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, s2.cend());
    ASSERT_EQ(s2.size(), 5);

    ASSERT_EQ(s1.size(), 0);
    ASSERT_EQ(s1.cbegin(), s1.cend());

    s1 = std::move(s2);
    pos = s1.cbegin();

    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, s1.cend());
    ASSERT_EQ(s1.size(), 5);

    ASSERT_EQ(s2.size(), 0);
    ASSERT_EQ(s2.cbegin(), s2.cend());

    auto s3 = std::move(s2);
    ASSERT_TRUE(s2.empty());
    ASSERT_TRUE(s3.empty());

#if __has_warning("-Wself-move")
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wself-move"
#endif

    s1 = std::move(s1);
    pos = s1.cbegin();
    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, s1.cend());
    ASSERT_EQ(s1.size(), 5);

#if __has_warning("-Wself-move")
# pragma clang diagnostic pop
#endif
}

TEST(intrusive_set_test, swap)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    intrusive::set<record> s1;
    ASSERT_TRUE(s1.insert(u).second);
    ASSERT_TRUE(s1.insert(o).second);
    ASSERT_TRUE(s1.insert(i).second);
    ASSERT_TRUE(s1.insert(e).second);
    ASSERT_TRUE(s1.insert(a).second);

    auto pos = s1.cbegin();
    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, s1.cend());
    ASSERT_EQ(s1.size(), 5);

    record x{'X'};
    record y{'Y'};
    record z{'Z'};

    intrusive::set<record> s2;
    ASSERT_TRUE(s2.insert(y).second);
    ASSERT_TRUE(s2.insert(x).second);
    ASSERT_TRUE(s2.insert(z).second);

    pos = s2.cbegin();
    ASSERT_EQ(*pos++, x);
    ASSERT_EQ(*pos++, y);
    ASSERT_EQ(*pos++, z);
    ASSERT_EQ(pos, s2.cend());
    ASSERT_EQ(s2.size(), 3);

    s1.swap(s2);
    pos = s1.cbegin();

    ASSERT_EQ(s1.size(), 3);
    ASSERT_EQ(*pos++, x);
    ASSERT_EQ(*pos++, y);
    ASSERT_EQ(*pos++, z);
    ASSERT_EQ(pos, s1.cend());

    pos = s2.cbegin();
    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, s2.cend());
    ASSERT_EQ(s2.size(), 5);
}

TEST(intrusive_set_test, equal_range)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    intrusive::set<record> set;
    ASSERT_TRUE(set.insert(u).second);
    ASSERT_TRUE(set.insert(o).second);
    ASSERT_TRUE(set.insert(i).second);
    ASSERT_TRUE(set.insert(e).second);
    ASSERT_TRUE(set.insert(a).second);

    {
        auto const eq = set.equal_range('A');
        ASSERT_EQ(*eq.first, a);
        ASSERT_EQ(eq.second, std::next(eq.first));
    }

    {
        auto const eq = set.equal_range('U');
        ASSERT_EQ(*eq.first, u);
        ASSERT_EQ(eq.second, std::next(eq.first));
        ASSERT_EQ(eq.second, set.cend());
    }

    {
        auto const eq = set.equal_range('H');
        ASSERT_EQ(eq.second, std::next(set.cbegin(), 2));
        ASSERT_EQ(eq.second, eq.first);
        ASSERT_EQ(*eq.first, i);
    }
}

TEST(intrusive_set_test, erase)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    intrusive::set<record> set;
    ASSERT_TRUE(set.insert(u).second);
    ASSERT_TRUE(set.insert(o).second);
    ASSERT_TRUE(set.insert(i).second);
    ASSERT_TRUE(set.insert(e).second);
    ASSERT_TRUE(set.insert(a).second);

    auto pos = set.cbegin();
    ASSERT_EQ(*pos++, a);
    ASSERT_EQ(*pos++, e);
    ASSERT_EQ(*pos++, i);
    ASSERT_EQ(*pos++, o);
    ASSERT_EQ(*pos++, u);
    ASSERT_EQ(pos, set.cend());
    ASSERT_EQ(5, set.size());

    pos = set.erase(std::next(set.cbegin(), 1));
    ASSERT_EQ(pos, std::next(set.cbegin(), 1));
    ASSERT_EQ(*pos, i);
    ASSERT_EQ(4, set.size());

    pos = set.erase(set.cbegin());
    ASSERT_EQ(pos, set.cbegin());
    ASSERT_EQ(*pos, i);
    ASSERT_EQ(3, set.size());

    pos = set.erase(std::prev(set.cend()));
    ASSERT_EQ(pos, set.cend());
    ASSERT_EQ(2, set.size());
}

TEST(intrusive_set_test, erase_range)
{
    record a{'A'};
    record e{'E'};
    record i{'I'};
    record o{'O'};
    record u{'U'};

    intrusive::set<record> set;
    ASSERT_TRUE(set.insert(u).second);
    ASSERT_TRUE(set.insert(o).second);
    ASSERT_TRUE(set.insert(i).second);
    ASSERT_TRUE(set.insert(e).second);
    ASSERT_TRUE(set.insert(a).second);

    auto pos = set.erase(set.cbegin(), std::prev(set.cend()));
    ASSERT_EQ(pos, set.cbegin());
    ASSERT_EQ(pos, std::prev(set.cend()));
    ASSERT_EQ(*pos, u);
    ASSERT_EQ(set.size(), 1);
    ASSERT_FALSE(set.empty());

    pos = set.erase(set.cbegin());
    ASSERT_EQ(pos, set.cbegin());
    ASSERT_EQ(pos, set.cend());
    ASSERT_EQ(set.size(), 0);
    ASSERT_TRUE(set.empty());
}


TEST(intrusive_multiset_test, insert)
{
    record a1{'A'};
    record b1{'B'};
    record c1{'C'};
    record d1{'D'};
    record e1{'E'};

    intrusive::multiset<record> set;
    set.insert(a1);
    set.insert(e1);
    set.insert(c1);
    set.insert(d1);
    set.insert(b1);

    record a2{'A'};
    record b2{'B'};
    record c2{'C'};
    record d2{'D'};
    record e2{'E'};

    auto pos = set.insert(a2);
    ASSERT_EQ(&*std::prev(pos), &a1);
    ASSERT_EQ(&*pos, &a2);

    pos = set.insert(e2);
    ASSERT_EQ(&*std::prev(pos), &e1);
    ASSERT_EQ(&*pos, &e2);

    pos = set.insert(c2);
    ASSERT_EQ(&*std::prev(pos), &c1);
    ASSERT_EQ(&*pos, &c2);

    pos = set.insert(d2);
    ASSERT_EQ(&*std::prev(pos), &d1);
    ASSERT_EQ(&*pos, &d2);

    pos = set.insert(b2);
    ASSERT_EQ(&*std::prev(pos), &b1);
    ASSERT_EQ(&*pos, &b2);
    ASSERT_EQ(set.size(), 10);
}

TEST(intrusive_multiset_test, equal_range)
{
    record a[] = {{'A'}, {'A'}};
    record e[] = {{'E'}, {'E'}};
    record i[] = {{'I'}, {'I'}};
    record o[] = {{'O'}, {'O'}};
    record u[] = {{'U'}, {'U'}};

    intrusive::multiset<record> set;

    set.insert(e[0]);
    set.insert(i[0]);
    set.insert(u[0]);
    set.insert(o[0]);
    set.insert(a[0]);

    set.insert(i[1]);
    set.insert(a[1]);
    set.insert(u[1]);
    set.insert(e[1]);
    set.insert(o[1]);

    {
        auto const ret = set.equal_range('A');
        ASSERT_EQ(ret.second, std::next(ret.first, 2));
        ASSERT_EQ(&a[0], &*ret.first);
        ASSERT_EQ(&a[1], &*std::prev(ret.second));
    }

    {
        auto const ret = set.equal_range('E');
        ASSERT_EQ(ret.second, std::next(ret.first, 2));
        ASSERT_EQ(&e[0], &*ret.first);
        ASSERT_EQ(&e[1], &*std::prev(ret.second));
    }

    {
        auto const ret = set.equal_range('I');
        ASSERT_EQ(ret.second, std::next(ret.first, 2));
        ASSERT_EQ(&i[0], &*ret.first);
        ASSERT_EQ(&i[1], &*std::prev(ret.second));
    }

    {
        auto const ret = set.equal_range('O');
        ASSERT_EQ(ret.second, std::next(ret.first, 2));
        ASSERT_EQ(&o[0], &*ret.first);
        ASSERT_EQ(&o[1], &*std::prev(ret.second));
    }

    {
        auto const ret = set.equal_range('U');
        ASSERT_EQ(ret.second, std::next(ret.first, 2));
        ASSERT_EQ(&u[0], &*ret.first);
        ASSERT_EQ(&u[1], &*std::prev(ret.second));
    }


    {
        auto const ret = set.equal_range(' ');
        ASSERT_EQ(ret.first, ret.second);
        ASSERT_EQ(ret.first, set.begin());
    }

    {
        auto const ret = set.equal_range('T');
        ASSERT_EQ(ret.first, ret.second);
        ASSERT_EQ(ret.first, std::prev(set.end(), 2));
        ASSERT_EQ(&*ret.first, &u[0]);
    }

    {
        auto const ret = set.equal_range('Z');
        ASSERT_EQ(ret.first, ret.second);
        ASSERT_EQ(ret.first, set.end());
    }
}

TEST(intrusive_multiset_test, erase_range)
{
    // sorted := [A,E,E,F,I,N,N,T,U,Y,Y,Z]
    std::array<record, 12> records {{
        {'Y'}, {'E'}, {'U'},
        {'T'}, {'A'}, {'F'},
        {'I'}, {'Y'}, {'E'},
        {'Z'}, {'N'}, {'N'},
    }};

    intrusive::multiset<record> s;
    s.insert(records.begin(), records.end());
    ASSERT_EQ(s.size(), records.size());

    // erase [E,E,F,I,N]
    auto pos = s.erase(std::next(s.cbegin(), 1), std::next(s.cbegin(), 6));
    ASSERT_EQ(pos, std::next(s.cbegin(), 1));
    ASSERT_EQ(*pos++, 'N');
    ASSERT_EQ(*pos++, 'T');
    ASSERT_EQ(*pos++, 'U');
    ASSERT_EQ(s.size(), 7);

    // erase [Y,Z]
    pos = s.erase(std::next(s.cbegin(), 5), s.cend());
    ASSERT_EQ(pos, s.cend());
    ASSERT_EQ(*(--pos), 'Y');
    ASSERT_EQ(*(--pos), 'U');
    ASSERT_EQ(*(--pos), 'T');
    ASSERT_EQ(*(--pos), 'N');
    ASSERT_EQ(*(--pos), 'A');
    ASSERT_EQ(s.size(), 5);
}

TEST(intrusive_multiset_test, random_insertion)
{
    auto generate_record = [
        urng = std::mt19937{std::random_device{}()},
        dist = std::uniform_int_distribution<char>{CHAR_MIN, CHAR_MAX}
    ]() mutable {
        return record{dist(urng)};
    };
    record records[512];
    std::generate(std::begin(records), std::end(records), generate_record);

    intrusive::multiset<record> s;
    for (auto&& r : records) {
        s.insert(r);
    }
}

