////////////////////////////////////////////////////////////////////////////////
//
// media/playlist.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/flat_map.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/numeric.hpp>
#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "core/filesystem.hpp"
#include "media/playlist.hpp"
#include "media/tags.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#include <lz4.h>
#include <lz4hc.h>


namespace amp {
namespace media {
namespace {

constexpr auto playlist_magic = net::to_host<BE>("AMPL"_4cc);

struct playlist_header
{
    uint32 magic;
    uint16 version;
    uint16 flags;
    uint32 size;

    bool valid() const noexcept
    { return (magic == playlist_magic) && (version == 1) && (flags == 0); }
};


auto pack_playlist_(std::vector<media::track> const& tracks)
{
    io::buffer buf;
    auto pos = 0_sz;

    auto write_size = [&](std::size_t const n) {
        buf.grow(sizeof(uint32), uninitialized);
        io::store<LE>(&buf[pos], static_cast<uint32>(n));
        pos += sizeof(uint32);
    };

    auto write_data = [&](void const* const p, std::size_t const n) {
        buf.grow(sizeof(uint32) + n, uninitialized);
        io::store<LE>(&buf[pos], static_cast<uint32>(n));
        pos += sizeof(uint32);
        std::copy_n(static_cast<uchar const*>(p), n, &buf[pos]);
        pos += n;
    };

    write_size(tracks.size());
    for (auto&& t : tracks) {
        write_data(t.location.data(), t.location.size());

        write_size(t.tags.size());
        for (auto&& v : t.tags) {
            write_data(v.first.data(), v.first.size());
            write_data(v.second.data(), v.second.size());
        }

        write_size(t.info.size());
        for (auto&& v : t.info) {
            write_data(v.first.data(), v.first.size());
            write_data(v.second.data(), v.second.size());
        }

        auto const n = sizeof(t.start_offset)
                     + sizeof(t.frames)
                     + sizeof(t.sample_rate)
                     + sizeof(t.channel_layout)
                     + sizeof(t.chapter);

        buf.grow(n, uninitialized);
        io::scatter<LE>(&buf[pos],
                        t.start_offset,
                        t.frames,
                        t.sample_rate,
                        t.channel_layout,
                        t.chapter);
        pos += n;
    }
    return buf;
}

std::vector<media::track> unpack_playlist_(io::reader r)
{
    auto load_dictionary = [&](media::dictionary& d) {
        auto n = r.read<uint32,LE>();
        d.reserve(n);

        while (n-- != 0) {
            auto k = u8string::intern(r.read_pascal_string<uint32,LE>());
            auto v = u8string::intern(r.read_pascal_string<uint32,LE>());
            d.emplace_hint_no_intern(d.cend(), std::move(k), std::move(v));
        }
    };

    std::vector<media::track> tracks(r.read<uint32,LE>());
    for (auto&& t : tracks) {
        t.location = net::uri::from_string(r.read_pascal_string<uint32,LE>());
        load_dictionary(t.tags);
        load_dictionary(t.info);

        r.gather<LE>(t.start_offset,
                     t.frames,
                     t.sample_rate,
                     t.channel_layout,
                     t.chapter);
    }
    return tracks;
}


void save_playlist_(io::stream& file, std::vector<media::track> const& tracks)
{
    auto const buf = pack_playlist_(tracks);
    auto const decompressed_size = static_cast<uint32>(buf.size());
    auto const max_compressed_size = LZ4_COMPRESSBOUND(buf.size());
    AMP_ASSERT(max_compressed_size > 0);

    io::buffer compressed{max_compressed_size, uninitialized};
    auto const ret = ::LZ4_compress_HC(
        reinterpret_cast<char const*>(buf.data()),
        reinterpret_cast<char*>(compressed.data()),
        static_cast<int>(decompressed_size),
        static_cast<int>(max_compressed_size),
        0);

    if (AMP_UNLIKELY(ret <= 0)) {
        raise(errc::failure, "LZ4 compression failed (code: %d)", ret);
    }

    playlist_header const head{playlist_magic, 1, 0, decompressed_size};
    file.scatter<LE>(head.magic, head.version, head.flags, head.size);

    auto const compressed_size = static_cast<uint32>(ret);
    AMP_ASSERT(compressed_size <= max_compressed_size);
    file.write(compressed.data(), compressed_size);
}

std::vector<media::track> load_playlist_(u8string const& path)
{
    if (!fs::exists(path)) {
        return {};
    }

    auto const file = io::open(net::uri::from_file_path(path),
                               io::in|io::binary);
    playlist_header head;
    file->gather<LE>(head.magic, head.version, head.flags, head.size);

    if (AMP_UNLIKELY(!head.valid())) {
        raise(errc::failure, "invalid AMP playlist");
    }

    io::buffer buf{head.size, uninitialized};
    io::buffer tmp{*file, numeric_cast<std::size_t>(file->remain())};

    auto const ret = ::LZ4_decompress_safe(
        reinterpret_cast<char const*>(tmp.data()),
        reinterpret_cast<char*>(buf.data()),
        static_cast<int>(tmp.size()),
        static_cast<int>(buf.size()));

    if (ret <= 0) {
        raise(errc::failure, "LZ4 decompression failed (code: %d)", ret);
    }
    if (static_cast<uint32>(ret) != head.size) {
        raise(errc::failure, "invalid decompressed size");
    }
    return unpack_playlist_(buf);
}

}     // namespace <unnamed>


ref_ptr<playlist> playlist::make(u8string p, uint32 const id)
{
    return ref_ptr<playlist>::consume(new playlist{std::move(p), id});
}

playlist::playlist(u8string p, uint32 const id) :
    path_{std::move(p)},
    tracks_{load_playlist_(path_)},
    position_{0},
    id_{id},
    ref_count_{1},
    gen_order_{media::playback_order::linear},
    unsaved_changes_{false}
{
}

playlist::size_type playlist::gen_position_(size_type pos, bool const forward)
{
    AMP_ASSERT(!empty());

    switch (gen_order_) {
    case media::playback_order::linear:
        if (forward) {
            return (++pos < size()) ? pos : 0;
        }
        else {
            return (--pos > 0) ? pos : (size() - 1);
        }
    case media::playback_order::repeat:
        return pos;
    case media::playback_order::random:
        static thread_local std::mt19937 urng{std::random_device{}()};
        return std::uniform_int_distribution<size_type>{0, size() - 1}(urng);
    }
}

void playlist::sort(std::string_view const key, media::sort_order const order)
{
    std::stable_sort(
        tracks_.begin(),
        tracks_.end(),
        [=](media::track const& x, media::track const& y) {
            auto const ret = tags::compare(x, y, key);
            return (sgn(ret) == static_cast<int>(order));
        });

    unsaved_changes_ = true;
}

void playlist::save()
{
    if (unsaved_changes_) {
        auto const file = io::open(net::uri::from_file_path(path_),
                                   io::out|io::trunc|io::binary);
        save_playlist_(*file, tracks_);
        unsaved_changes_ = false;
    }
}

void playlist::remove()
{
    fs::remove(path_);
    unsaved_changes_ = false;
}


void playlist_index::load(u8string const& path)
{
    auto const file = io::open(net::uri::from_file_path(path),
                               io::in|io::binary);

    uint32 entry_count;
    file->gather<LE>(entry_count, selection);

    entries.resize(entry_count);
    for (auto&& entry : entries) {
        uint32 len;
        file->gather<LE>(entry.uid, entry.pos, len);

        u8string_buffer buf(len, uninitialized);
        file->read(buf.data(), buf.size());
        entry.name = buf.promote();
    }
}

void playlist_index::save(u8string const& path) const
{
    auto const file = io::open(net::uri::from_file_path(path),
                               io::out|io::trunc|io::binary);

    file->scatter<LE>(static_cast<uint32>(entries.size()), selection);

    for (auto&& entry : entries) {
        file->scatter<LE>(entry.uid, entry.pos,
                          static_cast<uint32>(entry.name.size()));
        file->write(entry.name.data(), entry.name.size());
    }
}

}}    // namespace amp::media

