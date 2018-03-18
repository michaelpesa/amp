////////////////////////////////////////////////////////////////////////////////
//
// media/cue_sheet.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>

#include "media/cue_sheet.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>


namespace amp {
namespace cue {
namespace {

[[noreturn]] void raise(char const* const msg)
{
    amp::raise(errc::failure, "cue sheet: %s", msg);
}


void verify_file_type(std::string_view const type)
{
    if (!stricmpeq(type, "WAVE") &&
        !stricmpeq(type, "AIFF") &&
        !stricmpeq(type, "MP3")  &&
        !stricmpeq(type, "APE")  &&
        !stricmpeq(type, "FLAC") &&
        !stricmpeq(type, "WV")   &&
        !stricmpeq(type, "WAVPACK")) {
        cue::raise("invalid file type");
    }
}

auto parse_length(std::string_view const text)
{
    uint mm, ss, ff;
    if (std::sscanf(text.data(), "%u:%u:%u", &mm, &ss, &ff) == 3) {
        if (ss < 60 && ff < 75) {
            return std::chrono::minutes{mm}
                 + std::chrono::seconds{ss}
                 + cue::frames{ff};
        }
    }
    cue::raise("invalid time syntax");
}

auto parse_number(std::string_view const text)
{
    uint number;
    if (std::sscanf(text.data(), "%u", &number) == 1) {
        return number;
    }
    cue::raise("invalid syntax");
}

void trim_whitespace(std::string_view& s,
                     std::string_view::size_type pos = 0) noexcept
{
    pos = s.find_first_not_of(" \t", pos);
    s.remove_prefix(std::min(pos, s.size()));
}

auto read_token(std::string_view& line) noexcept
{
    auto const pos = line.find_first_of(" \t");
    auto const ret = line.substr(0, pos);

    trim_whitespace(line, pos);
    return ret;
}

auto read_string(std::string_view& line)
{
    std::string_view::size_type pos;
    std::string_view ret;

    if (line.size() >= 2 && line.front() == '\"') {
        pos = line.find('\"', 1);
        if (pos == line.npos) {
            cue::raise("invalid syntax");
        }
        ret = {line.data() + 1, pos - 1};
        pos = pos + 1;
    }
    else {
        pos = line.find_first_of(" \t");
        ret = line.substr(0, pos);
    }

    trim_whitespace(line, pos);
    return ret;
}

auto maybe_unquote(std::string_view s)
{
    if (s.size() >= 2 && s.front() == '\"') {
        if (s.back() != '\"') {
            cue::raise("invalid syntax");
        }
        s.remove_prefix(1);
        s.remove_suffix(1);
    }
    return s;
}


class parser
{
public:
    explicit parser(u8string);

    std::vector<cue::track> tracks;

private:
    void parse_line(std::string_view);
    void on_file(std::string_view, std::string_view);
    void on_track(uint, std::string_view);
    void on_index(uint, cue::frames);
    void on_pregap(cue::frames);
    void on_postgap(cue::frames);
    void on_isrc(std::string_view);
    void on_flags(std::string_view);
    void on_comment(std::string_view, u8string);

    void commit_track();

    struct {
        cue::frames index[100];
        cue::frames pregap;
        cue::frames postgap;
        std::string_view file;
        media::dictionary tags;
        uint number{};
        uint index_count;

        explicit operator bool() const noexcept
        {
            return (number != 0);
        }

        void reset(std::string_view const f, uint const n) noexcept
        {
            pregap = cue::frames::zero();
            postgap = cue::frames::zero();
            file = f;
            number = n;
            index_count = 0;
        }
    }
    current_track;

