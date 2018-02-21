////////////////////////////////////////////////////////////////////////////////
//
// ui/image_server.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/input.hpp>
#include <amp/media/image.hpp>
#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>

#include "core/filesystem.hpp"
#include "ui/image_server.hpp"
#include "ui/string.hpp"

#include <mutex>
#include <string_view>
#include <tuple>
#include <utility>

#include <QtGui/QImage>


namespace amp {
namespace ui {
namespace {

using namespace ::std::literals;


auto findInternalImage(net::uri const& location, media::image_type const type)
{
    if (auto input = audio::input::resolve(location, audio::pictures)) {
        auto image = input->get_image(type);
        return QImage::fromData(image.data(), static_cast<int>(image.size()),
                                image.mime_type().c_str());
    }
    return QImage{};
}

auto findExternalImage(net::uri const& location, media::image_type const type)
{
    auto const directory = fs::parent_path(location.get_file_path());
    if (!fs::exists(directory)) {
        return QImage{};
    }

    char const* const* prefixes;

    if (type == media::image_type::front_cover) {
        static constexpr char const* front_prefixes[] {
            "album", "cover", "folder", "front", nullptr,
        };
        prefixes = front_prefixes;
    }
    else if (type == media::image_type::back_cover) {
        static constexpr char const* back_prefixes[] {
            "back", nullptr,
        };
        prefixes = back_prefixes;
    }
    else if (type == media::image_type::media) {
        static constexpr char const* media_prefixes[] {
            "cd", "disc", "media", nullptr,
        };
        prefixes = media_prefixes;
    }
    else if (type == media::image_type::file_icon_32x32) {
        static constexpr char const* file_icon_prefixes[] {
            "file_icon", "icon", nullptr,
        };
        prefixes = file_icon_prefixes;
    }
    else if (type == media::image_type::artist) {
        static constexpr char const* artist_prefixes[] {
            "artist", nullptr,
        };
        prefixes = artist_prefixes;
    }
    else {
        return QImage{};
    }

    static constexpr char const* suffixes[] {
        "bmp", "jpeg", "jpg", "png", "ppm", "xpm", nullptr,
    };

    auto match = [](char const* const* x, u8string const& y) noexcept {
        do {
            if (stricmpeq(*x++, y.c_str())) {
                return true;
            }
        }
        while (*x != nullptr);
        return false;
    };

    QImage image;
    for (auto&& path : fs::directory_range{directory}) {
        if (match(prefixes, fs::stem(path)) &&
            match(suffixes, fs::extension(path))) {
            image.load(to_qstring(path));
            if (!image.isNull()) {
                break;
            }
        }
    }
    return image;
}

auto resolveImage(net::uri const& location, media::image_type const type)
{
    QImage image;
    if (location.scheme() == "file"sv) {
        try {
            image = findInternalImage(location, type);
            if (image.isNull()) {
                image = findExternalImage(location, type);
            }
        }
        catch (...) {
        }
    }
    return image;
}

}     // namespace <unnamed>


ImageServer::ImageServer(QObject* const parent) :
    QObject(parent),
    stop_(false),
    thrd_([this]{
        auto wait = [this]{
            std::unique_lock<std::mutex> lk{mtx_};
            cnd_.wait(lk);
            return !stop_;
        };

        net::uri          location;
        media::image_type type;

        while (wait()) {
            if (requests_.pop(std::tie(location, type))) {
                while (requests_.pop(std::tie(location, type))) {}
                Q_EMIT recv(ui::resolveImage(location, type));
            }
        }
    })
{
}

ImageServer::~ImageServer()
{
    {
        std::lock_guard<std::mutex> const lk{mtx_};
        stop_ = true;
    }
    cnd_.notify_one();
    thrd_.join();
}

}}    // namespace amp::ui

