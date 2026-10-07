#include "ump/KaviroCore.h"
#include <filesystem>
#include <iostream>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL: "<<#x<<"\n";return 1;}}while(0)
int main(){using namespace ump;auto d=std::filesystem::temp_directory_path()/"kaviro-core-tests";std::filesystem::remove_all(d);std::filesystem::create_directories(d);CHECK(stableMediaId(d/"a.mp4")==stableMediaId(d/"./a.mp4"));PlaybackController c;CHECK(c.load("x",1000));CHECK(c.seek(1500)&&c.positionMs()==1000);CHECK(c.setSpeed(NAN)&&c.speed()==1);PlaybackState s{"x",-5,99,200,false};s=sanitizePlaybackState(s,1000);CHECK(s.positionMs==0&&s.speed==4&&s.volume==100);MediaQueue q;MediaItem a{"a",d/"a","A","",MediaType::Audio,0},b{"b",d/"b","B","",MediaType::Audio,0};q.append(a);q.append(b);CHECK(q.current()->id=="a"&&q.next()->id=="b"&&q.next()==nullptr);{
auto db=(d/"s.db").string();PlaybackStore ps(db);CHECK(ps.open());s={"a",123,1.5,77,false};CHECK(ps.save(s));auto z=ps.load("a");CHECK(z.positionMs==123&&std::abs(z.speed-1.5)<.01&&z.volume==77);
HistoryStore hs((d/"h.db").string());CHECK(hs.open()&&hs.record(a,20)&&hs.recent().size()==1);
SettingsStore ss((d/"t.db").string());CHECK(ss.open()&&ss.set("theme","dark")&&ss.get("theme")=="dark");
}
std::error_code cleanupError;std::filesystem::remove_all(d,cleanupError);CHECK(!cleanupError);std::cout<<"KAVIRO core tests: PASS\n";}