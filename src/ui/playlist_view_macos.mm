////////////////////////////////////////////////////////////////////////////////
//
// ui/playlist_view_macos.mm
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "media/track.hpp"
#include "ui/playlist_model.hpp"
#include "ui/playlist_view.hpp"

#import <AppKit/NSWorkspace.h>
#import <Foundation/NSURL.h>


namespace amp {
namespace ui {

void PlaylistView::onShowInFinder()
{
    auto const index = currentIndex();
    if (!index.isValid()) {
        return;
    }

    auto const model = static_cast<PlaylistModel*>(QTreeView::model());
    auto const location = model->track(index).location;
    @autoreleasepool {
        NSURL* url = [NSURL URLWithString:@(location.data())];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
    };
}

}}    // namespace amp::ui

