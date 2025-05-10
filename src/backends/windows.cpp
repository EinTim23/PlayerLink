#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <psapi.h>
#include <shlobj.h>
#include <windows.h>
#include <appmodel.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.metadata.h>
#include <winrt/windows.media.control.h>
#include <winrt/windows.storage.streams.h>

#include <chrono>
#include <filesystem>

#include "../backend.hpp"
#include "../utils.hpp"

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;
#define EM_DASH "\xE2\x80\x94"

std::string toStdString(winrt::hstring& in) {
    const wchar_t* wideStr = in.c_str();
    int wideStrLen = static_cast<int>(in.size());
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, nullptr, 0, nullptr, nullptr);
    if (bufferSize <= 0)
        return "";

    std::string result(bufferSize, 0);

    WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, result.data(), bufferSize, nullptr, nullptr);

    return result;
}

bool CreateShortcut(std::string source, std::string target) {
    CoInitialize(nullptr);
    WCHAR src[MAX_PATH];
    IShellLinkW* pShellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink,
                                  reinterpret_cast<void**>(&pShellLink));

    if (SUCCEEDED(hr) && pShellLink) {
        MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, src, MAX_PATH);
        pShellLink->SetPath(src);

        IPersistFile* pPersistFile = nullptr;
        hr = pShellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pPersistFile));

        if (SUCCEEDED(hr) && pPersistFile) {
            WCHAR dst[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, target.c_str(), -1, dst, MAX_PATH);
            hr = pPersistFile->Save(dst, TRUE);
            pPersistFile->Release();
        }

        pShellLink->Release();
    }

    CoUninitialize();
    return SUCCEEDED(hr);
}

std::filesystem::path backend::getConfigDirectory() {
    std::filesystem::path configDirectoryPath = std::getenv("APPDATA");
    configDirectoryPath = configDirectoryPath / "PlayerLink";
    return configDirectoryPath;
}

bool backend::toggleAutostart(bool enabled) {
    std::filesystem::path shortcutPath = std::getenv("APPDATA");
    shortcutPath = shortcutPath / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";
    std::filesystem::create_directories(shortcutPath);
    shortcutPath = shortcutPath / "PlayerLink.lnk";

    if (!enabled && std::filesystem::exists(shortcutPath)) {
        std::filesystem::remove(shortcutPath);
        return true;
    }
    char binaryPath[MAX_PATH]{};
    GetModuleFileNameA(NULL, binaryPath, MAX_PATH);
    bool result = CreateShortcut(binaryPath, shortcutPath.string());
    return result;
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    static auto sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = sessionManager.GetCurrentSession();
    if (!currentSession)
        return nullptr;

    auto playbackInfo = currentSession.GetPlaybackInfo();
    try {
        auto mediaProperties = currentSession.TryGetMediaPropertiesAsync().get();
        auto timelineInformation = currentSession.GetTimelineProperties();
        if (!mediaProperties)
            return nullptr;

        auto endTime = std::chrono::duration_cast<std::chrono::milliseconds>(timelineInformation.EndTime()).count();
        auto elapsedTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(timelineInformation.Position()).count();

        auto thumbnail = mediaProperties.Thumbnail();
        std::string thumbnailData = "";

        if (thumbnail) {
            auto stream = thumbnail.OpenReadAsync().get();
            size_t size = static_cast<size_t>(stream.Size());

            DataReader reader(stream);
            reader.LoadAsync(static_cast<uint32_t>(size)).get();

            std::vector<uint8_t> buffer(size);
            reader.ReadBytes(buffer);
            reader.Close();

            thumbnailData = std::string(buffer.begin(), buffer.end());
            stream.Close();
        }

        std::string artist = toStdString(mediaProperties.Artist());
        std::string albumName = toStdString(mediaProperties.AlbumTitle());
        if (artist == "")
            artist = toStdString(mediaProperties.AlbumArtist());  // Needed for some apps

        if (artist.find(EM_DASH) != std::string::npos) {
            albumName = artist.substr(artist.find(EM_DASH) + 3);
            artist = artist.substr(0, artist.find(EM_DASH));
            utils::trim(artist);
            utils::trim(albumName);
        }

        std::string modelId = toStdString(currentSession.SourceAppUserModelId());

        return std::make_shared<MediaInfo>(
            playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused,
            toStdString(mediaProperties.Title()), std::move(artist), std::move(albumName), std::move(modelId),
            std::move(thumbnailData), endTime, elapsedTime);
    } catch (...) {
        return nullptr;
    }
}

bool backend::init() {
    return winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(
        L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager");
}

#undef EM_DASH
#endif
