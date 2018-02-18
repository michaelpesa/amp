////////////////////////////////////////////////////////////////////////////////
//
// ui/media_scanner.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_628C82E6_C409_44EF_90FB_43D215099026
#define AMP_INCLUDED_628C82E6_C409_44EF_90FB_43D215099026


#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "media/track.hpp"

#include <atomic>
#include <thread>
#include <utility>
#include <vector>

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>


Q_DECLARE_METATYPE(amp::media::track);


namespace amp {
namespace ui {

class MediaScanner final :
    public QObject
{
    Q_OBJECT

public:
    explicit MediaScanner(QObject* = nullptr);
    virtual ~MediaScanner();

    void run(std::vector<net::uri>, u8string);
    void cancel();

Q_SIGNALS:
    void resultReady(media::track);
    void finished();
    void canceled();

    void progressValueChanged(int);
    void progressRangeChanged(int, int);
    void progressTextChanged(QString const&);

private:
    AMP_INTERNAL_LINKAGE void loadTracks(net::uri const&);
    AMP_INTERNAL_LINKAGE void loadCueSheet(net::uri const&, u8string = {});

    QStringList errors_;
    std::thread thread_;
    std::atomic<bool> cancel_flag_{false};
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_628C82E6_C409_44EF_90FB_43D215099026

