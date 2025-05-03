#include <Foundation/NSObjCRuntime.h>
#ifdef __APPLE__
#include <AppKit/AppKit.h>
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <nlohmann-json/single_include/nlohmann/json.hpp>
#include <fstream>

#include "../backend.hpp"

void hideDockIcon(bool shouldHide) {
    if (shouldHide)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    else
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

NSString* getFilePathFromBundle(NSString* fileName, NSString* fileType) {
    NSString *filePath = [[NSBundle mainBundle] pathForResource:fileName ofType:fileType];
    return filePath;
}

NSString* executeCommand(NSString* command, NSArray* arguments) {
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = command;
    task.arguments = arguments;

    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    [task launch];

    NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
    NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    [task waitUntilExit];
    
    return output;
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    static NSString* script = getFilePathFromBundle(@"MediaRemote", @"js");
    NSString* output = executeCommand(@"/usr/bin/osascript", @[@"-l", @"JavaScript", script]);
    nlohmann::json j = nlohmann::json::parse([output UTF8String]);

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
    } catch (...) {} 

    return std::make_shared<MediaInfo>(paused, songTitle, songArtist, songAlbum, appName, "",
                                       durationMs, elapsedTimeMs);
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