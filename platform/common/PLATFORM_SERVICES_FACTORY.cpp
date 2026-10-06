#include "PLATFORM_SERVICES_FACTORY.h"
namespace ump {
namespace {
class UnsupportedPlatformServices final : public PlatformServices {
public:
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    std::vector<MediaLocation> pickMediaLocations() override { return {}; }
    bool persistAccess(const MediaLocation&) override { return false; }
    bool releaseAccess(const MediaLocation&) override { return false; }
    std::filesystem::path resolveLocalPath(const MediaLocation&) const override { return {}; }
    void onAppBackground() override {}
    void onAppForeground() override {}
};
}
std::unique_ptr<PlatformServices> createPlatformServices() { return std::make_unique<UnsupportedPlatformServices>(); }
}
