// C++20 / nox_api.h / Android 26+ ARM64 / Unity IL2CPP host data contract
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NoxVec3 {
    float x, y, z;
} NoxVec3;

typedef struct NoxActor {
    int32_t team;
    int32_t alive;
    float health;
    float max_health;
    NoxVec3 feet;
    NoxVec3 head;
    NoxVec3 joints[16];
    char name[48];
} NoxActor;

typedef struct NoxCamera {
    // Unity-style column-major clip matrix: projection * view.
    // Screen Y is inverted by the overlay's projector.
    float view_projection[16];
    NoxVec3 position;
    int32_t local_team;
} NoxCamera;

// Supply actual, internally consistent camera/actor data from the same frame.
// No game memory access or player discovery is performed by this library.
// Returns 1 on delivery, 0 if the bridge is uninitialized or delivery fails.
__attribute__((visibility("default")))
int nox_publish_frame(const NoxCamera* camera, const NoxActor* actors, uint32_t count);

#ifdef __cplusplus
}
#endif
