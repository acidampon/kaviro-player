#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_kaviro_player_MainActivity_nativeEngineStatus(JNIEnv* env, jclass) {
    constexpr const char* kStatus =
        "KAVIRO native Android boundary is loaded. "
        "Full native media engine wiring is the next release gate.";
    return env->NewStringUTF(kStatus);
}
