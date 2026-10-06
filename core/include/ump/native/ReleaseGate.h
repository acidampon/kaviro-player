#pragma once
#include "ump/native/NativeMediaEnginePackage.h"
namespace ump::native {
struct ReleaseGateResult { bool pass{false}; std::string reason; };
class ReleaseGate {
public:
    static ReleaseGateResult validateNativePackage(const NativeEnginePackageManifest&, const NativeEnginePackageExpectation&);
};
} // namespace ump::native
