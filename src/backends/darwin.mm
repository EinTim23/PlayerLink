#ifdef __APPLE__
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include <dispatch/dispatch.h>
#include "../MediaRemote.hpp"
#include "../backend.hpp"

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    std::string appName = "";
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
      MRMediaRemoteGetNowPlayingApplicationPID(dispatch_get_main_queue(), ^(pid_t pid) {
        if (pid > 0) {
            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            if (app) 
                appName = app.bundleIdentifier
        }
        dispatch_semaphore_signal(semaphore);
    });
    MRMediaRemoteGetNowPlayingInfo(dispatch_get_main_queue(), ^(CFDictionaryRef result) {
      if (result) {
          NSDictionary *playingInfo = (__bridge NSDictionary *)(result);
          NSLog(@"Now Playing Info: %@", playingInfo);
      }
       dispatch_semaphore_signal(semaphore);
    });
     dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);
    return nullptr;
}
bool backend::toggleAutostart(bool enabled) { return false; }
#endif