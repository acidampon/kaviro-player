#include "WindowsPlatformServices.h"
namespace ump {
PlatformCapabilities WindowsPlatformServices::capabilities()const noexcept{PlatformCapabilities c;c.platform=PlatformKind::Windows;c.folderPicker=true;c.persistentDocumentAccess=true;c.backgroundAudio=true;c.systemMediaControls=true;c.hardwareAcceleration=true;return c;}
std::vector<MediaLocation>WindowsPlatformServices::pickMediaLocations(){return{};}
bool WindowsPlatformServices::persistAccess(const MediaLocation&){return false;}
bool WindowsPlatformServices::releaseAccess(const MediaLocation&){return false;}
std::filesystem::path WindowsPlatformServices::resolveLocalPath(const MediaLocation&l)const{return l.kind==MediaAccessKind::LocalPath?std::filesystem::path(l.value):std::filesystem::path{};}
void WindowsPlatformServices::onAppBackground(){}
void WindowsPlatformServices::onAppForeground(){}
}
