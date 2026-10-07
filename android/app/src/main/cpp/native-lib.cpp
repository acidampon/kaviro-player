#include "ump/native/NativeMediaEngine.h"

#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_kaviro_player_MainActivity_nativeEngineStatus(JNIEnv* env, jclass) {
    ump::native::NativeMediaEngine engine;
    const auto info = engine.info();

    const std::string status =
        std::string("KAVIRO native engine boundary loaded. ") +
        "standalone=" + (info.standalone ? "true" : "false") +
        ", available=" + (info.available ? "true" : "false") +
        ", hardware=" + (info.hardwareDecode ? "active" : "inactive");

    return env->NewStringUTF(status.c_str());
}
