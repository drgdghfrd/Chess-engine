plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.chesszero.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.chesszero.app"
        minSdk = 24
        targetSdk = 35
        ndkVersion = "29.0.14206865"
        versionCode = 108
        versionName = "1.0.8"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17")
                val fathomDir = providers.gradleProperty("chesszeroFathomSourceDir")
                    .orElse(providers.environmentVariable("CHESSZERO_FATHOM_SOURCE_DIR"))
                    .orElse(file("../../third_party/Fathom").absolutePath)
                    .get()
                val fathomOffline = providers.gradleProperty("chesszeroFathomOffline")
                    .orElse(providers.environmentVariable("CHESSZERO_FATHOM_OFFLINE"))
                    .orElse("true")
                    .get()
                arguments += listOf(
                    "-DCMAKE_ANDROID_STL_TYPE=c++_static",
                    "-DCHESSZERO_ANDROID_16K=ON",
                    "-DCHESSZERO_OEX_ASSET_DIR=${project.layout.buildDirectory.get().asFile.absolutePath}/generated/oex",
                    "-DCHESSZERO_FATHOM_SOURCE_DIR=$fathomDir",
                    "-DCHESSZERO_FATHOM_OFFLINE=$fathomOffline",
                )
                targets += listOf("chesszero_android", "chesszero_oex")
            }
        }
    }

    signingConfigs {
        create("release") {
            val keystoreFile = providers.gradleProperty("chesszeroSigningStoreFile")
                .orElse(providers.environmentVariable("CHESSZERO_SIGNING_STORE_FILE"))
                .orNull
            val keystorePassword = providers.gradleProperty("chesszeroSigningStorePassword")
                .orElse(providers.environmentVariable("CHESSZERO_SIGNING_STORE_PASSWORD"))
                .orNull
            val keyAlias = providers.gradleProperty("chesszeroSigningKeyAlias")
                .orElse(providers.environmentVariable("CHESSZERO_SIGNING_KEY_ALIAS"))
                .orNull
            val keyPassword = providers.gradleProperty("chesszeroSigningKeyPassword")
                .orElse(providers.environmentVariable("CHESSZERO_SIGNING_KEY_PASSWORD"))
                .orNull

            if (keystoreFile != null && keystorePassword != null && keyAlias != null && keyPassword != null) {
                storeFile = file(keystoreFile)
                storePassword = keystorePassword
                this.keyAlias = keyAlias
                this.keyPassword = keyPassword
                enableV1Signing = true
                enableV2Signing = true
                enableV3Signing = true
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
            signingConfig = signingConfigs.getByName("release").takeIf {
                it.storeFile != null
            }
        }
    }

    androidResources {
        // The OEX provider uses AssetManager.openFd(), which requires the
        // executable asset to be stored uncompressed in the APK.
        noCompress += "so"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }

    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/oex"))

    externalNativeBuild {
        cmake {
            path = file("../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}
