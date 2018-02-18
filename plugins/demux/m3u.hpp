////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/m3u.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_877D02FC_8FD5_4F5C_B000_513DC9CA8A83
#define AMP_INCLUDED_877D02FC_8FD5_4F5C_B000_513DC9CA8A83


#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/memory.hpp>
#include <amp/net/uri.hpp>
#include <amp/string.hpp>
#include <amp/string_view.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <cmath>
#include <cstdlib>
#include <ratio>
#include <vector>


namespace amp {
namespace m3u {
namespace aux {

[[noreturn]] inline void raise_invalid_syntax(char const* const type,
                                              string_view const s)
{
    raise(errc::invalid_argument, "[M3U] invalid %s: '%.*s'",
          type, static_cast<int>(s.size()), s.data());
}

inline auto parse_integer(string_view const s)
{
    char* end;
    auto const x = std::strtoull(s.data(), &end, 0);

    if (end == s.data()) {
        raise_invalid_syntax("integer", s);
    }
    return x;
}

inline auto parse_duration(string_view const s)
{
    char* end;
    auto const x = std::strtod(s.data(), &end);

    if (end == s.data() || !std::isfinite(x) || x < 0) {
        raise_invalid_syntax("duration", s);
    }
    return static_cast<uint64>(std::llround(x * std::nano::den));
}

inline auto parse_quoted_string(string_view s)
{
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') {
        raise_invalid_syntax("quoted string", s);
    }

    s.remove_prefix(1);
    s.remove_suffix(1);

    for (auto const c : s) {
        if (c == '\n' || c == '\r' || c == '"') {
            raise_invalid_syntax("quoted string", s);
        }
    }
    return s;
}

inline bool match(string_view& x, string_view const y) noexcept
{
    if (y.size() <= x.size()) {
        if (mem::equal(x.data(), y.data(), y.size())) {
            x.remove_prefix(y.size());
            return true;
        }
    }
    return false;
}

}     // namespace aux


struct segment
{
    net::uri location;
    uint64   duration;

    explicit segment(net::uri u, uint64 const d) noexcept :
        location{std::move(u)},
        duration{d}
    {}
};


class media_playlist
{
public:
    explicit media_playlist(net::uri const& u) :
        location{u}
    {}

    void load()
    {
        auto const file = io::open(location, io::in);
        auto const text = u8string::from_text_file(*file);
        auto const lines = tokenize(text, '\n');
        auto const last = lines.end();
        auto first = lines.begin();

        if (first == last || *first++ != "#EXTM3U") {
            raise(errc::invalid_data_format, "not an extended M3U file");
        }

        while (first != last) {
            auto line = *first++;

            if (aux::match(line, "#EXT-X-ENDLIST")) {
                break;
            }
            else if (aux::match(line, "#EXT-X-VERSION:")) {
                if (version != 0) {
                    raise(errc::failure,
                          "media playlist cannot contain "
                          "multiple '#EXT-X-VERSION' tags");
                }
                version = m3u::aux::parse_integer(line);
            }
            else if (aux::match(line, "#EXTINF:")) {
                if (first == last) {
                    raise(errc::failure,
                          "'#EXTINF' tag must be followed by a "
                          "media segment URI");
                }

                auto const segment_duration = aux::parse_duration(line);
                auto const segment_location = net::uri::from_string(*first++);
                segments.emplace_back(segment_location.resolve(location),
                                      segment_duration);
            }
            else if (aux::match(line, "#EXT-X-PLAYLIST-TYPE:")) {
                is_live = (line != "VOD");
            }
        }

        if (version == 0) {
            raise(errc::failure,
                  "missing required attribute: '#EXT-X-VERSION'");
        }
        if (segments.empty()) {
            raise(errc::failure,
                  "media playlist contains no segments");
        }
    }

    bool has_codec(string_view const prefix) const noexcept
    {
        // FIXME: workaround to force selection of audio-only playlists.
#if 0
        for (auto&& codec : tokenize(codecs, ',')) {
            if (prefix.size() <= codec.size()) {
                if (mem::equal(codec.data(), prefix.data(), prefix.size())) {
                    return true;
                }
            }
        }
        return false;
#else
        if (codecs.find(',') == codecs.npos) {
            if (prefix.size() <= codecs.size()) {
                if (mem::equal(codecs.data(), prefix.data(), prefix.size())) {
                    return true;
                }
            }
        }
        return false;
#endif
    }

    net::uri                  location;
    std::vector<m3u::segment> segments;
    u8string                  codecs;
    uint64                    version{};
    bool                      is_live{};
};


class variant_playlist
{
public:
    explicit variant_playlist(ref_ptr<io::stream> s) :
        file{std::move(s)}
    {
        auto const& base_uri = file->location();
        auto const text = u8string::from_text_file(*file);
        auto const lines = tokenize(text, "\n");
        auto const last = lines.end();
        auto first = lines.begin();

        if (first == last || *first++ != "#EXTM3U") {
            raise(errc::failure, "not an extended M3U file");
        }

        while (first != last) {
            auto line = *first++;

            if (aux::match(line, "#EXT-X-STREAM-INF:")) {
                if (first == last) {
                    raise(errc::failure,
                          "'#EXT-X-STREAM-INF' tag must be "
                          "followed by a playlist URI");
                }

                auto const relative_uri = net::uri::from_string(*first++);
                m3u::media_playlist playlist{relative_uri.resolve(base_uri)};

                auto pos = line.find("CODECS=");
                if (pos != line.npos) {
                    line.remove_prefix(pos + 7);
                    playlist.codecs = aux::parse_quoted_string(line);
                }
                playlists.push_back(std::move(playlist));
            }
        }
    }

    media_playlist* find_by_codec(string_view const prefix) noexcept
    {
        for (auto&& playlist : playlists) {
            if (playlist.has_codec(prefix)) {
                return &playlist;
            }
        }
        return nullptr;
    }

private:
    ref_ptr<io::stream>              file;
    std::vector<m3u::media_playlist> playlists;
};

}}    // namespace amp::m3u


#endif  // AMP_INCLUDED_877D02FC_8FD5_4F5C_B000_513DC9CA8A83

