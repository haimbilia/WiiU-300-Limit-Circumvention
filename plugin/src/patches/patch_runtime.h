#pragma once

#include <cstdint>

namespace patches {

void initializeRuntime();
void deinitializeRuntime();
void reportMenuBuild(uint64_t titleId, uint16_t titleVersion);
void finishApplication();

} // namespace patches
