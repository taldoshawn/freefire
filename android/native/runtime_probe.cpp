// C++20 / runtime_probe.cpp / Android 26+ ARM64 / in-process IL2CPP image fingerprint
#include <jni.h>
#include <link.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
constexpr const char* kExpectedBuildId = "ee8bf32c4483429084fde022704ead01a94b6620";

struct ImageMap {
    uintptr_t il2cpp = 0;
    uintptr_t unity = 0;
    uintptr_t main = 0;
    std::string il2cpp_build_id;
};

std::string find_build_id(const dl_phdr_info* info) {
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
        const auto& segment = info->dlpi_phdr[i];
        if (segment.p_type != PT_NOTE || segment.p_filesz < 12) continue;
        const auto* note = reinterpret_cast<const unsigned char*>(
            info->dlpi_addr + segment.p_vaddr);
        size_t remaining = static_cast<size_t>(segment.p_filesz);
        while (remaining >= 12) {
            uint32_t name_size = 0, desc_size = 0, note_type = 0;
            std::memcpy(&name_size, note, 4);
            std::memcpy(&desc_size, note + 4, 4);
            std::memcpy(&note_type, note + 8, 4);
            note += 12;
            remaining -= 12;
            const size_t padded_name = (static_cast<size_t>(name_size) + 3U) & ~size_t(3U);
            const size_t padded_desc = (static_cast<size_t>(desc_size) + 3U) & ~size_t(3U);
            if (padded_name > remaining || padded_desc > remaining - padded_name) break;
            if (note_type == NT_GNU_BUILD_ID && name_size == 4 &&
                std::memcmp(note, "GNU\0", 4) == 0 && desc_size >= 4 && desc_size <= 32) {
                const auto* bytes = note + padded_name;
                std::string hex;
                hex.reserve(desc_size * 2);
                constexpr char digits[] = "0123456789abcdef";
                for (uint32_t j = 0; j < desc_size; ++j) {
                    hex.push_back(digits[bytes[j] >> 4]);
                    hex.push_back(digits[bytes[j] & 15]);
                }
                return hex;
            }
            note += padded_name + padded_desc;
            remaining -= padded_name + padded_desc;
        }
    }
    return {};
}

int inspect_loaded_image(dl_phdr_info* info, size_t, void* opaque) {
    auto& result = *static_cast<ImageMap*>(opaque);
    const char* path = info->dlpi_name ? info->dlpi_name : "";
    const char* basename = std::strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    if (!std::strcmp(basename, "libil2cpp.so")) {
        result.il2cpp = static_cast<uintptr_t>(info->dlpi_addr);
        result.il2cpp_build_id = find_build_id(info);
    }
    if (!std::strcmp(basename, "libunity.so")) result.unity = static_cast<uintptr_t>(info->dlpi_addr);
    if (!std::strcmp(basename, "libmain.so")) result.main = static_cast<uintptr_t>(info->dlpi_addr);
    return 0;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_nox_esp_EspOverlay_nativeProbe(JNIEnv* env, jclass) {
    ImageMap map{};
    dl_iterate_phdr(inspect_loaded_image, &map);
    const char* build_state = map.il2cpp_build_id.empty() ? "NO-ID" :
        (map.il2cpp_build_id == kExpectedBuildId ? "OB55-OK" : "BUILD-MISMATCH");
    char message[200];
    std::snprintf(message, sizeof(message),
        "IL2CPP %s 0x%llx [%s] | Unity %s | Main %s",
        map.il2cpp ? "OK" : "--",
        static_cast<unsigned long long>(map.il2cpp),
        build_state,
        map.unity ? "OK" : "--",
        map.main ? "OK" : "--");
    return env->NewStringUTF(message);
}
