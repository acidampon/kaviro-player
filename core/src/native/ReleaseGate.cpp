#include "ump/native/ReleaseGate.h"
namespace ump::native {
ReleaseGateResult ReleaseGate::validateNativePackage(const NativeEnginePackageManifest&m,const NativeEnginePackageExpectation&e){
 ReleaseGateResult r; r.pass=NativeEnginePackageValidator::validate(m,e,r.reason); return r;
}
}
