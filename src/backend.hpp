#ifndef _BACKEND_
#define _BACKEND_
#include <stdint.h>

#include <memory>
#include <string>

struct MediaInfo {
    bool paused;
    std::string songTitle;
    std::string songArtist;
    std::string songAlbum;
    std::string songThumbnailData;
    int64_t songDuration;
    int64_t songElapsedTime;
    std::string playbackSource;
    MediaInfo() {}
    MediaInfo(bool p, std::string title, std::string artist, std::string album, std::string source,
              std::string thumbnail, int duration, int elapsed)
        : paused(p),
          songTitle(title),
          songArtist(artist),
          songAlbum(album),
          songDuration(duration),
          songElapsedTime(elapsed),
          playbackSource(source),
          songThumbnailData(thumbnail) {}
};

namespace backend {
    bool init();
    bool toggleAutostart(bool enabled);
    std::shared_ptr<MediaInfo> getMediaInformation();
}  // namespace backend

#endif