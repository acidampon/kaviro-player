plugins {
    id("com.android.application")
}

android {
    namespace = "com.kaviro.player"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.kaviro.player"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        ndkVersion = "28.2.13676358"

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20")
            }
        }

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }
}
