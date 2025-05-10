#include <discord-rpc/discord_rpc.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/statline.h>
#include <wx/taskbar.h>
#include <wx/hyperlink.h>
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
        if (settings.odesli && songInfo.artworkURL != "") {
            activity.button2name = "Show on Song.link";
            activity.button2link = odesliUrl.c_str();
        }

        Discord_UpdatePresence(&activity);
    }
}

class AboutDialog : public wxDialog {
public:
    AboutDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, _("About PlayerLink"), wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE & ~wxRESIZE_BORDER) {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY, _("Made with <3 by EinTim"));
        label->Wrap(300);
        mainSizer->Add(label, 0, wxALL | wxALIGN_CENTER, 10);

        wxStaticText* copyrightText = new wxStaticText(this, wxID_ANY, "© 2024-2025 EinTim. All rights reserved.");
        copyrightText->Wrap(300);
        mainSizer->Add(copyrightText, 0, wxALL | wxALIGN_CENTER, 10);

        wxStaticText* creditsText =
            new wxStaticText(this, wxID_ANY,
                             _("Credits:\n- Developer: EinTim\n- Inspiration: Alexandra Göttlicher\n- Icons from: "
                               "heroicons.com\n- Open source "
                               "projects used in this:\n   wxWidgets, libcurl, libdbus, mbedtls, nlohmann-json."));
        creditsText->Wrap(300);
        mainSizer->Add(creditsText, 0, wxALL | wxALIGN_CENTER, 10);

        wxHyperlinkCtrl* link = new wxHyperlinkCtrl(this, wxID_ANY, _("Visit my website"), _("https://eintim.dev"));
        mainSizer->Add(link, 0, wxALL | wxALIGN_CENTER, 10);

        wxButton* okButton = new wxButton(this, wxID_OK, _("OK"));
        mainSizer->Add(okButton, 0, wxALL | wxALIGN_CENTER, 10);

        this->SetSizerAndFit(mainSizer);
        this->CenterOnScreen();
    }
};

