#pragma once

#include <coreinit/debug.h>
#include <string.h>
#include <whb/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_APP_TYPE "P"
#define LOG_APP_NAME "MenuTitleLimit"

#define __FILENAME__ ({                                \
    const char *__filename = __FILE__;                 \
    const char *__pos = strrchr(__filename, '/');      \
    if (!__pos) __pos = strrchr(__filename, '\\');    \
    __pos ? __pos + 1 : __filename;                    \
})

#define LOG_EX(LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ARGS...)                                    \
    do {                                                                                         \
        LOG_FUNC("[(%s)%s][%s]%s@L%04d: " LOG_LEVEL FMT LINE_END,                              \
                 LOG_APP_TYPE, LOG_APP_NAME, __FILENAME__, __FUNCTION__, __LINE__, ##ARGS);      \
    } while (0)

#ifdef DEBUG
#define LOG_INFO(FMT, ARGS...) LOG_EX(WHBLogPrintf, "INFO  ", "", FMT, ##ARGS)
#define LOG_WARN(FMT, ARGS...) LOG_EX(WHBLogPrintf, "WARN  ", "", FMT, ##ARGS)
#define LOG_ERROR(FMT, ARGS...) LOG_EX(WHBLogPrintf, "ERROR ", "", FMT, ##ARGS)
#else
#define LOG_INFO(FMT, ARGS...) LOG_EX(OSReport, "INFO  ", "\n", FMT, ##ARGS)
#define LOG_WARN(FMT, ARGS...) LOG_EX(OSReport, "WARN  ", "\n", FMT, ##ARGS)
#define LOG_ERROR(FMT, ARGS...) LOG_EX(OSReport, "ERROR ", "\n", FMT, ##ARGS)
#endif

void initLogging(void);
void deinitLogging(void);
void writeStatusLog(const char *format, ...);

#ifdef __cplusplus
}
#endif
