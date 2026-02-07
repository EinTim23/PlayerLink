#include <Foundation/NSObjCRuntime.h>
#ifdef __APPLE__
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <nlohmann-json/single_include/nlohmann/json.hpp>
#include <fstream>

#include "../MediaRemote.hpp"
#include "../backend.hpp"

void hideDockIcon(bool shouldHide) {
    if (shouldHide)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    else
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

NSString *getFilePathFromBundle(NSString *fileName, NSString *fileType) {
    NSString *filePath = [[NSBundle mainBundle] pathForResource:fileName ofType:fileType];
    return filePath;
}

std::string executeCommand(const std::string &command, const std::vector<std::string> &arguments) {
    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(command.c_str()));
        for (const auto &arg : arguments) argv.push_back(const_cast<char *>(arg.c_str()));
        argv.push_back(nullptr);

        execv(command.c_str(), argv.data());
        _exit(1);
    }

    close(pipefd[1]);

    std::string output;
    char buffer[4096];
    ssize_t count;
    while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, count);
    }

    close(pipefd[0]);
    waitpid(pid, nullptr, 0);

    return output;
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    // apple decided to prevent apps not signed by them to use media remote, so we use an apple script instead. But that
    // script only works on Sonoma or newer and the other one is arguably better, so keep the old method as well
    if (@available(macOS 15.0, *)) {
        static NSString *script = getFilePathFromBundle(@"MediaRemote", @"js");
        std::string output = executeCommand("/usr/bin/osascript", {"-l", "JavaScript", script.UTF8String});
        nlohmann::json j = nlohmann::json::parse(output);

        std::string appName = j["player"].get<std::string>();
        if (appName == "none")
            return nullptr;

        bool paused = j["playbackStatus"].get<int>() == 0;

        std::string songTitle = j["title"].get<std::string>();

        std::string songAlbum = j["album"].get<std::string>();

        std::string songArtist = j["artist"].get<std::string>();

        int64_t elapsedTimeMs = 0;
        int64_t durationMs = 0;
        try {
            double durationNumber = j["duration"].get<double>();
            durationMs = static_cast<int64_t>(durationNumber * 1000);

            double elapsedTimeNumber = j["elapsed"].get<double>();
            elapsedTimeMs = static_cast<int64_t>(elapsedTimeNumber * 1000);
        } catch (...) {
        }

        return std::make_shared<MediaInfo>(paused, songTitle, songArtist, songAlbum, appName, "", durationMs,
                                           elapsedTimeMs);
    } else {
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
}

std::filesystem::path backend::getConfigDirectory() {
    std::filesystem::path configDirectoryPath = std::getenv("HOME");
    configDirectoryPath = configDirectoryPath / "Library" / "Application Support" / "PlayerLink";
    return configDirectoryPath;
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

    // I would also like to use std::format here, but well I also want to support older mac os versions.
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