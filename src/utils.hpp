#ifndef _UTILS_
#define _UTILS_
#include <filesystem>
#include <fstream>
#include <nlohmann-json/single_include/nlohmann/json.hpp>
#include <sstream>
#include <string>

#define DEFAULT_CLIENT_ID "1301849203378622545"
#define DEFAULT_APP_NAME "Music"
#define CONFIG_FILENAME "known.json"

namespace utils {
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
        return app["client_id"] == "" ? DEFAULT_CLIENT_ID : app["client_id"];
    }

    inline std::string getAppName(std::string processName) {
        if (!std::filesystem::exists(CONFIG_FILENAME))
            return DEFAULT_APP_NAME;
        auto app = getApp(processName);
        return app["name"] == "" ? DEFAULT_APP_NAME : app["name"];
    }

    inline std::string getSearchEndpoint(std::string processName) {
        if (!std::filesystem::exists(CONFIG_FILENAME))
            return "";
        auto app = getApp(processName);
        return app["search_endpoint"] == "" ? "" : app["search_endpoint"];
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
                sprintf(bufHex, "%X", c);
                if (ic < 16)
                    new_str += "%0";
                else
                    new_str += "%";
                new_str += bufHex;
            }
        }
        return new_str;
    }
}  // namespace utils

#undef DEFAULT_APP_NAME
#undef CONFIG_FILENAME
#undef DEFAULT_CLIENT_ID
#endif