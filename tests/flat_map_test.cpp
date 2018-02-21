////////////////////////////////////////////////////////////////////////////////
//
// tests/flat_map_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/flat_map.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(flat_map_test, at)
{
    flat_map<std::string_view, int> m;
    ASSERT_TRUE(m.empty());

    m.emplace("kappa", 71);
    EXPECT_EQ(m.at("kappa"), 71);

    EXPECT_THROW(m.at("gamma"), std::out_of_range);
    EXPECT_EQ(m.count("gamma"), 0);
}

TEST(flat_map_test, insert)
{
    flat_map<char, int> m;
    ASSERT_TRUE(m.empty());

    {
        auto const r = m.insert({'A', 7});
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first, m.begin());
        EXPECT_EQ(r.first->first, 'A');
        EXPECT_EQ(r.first->second, 7);
        EXPECT_EQ(m.size(), 1);
    }

    {
        auto const r = m.insert(std::make_pair('B', 9));
        EXPECT_TRUE(r.second);
        EXPECT_EQ(r.first, std::next(m.begin()));
        EXPECT_EQ(r.first->first, 'B');
        EXPECT_EQ(r.first->second, 9);
        EXPECT_EQ(m.size(), 2);
    }

    {
        auto const r = m.insert({'A', -1});
        EXPECT_FALSE(r.second);
        EXPECT_EQ(r.first, m.begin());
        EXPECT_EQ(r.first->first, 'A');
        EXPECT_EQ(r.first->second, 7);
        EXPECT_EQ(m.size(), 2);
    }
}


template<typename Base>
class tagged_compare final :
    private Base
{
    int tag;

public:
    explicit tagged_compare(int const t) :
        tag(t)
    {}

    using Base::operator();

    bool operator==(tagged_compare const& x) const noexcept
    { return (tag == x.tag); }

    bool operator!=(tagged_compare const& x) const noexcept
    { return !(*this == x); }
};

TEST(flat_map_test, stateful_compare)
{
    tagged_compare<std::greater<>> comp{42};
    flat_map<int, std::string_view, decltype(comp)> m{comp};

    EXPECT_EQ(m.key_comp(), comp);
    EXPECT_TRUE(m.emplace(80, "eighty").second);
    EXPECT_EQ(m.size(), 1);

    EXPECT_TRUE(m.emplace(20, "twenty").second);
    EXPECT_EQ(m.size(), 2);

    EXPECT_FALSE(m.emplace(80, "eighty").second);
    EXPECT_EQ(m.size(), 2);

    // Ensure that the keys are sorted backwards
    EXPECT_TRUE(std::is_sorted(m.cbegin(), m.cend(), m.value_comp()));
}


TEST(flat_multimap_test, insert)
{
    flat_multimap<std::string_view, int> m {
        { "alpha", 33 },
        { "omega", 18 },
        { "gamma", 71 },
        { "alpha", 10 },
    };

    auto sorted = [&m]{
        return std::is_sorted(m.cbegin(), m.cend(), m.value_comp());
    };

    ASSERT_TRUE(sorted());

    {
        auto const eq = m.equal_range("alpha");
        EXPECT_EQ(std::distance(eq.first, eq.second), 2);

        auto pos = m.find("alpha");
        EXPECT_EQ((pos++)->second, 33);
        EXPECT_EQ((pos++)->second, 10);
        EXPECT_NE(pos, m.cend());
    }

    m.clear();
    ASSERT_TRUE(sorted());
    ASSERT_TRUE(m.empty());

    constexpr auto sigma_count = 3;
    m.reserve(m.size() + sigma_count);
    ASSERT_TRUE(sorted());

    auto emplace = [&m, pos = m.cend() - 1](auto&&... args) mutable {
        auto const ret = m.emplace(std::forward<decltype(args)>(args)...);
        EXPECT_EQ(ret - 1, pos);
        pos = ret;
    };

    emplace("sigma", 3);
    emplace(std::make_pair("sigma", 2));
    emplace(std::piecewise_construct,
            std::forward_as_tuple("sigma"),
            std::forward_as_tuple(1));
    ASSERT_TRUE(sorted());

    {
        auto const eq = m.equal_range("sigma");
        EXPECT_EQ(std::distance(eq.first, eq.second), sigma_count);
        EXPECT_TRUE(std::is_sorted(eq.first, eq.second, std::greater<>{}));
        EXPECT_TRUE(sorted());

        EXPECT_EQ(m.count("sigma"), sigma_count);
        EXPECT_EQ(m.erase("sigma"), sigma_count);
        EXPECT_EQ(m.count("sigma"), 0);
        EXPECT_TRUE(sorted());
    }

    constexpr auto kappa_count = 10;

    m.insert( {
        { "kappa", 5 },
        { "kappa", 6 },
        { "kappa", 7 },
        { "kappa", 8 },
        { "kappa", 9 },
    } );
    EXPECT_EQ(m.count("kappa"), (kappa_count / 2));
    EXPECT_TRUE(sorted());

    m.insert( {
        { "kappa", 0 },
        { "kappa", 1 },
        { "kappa", 2 },
        { "kappa", 3 },
        { "kappa", 4 },
    } );
    EXPECT_EQ(m.count("kappa"), kappa_count);
    EXPECT_TRUE(sorted());

    {
        auto const eq = m.equal_range("kappa");
        EXPECT_EQ(std::distance(eq.first, eq.second), kappa_count);

        auto const middle = std::is_sorted_until(eq.first, eq.second);
        EXPECT_EQ(std::is_sorted_until(middle, eq.second), eq.second);
        EXPECT_EQ(std::distance(eq.first, middle), (kappa_count / 2));

        EXPECT_EQ(m.count("kappa"), kappa_count);
        EXPECT_EQ(m.erase("kappa"), kappa_count);
        EXPECT_EQ(m.count("kappa"), 0);
    }

    m = {};
    EXPECT_TRUE(sorted());
    EXPECT_TRUE(m.empty());
}

TEST(flat_multimap_test, erase)
{
    flat_multimap<char, int> m {
        { 'x', 0 },
        { 'x', 1 },
        { 'x', 2 },
        { 'x', 3 },
        { 'x', 4 },
    };
    EXPECT_EQ(m.count('x'), 5);

    m.insert( {
        { 'x', -5 },
        { 'x', -4 },
        { 'x', -3 },
        { 'x', -2 },
        { 'x', -1 },
    } );
    EXPECT_EQ(m.count('x'), 10);

    auto const eq = m.equal_range('x');
    EXPECT_EQ(std::distance(eq.first, eq.second), 10);

    auto const pivot = std::is_sorted_until(eq.first, eq.second);
    EXPECT_EQ(std::is_sorted_until(pivot, eq.second), eq.second);
    EXPECT_EQ(std::distance(eq.first, pivot), 5);

    EXPECT_EQ(m.count('x'), 10);
    m.erase(pivot, eq.second);

    EXPECT_EQ(m.count('x'), 5);
    EXPECT_TRUE(std::is_sorted(m.cbegin(), m.cend()));

    EXPECT_EQ(m.erase('x'), 5);
    EXPECT_EQ(m.count('x'), 0);
    EXPECT_EQ(m.erase('x'), 0);
    EXPECT_TRUE(m.empty());
}

