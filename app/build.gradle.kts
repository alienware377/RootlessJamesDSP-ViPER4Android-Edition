import com.google.firebase.crashlytics.buildtools.gradle.CrashlyticsExtension
import java.util.Base64

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("kotlin-kapt")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
    id("com.google.devtools.ksp") version AndroidConfig.kspVersion
    id("dev.rikka.tools.refine") version AndroidConfig.rikkaRefineVersion
    id("org.jetbrains.kotlin.plugin.serialization") version "2.1.0"
}

android {

    val SUPPORTED_ABIS = setOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
    compileSdk = AndroidConfig.compileSdk
    // Neutral on purpose: this is project-wide, so it cannot say "Rootless"
    // without stamping that onto the rooted build's file names too. The variant
    // part of each output already reads rootless-... or rootful-...
    project.setProperty("archivesBaseName", "ViPER4Android-v${AndroidConfig.versionName}")

    defaultConfig {
        targetSdk = AndroidConfig.targetSdk
        versionCode = AndroidConfig.versionCode
        versionName = AndroidConfig.versionName

        manifestPlaceholders["label"] = "RootlessViPER4Android"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        buildConfigField("String", "COMMIT_COUNT", "\"${getCommitCount()}\"")
        buildConfigField("String", "UPSTREAM_VERSION", "\"${AndroidConfig.upstreamVersionName}\"")
        buildConfigField("String", "COMMIT_SHA", "\"${getGitSha()}\"")
        buildConfigField("String", "BUILD_TIME", "\"${getBuildTime()}\"")
        buildConfigField("boolean", "PREVIEW", "false")
        buildConfigField("boolean", "PLUGIN", "false")

        externalNativeBuild {
            cmake {
                arguments.addAll(listOf("-DANDROID_ARM_NEON=ON"))
                cFlags.add("-std=gnu11 -Wno-incompatible-pointer-types -Wno-implicit-int -Wno-implicit-function-declaration")
            }
        }

        ndk {
            abiFilters += SUPPORTED_ABIS
        }
    }

    signingConfigs {
        getByName("debug") {
            storeFile = rootProject.file("keystore/debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildTypes {
        getByName("debug") {
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-${getCommitCount()}"
            manifestPlaceholders["crashlyticsCollectionEnabled"] = "false"
        }
        getByName("release") {
            // No suffix. Up to v2.8.2 a release appended .v4a to distinguish
            // this fork from upstream's package; the identifiers now say
            // viper4android themselves, so the suffix was only noise.
            manifestPlaceholders += mapOf("crashlyticsCollectionEnabled" to "true")
            configure<CrashlyticsExtension> {
                nativeSymbolUploadEnabled = true
                mappingFileUploadEnabled = false
            }

            //proguardFiles("proguard-android-optimize.txt", "proguard-rules.pro")
            isMinifyEnabled = false
            isShrinkResources = false
            signingConfig = signingConfigs.getByName("debug")
        }
        create("preview") {
            initWith(getByName("release"))
            buildConfigField("boolean", "PREVIEW", "true")

            val debugType = getByName("debug")
            versionNameSuffix = debugType.versionNameSuffix
            matchingFallbacks.add("release")
        }
    }

    flavorDimensions += "version"
    flavorDimensions += "dependencies"
    productFlavors {
        create("fdroid") {
            dimension = "dependencies"
            buildConfigField("boolean", "FOSS_ONLY", "true")
            android.defaultConfig.externalNativeBuild.cmake.arguments += "-DNO_CRASHLYTICS=1"
        }
        create("full") {
            dimension = "dependencies"
            buildConfigField("boolean", "FOSS_ONLY", "false")
        }

        create("rootless") {
            dimension = "version"

            manifestPlaceholders["label"] = "RootlessViPER4Android"
            applicationId = "com.alienware377.viper4android.rootless"
            AndroidConfig.minSdk = 29
            minSdk = AndroidConfig.minSdk
            buildConfigField("boolean", "ROOTLESS", "true")
            buildConfigField("boolean", "PLUGIN", "false")
        }
        // Named "rootful" rather than "root" so every artefact this produces
        // says so: the variant name reaches task names, output paths and APK
        // file names, and calling the rooted build "Rootless...-root-..." was
        // the single most confusing thing about the downloads.
        create("rootful") {
            dimension = "version"

            manifestPlaceholders["label"] = "RootfulViPER4Android"
            // NOTE: archivesBaseName is a project-wide property, so setting it per
            // flavor makes the last-configured flavor win for every variant. The
            // base name is set once above and deliberately says neither rootless
            // nor rootful; the variant suffix distinguishes the outputs.
            applicationId = "com.alienware377.viper4android.rootful"
            AndroidConfig.minSdk = 26
            minSdk = AndroidConfig.minSdk
            buildConfigField("boolean", "ROOTLESS", "false")
            buildConfigField("boolean", "PLUGIN", "false")
        }
        create("plugin") {
            dimension = "version"

            AndroidConfig.minSdk = 26
            minSdk = AndroidConfig.minSdk
            buildConfigField("boolean", "ROOTLESS", "false")
            buildConfigField("boolean", "PLUGIN", "true")
        }
    }

    sourceSets {
        // Use different app icon for non-release builds
        getByName("debug").res.srcDirs("src/debug/res")
    }

    // Export multiple CPU architecture split apks
    splits {
        abi {
            isEnable = true
            reset()
            include(*SUPPORTED_ABIS.toTypedArray())
            isUniversalApk = true
        }
    }

    lint {
        abortOnError = false
        checkReleaseBuilds = false
        disable += "ObsoleteSdkInt"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        viewBinding = true
        // Disable unused features
        aidl = false
        renderScript = false
        shaders = false
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    namespace = "me.timschneeberger.rootlessjamesdsp"
}

// Hooks to upload native symbols to crashlytics automatically
afterEvaluate {
    getTasksByName("bundleRootlessFullRelease", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootlessFullRelease")
    getTasksByName("bundleRootfulFullRelease", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootfulFullRelease")
    getTasksByName("assembleRootlessFullRelease", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootlessFullRelease")
    getTasksByName("assembleRootfulFullRelease", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootfulFullRelease")

    getTasksByName("assembleRootlessFullPreview", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootlessFullRelease")
    getTasksByName("assembleRootfulFullPreview", false).firstOrNull()?.finalizedBy("uploadCrashlyticsSymbolFileRootfulFullRelease")
}

dependencies {
    // Kotlin extensions
    implementation("org.jetbrains.kotlin:kotlin-reflect:2.0.20")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.1")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.5.0")

    // AndroidX
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.lifecycle:lifecycle-livedata-ktx:2.8.7")
    implementation("androidx.constraintlayout:constraintlayout:2.2.0")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.navigation:navigation-fragment-ktx:2.8.4")
    implementation("androidx.navigation:navigation-ui-ktx:2.8.4")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.databinding:databinding-runtime:8.7.3")
    implementation("androidx.work:work-runtime-ktx:2.10.0")
    implementation("androidx.mediarouter:mediarouter:1.7.0")

    // Material
    implementation("com.google.android.material:material:1.9.0")

    // Dependency injection
    implementation("io.insert-koin:koin-android:3.3.3")
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.7")

    // Firebase
    "fullImplementation"(platform("com.google.firebase:firebase-bom:33.7.0"))
    "fullImplementation"("com.google.firebase:firebase-analytics-ktx")
    "fullImplementation"("com.google.firebase:firebase-crashlytics-ktx")
    "fullImplementation"("com.google.firebase:firebase-crashlytics-ndk")

    // Web API client
    implementation("com.google.code.gson:gson:2.11.0")
    implementation("com.squareup.retrofit2:retrofit:2.11.0")
    implementation("com.squareup.retrofit2:converter-gson:2.11.0")
    implementation("com.squareup.retrofit2:converter-scalars:2.9.0")

    // Logging
    implementation("com.jakewharton.timber:timber:5.0.1")
    implementation("com.github.bastienpaulfr:Treessence:1.0.0")

    // IO
    implementation("org.kamranzafar:jtar:2.3")
    implementation("com.squareup.okio:okio:3.6.0")

    // Room databases
    val roomVersion = "2.6.1"
    implementation("androidx.room:room-runtime:$roomVersion")
    ksp("androidx.room:room-compiler:$roomVersion")
    implementation("androidx.room:room-ktx:$roomVersion")

    // Script editor
    implementation(project(":codeview"))

    // Shizuku
    implementation("dev.rikka.shizuku:api:${AndroidConfig.shizukuVersion}")
    implementation("dev.rikka.shizuku:provider:${AndroidConfig.shizukuVersion}")

    // Used for backup file access
    implementation("com.github.tachiyomiorg:unifile:17bec43")

    // Root APIs
    "rootfulImplementation"("com.github.topjohnwu.libsu:core:5.0.4")

    // Hidden APIs
    implementation("dev.rikka.tools.refine:runtime:${AndroidConfig.rikkaRefineVersion}")
    implementation("org.lsposed.hiddenapibypass:hiddenapibypass:4.3")
    compileOnly(project(":hidden-api-refined"))
    implementation(project(":hidden-api-impl"))

    // Debug utilities
    debugImplementation("com.squareup.leakcanary:leakcanary-android:2.10")
    debugImplementation("com.plutolib:pluto:2.0.9")
    "previewImplementation"("com.plutolib:pluto-no-op:2.0.9")
    releaseImplementation("com.plutolib:pluto-no-op:2.0.9")
    debugImplementation("com.plutolib.plugins:bundle-core:2.0.9")
    "previewImplementation"("com.plutolib.plugins:bundle-core-no-op:2.0.9")
    releaseImplementation("com.plutolib.plugins:bundle-core-no-op:2.0.9")

    // Unit tests
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.6.1")
}

/*
 * Guards the launch allowlist.
 *
 * MainActivity refuses to start unless the running package and app label are
 * both listed, base64-encoded, in ContextExtensions.kt. Because the entries are
 * encoded, a missing one is invisible to grep and the failure only shows up on
 * a device, as "Cannot launch application. Please re-download the latest
 * version..." - which reads like a corrupt download rather than a build fault.
 *
 * Every release build appends .v4a to the applicationId, so each flavour needs
 * its own suffixed entry. This checks all of them at build time and prints the
 * exact string to add, turning a tester-facing mystery into a build error.
 */
tasks.register("verifyLaunchAllowlist") {
    group = "verification"
    description = "Checks every flavour's package and label are in the launch allowlist"

    doLast {
        val source = file("src/main/java/me/timschneeberger/rootlessjamesdsp/utils/extensions/ContextExtensions.kt")
        if (!source.exists()) throw GradleException("Cannot find ContextExtensions.kt to verify the allowlist")
        val text = source.readText()

        fun decodedSet(name: String): Set<String> {
            val block = Regex("$name = setOf\\((.*?)\\)\\s*\n", RegexOption.DOT_MATCHES_ALL)
                .find(text)?.groupValues?.get(1)
                ?: throw GradleException("Could not locate $name in ContextExtensions.kt")
            return Regex("\"([A-Za-z0-9+/=]+)\"").findAll(block)
                .map { Base64.getDecoder().decode(it.groupValues[1]).toString(Charsets.UTF_8) }
                .toSet()
        }

        val packages = decodedSet("PKGNAME_REFS")
        val labels = decodedSet("APPNAME_REFS")

        // applicationId per flavour, plus the .debug suffix debug builds carry.
        // Release builds no longer add a suffix.
        val expected = listOf(
            "com.alienware377.viper4android.rootless" to "RootlessViPER4Android",
            "com.alienware377.viper4android.rootful" to "RootfulViPER4Android"
        )
        val missing = mutableListOf<String>()
        expected.forEach { (appId, label) ->
            listOf(appId, "$appId.debug").forEach { pkg ->
                if (pkg !in packages) {
                    val enc = Base64.getEncoder().encodeToString(pkg.toByteArray(Charsets.UTF_8))
                    missing += "package '$pkg' -> add \"$enc\" to PKGNAME_REFS"
                }
            }
            if (label !in labels) {
                val enc = Base64.getEncoder().encodeToString(label.toByteArray(Charsets.UTF_8))
                missing += "label '$label' -> add \"$enc\" to APPNAME_REFS"
            }
        }

        if (missing.isNotEmpty()) {
            throw GradleException(
                "Launch allowlist is incomplete - the app would install but refuse to " +
                "start.\n  " + missing.joinToString("\n  ") +
                "\nEdit ContextExtensions.kt, then build again."
            )
        }
        logger.lifecycle("Launch allowlist OK: ${packages.size} packages, ${labels.size} labels")
    }
}

// Run before anything is assembled, so a missing entry fails the build rather
// than reaching a tester's device.
tasks.matching { it.name == "preBuild" }.configureEach {
    dependsOn("verifyLaunchAllowlist")
}
