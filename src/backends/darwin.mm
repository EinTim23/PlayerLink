#ifdef __APPLE__
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <fstream>

#include "../MediaRemote.hpp"
#include "../backend.hpp"

void hideDockIcon(bool shouldHide) {
    if (shouldHide)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    else
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    __block NSString *appName = nil;
    __block NSDictionary *playingInfo = nil;

    dispatch_group_t group = dispatch_group_create();

    dispatch_group_enter(group);
    MRMediaRemoteGetNowPlayingApplicationPID(dispatch_get_main_queue(), ^(pid_t pid) {
      if (pid > 0) {
          NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
          if (app)
              appName = [[app.bundleIdentifier copy] retain];
      }
      dispatch_group_leave(group);
    });

    dispatch_group_enter(group);
    MRMediaRemoteGetNowPlayingInfo(dispatch_get_main_queue(), ^(CFDictionaryRef result) {
      if (result)
          playingInfo = [[(__bridge NSDictionary *)result copy] retain];
      dispatch_group_leave(group);
    });

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    dispatch_release(group);
    if (appName == nil || playingInfo == nil)
        return nullptr;

    bool paused = [playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoPlaybackRate] intValue] == 0;

    NSString *title = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoTitle];
    std::string songTitle = title ? [title UTF8String] : "";

    NSString *album = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoAlbum];
    std::string songAlbum = album ? [album UTF8String] : "";

    NSString *artist = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoArtist];
    std::string songArtist = artist ? [artist UTF8String] : "";

    NSData *artworkData = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoArtworkData];

    std::string thumbnailData;
    if (artworkData)
        thumbnailData = std::string((const char *)[artworkData bytes], [artworkData length]);

    NSNumber *elapsedTimeNumber = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoElapsedTime];

    NSNumber *durationNumber = playingInfo[(__bridge NSString *)kMRMediaRemoteNowPlayingInfoDuration];

    int64_t elapsedTimeMs = elapsedTimeNumber ? static_cast<int64_t>([elapsedTimeNumber doubleValue] * 1000) : 0;

    int64_t durationMs = durationNumber ? static_cast<int64_t>([durationNumber doubleValue] * 1000) : 0;

    std::string appNameString = appName.UTF8String;

    [appName release];
    [playingInfo release];
    return std::make_shared<MediaInfo>(paused, songTitle, songArtist, songAlbum, appNameString, thumbnailData,
                                       durationMs, elapsedTimeMs);
}

bool backend::toggleAutostart(bool enabled) {
    std::filesystem::path launchAgentPath = std::getenv("HOME");
    launchAgentPath = launchAgentPath / "Library" / "LaunchAgents";
    std::filesystem::create_directories(launchAgentPath);
    launchAgentPath = launchAgentPath / "PlayerLink.plist";
    
    if (!enabled && std::filesystem::exists(launchAgentPath)) {
        std::filesystem::remove(launchAgentPath);
        return true;
    }
    NSString *binaryPath = [[[NSProcessInfo processInfo] arguments][0] stringByStandardizingPath];

    //I would also like to use std::format here, but well I also want to support older mac os versions.
    std::string formattedPlist =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n  <dict>\n\n    "
        "<key>Label</key>\n    <string>PlayerLink</string>\n    <key>ProgramArguments</key>\n      <array>\n        "
        "<string>" +
        std::string(binaryPath.UTF8String) +
        "</string>\n      </array>\n    <key>RunAtLoad</key>\n    <true/>\n    <key>AbandonProcessGroup</key>\n    "
        "<true/>\n  </dict>\n</plist>";
    std::ofstream o(launchAgentPath);
    o.write(formattedPlist.c_str(), formattedPlist.size());
    o.close();
    return true;
}

bool backend::init() {
    hideDockIcon(true);
    return true;
}

#undef LAUNCH_AGENT_TEMPLATE
#endif