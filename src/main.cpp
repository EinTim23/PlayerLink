#include <discord-rpc/discord_rpc.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/statline.h>
#include <wx/taskbar.h>
#include <wx/wx.h>

#include <chrono>
#include <thread>

#include "backend.hpp"
#include "lastfm.hpp"
#include "rsrc.hpp"
#include "utils.hpp"

std::string lastPlayingSong = "";
std::string lastMediaSource = "";
std::string currentSongTitle = "";
utils::SongInfo songInfo;
LastFM* lastfm = nullptr;

void handleRPCTasks() {
    while (true) {
        while (true) {
            DiscordEventHandlers discordHandler{};
            auto app = utils::getApp(lastMediaSource);
            Discord_Initialize(app.clientId.c_str(), &discordHandler);
            if (Discord_IsConnected())
                break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        while (true) {
            Discord_RunCallbacks();
            if (!Discord_IsConnected())
                break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        Discord_Shutdown();
    }
}

void initLastFM(bool checkMode = false) {
    if (lastfm)
        delete lastfm;
    auto settings = utils::getSettings();
    if (!settings.lastfm.enabled && !checkMode)
        return;
    lastfm = new LastFM(settings.lastfm.username, settings.lastfm.password, settings.lastfm.api_key,
                        settings.lastfm.api_secret);
    LastFM::LASTFM_STATUS status = lastfm->authenticate();
    if (status)
        wxMessageBox(_("Error authenticating at LastFM!"), _("PlayerLink"), wxOK | wxICON_ERROR);
    else if (checkMode)
        wxMessageBox(_("The LastFM authentication was successful."), _("PlayerLink"), wxOK | wxICON_INFORMATION);
}

void handleMediaTasks() {
    initLastFM();
    int64_t lastMs = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto mediaInformation = backend::getMediaInformation();
        auto settings = utils::getSettings();
        if (!mediaInformation) {
            currentSongTitle = "";
            Discord_ClearPresence();  // Nothing is playing rn, clear presence
            continue;
        }

        if (mediaInformation->paused) {
            lastMs = 0;
            currentSongTitle = "";
            Discord_ClearPresence();
            continue;
        }

        std::string currentlyPlayingSong = mediaInformation->songTitle + mediaInformation->songArtist +
                                           mediaInformation->songAlbum + std::to_string(mediaInformation->songDuration);
        int64_t currentMs = mediaInformation->songElapsedTime;

        bool shouldContinue =
            currentlyPlayingSong == lastPlayingSong && (lastMs <= currentMs) && (lastMs + 3000 >= currentMs);
        lastMs = currentMs;

        if (shouldContinue)
            continue;

        if (lastPlayingSong.find(mediaInformation->songTitle + mediaInformation->songArtist +
                                 mediaInformation->songAlbum) == std::string::npos &&
            lastfm)
            lastfm->scrobble(mediaInformation->songArtist, mediaInformation->songTitle);

        lastPlayingSong = currentlyPlayingSong;
        currentSongTitle = mediaInformation->songArtist + " - " + mediaInformation->songTitle;

        std::string currentMediaSource = mediaInformation->playbackSource;

        if (currentMediaSource != lastMediaSource) {
            lastMediaSource = currentMediaSource;
            Discord_Shutdown();
        }  // reinitialize with new client id

        auto app = utils::getApp(lastMediaSource);

        if (!app.enabled) {
            Discord_ClearPresence();
            continue;
        }
        std::string serviceName = app.appName;

        std::string activityState = "by " + mediaInformation->songArtist;
        DiscordRichPresence activity{};
        activity.type = app.type;
        activity.details = mediaInformation->songTitle.c_str();
        activity.state = activityState.c_str();
        activity.smallImageText = serviceName.c_str();
        songInfo = utils::getSongInfo(mediaInformation->songTitle + " " + mediaInformation->songArtist + " " +
                                      mediaInformation->songAlbum);

        activity.smallImageKey = "appicon";
        if (songInfo.artworkURL == "") {
            activity.smallImageKey = "";
            activity.largeImageKey = "appicon";
        } else {
            activity.largeImageKey = songInfo.artworkURL.c_str();
        }
        activity.largeImageText = mediaInformation->songAlbum.c_str();

        if (mediaInformation->songDuration != 0) {
            int64_t remainingTime = mediaInformation->songDuration - mediaInformation->songElapsedTime;
            activity.startTimestamp = time(nullptr) - (mediaInformation->songElapsedTime / 1000);
            activity.endTimestamp = time(nullptr) + (remainingTime / 1000);
        }
        std::string endpointURL = app.searchEndpoint;

        std::string searchQuery = mediaInformation->songTitle + " " + mediaInformation->songArtist;
        std::string buttonName = "Search on " + serviceName;
        std::string buttonText = endpointURL + utils::urlEncode(searchQuery);

        if (endpointURL != "") {
            activity.button1name = buttonName.c_str();
            activity.button1link = buttonText.c_str();
        }

        std::string odesliUrl = utils::getOdesliURL(songInfo);
        if(settings.odesli && songInfo.artworkURL != "") {
            activity.button2name = "Show on Song.link";
            activity.button2link = odesliUrl.c_str();
        }

        Discord_UpdatePresence(&activity);
    }
}
class PlayerLinkIcon : public wxTaskBarIcon {
public:
    PlayerLinkIcon(wxFrame* s) : settingsFrame(s) {}

    void OnMenuOpen(wxCommandEvent& evt) {
        settingsFrame->Show(true);
        settingsFrame->Raise();
    }

    void OnCopyOdesliURL(wxCommandEvent& evt) { utils::copyToClipboard(utils::getOdesliURL(songInfo)); }

    void OnMenuExit(wxCommandEvent& evt) { settingsFrame->Close(true); }

    void OnMenuAbout(wxCommandEvent& evt) {
        wxMessageBox(_("Made with <3 by EinTim"), _("PlayerLink"), wxOK | wxICON_INFORMATION);
    }

protected:
    virtual wxMenu* CreatePopupMenu() override {
        wxMenu* menu = new wxMenu;
        menu->Append(10004, currentSongTitle == "" ? _("Not Playing") : wxString::FromUTF8(currentSongTitle));
        menu->Enable(10004, false);
        menu->AppendSeparator();
        menu->Append(10005, _("Copy Odesli URL"));
        if (songInfo.artworkURL == "" || currentSongTitle == "")
            menu->Enable(10005, false);
        menu->Append(10001, _("Settings"));
        menu->Append(10003, _("About PlayerLink"));
        menu->AppendSeparator();
        menu->Append(10002, _("Quit PlayerLink..."));
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnCopyOdesliURL, this, 10005);
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnMenuOpen, this, 10001);
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnMenuExit, this, 10002);
        Bind(wxEVT_MENU, &PlayerLinkIcon::OnMenuAbout, this, 10003);
        return menu;
    }

