#ifndef _UTILS_
#define _UTILS_
#include <curl/include/curl/curl.h>
#include <wx/mstream.h>
#include <wx/wx.h>

#include <filesystem>
#include <fstream>
#include <nlohmann-json/single_include/nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "backend.hpp"

#define DEFAULT_CLIENT_ID "1301849203378622545"
#define DEFAULT_APP_NAME "Music"
#define CONFIG_FILENAME backend::getConfigDirectory() / "settings.json"

namespace utils {
    struct App {
        bool enabled;
        std::string appName;
        std::string clientId;
        std::string searchEndpoint;
        std::vector<std::string> processNames;
    };

    struct LastFMSettings {
        bool enabled;
        std::string username;
        std::string password;
        std::string api_key;
        std::string api_secret;
    };

    struct Settings {
        bool autoStart;
        bool anyOtherEnabled;
        LastFMSettings lastfm;
        std::vector<App> apps;
    };

    inline wxIcon loadIconFromMemory(const unsigned char* data, size_t size) {
        wxMemoryInputStream stream(data, size);
        wxImage img(stream, wxBITMAP_TYPE_PNG);
        if (img.IsOk()) {
            wxBitmap bmp(img);
            wxIcon icon;
            icon.CopyFromBitmap(bmp);
            return icon;
        }
        return wxNullIcon;
    }

    inline std::string ltrim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        return s;
    }

    inline std::string rtrim(std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        return s;
    }

    inline std::string trim(std::string& s) {
        ltrim(s);
        rtrim(s);
        return s;
    }

    inline std::string urlEncode(std::string str) {
        std::ostringstream encoded;
        encoded << std::hex << std::uppercase;

        for (unsigned char c : str) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else {
                encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            }
        }

        return encoded.str();
    }

    inline size_t curlWriteCallback(char* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    inline std::string getURLEncodedPostBody(const std::map<std::string, std::string>& parameters) {
        std::string encodedPostBody = "";
        for (const auto& parameter : parameters) {
            encodedPostBody += parameter.first;
            encodedPostBody += "=";
            encodedPostBody += parameter.second;
            encodedPostBody += "&";
        }
        return encodedPostBody.erase(encodedPostBody.length() - 1);
    }

    inline std::string httpRequest(std::string url, std::string requestType = "GET", std::string postData = "") {
        CURL* curl;
        CURLcode res;
        std::string buf;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, requestType.c_str());
            if (requestType != "GET" && requestType != "DELETE" && postData.length() > 0)
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
        return buf;
    }

    inline std::string getArtworkURL(std::string query) {
        std::string response =
            httpRequest("https://itunes.apple.com/search?media=music&entity=song&term=" + urlEncode(query));
        nlohmann::json j = nlohmann::json::parse(response);
        auto results = j["results"];
        if (results.size() > 0) {
            return results[0]["artworkUrl100"].get<std::string>();
        }
        return "";
    }

    inline void saveSettings(const App* newApp) {
        nlohmann::json j;

        if (std::filesystem::exists(CONFIG_FILENAME)) {
            std::ifstream i(CONFIG_FILENAME);
            i >> j;
        }

        if (!j.contains("apps")) {
            j["apps"] = nlohmann::json::array();
        }

        bool appFound = false;

        for (auto& appJson : j["apps"]) {
            if (appJson["name"] == newApp->appName) {
                appJson["client_id"] = newApp->clientId;
                appJson["search_endpoint"] = newApp->searchEndpoint;
                appJson["enabled"] = newApp->enabled;

                appJson["process_names"].clear();
                for (const auto& processName : newApp->processNames) {
                    appJson["process_names"].push_back(processName);
                }

                appFound = true;
                break;
            }
        }

        if (!appFound) {
            nlohmann::json appJson;
            appJson["name"] = newApp->appName;
            appJson["client_id"] = newApp->clientId;
            appJson["search_endpoint"] = newApp->searchEndpoint;
            appJson["enabled"] = newApp->enabled;

            for (const auto& processName : newApp->processNames) {
                appJson["process_names"].push_back(processName);
            }

            j["apps"].push_back(appJson);
        }

        std::ofstream o(CONFIG_FILENAME);
        o << j.dump(4);
        o.close();
    }

    inline void saveSettings(const Settings& settings) {
        nlohmann::json j;
        j["autostart"] = settings.autoStart;
        j["any_other"] = settings.anyOtherEnabled;

        j["lastfm"]["enabled"] = settings.lastfm.enabled;
        j["lastfm"]["api_key"] = settings.lastfm.api_key;
        j["lastfm"]["api_secret"] = settings.lastfm.api_secret;
        j["lastfm"]["username"] = settings.lastfm.username;
        j["lastfm"]["password"] = settings.lastfm.password;

        for (const auto& app : settings.apps) {
            nlohmann::json appJson;
            appJson["name"] = app.appName;
            appJson["client_id"] = app.clientId;
            appJson["search_endpoint"] = app.searchEndpoint;
            appJson["enabled"] = app.enabled;

            for (const auto& processName : app.processNames) appJson["process_names"].push_back(processName);

            j["apps"].push_back(appJson);
        }

        std::ofstream o(CONFIG_FILENAME);
        o << j.dump(4);
        o.close();
    }
    inline Settings getSettings() {
        std::filesystem::create_directories(backend::getConfigDirectory());
        Settings ret;
        if (!std::filesystem::exists(CONFIG_FILENAME)) {
            ret.anyOtherEnabled = true;
            ret.autoStart = false;
            saveSettings(ret);
            return ret;
        }

        try {
            std::ifstream i(CONFIG_FILENAME);
            nlohmann::json j;
            i >> j;

            ret.autoStart = j.value("autostart", false);
            ret.anyOtherEnabled = j.value("any_other", false);

            if (j.contains("lastfm")) {
                auto lastfm = j["lastfm"];
                ret.lastfm.enabled = lastfm.value("enabled", false);
                ret.lastfm.api_key = lastfm.value("api_key", "");
                ret.lastfm.api_secret = lastfm.value("api_secret", "");
                ret.lastfm.username = lastfm.value("username", "");
                ret.lastfm.password = lastfm.value("password", "");
            }

            for (const auto& app : j["apps"]) {
                App a;
                a.appName = app.value("name", "");
                a.clientId = app.value("client_id", "");
                a.searchEndpoint = app.value("search_endpoint", "");
                a.enabled = app.value("enabled", false);

                for (const auto& process : app["process_names"]) a.processNames.push_back(process.get<std::string>());

                ret.apps.push_back(a);
            }
        } catch (const nlohmann::json::parse_error&) {
        }
        return ret;
    }

    inline App getApp(std::string processName) {
        auto settings = getSettings();
        for (auto app : settings.apps) {
            for (auto procName : app.processNames) {
                if (procName == processName)
                    return app;
            }
        }
        App a;
        a.clientId = DEFAULT_CLIENT_ID;
        a.appName = DEFAULT_APP_NAME;
        a.enabled = settings.anyOtherEnabled;
        a.searchEndpoint = "";
        return a;
    }
}  // namespace utils

#undef DEFAULT_APP_NAME
#undef CONFIG_FILENAME
#undef DEFAULT_CLIENT_ID
#endif