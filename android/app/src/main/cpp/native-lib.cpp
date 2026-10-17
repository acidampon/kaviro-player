#include "AndroidOutputs.h"
#include "ump/native/NativeMediaEngine.h"

#include <android/native_window_jni.h>
#include <jni.h>

#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

struct AndroidPlayer {
    std::mutex mutex;
    ump::native::NativeMediaEngine engine;
    kaviro::android::AndroidAudioOutput audioOutput;
    kaviro::android::AndroidVideoOutput videoOutput;

    AndroidPlayer() {
        engine.attachAudioOutput(&audioOutput);
        engine.attachVideoOutput(&videoOutput);
    }

    ~AndroidPlayer() {
        engine.detachAudioOutput(&audioOutput);
        engine.detachVideoOutput(&videoOutput);
        audioOutput.close();
        videoOutput.close();
    }
};

std::mutex registryMutex;
std::unordered_map<std::uintptr_t, std::shared_ptr<AndroidPlayer>> players;
std::atomic<std::uintptr_t> nextHandle{1};

std::shared_ptr<AndroidPlayer> fromHandle(jlong handle) {
    if (handle <= 0) return {};
    const auto key = static_cast<std::uintptr_t>(handle);
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto found = players.find(key);
    return found == players.end() ? std::shared_ptr<AndroidPlayer>{} : found->second;
}

