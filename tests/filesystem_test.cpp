////////////////////////////////////////////////////////////////////////////////
//
// tests/filesystem_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/string_view.hpp>

#include "core/filesystem.hpp"

#include <gtest/gtest.h>


using namespace ::amp;


TEST(filesystem_test, parent_path)
{
    EXPECT_EQ(fs::parent_path(""), "");
    EXPECT_EQ(fs::parent_path("/"), "");
    EXPECT_EQ(fs::parent_path("/etc"), "/");
    EXPECT_EQ(fs::parent_path("/etc/"), "/etc");
    EXPECT_EQ(fs::parent_path("/etc/hosts"), "/etc");
}

TEST(filesystem_test, filename)
{
    EXPECT_EQ(fs::filename(""), "");
    EXPECT_EQ(fs::filename("/"), "");
    EXPECT_EQ(fs::filename("/etc/hosts"), "hosts");
    EXPECT_EQ(fs::filename("/home/user/docs/file.txt"), "file.txt");
}

