#ifdef __APPLE__
#import <Foundation/Foundation.h>

FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoTitle;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoAlbum;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoArtist;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoDuration;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoElapsedTime;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoArtworkData;
FOUNDATION_EXPORT CFStringRef _Nullable kMRMediaRemoteNowPlayingInfoPlaybackRate;

typedef void (^ MRMediaRemoteGetNowPlayingInfoCompletion)(CFDictionaryRef _Nullable information);
typedef void (^ MRMediaRemoteGetNowPlayingApplicationPIDCompletion)(int PID);

FOUNDATION_EXPORT void MRMediaRemoteGetNowPlayingApplicationPID(dispatch_queue_t _Nullable queue, MRMediaRemoteGetNowPlayingApplicationPIDCompletion _Nullable completion);
FOUNDATION_EXPORT void MRMediaRemoteGetNowPlayingInfo(dispatch_queue_t _Nullable queue, MRMediaRemoteGetNowPlayingInfoCompletion _Nullable completion);
#endif