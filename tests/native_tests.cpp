#include "ump/FfmpegMediaSession.h"
#include <iostream>
int main(){using namespace ump;PlaybackClock c{100,0,2,false};c=advancePlaybackClock(c,50);if(c.mediaUs!=200)return 1;if(clockDeltaUs(c,150)!=-50)return 2;FfmpegMediaSession s;if(s.standaloneReady())return 3;std::cout<<"KAVIRO native boundary tests: PASS\n";}