////////////////////////////////////////////////////////////////////////////////
//
// tests/uri_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/net/uri.hpp>

#include <stdexcept>

#include <gtest/gtest.h>


using namespace ::amp;
using namespace ::amp::literals;


TEST(uri_test, create_with_string)
{
    {
        auto const u = ""_uri;
        EXPECT_EQ(u.scheme(), "");
        EXPECT_EQ(u.userinfo(), "");
        EXPECT_EQ(u.host(), "");
        EXPECT_EQ(u.port(), "");
        EXPECT_EQ(u.path(), "");
        EXPECT_EQ(u.query(), "");
        EXPECT_EQ(u.fragment(), "");
    }

    {
        auto const u = "http://user:pass@example.com:80/path?query#frag"_uri;
        EXPECT_EQ(u.scheme(), "http");
        EXPECT_EQ(u.userinfo(), "user:pass");
        EXPECT_EQ(u.host(), "example.com");
        EXPECT_EQ(u.port(), "80");
        EXPECT_EQ(u.path(), "/path");
        EXPECT_EQ(u.query(), "query");
        EXPECT_EQ(u.fragment(), "frag");
    }
}

TEST(uri_test, resolve)
{
    ////////////////////////////////////////////////////////////////////////////
    // Normal examples
    ////////////////////////////////////////////////////////////////////////////

    auto const base = "http://a/b/c/d;p?q"_uri;

    EXPECT_EQ("g:h"_uri.resolve(base), "g:h"_uri);

    EXPECT_EQ("g"_uri.resolve(base), "http://a/b/c/g"_uri);

    EXPECT_EQ("./g"_uri.resolve(base), "http://a/b/c/g"_uri);

    EXPECT_EQ("g/"_uri.resolve(base), "http://a/b/c/g/"_uri);

    EXPECT_EQ("/g"_uri.resolve(base), "http://a/g"_uri);

    EXPECT_EQ("//g"_uri.resolve(base), "http://g"_uri);

    EXPECT_EQ("?y"_uri.resolve(base), "http://a/b/c/d;p?y"_uri);

    EXPECT_EQ("g?y"_uri.resolve(base), "http://a/b/c/g?y"_uri);

    EXPECT_EQ("#s"_uri.resolve(base), "http://a/b/c/d;p?q#s"_uri);

    EXPECT_EQ("g#s"_uri.resolve(base), "http://a/b/c/g#s"_uri);

    EXPECT_EQ("g?y#s"_uri.resolve(base), "http://a/b/c/g?y#s"_uri);

    EXPECT_EQ(";x"_uri.resolve(base), "http://a/b/c/;x"_uri);

    EXPECT_EQ("g;x"_uri.resolve(base), "http://a/b/c/g;x"_uri);

    EXPECT_EQ("g;x?y#s"_uri.resolve(base), "http://a/b/c/g;x?y#s"_uri);

    EXPECT_EQ("/g/x?y#s"_uri.resolve(base), "http://a/g/x?y#s"_uri);

    EXPECT_EQ(""_uri.resolve(base), "http://a/b/c/d;p?q"_uri);

    EXPECT_EQ("."_uri.resolve(base), "http://a/b/c/"_uri);

    EXPECT_EQ("./"_uri.resolve(base), "http://a/b/c/"_uri);

    EXPECT_EQ(".."_uri.resolve(base), "http://a/b/"_uri);

    EXPECT_EQ("../"_uri.resolve(base), "http://a/b/"_uri);

    EXPECT_EQ("../g"_uri.resolve(base), "http://a/b/g"_uri);

    EXPECT_EQ("../.."_uri.resolve(base), "http://a/"_uri);

    EXPECT_EQ("../../"_uri.resolve(base), "http://a/"_uri);

    EXPECT_EQ("../../g"_uri.resolve(base), "http://a/g"_uri);


    ////////////////////////////////////////////////////////////////////////////
    // Abnormal examples
    ////////////////////////////////////////////////////////////////////////////

    EXPECT_EQ("../../../g"_uri.resolve(base), "http://a/g"_uri);

    EXPECT_EQ("../../../../g"_uri.resolve(base), "http://a/g"_uri);

    EXPECT_EQ("/./g"_uri.resolve(base), "http://a/g"_uri);

    EXPECT_EQ("g."_uri.resolve(base), "http://a/b/c/g."_uri);

    EXPECT_EQ("g."_uri.resolve(base), "http://a/b/c/g."_uri);

    EXPECT_EQ("g.."_uri.resolve(base), "http://a/b/c/g.."_uri);

    EXPECT_EQ("..g"_uri.resolve(base), "http://a/b/c/..g"_uri);

    EXPECT_EQ("./../g"_uri.resolve(base), "http://a/b/g"_uri);

    EXPECT_EQ("./g/."_uri.resolve(base), "http://a/b/c/g/"_uri);

    EXPECT_EQ("g/./h"_uri.resolve(base), "http://a/b/c/g/h"_uri);

    EXPECT_EQ("g/../h"_uri.resolve(base), "http://a/b/c/h"_uri);

    EXPECT_EQ("g;x=1/./y"_uri.resolve(base), "http://a/b/c/g;x=1/y"_uri);

    EXPECT_EQ("g?y/./x"_uri.resolve(base), "http://a/b/c/g?y/./x"_uri);

    EXPECT_EQ("g?y/../x"_uri.resolve(base), "http://a/b/c/g?y/../x"_uri);

    EXPECT_EQ("g#s/./x"_uri.resolve(base), "http://a/b/c/g#s/./x"_uri);

    EXPECT_EQ("g#s/../x"_uri.resolve(base), "http://a/b/c/g#s/../x"_uri);
}

