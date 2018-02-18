////////////////////////////////////////////////////////////////////////////////
//
// plugins/flac/input.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/error.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <memory>
#include <utility>

#include <FLAC/callback.h>
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_decoder.h>


namespace std {

template<>
struct default_delete<FLAC__StreamDecoder>
{
    void operator()(FLAC__StreamDecoder* const decoder) const noexcept
    { FLAC__stream_decoder_delete(decoder); }
};

template<>
struct default_delete<FLAC__Metadata_Iterator>
{
    void operator()(FLAC__Metadata_Iterator* const iter) const noexcept
    { FLAC__metadata_iterator_delete(iter); }
};

template<>
struct default_delete<FLAC__Metadata_Chain>
{
    void operator()(FLAC__Metadata_Chain* const chain) const noexcept
    { FLAC__metadata_chain_delete(chain); }
};

}     // namespace std


namespace amp {
namespace flac {
namespace {

class metadata_chain
{
public:
    class iterator
    {
    public:
        using value_type        = FLAC__StreamMetadata;
        using pointer           = FLAC__StreamMetadata*;
        using reference         = FLAC__StreamMetadata&;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        iterator() noexcept :
            iter{}
        {}

        iterator& operator++()
        {
            if (!FLAC__metadata_iterator_next(iter)) {
                iter = nullptr;
            }
            return *this;
        }

        pointer operator->() const
        { return FLAC__metadata_iterator_get_block(iter); }

        reference operator*() const
        { return *operator->(); }

        bool operator==(iterator const& x) const noexcept
        { return (iter == x.iter); }

        bool operator!=(iterator const& x) const noexcept
        { return !(*this == x); }

    private:
        friend class metadata_chain;

        explicit iterator(FLAC__Metadata_Iterator* const x) noexcept :
            iter{x}
        {}

        FLAC__Metadata_Iterator* iter;
    };

    iterator begin() const
    { return iterator{first.get()}; }

    iterator end() const noexcept
    { return iterator{};  }

    explicit metadata_chain(io::stream& file, bool const is_ogg) :
        chain{FLAC__metadata_chain_new()},
        first{FLAC__metadata_iterator_new()}
    {
        if (AMP_UNLIKELY(!chain || !first)) {
            raise_bad_alloc();
        }

        static constexpr FLAC__IOCallbacks iocb {
            metadata_chain::read,
            metadata_chain::write,
            metadata_chain::seek,
            metadata_chain::tell,
            metadata_chain::eof,
            nullptr,
        };

        auto const read = is_ogg
                        ? &FLAC__metadata_chain_read_ogg_with_callbacks
                        : &FLAC__metadata_chain_read_with_callbacks;
        if (AMP_UNLIKELY(!(*read)(chain.get(), &file, iocb))) {
            raise(errc::failure, "failed to read FLAC stream metadata");
        }

        //FLAC__metadata_chain_merge_padding(chain.get());
        FLAC__metadata_iterator_init(first.get(), chain.get());
    }

private:
    static remove_pointer_t<FLAC__IOCallback_Read>  read;
    static remove_pointer_t<FLAC__IOCallback_Write> write;
    static remove_pointer_t<FLAC__IOCallback_Seek>  seek;
    static remove_pointer_t<FLAC__IOCallback_Tell>  tell;
    static remove_pointer_t<FLAC__IOCallback_Eof>   eof;

    std::unique_ptr<FLAC__Metadata_Chain>    chain;
    std::unique_ptr<FLAC__Metadata_Iterator> first;
};


std::size_t metadata_chain::read(void* const dst, std::size_t const size,
                                 std::size_t const n, void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return file.try_read(static_cast<uchar*>(dst), size * n);
    }
    catch (...) {
        return -1_sz;
    }
}

std::size_t metadata_chain::write(void const* const src, std::size_t const size,
                                  std::size_t const n, void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        auto const bytes = size * n;
        file.write(src, bytes);
        return bytes;
    }
    catch (...) {
        return -1_sz;
    }
}

int metadata_chain::seek(void* const opaque, int64 const off, int const whence)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        file.seek(off, static_cast<io::seekdir>(whence));
        return 0;
    }
    catch (...) {
        return -1;
    }
}

int64 metadata_chain::tell(void* const opaque)
{
    try {
        auto&& file = *static_cast<io::stream*>(opaque);
        return static_cast<int64>(file.tell());
    }
    catch (...) {
        return -1;
    }
}

int metadata_chain::eof(void* const opaque)
{
    auto&& file = *static_cast<io::stream*>(opaque);
    return file.eof();
}


