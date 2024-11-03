#include <discord-rpc/discord_rpc.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/statline.h>
#include <wx/taskbar.h>
#include <wx/wx.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "backend.hpp"
#include "rsrc.hpp"
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
class PlayerLinkIcon : public wxTaskBarIcon {
public:
    PlayerLinkIcon(wxFrame* s) : settingsFrame(s) {}

    void OnMenuOpen(wxCommandEvent& evt) { settingsFrame->Show(true); }

    void OnMenuExit(wxCommandEvent& evt) { settingsFrame->Close(true); }

protected:
    virtual wxMenu* CreatePopupMenu() override {
        wxMenu* menu = new wxMenu;
        menu->Append(10004, _("Not Playing"));  // TODO: make this dynamic
        menu->Enable(10004, false);
        menu->AppendSeparator();
        menu->Append(10001, _("Settings"));
        menu->Append(10003, _("About PlayerLink"));
        menu->AppendSeparator();
        menu->Append(10002, _("Quit PlayerLink..."));
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnMenuOpen, this, 10001);
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnMenuExit, this, 10002);
        return menu;
    }

private:
    wxFrame* settingsFrame;
};

class PlayerLinkFrame : public wxFrame {
protected:
    wxStaticText* settingsText;
    wxStaticLine* settingsDivider;
    wxStaticText* enabledAppsText;
    wxCheckBox* anyOtherCheckbox;
    wxStaticLine* appsDivider;
    wxStaticText* startupText;
    wxCheckBox* autostartCheckbox;

public:
    PlayerLinkFrame(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString,
                    const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(300, 200),
                    long style = wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER & ~wxMAXIMIZE_BOX)
        : wxFrame(parent, id, title, pos, size, style) {
        this->SetSizeHints(wxDefaultSize, wxDefaultSize);

        wxBoxSizer* mainContainer;
        mainContainer = new wxBoxSizer(wxVERTICAL);

        settingsText = new wxStaticText(this, wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize, 0);
        settingsText->Wrap(-1);
        mainContainer->Add(settingsText, 0, wxALIGN_CENTER | wxALL, 5);

        settingsDivider = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(settingsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* enabledAppsContainer;
        enabledAppsContainer = new wxBoxSizer(wxHORIZONTAL);

        enabledAppsText = new wxStaticText(this, wxID_ANY, _("Enabled Apps:"), wxDefaultPosition, wxDefaultSize, 0);
        enabledAppsText->Wrap(-1);
        enabledAppsContainer->Add(enabledAppsText, 0, wxALL, 5);

        wxBoxSizer* appCheckboxContainer;
        appCheckboxContainer = new wxBoxSizer(wxVERTICAL);

        auto apps = utils::getAllApps();

        for (auto app : apps) {
            auto checkbox = new wxCheckBox(this, wxID_ANY, _(app.appName), wxDefaultPosition, wxDefaultSize, 0);
            appCheckboxContainer->Add(checkbox, 0, wxALL, 5);
        }

        anyOtherCheckbox = new wxCheckBox(this, wxID_ANY, _("Any other"), wxDefaultPosition, wxDefaultSize, 0);
        appCheckboxContainer->Add(anyOtherCheckbox, 0, wxALL, 5);

        enabledAppsContainer->Add(appCheckboxContainer, 1, wxEXPAND, 5);

        mainContainer->Add(enabledAppsContainer, 0, 0, 5);

        appsDivider = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(appsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* settingsContainer;
        settingsContainer = new wxBoxSizer(wxHORIZONTAL);

        startupText = new wxStaticText(this, wxID_ANY, _("Startup:"), wxDefaultPosition, wxDefaultSize, 0);
        startupText->Wrap(-1);
        settingsContainer->Add(startupText, 0, wxALL, 5);

        autostartCheckbox = new wxCheckBox(this, wxID_ANY, _("Launch at login"), wxDefaultPosition, wxDefaultSize, 0);
        settingsContainer->Add(autostartCheckbox, 0, wxALL, 5);

        mainContainer->Add(settingsContainer, 0, wxEXPAND, 5);

        this->SetSizerAndFit(mainContainer);

        wxSize currentSize = this->GetSize();
        this->SetSize(size.GetWidth(), currentSize.GetHeight());
        this->Layout();

        this->Centre(wxBOTH);
    }
};
class PlayerLink : public wxApp {
public:
    virtual bool OnInit() override {
        if (wxSystemSettings::GetAppearance().IsSystemDark())  // To support the native dark mode on windows 10 and up
            this->SetAppearance(wxAppBase::Appearance::Dark);

        wxInitAllImageHandlers();
        PlayerLinkFrame* frame = new PlayerLinkFrame(nullptr, wxID_ANY, _("PlayerLink"));
        trayIcon = new PlayerLinkIcon(frame);
        frame->Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent& event) {
            if (event.CanVeto()) {
                frame->Hide();
                event.Veto();
            } else
                this->ExitMainLoop();
        });
        wxIcon icon = utils::loadIconFromMemory(icon_png, icon_png_size);
        trayIcon->SetIcon(icon, _("PlayerLink"));
        return true;
    }

private:
    PlayerLinkIcon* trayIcon;
};

wxIMPLEMENT_APP_NO_MAIN(PlayerLink);

int main(int argc, char** argv) {
    std::thread rpcThread(handleRPCTasks);
    rpcThread.detach();
    std::thread mediaThread(handleMediaTasks);
    mediaThread.detach();
    return wxEntry(argc, argv);
}