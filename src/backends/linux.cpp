#if !defined(_WIN32) && !defined(__APPLE__)
#include <dbus/dbus.h>

#include <iostream>

#include "../backend.hpp"
bool initialized = false;
DBusConnection* conn = nullptr;

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

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    if (!initialized) {
        DBusError err;
        dbus_error_init(&err);

        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (!conn) {
            dbus_error_free(&err);
            return nullptr;
        }
        initialized = true;
    }
    std::string player = getActivePlayer(conn);
    if (player == "")
        return nullptr;
    getNowPlaying(conn, player);
    return nullptr;
}

bool backend::toggleAutostart(bool enabled) { return false; }
#endif