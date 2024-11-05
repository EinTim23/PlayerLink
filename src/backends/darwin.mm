#ifdef __APPLE__
#include <Foundation/Foundation.h>
#include "../MediaRemote.hpp"
#include "../backend.hpp"

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    MRMediaRemoteGetNowPlayingInfo(dispatch_get_main_queue(), ^(CFDictionaryRef result) {
      if (result) {
          NSDictionary *playingInfo = (__bridge NSDictionary *)(result);
          NSLog(@"Now Playing Info: %@", playingInfo);
      }
    });
    return nullptr;
}
bool backend::toggleAutostart(bool enabled) { return false; }
#endif