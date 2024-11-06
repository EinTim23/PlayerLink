#if !defined(_WIN32) && !defined(__APPLE__)
#include <dbus/dbus.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <unistd.h>

#include "../backend.hpp"

DBusConnection* conn = nullptr;

std::string getExecutablePath() {
    char result[PATH_MAX]{};
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return (count != -1) ? std::string(result, count) : std::string();
}

std::string getActivePlayer(DBusConnection* conn) {
    DBusMessage* msg;
    DBusMessageIter args;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                       "ListNames");
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        dbus_error_free(&err);
        return "";
    }

    dbus_message_iter_init(reply, &args);
    DBusMessageIter sub;
    dbus_message_iter_recurse(&args, &sub);

    std::string active_player;
    while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
        const char* name;
        dbus_message_iter_get_basic(&sub, &name);

        if (std::string(name).find("org.mpris.MediaPlayer2.") == 0) {
            active_player = name;
            break;
        }

        dbus_message_iter_next(&sub);
    }

    dbus_message_unref(reply);
    return active_player;
}

bool isPlayerPaused(DBusConnection* conn, const std::string& player) {
    DBusError err;
    DBusMessage* msg;
    DBusMessage* reply;
    DBusMessageIter args;
    const char* playbackStatus = nullptr;

    dbus_error_init(&err);

    msg = dbus_message_new_method_call(player.c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
                                       "Get");

    if (!msg)
        return false;

    const char* interface = "org.mpris.MediaPlayer2.Player";
    const char* property = "PlaybackStatus";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        if (dbus_error_is_set(&err))
            dbus_error_free(&err);

        return false;
    }

    if (dbus_message_iter_init(reply, &args) && DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&args)) {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);
        dbus_message_iter_get_basic(&variant, &playbackStatus);
    }

    bool isPaused = (playbackStatus && std::string(playbackStatus) == "Paused");

    dbus_message_unref(reply);
    return isPaused;
}

void getNowPlaying(DBusConnection* conn, const std::string& player, MediaInfo& mediaInfo) {
    DBusError err;
    dbus_error_init(&err);

    auto sendDBusMessage = [&](const char* property) -> DBusMessage* {
        DBusMessage* msg = dbus_message_new_method_call(player.c_str(), "/org/mpris/MediaPlayer2",
                                                        "org.freedesktop.DBus.Properties", "Get");
        const char* iface = "org.mpris.MediaPlayer2.Player";
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
        dbus_message_unref(msg);
        return reply;
    };

    auto processMetadata = [&](DBusMessage* reply) {
        if (!reply) return;
        
        DBusMessageIter args;
        dbus_message_iter_init(reply, &args);
        if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) return;

        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);
        if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_ARRAY) return;

        DBusMessageIter array_iter;
        dbus_message_iter_recurse(&variant, &array_iter);
        
        while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter dict_entry;
            dbus_message_iter_recurse(&array_iter, &dict_entry);

            const char* key;
            dbus_message_iter_get_basic(&dict_entry, &key);
            dbus_message_iter_next(&dict_entry);

            DBusMessageIter value_variant;
            dbus_message_iter_recurse(&dict_entry, &value_variant);

            if (std::string(key) == "xesam:title" && dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_STRING) {
                const char* title;
                dbus_message_iter_get_basic(&value_variant, &title);
                mediaInfo.songTitle = title;
            } else if (std::string(key) == "xesam:album" && dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_STRING) {
                const char* album;
                dbus_message_iter_get_basic(&value_variant, &album);
                mediaInfo.songAlbum = album;
            } else if (std::string(key) == "xesam:artist" && dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_ARRAY) {
                DBusMessageIter artist_array;
                dbus_message_iter_recurse(&value_variant, &artist_array);
                if (dbus_message_iter_get_arg_type(&artist_array) == DBUS_TYPE_STRING) {
                    const char* artist;
                    dbus_message_iter_get_basic(&artist_array, &artist);
                    mediaInfo.songArtist = artist;
                }
            } else if (std::string(key) == "mpris:length" && 
                      (dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_INT64 || dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_UINT64)) {
                int64_t length;
                dbus_message_iter_get_basic(&value_variant, &length);
                mediaInfo.songDuration = length / 1000;
            }
            dbus_message_iter_next(&array_iter);
        }
    };

    DBusMessage* metadataReply = sendDBusMessage("Metadata");
    processMetadata(metadataReply);
    dbus_message_unref(metadataReply);

    DBusMessage* positionReply = sendDBusMessage("Position");
    if (positionReply) {
        DBusMessageIter args;
        dbus_message_iter_init(positionReply, &args);
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
            DBusMessageIter variant;
            dbus_message_iter_recurse(&args, &variant);
            if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_INT64 || dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT64) {
                int64_t position;
                dbus_message_iter_get_basic(&variant, &position);
                mediaInfo.songElapsedTime = position / 1000;
            }
        }
        dbus_message_unref(positionReply);
    }

    dbus_error_free(&err);
}

bool backend::init() {
    DBusError err;
    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) {
        if (dbus_error_is_set(&err)) 
            dbus_error_free(&err);

        //fallback to system bus if user doesn't have a session specific bus
        conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
        if(!conn) {
            if (dbus_error_is_set(&err)) 
                dbus_error_free(&err);

            return false;
        }
    }
    return true;
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    MediaInfo ret;
    std::string player = getActivePlayer(conn);
    if (player == "")
        return nullptr;
    getNowPlaying(conn, player, ret);
    ret.paused = isPlayerPaused(conn, player);
    ret.playbackSource = player;
    return std::make_shared<MediaInfo>(ret);
}

bool backend::toggleAutostart(bool enabled) {
    const char* xdgHome = std::getenv("XDG_CONFIG_HOME");

    const char* home = std::getenv("HOME");

    if (!xdgHome && !home)
        return false;

    std::string realxdgHome = xdgHome ? xdgHome : home;
    std::filesystem::path xdgAutostartPath = realxdgHome;
    if (!xdgHome)
        xdgAutostartPath = xdgAutostartPath / ".config";
    xdgAutostartPath = xdgAutostartPath / "autostart";

    std::filesystem::create_directories(xdgAutostartPath);

    std::filesystem::path desktopPath = xdgAutostartPath / "PlayerLink.desktop";

    if (!enabled && std::filesystem::exists(desktopPath)) {
        std::filesystem::remove(desktopPath);
        return true;
    }

    // I would like to use std::format here, but a lot of modern linux distributions like Debian 12 still use clang 14
    // which doesnt support std::format
    std::string formattedDesktop = "[Desktop Entry]\nType=Application\nName=PlayerLink\nExec=" + getExecutablePath() +
                                   "\nX-GNOME-Autostart-enabled=true\n";
    std::ofstream o(desktopPath);
    o.write(formattedDesktop.c_str(), formattedDesktop.size());
    o.close();
    return true;
}

#undef XDG_AUTOSTART_TEMPLATE
#endif