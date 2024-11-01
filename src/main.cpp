#include <discord-rpc/discord_rpc.h>

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

int main() {
    std::thread rpcThread(handleRPCTasks);
    rpcThread.detach();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto mediaInformation = backend::getMediaInformation();
        if (!mediaInformation) {
            Discord_ClearPresence();  // Nothing is playing rn, clear presence
            continue;
        }

        if (mediaInformation->paused) {
            Discord_ClearPresence();  // TODO: allow user to keep presence when paused(because for
                                      // some reason some people want this)
            continue;
        }

        std::string currentlyPlayingSong =
            mediaInformation->songTitle + mediaInformation->songArtist + mediaInformation->songAlbum;

        if (currentlyPlayingSong == lastPlayingSong)
            continue;

        lastPlayingSong = currentlyPlayingSong;

        std::string currentMediaSource = mediaInformation->playbackSource;
        std::cout << currentMediaSource << std::endl;
        if (currentMediaSource != lastMediaSource)
            Discord_Shutdown();  // reinitialize with new client id

        std::string serviceName = utils::getAppName(lastMediaSource);
        DiscordRichPresence activity{};
        activity.type = ActivityType::LISTENING;
        activity.details = mediaInformation->songTitle.c_str();
        activity.state = std::string("by " + mediaInformation->songArtist).c_str();
        activity.smallImageText = serviceName.c_str();
        std::string artworkURL = utils::getArtworkURL(mediaInformation->songTitle + " " + mediaInformation->songArtist +
                                                      " " + mediaInformation->songAlbum);

        activity.smallImageKey = "icon";
        if (artworkURL == "") {
            activity.smallImageKey = "";
            activity.largeImageText = "icon";
        } else {
            activity.largeImageText = mediaInformation->songAlbum.c_str();
            activity.largeImageKey = artworkURL.c_str();
        }

        if (mediaInformation->songDuration != 0) {
            int64_t remainingTime = mediaInformation->songDuration - mediaInformation->songElapsedTime;
            activity.startTimestamp = time(nullptr) - mediaInformation->songElapsedTime;
            activity.endTimestamp = time(nullptr) + remainingTime;
        }
        std::string endpointURL = utils::getSearchEndpoint(lastMediaSource);

        if (endpointURL != "") {
            activity.button1name = std::string("Search on " + serviceName).c_str();
            std::string searchQuery = mediaInformation->songTitle + " " + mediaInformation->songArtist;
            activity.button1link = std::string(endpointURL + utils::urlEncode(searchQuery)).c_str();
        }

        lastMediaSource = currentMediaSource;
        Discord_UpdatePresence(&activity);
    }
}