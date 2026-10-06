#include "ump/native/NativeMediaEnginePackage.h"
#include <fstream>
namespace ump::native {
namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos)return {}; auto b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);}
bool eq(const char* n,const std::string&a,const std::string&e,std::string&err){if(e.empty()||a==e)return true;err=std::string(n)+" mismatch";return false;}
}
bool NativeEnginePackageValidator::validate(const NativeEnginePackageManifest&m,const NativeEnginePackageExpectation&e,std::string&err){
 err.clear(); if(!m.standalone){err="native engine package is not standalone";return false;}
 if(!eq("engineName",m.engineName,e.engineName,err)||!eq("version",m.version,e.version,err)||!eq("buildId",m.buildId,e.buildId,err)||
    !eq("license",m.license,e.license,err)||!eq("upstreamSource",m.upstreamSource,e.upstreamSource,err)||
    !eq("upstreamChecksum",m.upstreamChecksum,e.upstreamChecksum,err)||!eq("platform",m.platform,e.platform,err)||
    !eq("architecture",m.architecture,e.architecture,err)||!eq("binaryChecksum",m.binaryChecksum,e.binaryChecksum,err)) return false;
 if(m.engineName.empty()||m.version.empty()||m.buildId.empty()||m.license.empty()){err="required metadata missing";return false;}
 if(m.binaryChecksum.empty()){err="binary checksum missing";return false;}
 return true;
}
bool NativeEnginePackageValidator::loadKeyValueManifest(const std::filesystem::path&p,NativeEnginePackageManifest&m,std::string&err){
 std::ifstream in(p); if(!in){err="cannot open native engine manifest";return false;} std::string line; std::size_t n=0;
 while(std::getline(in,line)){++n;line=trim(line);if(line.empty()||line[0]=='#')continue;auto x=line.find('=');if(x==std::string::npos){err="invalid manifest line "+std::to_string(n);return false;}
  auto k=trim(line.substr(0,x)),v=trim(line.substr(x+1));
  if(k=="engineName")m.engineName=v; else if(k=="version")m.version=v; else if(k=="buildId")m.buildId=v; else if(k=="license")m.license=v;
  else if(k=="upstreamSource")m.upstreamSource=v; else if(k=="upstreamChecksum")m.upstreamChecksum=v; else if(k=="platform")m.platform=v;
  else if(k=="architecture")m.architecture=v; else if(k=="binaryChecksum")m.binaryChecksum=v; else if(k=="standalone")m.standalone=(v=="true"||v=="1");
 }
 return true;
}
}
