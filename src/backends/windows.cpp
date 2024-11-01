#ifdef _WIN32
#include <winrt/windows.foundation.h>
#include <winrt/windows.media.control.h>
#include <winrt/windows.storage.streams.h>

#include <chrono>

#include "../backend.hpp"

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

// a winrt::hstring is just an extended std::wstring, so we can use std::strings built in conversion.
std::string toStdString(winrt::hstring in) {
    std::string converted = std::string(in.begin(), in.end());
    return converted;
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
    if (artist == "")
        artist = toStdString(mediaProperties.AlbumArtist());  // Needed for some apps

    return std::make_shared<MediaInfo>(
        playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused,
        toStdString(mediaProperties.Title()), artist, toStdString(mediaProperties.AlbumTitle()),
        toStdString(currentSession.SourceAppUserModelId()), thumbnailData, endTime, elapsedTime);
}

#endif