TEST(uri_test, relative_reference_path_with_colon)
{
    auto const u = "./this:that"_uri;
    EXPECT_EQ(u.scheme(), "");
    EXPECT_EQ(u.path(), "./this:that");
    EXPECT_EQ(u.resolve("http://a/"_uri), "http://a/this:that"_uri);
}

TEST(uri_test, relative_reference_with_host)
{
    auto const u = "//g"_uri;
    EXPECT_EQ(u.scheme(), "");
    EXPECT_EQ(u.host(), "g");
    EXPECT_EQ(u.path(), "");
}

TEST(uri_test, scheme_file)
{
    auto const u = "file:///bin/bash"_uri;
    EXPECT_EQ(u.scheme(), "file");
    EXPECT_EQ(u.path(), "/bin/bash");
}

TEST(uri_test, scheme_news)
{
    auto const u = "news:comp.infosystems.www.servers.unix"_uri;
    EXPECT_EQ(u.scheme(), "news");
    EXPECT_EQ(u.path(), "comp.infosystems.www.servers.unix");
}

TEST(uri_test, scheme_tel)
{
    auto const u = "tel:+1-816-555-1212"_uri;
    EXPECT_EQ(u.scheme(), "tel");
    EXPECT_EQ(u.path(), "+1-816-555-1212");
}

TEST(uri_test, scheme_ldap)
{
    auto const u = "ldap://[2001:db8::7]/c=GB?objectClass?one"_uri;
    EXPECT_EQ(u.scheme(), "ldap");
    EXPECT_EQ(u.host(), "[2001:db8::7]");
    EXPECT_EQ(u.path(), "/c=GB");
    EXPECT_EQ(u.query(), "objectClass?one");
}

TEST(uri_test, scheme_urn)
{
    auto const u = "urn:oasis:names:specification:docbook:dtd:xml:4.1.2"_uri;
    EXPECT_EQ(u.scheme(), "urn");
    EXPECT_EQ(u.path(), "oasis:names:specification:docbook:dtd:xml:4.1.2");
}

TEST(uri_test, scheme_mailto)
{
    auto const u = "mailto:john.doe@example.com?query#fragment"_uri;
    EXPECT_EQ(u.path(), "john.doe@example.com");
    EXPECT_EQ(u.query(), "query");
    EXPECT_EQ(u.fragment(), "fragment");
}

