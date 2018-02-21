////////////////////////////////////////////////////////////////////////////////
//
// amp/media/tags.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4
#define AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4


#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <cstddef>
#include <string_view>


namespace amp {
namespace tags {

AMP_EXPORT
u8string map_common_key(std::string_view);


using namespace ::std::literals;

constexpr auto bit_rate               = "bit rate"sv;
constexpr auto bits_per_sample        = "bits per sample"sv;
constexpr auto channel_layout         = "channel layout"sv;
constexpr auto channels               = "channels"sv;
constexpr auto codec                  = "codec"sv;
constexpr auto codec_profile          = "codec profile"sv;
constexpr auto container              = "container"sv;
constexpr auto tag_type               = "tag type"sv;

constexpr auto rg_album_gain          = "replaygain album gain"sv;
constexpr auto rg_album_peak          = "replaygain album peak"sv;
constexpr auto rg_track_gain          = "replaygain track gain"sv;
constexpr auto rg_track_peak          = "replaygain track peak"sv;

constexpr auto mb_album_id            = "musicbrainz album id"sv;
constexpr auto mb_album_artist_id     = "musicbrainz album artist id"sv;
constexpr auto mb_artist_id           = "musicbrainz artist id"sv;
constexpr auto mb_disc_id             = "musicbrainz disc id"sv;
constexpr auto mb_release_country     = "musicbrainz release country"sv;
constexpr auto mb_release_group_id    = "musicbrainz release group id"sv;
constexpr auto mb_track_id            = "musicbrainz track id"sv;
constexpr auto mb_work_id             = "musicbrainz work id"sv;

constexpr auto acoustid_id            = "acoustid id"sv;
constexpr auto acoustid_fingerprint   = "acoustid fingerprint"sv;

constexpr auto musicip_puid           = "musicip puid"sv;

constexpr auto album                  = "album"sv;
constexpr auto album_artist           = "album artist"sv;
constexpr auto album_artist_sort      = "album artist sort"sv;
constexpr auto album_sort             = "album sort"sv;
constexpr auto artist                 = "artist"sv;
constexpr auto artist_sort            = "artist sort"sv;
constexpr auto artist_web_page        = "artist web page"sv;
constexpr auto asin                   = "asin"sv;
constexpr auto audio_source_web_page  = "audio source web page"sv;
constexpr auto author                 = "author"sv;
constexpr auto barcode                = "barcode"sv;
constexpr auto bpm                    = "bpm"sv;
constexpr auto catalog_number         = "catalog number"sv;
constexpr auto comment                = "comment"sv;
constexpr auto commercial_information = "commercial information"sv;
constexpr auto compilation            = "compilation"sv;
constexpr auto composer               = "composer"sv;
constexpr auto composer_sort          = "composer sort"sv;
constexpr auto conductor              = "conductor"sv;
constexpr auto contact                = "contact"sv;
constexpr auto copyright              = "copyright"sv;
constexpr auto copyright_information  = "copyright information"sv;
constexpr auto creation_date          = "creation date"sv;
constexpr auto cue_sheet              = "cue sheet"sv;
constexpr auto date                   = "date"sv;
constexpr auto description            = "description"sv;
constexpr auto disc_id                = "disc id"sv;
constexpr auto disc_number            = "disc number"sv;
constexpr auto disc_total             = "disc total"sv;
constexpr auto emphasis               = "emphasis"sv;
constexpr auto encoded_by             = "encoded by"sv;
constexpr auto encoder                = "encoder"sv;
constexpr auto encoding_settings      = "encoding settings"sv;
constexpr auto encoding_time          = "encoding time"sv;
constexpr auto engineer               = "engineer"sv;
constexpr auto file_type              = "file type"sv;
constexpr auto file_web_page          = "file web page"sv;
constexpr auto gapless_album          = "gapless album"sv;
constexpr auto genre                  = "genre"sv;
constexpr auto group                  = "group"sv;
constexpr auto initial_key            = "initial key"sv;
constexpr auto isrc                   = "isrc"sv;
constexpr auto label                  = "label"sv;
constexpr auto language               = "language"sv;
constexpr auto license                = "license"sv;
constexpr auto location               = "location"sv;
constexpr auto lyricist               = "lyricist"sv;
constexpr auto lyrics                 = "lyrics"sv;
constexpr auto media_type             = "media type"sv;
constexpr auto mixer                  = "mixer"sv;
constexpr auto modification_date      = "modification date"sv;
constexpr auto mood                   = "mood"sv;
constexpr auto orchestra              = "orchestra"sv;
constexpr auto original_album         = "original album"sv;
constexpr auto original_artist        = "original artist"sv;
constexpr auto original_date          = "original date"sv;
constexpr auto original_filename      = "original file name"sv;
constexpr auto original_lyricist      = "original lyricist"sv;
constexpr auto owner                  = "owner"sv;
constexpr auto payment_web_page       = "payment web page"sv;
constexpr auto performer              = "performer"sv;
constexpr auto play_counter           = "play counter"sv;
constexpr auto playlist_delay         = "playlist delay"sv;
constexpr auto podcast                = "podcast"sv;
constexpr auto produced_notice        = "produced notice"sv;
constexpr auto producer               = "producer"sv;
constexpr auto publisher_web_page     = "publisher web page"sv;
constexpr auto radio_station          = "radio station"sv;
constexpr auto radio_station_owner    = "radio station owner"sv;
constexpr auto radio_station_web_page = "radio station web page"sv;
constexpr auto rating                 = "rating"sv;
constexpr auto remixer                = "remixer"sv;
constexpr auto subtitle               = "subtitle"sv;
constexpr auto tagging_date           = "tagging date"sv;
constexpr auto title                  = "title"sv;
constexpr auto title_sort             = "title sort"sv;
constexpr auto track_number           = "track number"sv;
constexpr auto track_total            = "track total"sv;
constexpr auto upc                    = "upc"sv;
constexpr auto user_web_page          = "user web page"sv;
constexpr auto writer                 = "writer"sv;

}}    // namespace amp::tags


#endif  // AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4

