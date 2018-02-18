////////////////////////////////////////////////////////////////////////////////
//
// core/registry.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/output.hpp>
#include <amp/error.hpp>
#include <amp/flat_map.hpp>
#include <amp/intrusive/set.hpp>
#include <amp/intrusive/slist.hpp>
#include <amp/io/stream.hpp>
#include <amp/net/uri.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>

#include "core/aux/dynamic_library.hpp"
#include "core/filesystem.hpp"
#include "core/registry.hpp"

#include <cstddef>


namespace amp {
namespace {

uint32 register_count_{};

char const* file_extension_(u8string const& path) noexcept
{
    if (!path.empty()) {
        auto pos = path.find_last_of('.');
        if (pos < (path.size() - 1)) {
            return path.data() + pos + 1;
        }
    }
    return nullptr;
}

auto& resolvers_() noexcept
{
    static struct {
        flat_map<char const*, io::stream_factory*, stricmp_less> stream;
        flat_multimap<char const*, audio::input_factory*, stricmp_less> input;
        flat_multimap<uint32, audio::decoder_factory*> decoder;
    } instance;
    return instance;
}

}     // namespace <unnamed>


void load_plugins()
{
    for (auto&& path : fs::directory_range{"build/plugins"}) {
        if (fs::extension(path) == dynamic_library::file_extension()) {
            try {
                register_count_ = 0;
                dynamic_library dylib{path};
                if (register_count_ == 0) {
                    raise(errc::failure, "plugin contains no classes");
                }
                dylib.detach();
            }
            catch (std::exception const& ex) {
                std::fprintf(stderr, "warning: failed to load plugin: %s\n",
                             ex.what());
            }
        }
    }
}



namespace io {

void stream_factory::register_(char const* const* first,
                               char const* const* const last) noexcept
{
    auto& resolver = resolvers_().stream;
    while (first != last) {
        resolver.emplace(*first++, this);
    }
    ++register_count_;
}

ref_ptr<stream> open(net::uri const& location, io::open_mode const mode)
{
    auto const scheme = location.scheme();
    if (scheme.empty()) {
        raise(errc::invalid_argument,
              "cannot open stream with a relative URI");
    }

    auto const& resolver = resolvers_().stream;
    auto const found = resolver.find(scheme);
    if (found == resolver.end()) {
        raise(errc::protocol_not_supported,
              "no handler for URI scheme: \"%.*s\"",
              static_cast<int>(scheme.size()), scheme.data());
    }
    return found->second->create(location, mode);
}

}     // namespace io


namespace audio {

intrusive::set<audio::output_session_factory> output_factories;
intrusive::set<audio::filter_factory> filter_factories;
intrusive::slist<audio::resampler_factory> resampler_factories;


void decoder_factory::register_(uint32 const* first,
                                uint32 const* const last) noexcept
{
    auto& resolver = resolvers_().decoder;
    while (first != last) {
        resolver.emplace(*first++, this);
    }
    ++register_count_;
}

void input_factory::register_(char const* const* first,
                              char const* const* const last) noexcept
{
    auto& resolver = resolvers_().input;
    while (first != last) {
        resolver.emplace(*first++, this);
    }
    ++register_count_;
}

void output_session_factory::register_() noexcept
{
    output_factories.insert(*this);
    ++register_count_;
}

void filter_factory::register_() noexcept
{
    filter_factories.insert(*this);
    ++register_count_;
}

void resampler_factory::register_() noexcept
{
    resampler_factories.push_back(*this);
    ++register_count_;
}

ref_ptr<decoder> decoder::resolve(audio::codec_format& fmt)
{
    auto const found = resolvers_().decoder.equal_range(fmt.codec_id);
    if (found.first == found.second) {
        raise(errc::protocol_not_supported,
              "no audio decoder(s) for codec: '%s'",
              audio::codec::name(fmt.codec_id).c_str());
    }

    std::exception_ptr ep;
    for (auto&& entry : make_range(found)) {
        try {
            return entry.second->create(fmt);
        }
        catch (...) {
            ep = std::current_exception();
        }
    }
    std::rethrow_exception(ep);
}

ref_ptr<input> input::resolve(ref_ptr<io::stream> const file,
                              audio::open_mode const mode)
{
    auto const path = file->location().get_file_path();
    auto const extension = file_extension_(path);
    if (!extension) {
        raise(errc::invalid_argument,
              "cannot open audio input for a path with no extension");
    }

    auto const found = resolvers_().input.equal_range(extension);
    if (found.first == found.second) {
        raise(errc::protocol_not_supported,
              "no audio input for file extension: '%s'", extension);
    }

    std::exception_ptr ep;
    for (auto&& entry : make_range(found)) {
        try {
            return entry.second->create(file, mode);
        }
        catch (...) {
            ep = std::current_exception();
        }
        file->rewind();
    }
    std::rethrow_exception(ep);
}

ref_ptr<input> input::resolve(net::uri const& location,
                              audio::open_mode const mode)
{
    return input::resolve(io::open(location, io::in|io::binary), mode);
}

u8string get_input_file_filter()
{
    auto buf = u8string_buffer::from_utf8_unchecked("*.cue");

    auto const& resolver = resolvers_().input;
    auto const last = resolver.cend();
    for (auto first = resolver.cbegin(); first != last; ) {
        auto const key = first->first;
        buf.appendf(" *.%s", key);

        while (++first != last && stricmpeq(first->first, key)) {}
    }
    return buf.promote();
}

bool have_input_for(u8string const& path) noexcept
{
    if (auto const extension = file_extension_(path)) {
        auto const& resolver = resolvers_().input;
        return resolver.find(extension) != resolver.end();
    }
    return false;
}

}}    // namespace amp::audio

