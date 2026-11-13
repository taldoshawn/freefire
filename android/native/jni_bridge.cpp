// C++20 / jni_bridge.cpp / Android 26+ ARM64 / in-process JNI feed
#include "nox_api.h"

#include <jni.h>
#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace {
JavaVM* g_vm = nullptr;
jclass g_overlay_class = nullptr;
jmethodID g_publish_method = nullptr;
std::mutex g_state_mutex;
constexpr std::size_t kStride = 58;
constexpr uint32_t kMaxActors = 128;

std::string safe_label(const char name[48]) {
    std::string label;
    label.reserve(48);
    for (std::size_t i = 0; i < 48 && name[i]; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        label.push_back(c >= 32 && c <= 126 ? static_cast<char>(c) : '?');
    }
    return label;
}
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_nox_esp_EspOverlay_nativeInit(JNIEnv* env, jclass clazz) {
    std::scoped_lock lock(g_state_mutex);
    if (!g_overlay_class) {
        g_overlay_class = static_cast<jclass>(env->NewGlobalRef(clazz));
    }
    if (g_overlay_class && !g_publish_method) {
        g_publish_method = env->GetStaticMethodID(
            g_overlay_class, "publishFrame", "([F[F[Ljava/lang/String;[FI)V");
    }
}

extern "C" __attribute__((visibility("default")))
int nox_publish_frame(const NoxCamera* camera,
                      const NoxActor* actors,
                      uint32_t count) {
    if (!camera || (count && !actors) || count > kMaxActors || !g_vm) {
        return 0;
    }

    jclass overlay_class = nullptr;
    jmethodID publish_method = nullptr;
    {
        std::scoped_lock lock(g_state_mutex);
        overlay_class = g_overlay_class;
        publish_method = g_publish_method;
    }
    if (!overlay_class || !publish_method) return 0;

    JNIEnv* env = nullptr;
    const jint state = g_vm->GetEnv(
        reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    bool attached = false;
    if (state == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return 0;
        attached = true;
    } else if (state != JNI_OK) {
        return 0;
    }

    std::vector<jfloat> packed(kStride * count);
    for (uint32_t i = 0; i < count; ++i) {
        const NoxActor& a = actors[i];
        jfloat* p = packed.data() + i * kStride;
        p[0] = static_cast<float>(a.team);
        p[1] = static_cast<float>(a.alive);
        p[2] = a.health;
        p[3] = a.max_health;
        p[4] = a.feet.x;
        p[5] = a.feet.y;
        p[6] = a.feet.z;
        p[7] = a.head.x;
        p[8] = a.head.y;
        p[9] = a.head.z;
        for (int j = 0; j < 16; ++j) {
            p[10 + j*3] = a.joints[j].x;
            p[11 + j*3] = a.joints[j].y;
            p[12 + j*3] = a.joints[j].z;
        }
    }

    jfloatArray jm = env->NewFloatArray(16);
    jfloatArray ja = env->NewFloatArray(static_cast<jsize>(packed.size()));
    jfloatArray jc = env->NewFloatArray(3);
    jclass string_class = env->FindClass("java/lang/String");
    jobjectArray names = string_class
        ? env->NewObjectArray(static_cast<jsize>(count), string_class, nullptr)
        : nullptr;

    if (jm && ja && jc && string_class && names) {
        const std::array<jfloat, 3> pos{
            camera->position.x, camera->position.y, camera->position.z
        };
        env->SetFloatArrayRegion(jm, 0, 16, camera->view_projection);
        env->SetFloatArrayRegion(ja, 0,
            static_cast<jsize>(packed.size()), packed.data());
        env->SetFloatArrayRegion(jc, 0, 3, pos.data());

        for (uint32_t i = 0; i < count && !env->ExceptionCheck(); ++i) {
            std::string label = safe_label(actors[i].name);
            jstring name = env->NewStringUTF(label.c_str());
            if (name) {
                env->SetObjectArrayElement(names, static_cast<jsize>(i), name);
                env->DeleteLocalRef(name);
            }
        }
        if (!env->ExceptionCheck()) {
            env->CallStaticVoidMethod(overlay_class, publish_method,
                                      jm, ja, names, jc, camera->local_team);
        }
    }

    const bool ok = !env->ExceptionCheck() && jm && ja && jc && names;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (names) env->DeleteLocalRef(names);
    if (string_class) env->DeleteLocalRef(string_class);
    if (jc) env->DeleteLocalRef(jc);
    if (ja) env->DeleteLocalRef(ja);
    if (jm) env->DeleteLocalRef(jm);
    if (attached) g_vm->DetachCurrentThread();
    return ok ? 1 : 0;
}