private:
    wxFrame* settingsFrame;
};

class wxTextCtrlWithPlaceholder : public wxTextCtrl {
public:
    wxTextCtrlWithPlaceholder(wxWindow* parent, wxWindowID id, const wxString& value,
                              const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                              long style = 0, const wxValidator& validator = wxDefaultValidator,
                              const wxString& name = "textCtrl")
        : wxTextCtrl(parent, id, value, pos, size, style, validator, name),
          placeholder(""),
          showPlaceholder(true),
          isPassword((style & wxTE_PASSWORD) != 0) {
        Bind(wxEVT_SET_FOCUS, &wxTextCtrlWithPlaceholder::OnFocus, this);
        Bind(wxEVT_KILL_FOCUS, &wxTextCtrlWithPlaceholder::OnBlur, this);
    }

    void SetPlaceholderText(const wxString& p) {
        placeholder = p;
        if (GetValue().IsEmpty() || showPlaceholder)
            UpdatePlaceholder();
    }

protected:
    void OnFocus(wxFocusEvent& event) {
        if (showPlaceholder && GetValue() == placeholder) {
            Clear();
            if (isPassword)
                SetStyleToPassword();
        }

        showPlaceholder = false;
        event.Skip();
    }

    void OnBlur(wxFocusEvent& event) {
        if (GetValue().IsEmpty()) {
            showPlaceholder = true;
            UpdatePlaceholder();
        }
        event.Skip();
    }

private:
    wxString placeholder;
    bool showPlaceholder;
    bool isPassword;

