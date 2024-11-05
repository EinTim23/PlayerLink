#ifdef __APPLE__
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include "../MediaRemote.hpp"
#include "../backend.hpp"

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
      MRMediaRemoteGetNowPlayingApplicationPID(dispatch_get_main_queue(), ^(pid_t pid) {
        if (pid > 0) {
            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            if (app) {
                NSLog(@"%@", app.bundleIdentifier);
            }
        }
    });
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