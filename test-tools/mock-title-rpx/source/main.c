#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>

int main(int argc, char **argv) {
    WHBProcInit();
    WHBLogConsoleInit();

    WHBLogPrintf("Wii U Menu title-limit test");
    WHBLogPrintf(" ");
    WHBLogPrintf("This is a harmless SD-card mock title.");
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        WHBLogPrintf("Launched as: %s", argv[0]);
    }
    WHBLogPrintf(" ");
    WHBLogPrintf("Press HOME to return to the Wii U Menu.");

    while (WHBProcIsRunning()) {
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(50));
    }

    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
