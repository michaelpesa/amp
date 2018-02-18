////////////////////////////////////////////////////////////////////////////////
//
// amp/media/tags.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4
#define AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4


#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>

#include <cstddef>


namespace amp {
namespace tags {

AMP_EXPORT
u8string map_common_key(string_view);


constexpr auto bit_rate               = "bit rate"_sv;
constexpr auto bits_per_sample        = "bits per sample"_sv;
constexpr auto channel_layout         = "channel layout"_sv;
constexpr auto channels               = "channels"_sv;
constexpr auto codec                  = "codec"_sv;
constexpr auto codec_profile          = "codec profile"_sv;
constexpr auto container              = "container"_sv;
constexpr auto tag_type               = "tag type"_sv;

constexpr auto rg_album_gain          = "replaygain album gain"_sv;
constexpr auto rg_album_peak          = "replaygain album peak"_sv;
constexpr auto rg_track_gain          = "replaygain track gain"_sv;
constexpr auto rg_track_peak          = "replaygain track peak"_sv;

constexpr auto mb_album_id            = "musicbrainz album id"_sv;
constexpr auto mb_album_artist_id     = "musicbrainz album artist id"_sv;
constexpr auto mb_artist_id           = "musicbrainz artist id"_sv;
constexpr auto mb_disc_id             = "musicbrainz disc id"_sv;
constexpr auto mb_release_country     = "musicbrainz release country"_sv;
constexpr auto mb_release_group_id    = "musicbrainz release group id"_sv;
constexpr auto mb_track_id            = "musicbrainz track id"_sv;
constexpr auto mb_work_id             = "musicbrainz work id"_sv;

constexpr auto acoustid_id            = "acoustid id"_sv;
constexpr auto acoustid_fingerprint   = "acoustid fingerprint"_sv;

constexpr auto musicip_puid           = "musicip puid"_sv;

constexpr auto album                  = "album"_sv;
constexpr auto album_artist           = "album artist"_sv;
constexpr auto album_artist_sort      = "album artist sort"_sv;
constexpr auto album_sort             = "album sort"_sv;
constexpr auto artist                 = "artist"_sv;
constexpr auto artist_sort            = "artist sort"_sv;
constexpr auto artist_web_page        = "artist web page"_sv;
constexpr auto asin                   = "asin"_sv;
constexpr auto audio_source_web_page  = "audio source web page"_sv;
constexpr auto author                 = "author"_sv;
constexpr auto barcode                = "barcode"_sv;
constexpr auto bpm                    = "bpm"_sv;
constexpr auto catalog_number         = "catalog number"_sv;
constexpr auto comment                = "comment"_sv;
constexpr auto commercial_information = "commercial information"_sv;
constexpr auto compilation            = "compilation"_sv;
constexpr auto composer               = "composer"_sv;
constexpr auto composer_sort          = "composer sort"_sv;
constexpr auto conductor              = "conductor"_sv;
constexpr auto contact                = "contact"_sv;
constexpr auto copyright              = "copyright"_sv;
constexpr auto copyright_information  = "copyright information"_sv;
constexpr auto creation_date          = "creation date"_sv;
constexpr auto cue_sheet              = "cue sheet"_sv;
constexpr auto date                   = "date"_sv;
constexpr auto description            = "description"_sv;
constexpr auto disc_id                = "disc id"_sv;
constexpr auto disc_number            = "disc number"_sv;
constexpr auto disc_total             = "disc total"_sv;
constexpr auto emphasis               = "emphasis"_sv;
constexpr auto encoded_by             = "encoded by"_sv;
constexpr auto encoder                = "encoder"_sv;
constexpr auto encoding_settings      = "encoding settings"_sv;
constexpr auto encoding_time          = "encoding time"_sv;
constexpr auto engineer               = "engineer"_sv;
constexpr auto file_type              = "file type"_sv;
constexpr auto file_web_page          = "file web page"_sv;
constexpr auto gapless_album          = "gapless album"_sv;
constexpr auto genre                  = "genre"_sv;
constexpr auto group                  = "group"_sv;
constexpr auto initial_key            = "initial key"_sv;
constexpr auto isrc                   = "isrc"_sv;
constexpr auto label                  = "label"_sv;
constexpr auto language               = "language"_sv;
constexpr auto license                = "license"_sv;
constexpr auto location               = "location"_sv;
constexpr auto lyricist               = "lyricist"_sv;
constexpr auto lyrics                 = "lyrics"_sv;
constexpr auto media_type             = "media type"_sv;
constexpr auto mixer                  = "mixer"_sv;
constexpr auto modification_date      = "modification date"_sv;
constexpr auto mood                   = "mood"_sv;
constexpr auto orchestra              = "orchestra"_sv;
constexpr auto original_album         = "original album"_sv;
constexpr auto original_artist        = "original artist"_sv;
constexpr auto original_date          = "original date"_sv;
constexpr auto original_filename      = "original file name"_sv;
constexpr auto original_lyricist      = "original lyricist"_sv;
constexpr auto owner                  = "owner"_sv;
constexpr auto payment_web_page       = "payment web page"_sv;
constexpr auto performer              = "performer"_sv;
constexpr auto play_counter           = "play counter"_sv;
constexpr auto playlist_delay         = "playlist delay"_sv;
constexpr auto podcast                = "podcast"_sv;
constexpr auto produced_notice        = "produced notice"_sv;
constexpr auto producer               = "producer"_sv;
constexpr auto publisher_web_page     = "publisher web page"_sv;
constexpr auto radio_station          = "radio station"_sv;
constexpr auto radio_station_owner    = "radio station owner"_sv;
constexpr auto radio_station_web_page = "radio station web page"_sv;
constexpr auto rating                 = "rating"_sv;
constexpr auto remixer                = "remixer"_sv;
constexpr auto subtitle               = "subtitle"_sv;
constexpr auto tagging_date           = "tagging date"_sv;
constexpr auto title                  = "title"_sv;
constexpr auto title_sort             = "title sort"_sv;
constexpr auto track_number           = "track number"_sv;
constexpr auto track_total            = "track total"_sv;
constexpr auto upc                    = "upc"_sv;
constexpr auto user_web_page          = "user web page"_sv;
constexpr auto writer                 = "writer"_sv;

}}    // namespace amp::tags


#endif  // AMP_INCLUDED_A30B2407_797C_468E_9D49_CF093BFD84D4

