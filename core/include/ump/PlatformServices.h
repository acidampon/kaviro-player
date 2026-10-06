#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ump {

enum class PlatformKind { Unknown, Android, Windows };
enum class MediaAccessKind { LocalPath, DocumentUri, ContentUri };

struct MediaLocation {
    std::string value;
    MediaAccessKind kind{MediaAccessKind::LocalPath};
    bool persistent{false};
};

struct PlatformCapabilities {
    PlatformKind platform{PlatformKind::Unknown};
    bool folderPicker{false};
    bool persistentDocumentAccess{false};
    bool backgroundAudio{false};
    bool pictureInPicture{false};
    bool systemMediaControls{false};
    bool hardwareAcceleration{false};
};

class PlatformServices {
public:
    virtual ~PlatformServices() = default;
    virtual PlatformCapabilities capabilities() const noexcept = 0;
    virtual std::vector<MediaLocation> pickMediaLocations() = 0;
    virtual bool persistAccess(const MediaLocation& location) = 0;
    virtual bool releaseAccess(const MediaLocation& location) = 0;
    virtual std::filesystem::path resolveLocalPath(const MediaLocation& location) const = 0;
    virtual void onAppBackground() = 0;
    virtual void onAppForeground() = 0;
};

} // namespace ump