[[noreturn]] void raise(FLAC__StreamDecoderState const state)
{
    errc ec;
    switch (state) {
    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
        raise_bad_alloc();
    case FLAC__STREAM_DECODER_END_OF_STREAM:
        ec = errc::end_of_file;
        break;
    case FLAC__STREAM_DECODER_SEEK_ERROR:
        ec = errc::seek_error;
        break;
    case FLAC__STREAM_DECODER_OGG_ERROR:
    case FLAC__STREAM_DECODER_ABORTED:
        ec = errc::read_fault;
        break;
    case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
    case FLAC__STREAM_DECODER_READ_METADATA:
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
    case FLAC__STREAM_DECODER_READ_FRAME:
    case FLAC__STREAM_DECODER_UNINITIALIZED:
        ec = errc::failure;
        break;
    }
    raise(ec, "FLAC: %s", FLAC__StreamDecoderStateString[state]);
}

[[noreturn]] void raise(FLAC__StreamDecoderInitStatus const status)
{
    errc ec;
    switch (status) {
    case FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR:
        raise_bad_alloc();
    case FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
        ec = errc::protocol_not_supported;
        break;
    case FLAC__STREAM_DECODER_INIT_STATUS_OK:
    case FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS:
    case FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE:
    case FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED:
        ec = errc::invalid_data_format;
        break;
    }
    raise(ec, "FLAC: %s", FLAC__StreamDecoderInitStatusString[status]);
}


inline bool is_ogg_stream(io::stream& file)
{
    id3v2::skip(file);

    uint8 buf[33];
    file.peek(buf, sizeof(buf));

    if (io::load<uint32,BE>(buf) == "fLaC"_4cc) {
        return false;
    }
    if (io::load<uint32,BE>(buf +  0) == "OggS"_4cc &&
        io::load<uint32,BE>(buf + 29) == "FLAC"_4cc) {
        return true;
    }

    raise(errc::invalid_data_format, "no FLAC file signature");
}


class input
{
public:
    explicit input(ref_ptr<io::stream>, audio::open_mode);

    void read(audio::packet&);
    void seek(uint64);

    auto get_format() const noexcept;
    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    static remove_pointer_t<FLAC__StreamDecoderReadCallback>     read;
    static remove_pointer_t<FLAC__StreamDecoderSeekCallback>     seek;
    static remove_pointer_t<FLAC__StreamDecoderTellCallback>     tell;
    static remove_pointer_t<FLAC__StreamDecoderLengthCallback>   length;
    static remove_pointer_t<FLAC__StreamDecoderEofCallback>      eof;
    static remove_pointer_t<FLAC__StreamDecoderWriteCallback>    write;
    static remove_pointer_t<FLAC__StreamDecoderMetadataCallback> metadata;
    static remove_pointer_t<FLAC__StreamDecoderErrorCallback>    error;

    ref_ptr<io::stream> const file;
    audio::packet readbuf;
    std::unique_ptr<audio::pcm::blitter> blitter;
    std::unique_ptr<FLAC__StreamDecoder> decoder;
    FLAC__StreamMetadata_StreamInfo info;
    uint64 cached_pos{};
    uint32 average_bit_rate;
    bool const is_ogg;
};

FLAC__StreamDecoderReadStatus
input::read(FLAC__StreamDecoder const*, uint8* const dst,
            std::size_t* const size, void* const opaque)
{
    try {
        *size = static_cast<input*>(opaque)->file->try_read(dst, *size);
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    catch (...) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
}

FLAC__StreamDecoderSeekStatus
input::seek(FLAC__StreamDecoder const*, uint64 const pos, void* const opaque)
{
    try {
        static_cast<input*>(opaque)->file->seek(pos);
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }
    catch (...) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
}

FLAC__StreamDecoderTellStatus
input::tell(FLAC__StreamDecoder const*, uint64* const pos, void* const opaque)
{
    try {
        *pos = static_cast<input*>(opaque)->file->tell();
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }
    catch (...) {
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }
}

FLAC__StreamDecoderLengthStatus
input::length(FLAC__StreamDecoder const*, uint64* const len,
              void* const opaque)
{
    try {
        *len = static_cast<input*>(opaque)->file->size();
        return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }
    catch (...) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }
}

int input::eof(FLAC__StreamDecoder const*, void* const opaque)
{
    return static_cast<input*>(opaque)->file->eof();
}

FLAC__StreamDecoderWriteStatus
input::write(FLAC__StreamDecoder const*, FLAC__Frame const* const frame,
             int32 const* const* const source, void* const opaque)
{
    try {
        auto&& self = *static_cast<input*>(opaque);
        self.blitter->convert(source, frame->header.blocksize, self.readbuf);
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }
    catch (...) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
}

void input::metadata(FLAC__StreamDecoder const*,
                     FLAC__StreamMetadata const* const metadata,
                     void* const opaque)
{
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        static_cast<input*>(opaque)->info = metadata->data.stream_info;
    }
}

void input::error(FLAC__StreamDecoder const*,
                  FLAC__StreamDecoderErrorStatus const status, void*)
{
    std::fprintf(stderr, "[FLAC] stream decoder error: %s\n",
                 FLAC__StreamDecoderErrorStatusString[status]);
}


