project(dbus-lib)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/vendor/libdbus/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/vendor/libdbus/cmake/modules")
SET(DBUS_DIR ${CMAKE_SOURCE_DIR}/vendor/libdbus/dbus)

include(MacrosAutotools)
autoinit(${CMAKE_SOURCE_DIR}/vendor/libdbus/configure.ac)
autoversion(dbus)

include (TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

set(DBUS_PATCH_VERSION "0")

include(Macros)
string(TIMESTAMP DBUS_BUILD_TIMESTAMP "%Y%m%d%H%M" UTC)
set(BUILD_FILEVERSION ${DBUS_MAJOR_VERSION},${DBUS_MINOR_VERSION},${DBUS_MICRO_VERSION},${DBUS_PATCH_VERSION})
set(BUILD_TIMESTAMP ${DBUS_BUILD_TIMESTAMP})
if(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DBUS_LINUX 1)
endif()
set(_GNU_SOURCE 1)
set(DBUS_LIBRARIES dbus-1)
set(DBUS_INTERNAL_LIBRARIES dbus-internal)

set(DBUS_INTERNAL_ADD_LIBRARY_OPTIONS STATIC)
set(DBUS_INTERNAL_CLIENT_DEFINITIONS "-DDBUS_COMPILATION")
add_definitions(-DHAVE_CONFIG_H)
include(ConfigureChecks)

set(DBUS_SESSION_SOCKET_DIR "" CACHE STRING "Default directory for session socket")

set(DBUS_SYSTEM_PID_FILE ${DBUS_RUNSTATEDIR}/dbus/pid)

set(DBUS_CONSOLE_AUTH_DIR "" CACHE STRING "Directory to check for pam_console/pam_foreground flag files, or empty to ignore")

set(DBUS_SYSTEM_BUS_DEFAULT_ADDRESS "unix:path=${DBUS_RUNSTATEDIR}/dbus/system_bus_socket" CACHE STRING "system bus default address")

set(DBUS_SESSION_SOCKET_DIR "" CACHE STRING "Default directory for session socket")
if(UNIX)
    if (CMAKE_CROSSCOMPILING)
        if (NOT DBUS_SESSION_SOCKET_DIR)
            message(FATAL_ERROR "cannot autodetect session socket directory "
                    "when crosscompiling, pass -DDBUS_SESSION_SOCKET_DIR=...")
        endif()
    elseif(NOT $ENV{TMPDIR} STREQUAL "")
        set(DBUS_SESSION_SOCKET_DIR $ENV{TMPDIR})
    elseif(NOT $ENV{TEMP} STREQUAL "")
        set(DBUS_SESSION_SOCKET_DIR $ENV{TEMP})
    elseif(NOT $ENV{TMP} STREQUAL "")
        set(DBUS_SESSION_SOCKET_DIR $ENV{TMP})
    else()
        set(DBUS_SESSION_SOCKET_DIR /tmp)
    endif()
endif()

set(DBUS_SESSION_BUS_LISTEN_ADDRESS "unix:tmpdir=${DBUS_SESSION_SOCKET_DIR}" CACHE STRING "session bus default listening address")
set(DBUS_SESSION_BUS_CONNECT_ADDRESS "autolaunch:" CACHE STRING "session bus fallback address for clients")
set(DBUS_SYSTEM_CONFIG_FILE  ${DBUS_DATADIR}/dbus-1/system.conf)
set(DBUS_SESSION_CONFIG_FILE ${DBUS_DATADIR}/dbus-1/session.conf)
set(DBUS_MACHINE_UUID_FILE   ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/dbus/machine-id)
set(DBUS_USER "messagebus")
set(DBUS_TEST_USER "nobody")
set(DBUS_SESSION_CONF_MAYBE_AUTH_EXTERNAL "<auth>EXTERNAL</auth>")


set(DBUS_DAEMON_NAME "dbus-daemon" CACHE STRING "The name of the dbus daemon executable")
configure_file(${CMAKE_SOURCE_DIR}/vendor/libdbus/cmake/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${DBUS_DIR}/dbus-arch-deps.h.in ${CMAKE_CURRENT_BINARY_DIR}/dbus/dbus-arch-deps.h )

add_definitions(-DDBUS_COMPILATION)

set (DBUS_LIB_SOURCES
	${DBUS_DIR}/dbus-address.c
	${DBUS_DIR}/dbus-auth.c
	${DBUS_DIR}/dbus-bus.c
	${DBUS_DIR}/dbus-test-tap.c
	${DBUS_DIR}/dbus-connection.c
	${DBUS_DIR}/dbus-credentials.c
	${DBUS_DIR}/dbus-errors.c
	${DBUS_DIR}/dbus-keyring.c
	${DBUS_DIR}/dbus-marshal-header.c
	${DBUS_DIR}/dbus-marshal-byteswap.c
	${DBUS_DIR}/dbus-marshal-recursive.c
	${DBUS_DIR}/dbus-marshal-validate.c
	${DBUS_DIR}/dbus-message.c
	${DBUS_DIR}/dbus-misc.c
	${DBUS_DIR}/dbus-nonce.c
	${DBUS_DIR}/dbus-object-tree.c
	${DBUS_DIR}/dbus-pending-call.c
	${DBUS_DIR}/dbus-resources.c
	${DBUS_DIR}/dbus-server.c
	${DBUS_DIR}/dbus-server-socket.c
	${DBUS_DIR}/dbus-server-debug-pipe.c
	${DBUS_DIR}/dbus-sha.c
	${DBUS_DIR}/dbus-signature.c
	${DBUS_DIR}/dbus-syntax.c
	${DBUS_DIR}/dbus-timeout.c
	${DBUS_DIR}/dbus-threads.c
	${DBUS_DIR}/dbus-transport.c
	${DBUS_DIR}/dbus-transport-socket.c
	${DBUS_DIR}/dbus-watch.c
	${DBUS_DIR}/dbus-transport-unix.c
	${DBUS_DIR}/dbus-server-unix.c
	${DBUS_DIR}/dbus-dataslot.c
	${DBUS_DIR}/dbus-file.c
	${DBUS_DIR}/dbus-hash.c
	${DBUS_DIR}/dbus-internals.c
	${DBUS_DIR}/dbus-list.c
	${DBUS_DIR}/dbus-marshal-basic.c
	${DBUS_DIR}/dbus-memory.c
	${DBUS_DIR}/dbus-mempool.c
	${DBUS_DIR}/dbus-string.c
	${DBUS_DIR}/dbus-sysdeps.c
	${DBUS_DIR}/dbus-pipe.c
	${DBUS_DIR}/dbus-file-unix.c
	${DBUS_DIR}/dbus-pipe-unix.c
	${DBUS_DIR}/dbus-sysdeps-unix.c
	${DBUS_DIR}/dbus-sysdeps-pthread.c
	${DBUS_DIR}/dbus-userdb.c
)
file(GLOB libdbus_config_dir
    "${CMAKE_BINARY_DIR}/vendor/dbus"
)

add_library(dbus STATIC ${DBUS_LIB_SOURCES})
target_include_directories(dbus PRIVATE ${libdbus_config_dir} ${CMAKE_SOURCE_DIR}/vendor/libdbus)