class EditAppDialog : public wxDialog {
public:
    EditAppDialog(wxWindow* parent, const wxString title, utils::App* app)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE & ~wxRESIZE_BORDER) {
        Bind(wxEVT_CLOSE_WINDOW, &EditAppDialog::OnClose, this);

        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxFlexGridSizer* formSizer = new wxFlexGridSizer(2, 0, 5);

        // Make the second column growable (the one with the text controls)
        formSizer->AddGrowableCol(1, 1);

        // Application name
        formSizer->Add(new wxStaticText(this, wxID_ANY, _("Application name:")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        auto nameInput = new wxTextCtrl(this, wxID_ANY);
        nameInput->SetValue(app->appName);
        nameInput->SetHint(_("Example: Apple Music"));
        nameInput->Bind(wxEVT_TEXT,
                        [this, app](wxCommandEvent& event) { app->appName = event.GetString().ToStdString(); });
        formSizer->Add(nameInput, 1, wxALL | wxEXPAND, 5);

        // Client ID
        formSizer->Add(new wxStaticText(this, wxID_ANY, _("Discord client id:")), 0, wxALL | wxALIGN_CENTER_VERTICAL,
                       5);
        auto clientIdInput = new wxTextCtrl(this, wxID_ANY);
        clientIdInput->SetHint(_("Example: 1337188104829665340"));
        clientIdInput->SetValue(app->clientId);
        clientIdInput->Bind(wxEVT_TEXT,
                            [this, app](wxCommandEvent& event) { app->clientId = event.GetString().ToStdString(); });
        formSizer->Add(clientIdInput, 1, wxALL | wxEXPAND, 5);

        // Search endpoint
        formSizer->Add(new wxStaticText(this, wxID_ANY, _("Search endpoint:")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        auto searchEndpointInput = new wxTextCtrl(this, wxID_ANY);
        searchEndpointInput->SetValue(app->searchEndpoint);
        searchEndpointInput->SetHint(_("Search endpoint: https://music.apple.com/search?term="));
        searchEndpointInput->Bind(
            wxEVT_TEXT, [this, app](wxCommandEvent& event) { app->searchEndpoint = event.GetString().ToStdString(); });
        formSizer->Add(searchEndpointInput, 1, wxALL | wxEXPAND, 5);

        mainSizer->Add(formSizer, 0, wxEXPAND | wxALL, 5);

        // Dropdown
        wxBoxSizer* dropdownSizer = new wxBoxSizer(wxHORIZONTAL);
        dropdownSizer->Add(new wxStaticText(this, wxID_ANY, "Activity Type:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        wxString choices[] = {_("Listening"), _("Watching"), _("Playing")};
        wxChoice* activityChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, choices);

        switch (app->type) {
        case ActivityType::PLAYING:
            activityChoice->SetSelection(2);
            break;
        case ActivityType::LISTENING:
            activityChoice->SetSelection(0);
            break;
        case ActivityType::WATCHING:
            activityChoice->SetSelection(1);
            break;
        default:
            activityChoice->SetSelection(0);
        }

        activityChoice->Bind(wxEVT_CHOICE, [activityChoice, app](wxCommandEvent& event) {
            const std::map<wxString, ActivityType> typeMap = {
                {_("Listening"), ActivityType::LISTENING},
                {_("Watching"), ActivityType::WATCHING},
                {_("Playing"), ActivityType::PLAYING},
            };
            app->type = typeMap.at(event.GetString());
        });

        dropdownSizer->Add(activityChoice, 1, wxALL | wxEXPAND, 5);
        mainSizer->Add(dropdownSizer, 0, wxEXPAND);

        // Process names group
        wxStaticBoxSizer* processBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Process names"));

        wxListBox* listBox = new wxListBox(this, wxID_ANY);

        for (auto& process : app->processNames) {
            listBox->Append(process);
        }

        processBox->Add(listBox, 1, wxALL | wxEXPAND, 5);

        // Add input + buttons
        wxBoxSizer* addSizer = new wxBoxSizer(wxHORIZONTAL);
        auto processNameInput =
            new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, wxDefaultSize.GetHeight()), 0);
        processNameInput->SetHint(_("Process name"));
        const auto delete_button_texture = utils::loadSettingsIcon(trash_svg, trash_svg_size);
        const auto add_button_texture = utils::loadSettingsIcon(plus_svg, plus_svg_size);
        wxBitmapButton* addButton = new wxBitmapButton(this, wxID_ANY, add_button_texture);
        wxBitmapButton* removeButton = new wxBitmapButton(this, wxID_ANY, delete_button_texture);

        addSizer->Add(processNameInput, 1, wxALL | wxEXPAND, 5);
        addSizer->Add(addButton, 0, wxALL, 5);
        addSizer->Add(removeButton, 0, wxALL, 5);
        processBox->Add(addSizer, 0, wxEXPAND);
        mainSizer->Add(processBox, 1, wxALL | wxEXPAND, 5);

        SetSizerAndFit(mainSizer);
        Centre();

        // Bind events
        addButton->Bind(wxEVT_BUTTON, [processNameInput, listBox, app](wxCommandEvent& event) {
            wxString name = processNameInput->GetValue().Trim();
            if (!name.IsEmpty()) {
                listBox->Append(name);
                app->processNames.push_back(name.ToStdString());
                processNameInput->Clear();
            }
        });

        removeButton->Bind(wxEVT_BUTTON, [listBox, app](wxCommandEvent& event) {
            int selection = listBox->GetSelection();

            if (selection != wxNOT_FOUND) {
                app->processNames.erase(std::find(app->processNames.begin(), app->processNames.end(),
                                                  listBox->GetString(selection).ToStdString()));
                listBox->Delete(selection);
            }
        });

        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

        wxButton* okButton = new wxButton(this, wxID_OK, _("Save"));
        wxButton* cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
        buttonSizer->Add(okButton, 0, wxALL | wxALIGN_CENTER, 10);
        buttonSizer->Add(cancelButton, 0, wxALL | wxALIGN_CENTER, 10);

        mainSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER);
        this->SetSizerAndFit(mainSizer);
        this->CenterOnParent();
    }

private:
    void OnClose(wxCloseEvent& event) { EndModal(wxID_CANCEL); }
};

class PlayerLinkIcon : public wxTaskBarIcon {
public:
    PlayerLinkIcon(wxFrame* s) : settingsFrame(s), aboutDlg(nullptr) {}

protected:
    virtual wxMenu* CreatePopupMenu() override {
        wxMenu* menu = new wxMenu;
        menu->Append(10004, currentSongTitle == "" ? _("Not Playing") : currentSongTitle);
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
    void OnMenuOpen(wxCommandEvent& evt) {
        settingsFrame->Show(true);
        settingsFrame->Raise();
    }

    void OnCopyOdesliURL(wxCommandEvent& evt) { utils::copyToClipboard(utils::getOdesliURL(songInfo)); }

    void OnMenuExit(wxCommandEvent& evt) { settingsFrame->Close(true); }

    void OnMenuAbout(wxCommandEvent& evt) {
        aboutDlg.Show(true);
        aboutDlg.Raise();
    }
    wxFrame* settingsFrame;
    AboutDialog aboutDlg;
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
        wxPanel* panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        panel->SetSizer(mainContainer);

        wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);
        frameSizer->Add(panel, 1, wxEXPAND);
        // header start
        auto settingsText = new wxStaticText(panel, wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize, 0);
        settingsText->Wrap(-1);
        mainContainer->Add(settingsText, 0, wxALIGN_CENTER | wxALL, 5);
        // header end

        // enabled apps start
        auto settingsDivider = new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(settingsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* enabledAppsContainer;
        enabledAppsContainer = new wxBoxSizer(wxHORIZONTAL);

        auto enabledAppsText =
            new wxStaticText(panel, wxID_ANY, _("Enabled Apps:"), wxDefaultPosition, wxDefaultSize, 0);
        enabledAppsText->Wrap(-1);
        enabledAppsContainer->Add(enabledAppsText, 0, wxALL, 5);

        const auto edit_button_texture = utils::loadSettingsIcon(pencil_svg, pencil_svg_size);
        const auto delete_button_texture = utils::loadSettingsIcon(trash_svg, trash_svg_size);
        const auto add_button_texture = utils::loadSettingsIcon(plus_svg, plus_svg_size);

        wxBoxSizer* appCheckboxContainer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* rowSizer = new wxBoxSizer(wxHORIZONTAL);

        wxBitmapButton* addButton = new wxBitmapButton(panel, wxID_ANY, add_button_texture);
        addButton->Bind(wxEVT_BUTTON, [this, panel, frameSizer, size, delete_button_texture, edit_button_texture,
                                       appCheckboxContainer](wxCommandEvent& event) {
            utils::App* app = new utils::App();
            EditAppDialog dlg{this, _("Add new application"), app};
            if (dlg.ShowModal() == wxID_OK) {
                auto settings = utils::getSettings();
                settings.apps.push_back(*app);
                utils::saveSettings(settings);
                addCheckboxToContainer(panel, appCheckboxContainer, frameSizer, size, delete_button_texture,
                                       edit_button_texture, *app);
                this->SetSizerAndFit(frameSizer);
                wxSize currentSize = this->GetSize();
                this->SetSize(size.GetWidth(), currentSize.GetHeight());
                this->Layout();
            } else
                delete app;
        });
        rowSizer->Add(addButton, 0, wxALL | wxALIGN_CENTER_VERTICAL);

        wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
        vSizer->Add(rowSizer, 0, wxALL, 5);
        enabledAppsContainer->Add(vSizer, 0, wxEXPAND);
        auto settings = utils::getSettings();

        for (const auto& app : settings.apps) {
            addCheckboxToContainer(panel, appCheckboxContainer, frameSizer, size, delete_button_texture,
                                   edit_button_texture, app);
        }

        auto anyOtherCheckbox = new wxCheckBox(panel, wxID_ANY, _("Any other"), wxDefaultPosition, wxDefaultSize, 0);
        anyOtherCheckbox->SetValue(settings.anyOtherEnabled);
        anyOtherCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.anyOtherEnabled = isChecked;
            utils::saveSettings(settings);
        });

        vSizer->Add(appCheckboxContainer, 1, wxEXPAND, 5);
        vSizer->Add(anyOtherCheckbox, 1, wxALL, 5);

        mainContainer->Add(enabledAppsContainer, 0, 0, 5);
        // enabled apps end

        // LastFM start
        auto lastfmDivider = new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(lastfmDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* lastFMContainer;
        lastFMContainer = new wxBoxSizer(wxHORIZONTAL);

        auto lastfmText = new wxStaticText(panel, wxID_ANY, _("LastFM:"), wxDefaultPosition, wxDefaultSize, 0);
        lastfmText->Wrap(-1);
        lastFMContainer->Add(lastfmText, 0, wxALL, 5);

        wxBoxSizer* lastfmSettingsContainer;
        lastfmSettingsContainer = new wxBoxSizer(wxVERTICAL);

        auto lastfmEnabledCheckbox = new wxCheckBox(panel, wxID_ANY, _("Enabled"), wxDefaultPosition, wxDefaultSize, 0);
        lastfmEnabledCheckbox->SetValue(settings.lastfm.enabled);
        lastfmEnabledCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.lastfm.enabled = isChecked;
            utils::saveSettings(settings);
        });
        lastfmSettingsContainer->Add(lastfmEnabledCheckbox, 0, wxALIGN_CENTER | wxALL, 5);
        lastFMContainer->Add(lastfmSettingsContainer, 1, wxEXPAND, 5);

        auto usernameInput = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
        usernameInput->SetHint(_("Username"));
        usernameInput->SetValue(settings.lastfm.username);
        usernameInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.username = data;
            utils::saveSettings(settings);
        });
        auto passwordInput =
            new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
        passwordInput->SetHint(_("Password"));
        passwordInput->SetValue(settings.lastfm.password);
        passwordInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.password = data;
            utils::saveSettings(settings);
        });
        auto apikeyInput = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
        apikeyInput->SetHint(_("API-Key"));
        apikeyInput->SetValue(settings.lastfm.api_key);
        apikeyInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.api_key = data;
            utils::saveSettings(settings);
        });
        auto apisecretInput =
            new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
        apisecretInput->SetHint(_("API-Secret"));
        apisecretInput->SetValue(settings.lastfm.api_secret);
        apisecretInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            auto settings = utils::getSettings();
            std::string data = event.GetString().ToStdString();
            settings.lastfm.api_secret = data;
            utils::saveSettings(settings);
        });

        auto checkButton = new wxButton(panel, wxID_ANY, _("Check credentials"));
        checkButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) { initLastFM(true); });
        mainContainer->Add(lastFMContainer, 0, 0, 5);

        mainContainer->Add(usernameInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(passwordInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(apikeyInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(apisecretInput, 0, wxEXPAND | wxALL, 5);
        mainContainer->Add(checkButton, 0, wxEXPAND | wxALL, 10);

        // Last FM End

        // settings start
        auto appsDivider = new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
        mainContainer->Add(appsDivider, 0, wxEXPAND | wxALL, 5);

        wxBoxSizer* settingsContainer;
        settingsContainer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* startupContainer;
        startupContainer = new wxBoxSizer(wxHORIZONTAL);

        auto startupText = new wxStaticText(panel, wxID_ANY, _("Startup:"), wxDefaultPosition, wxDefaultSize, 0);
        startupText->Wrap(-1);
        startupContainer->Add(startupText, 0, wxALL, 5);

        auto autostartCheckbox =
            new wxCheckBox(panel, wxID_ANY, _("Launch at login"), wxDefaultPosition, wxDefaultSize, 0);
        autostartCheckbox->SetValue(settings.autoStart);
        autostartCheckbox->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& event) {
            bool isChecked = event.IsChecked();
            auto settings = utils::getSettings();
            settings.autoStart = isChecked;
            backend::toggleAutostart(isChecked);
            utils::saveSettings(settings);
        });

        auto odesliCheckbox =
            new wxCheckBox(panel, wxID_ANY, _("Odesli integration"), wxDefaultPosition, wxDefaultSize, 0);
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

        this->SetSizerAndFit(frameSizer);
        wxSize currentSize = this->GetSize();
        this->SetSize(size.GetWidth(), currentSize.GetHeight());
        this->Layout();
        this->Centre(wxBOTH);
        panel->SetFocus();
    }

