#include <discord-rpc/discord_rpc.h>
#include <wx/wx.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "backend.hpp"
#include "utils.hpp"

std::string lastPlayingSong = "";
std::string lastMediaSource = "";

void handleRPCTasks() {
    while (true) {
        DiscordEventHandlers discordHandler{};
        Discord_Initialize(utils::getClientID(lastMediaSource).c_str(), &discordHandler);
        if (Discord_IsConnected())
            break;
    }
    while (true) {
        Discord_RunCallbacks();
        if (!Discord_IsConnected())
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    handleRPCTasks();  // this could theoretically cause a stack overflow if discord is restarted often enough
}

void handleMediaTasks() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto mediaInformation = backend::getMediaInformation();
        if (!mediaInformation) {
            Discord_ClearPresence();  // Nothing is playing rn, clear presence
            continue;
        }

        if (mediaInformation->paused) {
            lastPlayingSong = "";
            Discord_ClearPresence();  // TODO: allow user to keep presence when paused(because for
                                      // some reason some people want this)
            continue;
        }

        std::string currentlyPlayingSong = mediaInformation->songTitle + mediaInformation->songArtist +
                                           mediaInformation->songAlbum + std::to_string(mediaInformation->songDuration);

        if (currentlyPlayingSong == lastPlayingSong)
            continue;

        lastPlayingSong = currentlyPlayingSong;

        std::string currentMediaSource = mediaInformation->playbackSource;

        if (currentMediaSource != lastMediaSource) {
            lastMediaSource = currentMediaSource;
            Discord_Shutdown();
        }  // reinitialize with new client id

        std::string serviceName = utils::getAppName(lastMediaSource);

        std::string activityState = "by " + mediaInformation->songArtist;
        DiscordRichPresence activity{};
        activity.type = ActivityType::LISTENING;
        activity.details = mediaInformation->songTitle.c_str();
        activity.state = activityState.c_str();
        activity.smallImageText = serviceName.c_str();
        std::string artworkURL = utils::getArtworkURL(mediaInformation->songTitle + " " + mediaInformation->songArtist +
                                                      " " + mediaInformation->songAlbum);

        activity.smallImageKey = "appicon";
        if (artworkURL == "") {
            activity.smallImageKey = "";
            activity.largeImageKey = "appicon";
        } else {
            activity.largeImageKey = artworkURL.c_str();
        }
        activity.largeImageText = mediaInformation->songAlbum.c_str();

        if (mediaInformation->songDuration != 0) {
            int64_t remainingTime = mediaInformation->songDuration - mediaInformation->songElapsedTime;
            activity.startTimestamp = time(nullptr) - (mediaInformation->songElapsedTime / 1000);
            activity.endTimestamp = time(nullptr) + (remainingTime / 1000);
        }
        std::string endpointURL = utils::getSearchEndpoint(lastMediaSource);

        std::string searchQuery = mediaInformation->songTitle + " " + mediaInformation->songArtist;
        std::string buttonName = "Search on " + serviceName;
        std::string buttonText = endpointURL + utils::urlEncode(searchQuery);

        if (endpointURL != "") {
            activity.button1name = buttonName.c_str();
            activity.button1link = buttonText.c_str();
        }

        Discord_UpdatePresence(&activity);
    }
}
class MyApp : public wxApp {
public:
    virtual bool OnInit() override {
        wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Hello wxWidgets", wxDefaultPosition, wxSize(400, 300));
        wxStaticText* text = new wxStaticText(frame, wxID_ANY, "Hello World", wxPoint(150, 130));
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(MyApp);

int main(int argc, char** argv) {
    std::thread rpcThread(handleRPCTasks);
    rpcThread.detach();
    std::thread mediaThread(handleMediaTasks);
    mediaThread.detach();
    return wxEntry(argc, argv);
}