TEST(uri_test, scheme_xmpp)
{
    auto const u = "xmpp:node@example.com?message;subject=Hello%20World"_uri;
    EXPECT_EQ(u.scheme(), "xmpp");
    EXPECT_EQ(u.path(), "node@example.com");
    EXPECT_EQ(u.query(), "message;subject=Hello%20World");
}

TEST(uri_test, ipv4_address)
{
    auto const u = "http://129.79.245.252/"_uri;
    EXPECT_EQ(u.scheme(), "http");
    EXPECT_EQ(u.host(), "129.79.245.252");
    EXPECT_EQ(u.path(), "/");
}

TEST(uri_test, ipv6_address)
{
    {
        auto const u = "http://[1080:0:0:0:8:800:200C:417A]/"_uri;
        EXPECT_EQ(u.scheme(), "http");
        EXPECT_EQ(u.host(), "[1080:0:0:0:8:800:200c:417a]");
        EXPECT_EQ(u.path(), "/");
    }

    {
        auto const u = "http://[::1]/"_uri;
        EXPECT_EQ(u.scheme(), "http");
        EXPECT_EQ(u.host(), "[::1]");
        EXPECT_EQ(u.path(), "/");
    }

    EXPECT_THROW("http://[1080:0:0:0:8:800:200c:417a/"_uri, std::runtime_error);
}

TEST(uri_test, ipv6_v4inv6)
{
    auto const u = "http://[::ffff:192.0.2.128]/"_uri;
    EXPECT_EQ(u.scheme(), "http");
    EXPECT_EQ(u.host(), "[::ffff:192.0.2.128]");
    EXPECT_EQ(u.path(), "/");
}

TEST(uri_test, empty_port)
{
    auto const u = "http://123.34.23.56:/"_uri;
    EXPECT_TRUE(u.port().empty());
}

TEST(uri_test, empty_user_name_in_userinfo)
{
    auto const u = "ftp://:@localhost"_uri;
    EXPECT_EQ(u.userinfo(), ":");
    EXPECT_EQ(u.host(), "localhost");
}

TEST(uri_test, with_file_path)
{
    {
        auto const u = net::uri::from_file_path("/absolute/path");
        EXPECT_EQ(u.get_file_path(), "/absolute/path");
        EXPECT_EQ(u, "file:///absolute/path"_uri);
    }

    {
        auto const u = net::uri::from_file_path("relative/path");
        EXPECT_EQ(u.get_file_path(), "relative/path");
        EXPECT_EQ(u, "relative/path"_uri);
    }

    {
        auto const u = net::uri::from_file_path("relative path");
        EXPECT_EQ(u.get_file_path(), "relative path");
        EXPECT_EQ(u, "relative%20path"_uri);
    }
}

TEST(uri_test, scheme_and_host_case_normalization)
{
    {
        auto const u = "HTTP://ExamPLE.Com"_uri;
        EXPECT_EQ(u.scheme(), "http");
        EXPECT_EQ(u.host(), "example.com");
    }

    {
        auto const u = "httpS://[1080:0:0:0:8:800:200C:417A]/"_uri;
        EXPECT_EQ(u.scheme(), "https");
        EXPECT_EQ(u.host(), "[1080:0:0:0:8:800:200c:417a]");
    }
}


TEST(uri_test, unreserved_character_normalization)
{
    {
        auto const u = "http://example.com/%7Eglynos/"_uri;
        EXPECT_EQ(u.path(), "/~glynos/");
    }

    {
        auto const u = "http://example%2Ecom/%7E%66%6F%6F%62%61%72/"_uri;
        EXPECT_EQ(u.host(), "example.com");
        EXPECT_EQ(u.path(), "/~foobar/");
    }
}

TEST(uri_test, path_normalization)
{
    {
        auto const u = "http://example.com/a/../b?key=value#fragment"_uri;
        EXPECT_EQ(u.path(), "/b");
    }

    {
        auto const u = "http://example.com/./"_uri;
        EXPECT_EQ(u.path(), "/");
    }

    {
        auto const u = "http://example.com/./."_uri;
        EXPECT_EQ(u.path(), "/");
    }

    {
        auto const u = "http://example.com//"_uri;
        EXPECT_EQ(u.path(), "/");
    }
}