    std::string_view current_file;
    media::dictionary global_tags;
    cue::frames last_index_offset;
    bool various_artists{};
};


inline parser::parser(u8string text)
{
    for (auto line : tokenize(text, "\r\n")) {
        trim_whitespace(line);
        if (!line.empty()) {
            parse_line(line);
        }
    }

    if (!current_track) {
        cue::raise("must contain at least one track");
    }
    commit_track();
    global_tags.emplace(tags::track_total, to_u8string(current_track.number));
    global_tags.emplace(tags::cue_sheet, std::move(text));

    for (auto&& track : tracks) {
        track.tags.merge(global_tags);
    }
}

inline void parser::commit_track()
{
    AMP_ASSERT(current_track);
    if (current_track.index_count < 2) {
        cue::raise("missing 'INDEX 01'");
    }

    auto& t = tracks.emplace_back();
    t.start = current_track.index[1];
    t.file = current_track.file;
    t.tags = std::move(current_track.tags);
    t.tags.emplace(tags::track_number, to_u8string(current_track.number));
}

inline void parser::parse_line(std::string_view line)
{
    auto const cmd = read_token(line);
    if (stricmpeq(cmd, "CATALOG")   ||
        stricmpeq(cmd, "PERFORMER") ||
        stricmpeq(cmd, "TITLE")     ||
        stricmpeq(cmd, "SONGWRITER")) {
        on_comment(cmd, maybe_unquote(line));
    }
    else if (stricmpeq(cmd, "REM")) {
        auto const key = read_token(line);
        on_comment(key, maybe_unquote(line));
    }
    else if (stricmpeq(cmd, "FILE")) {
        auto const file = read_string(line);
        on_file(file, read_token(line));
    }
    else if (stricmpeq(cmd, "INDEX")) {
        auto const number = parse_number(read_token(line));
        auto const offset = parse_length(read_token(line));
        on_index(number, offset);
    }
    else if (stricmpeq(cmd, "TRACK")) {
        auto const number = parse_number(read_token(line));
        on_track(number, line);
    }
    else if (stricmpeq(cmd, "PREGAP")) {
        on_pregap(parse_length(read_token(line)));
    }
    else if (stricmpeq(cmd, "POSTGAP")) {
        on_postgap(parse_length(read_token(line)));
    }
    else if (stricmpeq(cmd, "FLAGS")) {
        on_flags(line);
    }
    else if (stricmpeq(cmd, "ISRC")) {
        on_isrc(line);
    }
    else if (stricmpeq(cmd, "CDTEXTFILE")) {
        // ignore
    }
    else {
        cue::raise("invalid command");
    }
}

inline void parser::on_file(std::string_view const file,
                            std::string_view const type)
{
    verify_file_type(type);
    current_file = file;
    last_index_offset = cue::frames::max();
}

inline void parser::on_track(uint const number, std::string_view const type)
{
    if (current_file.empty()) {
        cue::raise("track cannot appear before file");
    }

    if (number < 1 || number > 99) {
        cue::raise("invalid track number");
    }
    if (!stricmpeq(type, "AUDIO")) {
        cue::raise("invalid track type");
    }

    if (current_track) {
        if (number != (current_track.number + 1)) {
            cue::raise("track numbers must be in ascending order");
        }
        commit_track();
    }
    current_track.reset(current_file, number);
}

inline void parser::on_index(uint const number, cue::frames const offset)
{
    if (!current_track) {
        cue::raise("index cannot occur before track");
    }
    if (last_index_offset != cue::frames::max()) {
        if (last_index_offset >= offset) {
            cue::raise("index times must be in ascending order");
        }
    }
    else {
        if (offset != cue::frames::zero()) {
            cue::raise("first index of file must be zero");
        }
    }
    last_index_offset = offset;

    if (current_track.postgap != cue::frames::zero()) {
        cue::raise("postgap must occur before index");
    }

    if (number == 1 && current_track.index_count == 0) {
        current_track.index[current_track.index_count++] = offset;
    }
    else if (number != current_track.index_count || number > 99) {
        cue::raise("sheet track number");
    }
    current_track.index[current_track.index_count++] = offset;
}

inline void parser::on_postgap(cue::frames const length)
{
    if (!current_track) {
        cue::raise("postgap must occur after track");
    }
    current_track.postgap = length;
}

inline void parser::on_pregap(cue::frames const length)
{
    if (!current_track || (current_track.index_count != 0)) {
        cue::raise("pregap must appear before track index");
    }
    current_track.pregap = length;
}

inline void parser::on_isrc(std::string_view const text)
{
    if (!current_track || (current_track.index_count != 0)) {
        cue::raise("isrc must appear before track index");
    }
    current_track.tags.emplace(tags::isrc, text);
}

inline void parser::on_flags(std::string_view const text)
{
    if (!current_track || (current_track.index_count != 0)) {
        cue::raise("flags must appear before track index");
    }
    (void) text;
}

inline void parser::on_comment(std::string_view const name, u8string value)
{
    auto key = [&]() -> u8string {
        if (stricmpeq(name, "PERFORMER")) {
            if (current_track && !various_artists) {
                auto found = global_tags.find(tags::artist);
                if (found != global_tags.end() && (value != found->second)) {
                    global_tags.emplace(tags::album_artist, found->second);
                    various_artists = true;
                }
            }
            return tags::artist;
        }
        if (stricmpeq(name, "TITLE")) {
            return (current_track ? tags::title : tags::album);
        }
        return tags::map_common_key(name);
    }();

    auto&& tags = (current_track ? current_track.tags : global_tags);
    tags.emplace(std::move(key), std::move(value));
}

}     // namespace <unnamed>

std::vector<cue::track> parse(u8string text)
{
    return cue::parser{std::move(text)}.tracks;
}

}}    // namespace amp::cue

