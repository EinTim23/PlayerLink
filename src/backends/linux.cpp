#if !defined(_WIN32) && !defined(__APPLE__)
#include <dbus/dbus.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "../backend.hpp"

DBusConnection* conn = nullptr;

std::string getExecutablePath() {
    char result[PATH_MAX];
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

    if (!msg) {
        std::cerr << "Message Null" << std::endl;
        return false;
    }

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

void getNowPlaying(DBusConnection* conn, const std::string& player) {
    DBusMessage* msg;
    DBusMessageIter args;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(player.c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
                                       "Get");
    const char* iface = "org.mpris.MediaPlayer2.Player";
    const char* prop = "Metadata";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        std::cerr << "Failed to get metadata: " << (err.message ? err.message : "Unknown error") << std::endl;
        dbus_error_free(&err);
        return;
    }

    dbus_message_iter_init(reply, &args);
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);

        if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY) {
            DBusMessageIter array_iter;
            dbus_message_iter_recurse(&variant, &array_iter);

            while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter dict_entry;
                dbus_message_iter_recurse(&array_iter, &dict_entry);

                const char* key;
                dbus_message_iter_get_basic(&dict_entry, &key);
                dbus_message_iter_next(&dict_entry);

                if (std::string(key) == "xesam:title") {
                    DBusMessageIter value_variant;
                    dbus_message_iter_recurse(&dict_entry, &value_variant);

                    if (dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_STRING) {
                        const char* title;
                        dbus_message_iter_get_basic(&value_variant, &title);
                        std::cout << "Title: " << title << std::endl;
                    }
                } else if (std::string(key) == "xesam:artist") {
                    DBusMessageIter value_variant;
                    dbus_message_iter_recurse(&dict_entry, &value_variant);

                    if (dbus_message_iter_get_arg_type(&value_variant) == DBUS_TYPE_ARRAY) {
                        DBusMessageIter artist_array;
                        dbus_message_iter_recurse(&value_variant, &artist_array);

                        std::cout << "Artist(s): ";
                        bool first = true;
                        while (dbus_message_iter_get_arg_type(&artist_array) == DBUS_TYPE_STRING) {
                            const char* artist;
                            dbus_message_iter_get_basic(&artist_array, &artist);
                            if (!first)
                                std::cout << ", ";
                            std::cout << artist;
                            first = false;
                            dbus_message_iter_next(&artist_array);
                        }
                        std::cout << std::endl;
                    }
                } else if (std::string(key) == "mpris:length") {
                    DBusMessageIter value_variant;
                    dbus_message_iter_recurse(&dict_entry, &value_variant);
                    int arg_type = dbus_message_iter_get_arg_type(&value_variant);
                    if (arg_type == DBUS_TYPE_INT64 || arg_type == DBUS_TYPE_UINT64) {
                        int64_t length;
                        dbus_message_iter_get_basic(&value_variant, &length);
                        std::cout << "Length (Duration): " << length / 1000000.0 << " seconds" << std::endl;
                    }
                }

                dbus_message_iter_next(&array_iter);
            }
        }
    } else {
        std::cerr << "Unexpected reply type for Metadata" << std::endl;
    }

    dbus_message_unref(reply);
    msg = dbus_message_new_method_call(player.c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
                                       "Get");
    prop = "Position";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        std::cerr << "Failed to get position: " << (err.message ? err.message : "Unknown error") << std::endl;
        dbus_error_free(&err);
        return;
    }

    dbus_message_iter_init(reply, &args);
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);
        int arg_type = dbus_message_iter_get_arg_type(&variant);
        if (arg_type == DBUS_TYPE_INT64 || arg_type == DBUS_TYPE_UINT64) {
            int64_t position;
            dbus_message_iter_get_basic(&variant, &position);
            std::cout << "Current Position: " << position / 1000000.0 << " seconds" << std::endl;
        }
    }
    dbus_message_unref(reply);
}

bool backend::init() {
    DBusError err;
    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) {
        dbus_error_free(&err);
        return false;
    }
    return true;
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    std::string player = getActivePlayer(conn);
    if (player == "")
        return nullptr;
    getNowPlaying(conn, player);
    std::cout << "isPaused: " << isPlayerPaused(conn, player) << " \n";
    return nullptr;
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

    std::string formattedPlist = "[Desktop Entry]\nType=Application\nName=PlayerLink\nExec=" + getExecutablePath() +
                                 "\nX-GNOME-Autostart-enabled=true\n";
    std::ofstream o(desktopPath);
    o.write(formattedPlist.c_str(), formattedPlist.size());
    o.close();
    return true;
}

#undef XDG_AUTOSTART_TEMPLATE
#endif