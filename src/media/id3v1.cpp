////////////////////////////////////////////////////////////////////////////////
//
// media/id3/v1/tag.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/io/stream.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <array>
#include <cstddef>


namespace amp {
namespace id3v1 {
namespace {

constexpr std::array<char const*, 148> genre_names {{
    "Blues",
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "Alternative Rock",
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",
    "New Wave",
    "Psychedelic",
    "Rave",
    "Showtunes",
    "Trailer",
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",
    "Folk-Rock",
    "National Folk",
    "Swing",
    "Fast Fusion",
    "Bebob",
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Acoustic",
    "Humour",
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",
    "Ballad",
    "Power Ballad",
    "Rhythmic Soul",
    "Freestyle",
    "Duet",
    "Punk Rock",
    "Drum Solo",
    "A Cappella",
    "Euro-House",
    "Dance Hall",
    "Goa Trance",
    "Drum & Bass",
    "Club-House",
    "Hardcore Techno",
    "Terror",
    "Indie",
    "Britpop",
    "Afro-Punk",
    "Polsk Punk",
    "Beat",
    "Christian Gangsta Rap",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary Christian",
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Thrash Metal",
    "Anime",
    "J-pop",
    "Synthpop",
}};


bool is_valid_tag(uchar const(&buf)[128]) noexcept
{
    return (buf[0] == 'T') && (buf[1] == 'A') && (buf[2] == 'G');
}

auto read_string(uchar const* const buf, std::size_t n)
{
    auto const s = reinterpret_cast<char const*>(buf);
    for (; n != 0 && s[n - 1] == '\0'; --n) {}
    for (; n != 0 && s[n - 1] ==  ' '; --n) {}
    return u8string::from_latin1(s, n);
}

}     // namespace <unnamed>


u8string get_genre_name(uint8 const index)
{
    return (index < id3v1::genre_names.size())
         ? u8string::from_utf8_unchecked(id3v1::genre_names[index])
         : u8string{};
}

bool find(io::stream& file)
{
    uchar buf[128];
    file.seek(-int64{sizeof(buf)}, file.end);

    if (file.try_read(buf, sizeof(buf)) == sizeof(buf)) {
        if (id3v1::is_valid_tag(buf)) {
            file.seek(-int64{sizeof(buf)}, file.end);
            return true;
        }
    }
    return false;
}

void read(io::stream& file, media::dictionary& dict)
{
    uchar buf[128];
    file.read(buf, sizeof(buf));

    if (id3v1::is_valid_tag(buf)) {
        dict.emplace(tags::title,    id3v1::read_string(&buf[ 3], 30));
        dict.emplace(tags::artist,   id3v1::read_string(&buf[33], 30));
        dict.emplace(tags::album,    id3v1::read_string(&buf[63], 30));
        dict.emplace(tags::date,     id3v1::read_string(&buf[93],  4));
        dict.emplace(tags::comment,  id3v1::read_string(&buf[97], 30));
        dict.emplace(tags::genre,    id3v1::get_genre_name(buf[127]));
        dict.emplace(tags::tag_type, u8string::from_utf8_unchecked("ID3v1"));

        if (buf[125] == 0 && buf[126] != 0) {
            dict.emplace(tags::track_number, to_u8string(buf[126]));
        }
    }
}

}}    // namespace amp::id3v1

