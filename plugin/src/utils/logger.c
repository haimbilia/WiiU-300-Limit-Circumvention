#ifdef DEBUG
#include <stdint.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>

static uint32_t sModuleLogInit = 0;
static uint32_t sCafeLogInit = 0;
static uint32_t sUdpLogInit = 0;
#endif

#include <stdarg.h>
#include <stdio.h>

#define STATUS_LOG_PATH "fs:/vol/external01/wiiu/environments/aroma/plugins/WiiUMenuTitleLimit-status.log"

void initLogging(void) {
#ifdef DEBUG
    if (!(sModuleLogInit = WHBLogModuleInit())) {
        sCafeLogInit = WHBLogCafeInit();
        sUdpLogInit = WHBLogUdpInit();
    }
#endif
}

void deinitLogging(void) {
#ifdef DEBUG
    if (sModuleLogInit) {
        WHBLogModuleDeinit();
        sModuleLogInit = 0;
    }
    if (sCafeLogInit) {
        WHBLogCafeDeinit();
        sCafeLogInit = 0;
    }
    if (sUdpLogInit) {
        WHBLogUdpDeinit();
        sUdpLogInit = 0;
    }
#endif
}

void writeStatusLog(const char *format, ...) {
    FILE *file = fopen(STATUS_LOG_PATH, "a");
    if (!file) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    vfprintf(file, format, arguments);
    va_end(arguments);
    fputc('\n', file);
    fclose(file);
}
