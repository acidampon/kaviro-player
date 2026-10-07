# KAVIRO Android

This module is the first Android build boundary for KAVIRO Player.

It currently proves:

- Gradle/AGP project resolution
- Android SDK/API 37 configuration
- NDK 28.2.13676358 configuration
- CMake native compilation
- JNI library packaging
- arm64-v8a and x86_64 APK native libraries

It intentionally does **not** claim media playback yet. The next native milestone is wiring the recovered KAVIRO C++ engine into this module, followed by verified FFmpeg packaging for Android.

Android's official build model supports C/C++ through CMake/NDK and packages the resulting native library into the APK. See the Android native-code documentation for the supported integration model.
