////////////////////////////////////////////////////////////////////////////////
//
// ui/image_server.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C5764A91_0490_428C_93DC_3214CFAA88BD
#define AMP_INCLUDED_C5764A91_0490_428C_93DC_3214CFAA88BD


#include <amp/stddef.hpp>
#include <amp/media/image.hpp>
#include <amp/net/uri.hpp>

#include "core/spsc_queue.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#include <QtCore/QObject>
#include <QtGui/QImage>


namespace amp {
namespace ui {

class ImageServer final :
    public QObject
{
    Q_OBJECT

public:
    explicit ImageServer(QObject* = nullptr);
    ~ImageServer();

    void send(net::uri location, media::image_type const type)
    {
        requests_.emplace(std::move(location), type);
        cnd_.notify_one();
    }

Q_SIGNALS:
    void recv(QImage);

private:
    using request = std::pair<net::uri, media::image_type>;

    bool                    stop_;
    spsc::queue<request>    requests_;
    std::condition_variable cnd_;
    std::mutex              mtx_;
    std::thread             thrd_;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_C5764A91_0490_428C_93DC_3214CFAA88BD

