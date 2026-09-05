#include "patches/patch_runtime.h"
#include "utils/logger.h"

#include <coreinit/mcp.h>
#include <coreinit/title.h>
#include <cstdint>
#include <wups.h>

WUPS_PLUGIN_NAME("Wii U Menu Title Limit");
WUPS_PLUGIN_DESCRIPTION("Raises the USA Wii U Menu v277 software-slot limit to 810");
WUPS_PLUGIN_VERSION("v0.2.1-rc1");
WUPS_PLUGIN_AUTHOR("Wii U Title Limit project contributors");
WUPS_PLUGIN_LICENSE("BSD-2-Clause");

WUPS_USE_WUT_DEVOPTAB();

namespace {
constexpr const char *kPluginVersion = "v0.2.1-rc1";

uint16_t getOwnTitleVersion(uint64_t fallbackTitleId, uint64_t *outTitleId) {
    if (outTitleId) {
        *outTitleId = fallbackTitleId;
    }

    const int32_t handle = static_cast<int32_t>(MCP_Open());
    if (handle < 0) {
        LOG_WARN("MCP_Open failed while reading application identity result=%d", handle);
        return 0;
    }

    MCPTitleListType titleInfo{};
    const MCPError result = MCP_GetOwnTitleInfo(handle, &titleInfo);
    MCP_Close(handle);
    if (static_cast<int32_t>(result) < 0) {
        LOG_WARN("MCP_GetOwnTitleInfo failed result=%d", static_cast<int32_t>(result));
        return 0;
    }

    if (outTitleId) {
        *outTitleId = titleInfo.titleId;
    }
    return titleInfo.titleVersion;
}
} // namespace

INITIALIZE_PLUGIN() {
    initLogging();
    LOG_INFO("plugin initialized");
    writeStatusLog("plugin=%s event=initialize", kPluginVersion);
    patches::initializeRuntime();
}

DEINITIALIZE_PLUGIN() {
    patches::deinitializeRuntime();
    LOG_INFO("plugin deinitialized");
    deinitLogging();
}

ON_APPLICATION_START() {
    uint64_t titleId = OSGetTitleID();
    const uint16_t titleVersion = getOwnTitleVersion(titleId, &titleId);
    patches::reportMenuBuild(titleId, titleVersion);
}

ON_APPLICATION_ENDS() {
    patches::finishApplication();
}
