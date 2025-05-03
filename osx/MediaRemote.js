ObjC.import('Foundation');
try {
    const frameworkPath = '/System/Library/PrivateFrameworks/MediaRemote.framework';
    const framework = $.NSBundle.bundleWithPath($(frameworkPath));
    framework.load

    const MRNowPlayingRequest = $.NSClassFromString('MRNowPlayingRequest');

    const playerPath = MRNowPlayingRequest.localNowPlayingPlayerPath;
	const bundleID = ObjC.unwrap(playerPath.client.bundleIdentifier);

    const nowPlayingItem = MRNowPlayingRequest.localNowPlayingItem;
    const info = nowPlayingItem.nowPlayingInfo;

    const title = info.valueForKey('kMRMediaRemoteNowPlayingInfoTitle');
    const album = info.valueForKey('kMRMediaRemoteNowPlayingInfoAlbum');
    const artist = info.valueForKey('kMRMediaRemoteNowPlayingInfoArtist');
    const duration = info.valueForKey('kMRMediaRemoteNowPlayingInfoDuration');
    const playbackStatus = info.valueForKey('kMRMediaRemoteNowPlayingInfoPlaybackRate');
    const elapsed = info.valueForKey('kMRMediaRemoteNowPlayingInfoElapsedTime');

    JSON.stringify({
        title: ObjC.unwrap(title),
        album: ObjC.unwrap(album),
        artist: ObjC.unwrap(artist),
        duration: ObjC.unwrap(duration),
        playbackStatus: ObjC.unwrap(playbackStatus),
        elapsed: ObjC.unwrap(elapsed),
        player: ObjC.unwrap(bundleID)
    });
} catch (error) {
	JSON.stringify({ player: 'none', error: error.toString() });
}