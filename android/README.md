# KAVIRO Android

This module is the first Android build boundary for KAVIRO Player.

It currently proves:

- Gradle/AGP project resolution
- Android SDK/API 37 configuration
- NDK 28.2.13676358 configuration
- CMake native compilation
- JNI library packaging
- arm64-v8a and x86_64 APK native libraries

It now wires the recovered KAVIRO native engine into the Android shell, including SurfaceView video presentation and AAudio PCM output. CI builds pinned FFmpeg 9.0.2 for arm64-v8a and x86_64 and packages a debug APK. Playback remains subject to CI/device verification.

Android's official build model supports C/C++ through CMake/NDK and packages the resulting native library into the APK. See the Android native-code documentation for the supported integration model.
