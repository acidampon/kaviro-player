#include "AndroidPlatformServices.h"
namespace ump {
PlatformCapabilities AndroidPlatformServices::capabilities()const noexcept{PlatformCapabilities c;c.platform=PlatformKind::Android;c.folderPicker=true;c.persistentDocumentAccess=true;c.backgroundAudio=true;c.pictureInPicture=true;c.systemMediaControls=true;c.hardwareAcceleration=true;return c;}
std::vector<MediaLocation>AndroidPlatformServices::pickMediaLocations(){return{};}
bool AndroidPlatformServices::persistAccess(const MediaLocation&){return false;}
bool AndroidPlatformServices::releaseAccess(const MediaLocation&){return false;}
std::filesystem::path AndroidPlatformServices::resolveLocalPath(const MediaLocation&)const{return{};}
void AndroidPlatformServices::onAppBackground(){}
void AndroidPlatformServices::onAppForeground(){}
}