jstring makeString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_kaviro_player_MainActivity_nativeCreate(JNIEnv*, jclass) {
    auto player = std::make_shared<AndroidPlayer>();
    const auto handle = nextHandle.fetch_add(1, std::memory_order_relaxed);
    if (handle == 0) return 0;
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        players.emplace(handle, std::move(player));
    }
    return static_cast<jlong>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_com_kaviro_player_MainActivity_nativeRelease(JNIEnv*, jclass, jlong handle) {
    if (handle <= 0) return;
    // Removing the registry entry prevents new calls from acquiring the player.
    // Calls already in flight own shared_ptr references, so destruction waits
    // until the last call has left its critical section instead of racing JNI.
    std::lock_guard<std::mutex> lock(registryMutex);
    players.erase(static_cast<std::uintptr_t>(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeOpen(
    JNIEnv* env, jclass, jlong handle, jstring path) {
    auto player = fromHandle(handle);
    if (player == nullptr || path == nullptr) return JNI_FALSE;

    const char* rawPath = env->GetStringUTFChars(path, nullptr);
    if (rawPath == nullptr) return JNI_FALSE;

    std::lock_guard<std::mutex> lock(player->mutex);
    const bool opened = player->engine.open(std::filesystem::path(rawPath), true);
    env->ReleaseStringUTFChars(path, rawPath);
    return opened ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeOpenFd(
    JNIEnv*, jclass, jlong handle, jint fd) {
    auto player = fromHandle(handle);
    if (player == nullptr || fd < 0) return JNI_FALSE;

    const std::string procPath = std::string("/proc/self/fd/") + std::to_string(fd);
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.open(std::filesystem::path(procPath), true) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeSetSurface(
    JNIEnv* env, jclass, jlong handle, jobject surface) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;

    std::lock_guard<std::mutex> lock(player->mutex);
    ANativeWindow* window = surface == nullptr
        ? nullptr
        : ANativeWindow_fromSurface(env, surface);
    player->videoOutput.setWindow(window);
    if (window != nullptr) ANativeWindow_release(window);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativePlay(JNIEnv*, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.play() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativePause(JNIEnv*, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.pause() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeStop(
    JNIEnv*, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.stop() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeSeekMs(
    JNIEnv*, jclass, jlong handle, jlong positionMs) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.seekMs(static_cast<std::int64_t>(positionMs)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_kaviro_player_MainActivity_nativePositionMs(
    JNIEnv*, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return 0;
    std::lock_guard<std::mutex> lock(player->mutex);
    return static_cast<jlong>(player->engine.positionMs());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_kaviro_player_MainActivity_nativeDurationMs(
    JNIEnv*, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return 0;
    std::lock_guard<std::mutex> lock(player->mutex);
    return static_cast<jlong>(player->engine.durationMs());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativePump(
    JNIEnv*, jclass, jlong handle, jint maxFrames) {
    auto player = fromHandle(handle);
    if (player == nullptr || maxFrames < 0) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.pump(static_cast<std::size_t>(maxFrames)) ? JNI_TRUE : JNI_FALSE;
}


extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_kaviro_player_MainActivity_nativeVideoTracks(JNIEnv* env, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(player->mutex);
    const auto tracks = player->engine.tracks();
    jclass stringClass = env->FindClass("java/lang/String");
    if (stringClass == nullptr) return nullptr;
    int count = 0;
    for (const auto& t : tracks) if (t.type == ump::FfmpegStreamType::Video) ++count;
    jobjectArray result = env->NewObjectArray(count, stringClass, nullptr);
    if (result == nullptr) return nullptr;
    int out = 0;
    for (const auto& t : tracks) {
        if (t.type != ump::FfmpegStreamType::Video) continue;
        std::string label = std::to_string(t.streamIndex) + "\t" + (t.title.empty() ? t.language : t.title);
        if (label.back() == "\t"[0]) label += "Video track";
        if (t.selected) label = "[Selected] " + label;
        jstring item = env->NewStringUTF(label.c_str());
        env->SetObjectArrayElement(result, out++, item);
        env->DeleteLocalRef(item);
    }
    return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_kaviro_player_MainActivity_nativeAudioTracks(JNIEnv* env, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(player->mutex);
    const auto tracks = player->engine.tracks();
    jclass stringClass = env->FindClass("java/lang/String");
    if (stringClass == nullptr) return nullptr;
    int count = 0;
    for (const auto& t : tracks) if (t.type == ump::FfmpegStreamType::Audio) ++count;
    jobjectArray result = env->NewObjectArray(count, stringClass, nullptr);
    if (result == nullptr) return nullptr;
    int out = 0;
    for (const auto& t : tracks) {
        if (t.type != ump::FfmpegStreamType::Audio) continue;
        std::string label = std::to_string(t.streamIndex) + "\t" + (t.title.empty() ? t.language : t.title);
        if (label.back() == "\t"[0]) label += "Audio track";
        if (t.selected) label = "[Selected] " + label;
        jstring item = env->NewStringUTF(label.c_str());
        env->SetObjectArrayElement(result, out++, item);
        env->DeleteLocalRef(item);
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeSelectAudioTrack(JNIEnv*, jclass, jlong handle, jint streamIndex) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.selectAudioTrack(static_cast<int>(streamIndex)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_kaviro_player_MainActivity_nativeSelectVideoTrack(JNIEnv*, jclass, jlong handle, jint streamIndex) {
    auto player = fromHandle(handle);
    if (player == nullptr) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(player->mutex);
    return player->engine.selectVideoTrack(static_cast<int>(streamIndex)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_kaviro_player_MainActivity_nativeState(JNIEnv* env, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return makeString(env, "closed");

    std::lock_guard<std::mutex> lock(player->mutex);
    switch (player->engine.state()) {
        case ump::native::NativeEngineState::Open: return makeString(env, "open");
        case ump::native::NativeEngineState::Playing: return makeString(env, "playing");
        case ump::native::NativeEngineState::Paused: return makeString(env, "paused");
        case ump::native::NativeEngineState::Ended: return makeString(env, "ended");
        case ump::native::NativeEngineState::Error: return makeString(env, "error");
        case ump::native::NativeEngineState::Closed: return makeString(env, "closed");
    }
    return makeString(env, "unknown");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_kaviro_player_MainActivity_nativeLastError(JNIEnv* env, jclass, jlong handle) {
    auto player = fromHandle(handle);
    if (player == nullptr) return makeString(env, "player handle is invalid");
    std::lock_guard<std::mutex> lock(player->mutex);
    return makeString(env, player->engine.lastError());
}

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

// Track-selection JNI boundary is intentionally kept minimal; UI uses native track enumeration when available.
