#ifdef _WIN32
#include <winrt/windows.foundation.h>
#include <winrt/windows.media.control.h>
#include <winrt/windows.storage.streams.h>

#include <chrono>
#include <codecvt>

#include "../backend.hpp"
#include "../utils.hpp"

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;
#define EM_DASH "\xE2\x80\x94"
// codecvt is deprecated, but there is no good portable way to do this, I could technically use the winapi as this is the windows backend tho
std::string toStdString(winrt::hstring in) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(in.c_str());
}

std::shared_ptr<MediaInfo> backend::getMediaInformation() {
    auto sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    auto currentSession = sessionManager.GetCurrentSession();
    if (!currentSession)
        return nullptr;

    auto playbackInfo = currentSession.GetPlaybackInfo();
    auto mediaProperties = currentSession.TryGetMediaPropertiesAsync().get();
    auto timelineInformation = currentSession.GetTimelineProperties();
    if (!mediaProperties)
        return nullptr;

    auto endTime = std::chrono::duration_cast<std::chrono::milliseconds>(timelineInformation.EndTime()).count();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(timelineInformation.Position()).count();

    auto thumbnail = mediaProperties.Thumbnail();
    std::string thumbnailData = "";

    if (thumbnail) {
        auto stream = thumbnail.OpenReadAsync().get();
        size_t size = static_cast<size_t>(stream.Size());

        DataReader reader(stream);
        reader.LoadAsync(static_cast<uint32_t>(size)).get();

        std::vector<uint8_t> buffer(size);
        reader.ReadBytes(buffer);
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

    return std::make_shared<MediaInfo>(
        playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused,
        toStdString(mediaProperties.Title()), artist, albumName, toStdString(currentSession.SourceAppUserModelId()),
        thumbnailData, endTime, elapsedTime);
}
#undef EM_DASH
#endif