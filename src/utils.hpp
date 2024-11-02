#ifndef _UTILS_
#define _UTILS_
#include <curl/include/curl/curl.h>

#include <filesystem>
#include <fstream>
#include <nlohmann-json/single_include/nlohmann/json.hpp>
#include <sstream>
#include <string>

#define DEFAULT_CLIENT_ID "1301849203378622545"
#define DEFAULT_APP_NAME "Music"
#define CONFIG_FILENAME "known.json"

namespace utils {
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
        std::string new_str = "";
        char c;
        int ic;
        const char* chars = str.c_str();
        char bufHex[10];
        int len = strlen(chars);

        for (int i = 0; i < len; i++) {
            c = chars[i];
            ic = c;
            if (c == ' ')
                new_str += '+';
            else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                new_str += c;
            else {
                snprintf(bufHex, sizeof(bufHex), "%X", c);
                if (ic < 16)
                    new_str += "%0";
                else
                    new_str += "%";
                new_str += bufHex;
            }
        }
        return new_str;
    }

    inline size_t curlWriteCallback(char* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    inline std::string getRequest(std::string url) {
        CURL* curl;
        CURLcode res;
        std::string buf;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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
            getRequest("https://itunes.apple.com/search?media=music&entity=song&term=" + urlEncode(query));
        nlohmann::json j = nlohmann::json::parse(response);
        auto results = j["results"];
        if (results.size() > 0) {
            return results[0]["artworkUrl100"].get<std::string>();
        }
        return "";
    }

    inline nlohmann::json getApp(std::string processName) {
        std::ifstream i("known.json");
        std::stringstream s;
        s << i.rdbuf();
        i.close();

        try {
            nlohmann::json j = nlohmann::json::parse(s.str());
            auto apps = j["apps"];
            for (auto app : apps) {
                auto processNames = app["process_names"];
                for (auto process : processNames) {
                    if (process.get<std::string>() == processName)
                        return app;
                }
            }
        } catch (nlohmann::json::parse_error& ex) {
        }  // TODO: handle parse errors
        return nlohmann::json();
    }

    inline std::string getClientID(std::string processName) {
        if (!std::filesystem::exists(CONFIG_FILENAME))
            return DEFAULT_CLIENT_ID;
        auto app = getApp(processName);
        return app.contains("client_id") ? app["client_id"].get<std::string>() : DEFAULT_CLIENT_ID;
    }

    inline std::string getAppName(std::string processName) {
        if (!std::filesystem::exists(CONFIG_FILENAME))
            return DEFAULT_APP_NAME;
        auto app = getApp(processName);
        return app.contains("name") ? app["name"].get<std::string>() : DEFAULT_APP_NAME;
    }

    inline std::string getSearchEndpoint(std::string processName) {
        if (!std::filesystem::exists(CONFIG_FILENAME))
            return "";
        auto app = getApp(processName);
        return app.contains("search_endpoint") ? app["search_endpoint"].get<std::string>() : "";
    }
}  // namespace utils

#undef DEFAULT_APP_NAME
#undef CONFIG_FILENAME
#undef DEFAULT_CLIENT_ID
#endif