////////////////////////////////////////////////////////////////////////////////
//
// tests/spsc_queue_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/optional.hpp>
#include <amp/range.hpp>

#include "core/spsc_queue.hpp"

#include <cstddef>
#include <iterator>
#include <numeric>
#include <tuple>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(spsc_queue_test, clear)
{
    spsc::queue<int> q;
    ASSERT_EQ(q.pop(), nullopt);
    ASSERT_TRUE(q.empty());

    q.push(123);
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(q.pop(), 123);
    ASSERT_EQ(q.pop(), nullopt);
    ASSERT_TRUE(q.empty());

    auto const rng = xrange(-5, 5);
    q.push(rng.begin(), rng.end());
    ASSERT_FALSE(q.empty());

    q.clear();
    ASSERT_EQ(q.pop(), nullopt);
    ASSERT_TRUE(q.empty());

    q.push(rng.begin(), rng.end());
    ASSERT_FALSE(q.empty());

    for (auto const x : rng) {
        ASSERT_EQ(q.pop(), x);
    }
    ASSERT_EQ(q.pop(), nullopt);
    ASSERT_TRUE(q.empty());

    q.clear();
    ASSERT_EQ(q.pop(), nullopt);
    ASSERT_TRUE(q.empty());
}

TEST(spsc_queue_test, push_and_pop)
{
    auto const rng = xrange(16);
    auto const mid = rng.begin() + (rng.size() / 2);

    spsc::queue<int> q;
    q.push(rng.begin(), mid);

    for (auto const i : xrange(0, 4)) {
        ASSERT_EQ(q.pop(), i);
    }
    ASSERT_FALSE(q.empty());

    q.push(mid, rng.end());
    ASSERT_FALSE(q.empty());

    for (auto const i : xrange(4, 8)) {
        ASSERT_EQ(q.pop(), i);
    }
    ASSERT_FALSE(q.empty());

    for (auto const i : xrange(8, 16)) {
        ASSERT_EQ(q.pop(), i);
    }
    ASSERT_TRUE(q.empty());
}

TEST(spsc_queue_test, push_iterator_range)
{
    auto const rng = xrange(-30L, 15L);
    auto const mid = rng.split(rng.size() / 2);

    spsc::queue<long> q;
    ASSERT_TRUE(q.empty());

    q.push(mid.second.begin(), mid.second.end());
    q.push(mid.first.begin(),  mid.first.end());
    ASSERT_FALSE(q.empty());

    auto accum = 0L;
    auto const total = q.for_each([&](long x) { accum += x; });

    ASSERT_EQ(total, rng.size());
    ASSERT_EQ(accum, std::accumulate(rng.begin(), rng.end(), 0L));
    ASSERT_TRUE(q.empty());
}

TEST(spsc_queue_test, non_movable_types)
{
    class non_movable
    {
    public:
        explicit non_movable(int& c) noexcept :
            counter_(c)
        { ++counter_; }

        ~non_movable()
        { --counter_; }

        non_movable(non_movable const&) = delete;
        non_movable& operator=(non_movable const&) = delete;

    private:
        int& counter_;
    };

    auto counter = 0;

    spsc::queue<non_movable> q;
    q.emplace(counter);
    q.emplace(counter);
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(counter, 2);

    ASSERT_TRUE(q.pop(std::ignore));
    ASSERT_EQ(counter, 1);

    ASSERT_TRUE(q.pop(std::ignore));
    ASSERT_EQ(counter, 0);

    ASSERT_FALSE(q.pop(std::ignore));
    ASSERT_EQ(counter, 0);

    q.emplace(counter);
    q.emplace(counter);
    q.emplace(counter);
    ASSERT_EQ(counter, 3);

    q.clear();
    ASSERT_EQ(counter, 0);
    ASSERT_TRUE(q.empty());
    ASSERT_FALSE(q.pop(std::ignore));
}

