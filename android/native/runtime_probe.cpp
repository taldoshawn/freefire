// C++20 / runtime_probe.cpp / Android 26+ ARM64 / in-process library mapping probe
#include <jni.h>
#include <link.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

struct ImageMap {
    uintptr_t il2cpp = 0;
    uintptr_t unity = 0;
    uintptr_t main = 0;
};

int inspect_loaded_image(dl_phdr_info* info, size_t, void* opaque) {
    auto& result = *static_cast<ImageMap*>(opaque);
    const char* path = info->dlpi_name ? info->dlpi_name : "";
    const char* basename = std::strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    if (!std::strcmp(basename, "libil2cpp.so")) result.il2cpp = info->dlpi_addr;
    if (!std::strcmp(basename, "libunity.so")) result.unity = info->dlpi_addr;
    if (!std::strcmp(basename, "libmain.so")) result.main = info->dlpi_addr;
    return 0;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_nox_esp_EspOverlay_nativeProbe(JNIEnv* env, jclass) {
    ImageMap map{};
    dl_iterate_phdr(inspect_loaded_image, &map);
    char message[192];
    std::snprintf(message, sizeof(message),
        "IL2CPP %s 0x%llx | Unity %s 0x%llx | Main %s 0x%llx",
        map.il2cpp ? "OK" : "--",
        static_cast<unsigned long long>(map.il2cpp),
        map.unity ? "OK" : "--",
        static_cast<unsigned long long>(map.unity),
        map.main ? "OK" : "--",
        static_cast<unsigned long long>(map.main));
    return env->NewStringUTF(message);
}