input::input(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)},
    is_ogg{flac::is_ogg_stream(*file)}
{
    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    decoder.reset(FLAC__stream_decoder_new());
    if (AMP_UNLIKELY(decoder == nullptr)) {
        raise_bad_alloc();
    }

    auto const init = is_ogg ? &FLAC__stream_decoder_init_ogg_stream
                             : &FLAC__stream_decoder_init_stream;
    auto const status = (*init)(decoder.get(),
                                input::read,
                                input::seek,
                                input::tell,
                                input::length,
                                input::eof,
                                input::write,
                                input::metadata,
                                input::error,
                                this);
    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        flac::raise(status);
    }
    if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder.get())) {
        flac::raise(FLAC__stream_decoder_get_state(decoder.get()));
    }

    average_bit_rate = static_cast<uint32>(muldiv(file->size() - file->tell(),
                                                  info.sample_rate * 8,
                                                  info.total_samples));

    // Avoid creating a PCM blitter if we're just reading metadata.
    if (mode & audio::playback) {
        audio::pcm::spec spec;
        spec.bytes_per_sample = 4;
        spec.bits_per_sample = info.bits_per_sample;
        spec.channels = info.channels;
        spec.flags = audio::pcm::signed_int
                   | audio::pcm::host_endian
                   | audio::pcm::non_interleaved;

        blitter = audio::pcm::blitter::create(spec);
        FLAC__stream_decoder_get_decode_position(decoder.get(), &cached_pos);
    }
}

void input::read(audio::packet& pkt)
{
    if (AMP_LIKELY(readbuf.empty())) {
        auto const ok = FLAC__stream_decoder_process_single(decoder.get());
        if (AMP_UNLIKELY(!ok)) {
            flac::raise(FLAC__stream_decoder_get_state(decoder.get()));
        }
    }
    readbuf.swap(pkt);
    readbuf.clear();

    auto bit_rate = average_bit_rate;
    if (pkt.frames() != 0) {
        uint64 pos;
        if (FLAC__stream_decoder_get_decode_position(decoder.get(), &pos)) {
            if (pos > cached_pos) {
                bit_rate = static_cast<uint32>(muldiv(pos - cached_pos,
                                                      info.sample_rate * 8,
                                                      pkt.frames()));
                cached_pos = pos;
            }
        }
    }
    pkt.set_bit_rate(bit_rate);
}

void input::seek(uint64 const pts)
{
    readbuf.clear();
    if (!FLAC__stream_decoder_seek_absolute(decoder.get(), pts)) {
        readbuf.clear();
        if (!FLAC__stream_decoder_flush(decoder.get())) {
            flac::raise(FLAC__stream_decoder_get_state(decoder.get()));
        }
    }
    FLAC__stream_decoder_get_decode_position(decoder.get(), &cached_pos);
}

auto input::get_format() const noexcept
{
    audio::format ret;
    ret.sample_rate    = info.sample_rate;
    ret.channels       = info.channels;
    ret.channel_layout = audio::xiph_channel_layout(info.channels);
    return ret;
}

auto input::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info out{get_format()};
    out.codec_id         = audio::codec::flac;
    out.frames           = info.total_samples;
    out.bits_per_sample  = info.bits_per_sample;
    out.average_bit_rate = average_bit_rate;
    if (is_ogg) {
        out.props.emplace(tags::container, "Ogg FLAC");
    }

    auto get_comment = [](auto const& comment) noexcept {
        return string_view {
            reinterpret_cast<char const*>(comment.entry),
            static_cast<std::size_t>(comment.length),
        };
    };

    for (auto&& block : flac::metadata_chain{*file, is_ogg}) {
        if (block.type != FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            continue;
        }

        auto&& vc = block.data.vorbis_comment;
        out.props.try_emplace(tags::encoder, get_comment(vc.vendor_string));

        out.tags.reserve(out.tags.size() + vc.num_comments);
        for (auto const i : xrange(vc.num_comments)) {
            auto comment = get_comment(vc.comments[i]);
            if (!comment.empty()) {
                auto found = comment.find('=');
                if (found < (comment.size() - 1)) {
                    auto const key   = comment.substr(0, found);
                    auto const value = comment.substr(found + 1);
                    out.tags.emplace(tags::map_common_key(key), value);
                }
            }
        }
    }
    return out;
}

auto input::get_image(media::image_type const type)
{
    media::image image;
    for (auto&& block : flac::metadata_chain{*file, is_ogg}) {
        if (block.type != FLAC__METADATA_TYPE_PICTURE) {
            continue;
        }
        if (block.data.picture.type != as_underlying(type)) {
            continue;
        }

        auto&& pic = block.data.picture;
        image.set_data(pic.data, pic.data_length);
        image.set_mime_type(pic.mime_type);
        image.set_description(reinterpret_cast<char const*>(pic.description));
        break;
    }
    return image;
}

auto input::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(input, "fla", "flac", "oga", "ogg");

}}}   // namespace amp::flac::<unnamed>