    void UpdatePlaceholder() {
        if (isPassword)
            SetStyleToNormal();
        SetValue(placeholder);
    }

    void SetStyleToPassword() { SetWindowStyle(GetWindowStyle() | wxTE_PASSWORD); }

    void SetStyleToNormal() { SetWindowStyle(GetWindowStyle() & ~wxTE_PASSWORD); }
};

class PlayerLinkFrame : public wxFrame {
public:
    PlayerLinkFrame(wxWindow* parent, wxIcon& icon, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString,
                    const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(300, 200),
                    long style = wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER & ~wxMAXIMIZE_BOX)
        : wxFrame(parent, id, title, pos, size, style) {
        this->SetSizeHints(wxDefaultSize, wxDefaultSize);
        this->SetIcon(icon);

        auto mainContainer = new wxBoxSizer(wxVERTICAL);
        // header start
        auto settingsText = new wxStaticText(this, wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize, 0);
        settingsText->Wrap(-1);
        mainContainer->Add(settingsText, 0, wxALIGN_CENTER | wxALL, 5);
        // header end

        // enabled apps start
        auto settingsDivider = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(settingsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* enabledAppsContainer;
        enabledAppsContainer = new wxBoxSizer(wxHORIZONTAL);

        auto enabledAppsText =
            new wxStaticText(this, wxID_ANY, _("Enabled Apps:"), wxDefaultPosition, wxDefaultSize, 0);
        enabledAppsText->Wrap(-1);
        enabledAppsContainer->Add(enabledAppsText, 0, wxALL, 5);

        wxBoxSizer* appCheckboxContainer;
        appCheckboxContainer = new wxBoxSizer(wxVERTICAL);

        auto settings = utils::getSettings();

        for (auto app : settings.apps) {
            auto checkbox = new wxCheckBox(this, wxID_ANY, app.appName, wxDefaultPosition, wxDefaultSize, 0);
            checkbox->SetValue(app.enabled);
            checkbox->SetClientData(new utils::App(app));
            checkbox->Bind(wxEVT_CHECKBOX, [checkbox](wxCommandEvent& event) {
                bool isChecked = checkbox->IsChecked();
                utils::App* appData = static_cast<utils::App*>(checkbox->GetClientData());
                appData->enabled = isChecked;
                utils::saveSettings(appData);
            });
            checkbox->Bind(wxEVT_DESTROY, [checkbox](wxWindowDestroyEvent&) {
                delete static_cast<utils::App*>(checkbox->GetClientData());
            });
            appCheckboxContainer->Add(checkbox, 0, wxALL, 5);
        }

        auto anyOtherCheckbox = new wxCheckBox(this, wxID_ANY, _("Any other"), wxDefaultPosition, wxDefaultSize, 0);
        anyOtherCheckbox->SetValue(settings.anyOtherEnabled);
        anyOtherCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.anyOtherEnabled = isChecked;
            utils::saveSettings(settings);
        });

        appCheckboxContainer->Add(anyOtherCheckbox, 0, wxALL, 5);

        enabledAppsContainer->Add(appCheckboxContainer, 1, wxEXPAND, 5);

        mainContainer->Add(enabledAppsContainer, 0, 0, 5);
        // enabled apps end

