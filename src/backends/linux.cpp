#if !defined(_WIN32) && !defined(__APPLE__)

std::shared_ptr<MediaInfo> backend::getMediaInformation() { return nullptr; }

#endif