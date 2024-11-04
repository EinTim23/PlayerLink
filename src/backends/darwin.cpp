#ifdef __APPLE__
#include "../backend.hpp"
std::shared_ptr<MediaInfo> backend::getMediaInformation() { return nullptr; }
bool backend::toggleAutostart(bool enabled) { return false, }
#endif