#ifndef _LASTFM_
#define _LASTFM_

#include <md5.hpp>
#include <string>

#include "utils.hpp"

class LastFM {
public:
    enum LASTFM_STATUS {
        SUCCESS = 0,
        AUTHENTICATION_FAILED = 4,
        INVALID_API_KEY = 10,
        RATE_LIMIT_REACHED = 29,
        API_KEY_SUSPENDED = 26,
        UNKNOWN_ERROR = 16,
        INVALID_SESSION_KEY = 9,
        SERVICE_TEMPORARILY_UNAVAILABLE = 13,
    };

    LastFM(std::string u, std::string p, std::string ak, std::string as)
        : username(u), password(p), api_key(ak), api_secret(as), authenticated(false) {}

    std::string getApiSignature(const std::map<std::string, std::string>& parameters) {
        std::string unhashedSignature = "";
        std::map<std::string, std::string> sortedParameters = parameters;

        for (const auto& parameter : sortedParameters) {
            if (parameter.first == "format" || parameter.first == "callback")
                continue;
            unhashedSignature += parameter.first + parameter.second;
        }
        unhashedSignature += api_secret;
        return md5::md5_hash_hex(unhashedSignature);
    }

    LASTFM_STATUS authenticate() {
        std::map<std::string, std::string> parameters = {{"api_key", api_key},
                                                         {"password", password},
                                                         {"username", username},
                                                         {"method", "auth.getMobileSession"},
                                                         {"format", "json"}};
        parameters["api_sig"] = getApiSignature(parameters);
        std::string postBody = utils::getURLEncodedPostBody(parameters);
        std::string response = utils::httpRequest(api_base, "POST", postBody);
        auto j = nlohmann::json::parse(response);
        if (j.contains("error"))
            return j["error"].get<LASTFM_STATUS>();

        session_token = j["session"]["key"].get<std::string>();
        authenticated = true;
        return LASTFM_STATUS::SUCCESS;
    }

    LASTFM_STATUS scrobble(std::string artist, std::string track) {
        if (!authenticated)
            return LASTFM_STATUS::AUTHENTICATION_FAILED;

        std::map<std::string, std::string> parameters = {
            {"api_key", api_key},  {"method", "track.scrobble"},
            {"sk", session_token}, {"artist", artist},
            {"track", track},      {"timestamp", std::to_string(time(NULL))},
            {"format", "json"}};

        parameters["api_sig"] = getApiSignature(parameters);

        // if you url encode the parameters before making the signature, the signature will be invalid. But url encoding
        // is needed to properly handle non ascii characters. Notice to myself: The last fm api is horrible.
        parameters["artist"] = utils::urlEncode(artist);
        parameters["track"] = utils::urlEncode(track);
        std::string postBody = utils::getURLEncodedPostBody(parameters);
        std::string response = utils::httpRequest(api_base, "POST", postBody);

        return LASTFM_STATUS::SUCCESS;
    }

private:
    bool authenticated;
    std::string session_token;
    std::string username;
    std::string password;
    std::string api_key;
    std::string api_secret;
    const std::string api_base = "https://ws.audioscrobbler.com/2.0/";
};

#endif