        // LastFM start
        auto lastfmDivider = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(lastfmDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* lastFMContainer;
        lastFMContainer = new wxBoxSizer(wxHORIZONTAL);

        auto lastfmText = new wxStaticText(this, wxID_ANY, _("LastFM:"), wxDefaultPosition, wxDefaultSize, 0);
        lastfmText->Wrap(-1);
        lastFMContainer->Add(lastfmText, 0, wxALL, 5);

        wxBoxSizer* lastfmSettingsContainer;
        lastfmSettingsContainer = new wxBoxSizer(wxVERTICAL);

        auto lastfmEnabledCheckbox = new wxCheckBox(this, wxID_ANY, _("Enabled"), wxDefaultPosition, wxDefaultSize, 0);
        lastfmEnabledCheckbox->SetValue(settings.lastfm.enabled);
        lastfmEnabledCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.lastfm.enabled = isChecked;
            utils::saveSettings(settings);
        });
        lastfmSettingsContainer->Add(lastfmEnabledCheckbox, 0, wxALIGN_CENTER | wxALL, 5);
        lastFMContainer->Add(lastfmSettingsContainer, 1, wxEXPAND, 5);

        auto usernameInput =
            new wxTextCtrlWithPlaceholder(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
        usernameInput->SetPlaceholderText(_("Username"));
        usernameInput->SetValue(settings.lastfm.username);
        usernameInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.username = data;
            utils::saveSettings(settings);
        });
        auto passwordInput = new wxTextCtrlWithPlaceholder(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                                           wxDefaultSize, wxTE_PASSWORD);
        passwordInput->SetPlaceholderText(_("Password"));
        passwordInput->SetValue(settings.lastfm.password);
        passwordInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.password = data;
            utils::saveSettings(settings);
        });
        auto apikeyInput =
            new wxTextCtrlWithPlaceholder(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
        apikeyInput->SetPlaceholderText(_("API-Key"));
        apikeyInput->SetValue(settings.lastfm.api_key);
        apikeyInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.api_key = data;
            utils::saveSettings(settings);
        });
        auto apisecretInput = new wxTextCtrlWithPlaceholder(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                                            wxDefaultSize, wxTE_PASSWORD);
        apisecretInput->SetPlaceholderText(_("API-Secret"));
        apisecretInput->SetValue(settings.lastfm.api_secret);
        apisecretInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.api_secret = data;
            utils::saveSettings(settings);
        });

        auto checkButton = new wxButton(this, wxID_ANY, _("Check credentials"));
        checkButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) { initLastFM(true); });
        mainContainer->Add(lastFMContainer, 0, 0, 5);

        mainContainer->Add(usernameInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(passwordInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(apikeyInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(apisecretInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(checkButton, 0, wxEXPAND | wxALL, 10);

        // Last FM End

        // settings start
        auto appsDivider = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(appsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* settingsContainer;
        settingsContainer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* startupContainer;
        startupContainer = new wxBoxSizer(wxHORIZONTAL);

        auto startupText = new wxStaticText(this, wxID_ANY, _("Startup:"), wxDefaultPosition, wxDefaultSize, 0);
        startupText->Wrap(-1);
        startupContainer->Add(startupText, 0, wxALL, 5);

        auto autostartCheckbox =
            new wxCheckBox(this, wxID_ANY, _("Launch at login"), wxDefaultPosition, wxDefaultSize, 0);
        autostartCheckbox->SetValue(settings.autoStart);
        autostartCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.autoStart = isChecked;
            backend::toggleAutostart(isChecked);
            utils::saveSettings(settings);
        });

        auto odesliCheckbox =
            new wxCheckBox(this, wxID_ANY, _("Odesli integration"), wxDefaultPosition, wxDefaultSize, 0);
        odesliCheckbox->SetValue(settings.odesli);
        odesliCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.odesli = isChecked;
            utils::saveSettings(settings);
        });

        settingsContainer->Add(autostartCheckbox, 0, wxALL, 5);
        settingsContainer->Add(odesliCheckbox, 0, wxALL, 5);
        startupContainer->Add(settingsContainer);
        mainContainer->Add(startupContainer, 0, wxEXPAND, 5);

        // settings end
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
        if (!backend::init()) {
            wxMessageBox(_("Error initializing platform backend!"), _("PlayerLink"), wxOK | wxICON_ERROR);
            return false;
        }

        if (wxSystemSettings::GetAppearance().IsSystemDark())  // To support the native dark mode on windows 10 and up
            this->SetAppearance(wxAppBase::Appearance::Dark);

        wxInitAllImageHandlers();
        wxIcon icon = utils::loadIconFromMemory(icon_png, icon_png_size);
        PlayerLinkFrame* frame = new PlayerLinkFrame(nullptr, icon, wxID_ANY, _("PlayerLink"));
        trayIcon = new PlayerLinkIcon(frame);
        frame->Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent& event) {
            if (event.CanVeto()) {
                frame->Hide();
                event.Veto();
            } else
                std::exit(0);
        });

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