private:
    void addCheckboxToContainer(wxPanel* panel, wxBoxSizer* container, wxBoxSizer* frameSizer, const wxSize& size,
                                const wxBitmap& delete_button_texture, const wxBitmap& edit_button_texture,
                                const utils::App& app) {
        wxBoxSizer* checkboxRowSizer = new wxBoxSizer(wxHORIZONTAL);

        auto checkbox = new wxCheckBox(panel, wxID_ANY, app.appName, wxDefaultPosition, wxDefaultSize, 0);
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

        wxBitmapButton* editButton = new wxBitmapButton(panel, wxID_ANY, edit_button_texture);
        editButton->Bind(wxEVT_BUTTON, [this, checkbox](wxCommandEvent& event) {
            utils::App* appData = static_cast<utils::App*>(checkbox->GetClientData());
            utils::App backupApp = *appData;
            EditAppDialog dlg{this, _("Edit application") + " " + appData->appName, appData};
            if (dlg.ShowModal() == wxID_OK) {
                auto settings = utils::getSettings();
                for (auto& app : settings.apps) {
                    if (app == backupApp)
                        app = *appData;
                }
                utils::saveSettings(settings);
                checkbox->SetLabelText(appData->appName);
                this->Layout();
            }
        });

        wxBitmapButton* deleteButton = new wxBitmapButton(panel, wxID_ANY, delete_button_texture);
        deleteButton->Bind(wxEVT_BUTTON,
                           [this, checkboxRowSizer, container, frameSizer, checkbox, size](wxCommandEvent& event) {
                               utils::App* appData = static_cast<utils::App*>(checkbox->GetClientData());
                               auto settings = utils::getSettings();
                               settings.apps.erase(std::find(settings.apps.begin(), settings.apps.end(), *appData));
                               utils::saveSettings(settings);
                               container->Detach(checkboxRowSizer);

                               this->CallAfter([this, checkboxRowSizer, frameSizer, size]() {
                                   checkboxRowSizer->Clear(true);
                                   this->SetSizerAndFit(frameSizer);
                                   wxSize currentSize = this->GetSize();
                                   this->SetSize(size.GetWidth(), currentSize.GetHeight());
                                   this->Layout();
                               });
                           });

        checkboxRowSizer->Add(checkbox, 1, wxALL | wxALIGN_CENTER_VERTICAL);
        checkboxRowSizer->Add(editButton, 0, wxALL | wxALIGN_CENTER_VERTICAL);
        checkboxRowSizer->Add(deleteButton, 0, wxALL | wxALIGN_CENTER_VERTICAL);

        container->Add(checkboxRowSizer, 0, wxALL, 5);
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