#pragma once
#include <filesystem>
#include <string>

namespace ump::native {
struct NativeEnginePackageManifest {
    std::string engineName, version, buildId, license, upstreamSource, upstreamChecksum;
    std::string platform, architecture, binaryChecksum;
    bool standalone{false};
};
struct NativeEnginePackageExpectation {
    std::string engineName, version, buildId, license, upstreamSource, upstreamChecksum;
    std::string platform, architecture, binaryChecksum;
};
class NativeEnginePackageValidator {
public:
    static bool validate(const NativeEnginePackageManifest&, const NativeEnginePackageExpectation&, std::string& error);
    static bool loadKeyValueManifest(const std::filesystem::path&, NativeEnginePackageManifest&, std::string& error);
};
} // namespace ump::native
