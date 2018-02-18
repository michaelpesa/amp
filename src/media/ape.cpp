////////////////////////////////////////////////////////////////////////////////
//
// media/ape.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/cxp/char.hpp>
#include <amp/cxp/string.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/ape.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/memory.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>


namespace amp {
namespace ape {
namespace {

constexpr auto header_size = 32_sz;

struct header
{
    char preamble[8];
    uint32 version;
    uint32 size;
    uint32 items;
    uint32 flags;
    uint64 reserved;

    bool read(io::stream& file)
    {
        constexpr char const apetagex[] { 'A','P','E','T','A','G','E','X' };

        file.gather<LE>(preamble, version, size, items, flags, reserved);
        return mem::equal(preamble, apetagex) && validate_no_preamble_();
    }

    bool read_no_preamble(io::reader& r) noexcept
    {
        if (r.size() >= (ape::header_size - 8)) {
            r.gather_unchecked<LE>(version, size, items, flags, reserved);
            return validate_no_preamble_();
        }
        return false;
    }

private:
    bool validate_no_preamble_() const noexcept
    {
        return (size >= ape::header_size)
            && (reserved == 0)
            && ((version == 1000 && !(flags & 0xffffffff)) ||
                (version == 2000 && !(flags & 0x1ffffff8)));
    }
};


bool find_footer_(io::stream& file, ape::header& footer)
{
    file.seek(-int64{ape::header_size}, io::seekdir::end);
    if (!footer.read(file)) {
        file.seek(-int64{ape::header_size} - 128, io::seekdir::end);
        if (!footer.read(file)) {
            return false;
        }
    }

    file.seek(-int64{footer.size}, file.cur);
    return true;
}


struct item
{
    explicit item(io::reader& r)
    {
        uint32 size;
        r.gather<LE>(size, flags);
        key = reinterpret_cast<char const*>(r.peek());

        auto const max_key_size = std::min(r.remain(), 256_sz);
        auto const key_size = cxp::strlen(key, max_key_size);

        if (key_size < 2 ||
            key_size == max_key_size ||
            !std::all_of(key, key + key_size, cxp::isprint)) {
            raise(errc::failure, "invalid APETag item key");
        }

        r.skip_unchecked(key_size + 1);
        value = {reinterpret_cast<char const*>(r.read_n(size)), size};
    }

    bool is_text() const noexcept
    { return ((flags >> 1) & 0x3) == 0x0; }

    bool is_binary() const noexcept
    { return ((flags >> 1) & 0x3) == 0x1; }

    //bool is_external() const noexcept
    //{ return ((flags >> 1) & 0x3) == 0x2; }

    char const* key;
    string_view value;
    uint32 flags;
};

void read_(ape::header const& header, io::reader r, media::dictionary& dict)
{
    for ([[maybe_unused]] auto _ : xrange(header.items)) {
        ape::item const item{r};
        if (!item.is_text()) {
            continue;
        }

        auto const key = tags::map_common_key(item.key);
        for (auto const& value : tokenize(item.value, '\0')) {
            dict.emplace(key, u8string::from_utf8_lossy(value));
        }
    }

    auto const version = (header.version == 1000 ? 1 : 2);
    dict.emplace(tags::tag_type, u8format("APEv%d", version));
}

}     // namespace <unnamed>


bool find(io::stream& file)
{
    ape::header footer;
    return ape::find_footer_(file, footer);
}

void read(io::stream& file, media::dictionary& dict)
{
    ape::header footer;
    if (ape::find_footer_(file, footer)) {
        io::buffer buf{file, footer.size};
        ape::read_(footer, buf, dict);
    }
}

void read_no_preamble(void const* const data, std::size_t const size,
                      media::dictionary& dict)
{
    io::reader r{data, size};
    ape::header header;
    if (header.read_no_preamble(r)) {
        ape::read_(header, r, dict);
    }
}

media::image find_image(io::stream& file, media::image_type const type)
{
    ape::header footer;
    if (!ape::find_footer_(file, footer)) {
        return {};
    }

    char const* key;
    if (type == media::image_type::front_cover) {
        key = "cover art (front)";
    }
    else if (type == media::image_type::back_cover) {
        key = "cover art (back)";
    }
    else {
        return {};
    }

    io::buffer buf{file, footer.size};
    io::reader r{buf};

    for ([[maybe_unused]] auto _ : xrange(footer.items)) {
        ape::item const item{r};
        if (!item.is_binary() || !stricmpeq(item.key, key)) {
            continue;
        }

        auto found = item.value.find('\0');
        if (found != item.value.npos) {
            found += 1;
            return media::image(item.value.data() + found,
                                item.value.size() - found);
        }
    }
    return {};
}

}}    // namespace amp::ape

