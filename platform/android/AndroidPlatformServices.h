#pragma once
#include "ump/PlatformServices.h"
namespace ump {
class AndroidPlatformServices final : public PlatformServices {
public:
    PlatformCapabilities capabilities() const noexcept override;
    std::vector<MediaLocation> pickMediaLocations() override;
    bool persistAccess(const MediaLocation&) override;
    bool releaseAccess(const MediaLocation&) override;
    std::filesystem::path resolveLocalPath(const MediaLocation&) const override;
    void onAppBackground() override;
    void onAppForeground() override;
};
}
