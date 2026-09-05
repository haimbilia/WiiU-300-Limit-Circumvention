#include "patch_runtime.h"

#include "utils/logger.h"

#include <coreinit/cache.h>
#include <coreinit/dynload.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memorymap.h>
#include <coreinit/thread.h>
#include <function_patcher/function_patching.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string_view>

#ifndef TITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH
#define TITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH 0
#endif

namespace patches {
namespace {
constexpr uint64_t kUsaMenuTitleId = 0x0005001010040100ULL;
constexpr uint16_t kUsaMenuVersion = 277;
constexpr uint32_t kCanonicalTextBase = 0x02000000;
constexpr uint32_t kExtendedSlotCapacity = 810;
constexpr uint32_t kExtendedTitleBufferCapacity = kExtendedSlotCapacity + 4;
static_assert(kExtendedTitleBufferCapacity == 0x32e);
constexpr const char *kProfileName = "USA-v277-b67deb8fb368";
constexpr size_t kPatchCount = 107;
constexpr uint32_t kIconEvictionFunctionOffset = 0x000cef5c;
constexpr uint32_t kTitleListBuildFunctionOffset = 0x000d57fc;
constexpr uint32_t kLayoutIngestFunctionOffset = 0x001716dc;
constexpr uint32_t kLayoutReconcileFunctionOffset = 0x00171f40;
constexpr uint32_t kLayoutFinalizeFunctionOffset = 0x001736f8;
constexpr uint32_t kModelOwnerFunctionOffset = 0x00166ab8;
constexpr uint32_t kSaveSyncFunctionOffset = 0x00166c50;
constexpr uint32_t kRootSaveSyncFunctionOffset = 0x001719f8;
constexpr uint32_t kFolderSaveSyncFunctionOffset = 0x001700f4;
constexpr uint32_t kUiPopulateFunctionOffset = 0x0020c08c;
constexpr uint32_t kTitleUiOpFunctionOffset = 0x0016f298;
constexpr uint32_t kIconCheckFunctionOffset = 0x00247e88;
constexpr uint32_t kModelDispatchFunctionOffset = 0x002663ac;
constexpr uint32_t kUiOrchestratorFunctionOffset = 0x001fa5bc;
constexpr uint32_t kSceneUpdateFunctionOffset = 0x0020cb30;
constexpr uint32_t kAssertCodeFunctionOffset = 0x00048080;
constexpr uint32_t kPanicFunctionOffset = 0x0042ce04;
constexpr uint32_t kIconRecordCount = 300;
constexpr uint32_t kIconRecordSize = 0x20;
constexpr uint32_t kIconRecordsOffset = 0x20;
constexpr uint32_t kIconEvictionTraceLimit = 32;
constexpr uint64_t kTargetTitleIds[] = {kUsaMenuTitleId};

using KernelCopyDataFn = void (*)(uint32_t destination, uint32_t source,
                                  uint32_t length);

struct SignatureWord {
    int32_t delta;
    uint32_t expected;
};

struct InstructionPatch {
    const char *name;
    uint32_t canonicalAddress;
    uint32_t expected;
    uint32_t replacement;
    const SignatureWord *signature;
    size_t signatureCount;
};

constexpr SignatureWord kScratchVectorSignature[] = {
        {-4, 0x387f001c},
        {0, 0x3880012c},
        {4, 0x38a00000},
        {8, 0x38c00004},
        {12, 0x482bc699},
};

constexpr SignatureWord kFullFlagSignature[] = {
        {-4, 0x38180002},
        {0, 0x2000012c},
        {4, 0x7d800110},
        {8, 0x7d2c00d0},
        {12, 0x7fa3eb78},
        {16, 0x99390008},
};

constexpr std::array<InstructionPatch, kPatchCount> kPatches = {{
        {
                "layout-scratch-vector",
                0x021714bc,
                0x3880012c, // li r4, 300
                0x3880032a, // li r4, 810
                kScratchVectorSignature,
                std::size(kScratchVectorSignature),
        },
        {
                "layout-full-flag",
                0x0217385c,
                0x2000012c, // subfic r0, r0, 300
                0x2000032a, // subfic r0, r0, 810
                kFullFlagSignature,
                std::size(kFullFlagSignature),
        },
        // The stock layout owns three 360-record vectors and many loops that
        // traverse them. Raise the complete coherent layout path to 54 pages
        // (810 slots), including page validation and lookup/count helpers.
        {"layout-vector-zero-init-count", 0x02171278, 0x3ba00168,
         0x3ba0032a, nullptr, 0},
        {"layout-vector-two-init-count", 0x02171334, 0x3ba00168,
         0x3ba0032a, nullptr, 0},
        {"layout-vector-one-init-count", 0x021713f0, 0x3ba00168,
         0x3ba0032a, nullptr, 0},
        {"layout-vector-zero-capacity", 0x021714a8, 0x38800168,
         0x3880032a, nullptr, 0},
        {"layout-vector-one-capacity", 0x021714d4, 0x38800168,
         0x3880032a, nullptr, 0},
        {"layout-vector-two-capacity", 0x021714ec, 0x38800168,
         0x3880032a, nullptr, 0},
        {"layout-reset-vector-one-count", 0x0217153c, 0x3b800168,
         0x3b80032a, nullptr, 0},
        {"layout-reset-vector-two-count", 0x021715c8, 0x3b800168,
         0x3b80032a, nullptr, 0},
        {"layout-reset-vector-zero-count", 0x02171644, 0x3b800168,
         0x3b80032a, nullptr, 0},
        {"layout-ingest-count", 0x02171708, 0x3b000168, 0x3b00032a,
         nullptr, 0},
        {"layout-load-vector-one-size-check", 0x02171874, 0x2c050168,
         0x2c05032a, nullptr, 0},
        {"layout-load-vector-one-count", 0x02171898, 0x3be00168,
         0x3be0032a, nullptr, 0},
        {"layout-load-vector-two-size-check", 0x02171940, 0x2c060168,
         0x2c06032a, nullptr, 0},
        {"layout-load-vector-two-count", 0x02171964, 0x3bc00168,
         0x3bc0032a, nullptr, 0},
        {"layout-save-vector-count", 0x02171a3c, 0x3b600168,
         0x3b60032a, nullptr, 0},
        {"layout-reconcile-phase-zero-count", 0x02171f54, 0x3b600168,
         0x3b60032a, nullptr, 0},
        {"layout-reconcile-phase-one-count", 0x021722cc, 0x3a800168,
         0x3a80032a, nullptr, 0},
        {"layout-reconcile-phase-two-count", 0x02172788, 0x3a600168,
         0x3a60032a, nullptr, 0},
        {"layout-reconcile-phase-three-count", 0x02172d88, 0x3ba00168,
         0x3ba0032a, nullptr, 0},
        {"layout-reconcile-phase-four-count", 0x021731b8, 0x3b400168,
         0x3b40032a, nullptr, 0},
        // The reconciliation scratch grid is a 60-row array of 60-cell
        // records.  Fifty-four Menu pages can generate an outer coordinate
        // above 59, so grow only the outer row allocation to 96.  Keep the
        // established 0x400-byte row format and 60-cell inner dimension.
        {"layout-grid-allocation-upper", 0x021643ac, 0x3c600001,
         0x3c600002, nullptr, 0}, // lis r3,1 -> 2
        {"layout-grid-allocation-lower", 0x021643b0, 0x3863f010,
         0x38638010, nullptr, 0}, // 0xf010 -> 0x18010
        {"layout-grid-constructor-upper", 0x0216fa04, 0x3fc00001,
         0x3fc00002, nullptr, 0}, // lis r30,1 -> 2
        {"layout-grid-constructor-lower", 0x0216fa0c, 0x3bdef000,
         0x3bde8000, nullptr, 0}, // 0xf000 -> 0x18000
        {"layout-grid-construction-row-count", 0x0216fa48, 0x3880003c,
         0x38800060, nullptr, 0}, // 60 -> 96 rows
        {"layout-grid-dirty-flag-upper", 0x0216fedc, 0x3d830001,
         0x3d830002, nullptr, 0},
        {"layout-grid-dirty-flag-lower", 0x0216fee4, 0x980cf008,
         0x980c8008, nullptr, 0}, // +0xf008 -> +0x18008
        {"layout-grid-phase-one-row-bound-a", 0x02172310, 0x2800003c,
         0x28000060, nullptr, 0},
        {"layout-grid-phase-one-row-bound-b", 0x02172468, 0x2805003c,
         0x28050060, nullptr, 0},
        {"layout-grid-phase-one-row-bound-c", 0x02172574, 0x2808003c,
         0x28080060, nullptr, 0},
        {"layout-grid-phase-one-row-bound-d", 0x02172654, 0x2800003c,
         0x28000060, nullptr, 0},
        {"layout-grid-phase-one-row-bound-e", 0x021726dc, 0x2806003c,
         0x28060060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-a", 0x021727d0, 0x2800003c,
         0x28000060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-b", 0x021728bc, 0x2800003c,
         0x28000060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-c", 0x021729a4, 0x2809003c,
         0x28090060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-d", 0x02172ac4, 0x280a003c,
         0x280a0060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-e", 0x02172b7c, 0x2806003c,
         0x28060060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-f", 0x02172c5c, 0x2809003c,
         0x28090060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-g", 0x02172cb0, 0x280a003c,
         0x280a0060, nullptr, 0},
        {"layout-grid-phase-two-row-bound-h", 0x02172ce0, 0x280a003c,
         0x280a0060, nullptr, 0},
        {"layout-page-upper-bound", 0x02173804, 0x2c1f0018,
         0x2c1f0036, nullptr, 0}, // 24 -> 54 pages
        {"layout-slot-product-check-one", 0x02173824, 0x2c000168,
         0x2c00032a, nullptr, 0},
        {"layout-page-clamp-check", 0x02173834, 0x2c0c0018,
         0x2c0c0036, nullptr, 0},
        {"layout-page-clamp-value", 0x0217383c, 0x39800018,
         0x39800036, nullptr, 0},
        {"layout-slot-product-check-two", 0x02173844, 0x2c000168,
         0x2c00032a, nullptr, 0},
        {"layout-title-lookup-count", 0x02173bd4, 0x3b600168,
         0x3b60032a, nullptr, 0},
        {"layout-valid-title-count", 0x02173c78, 0x3b800168,
         0x3b80032a, nullptr, 0},
        {"layout-valid-title-full-check", 0x02173d74, 0x6ba90168,
         0x6ba9032a, nullptr, 0},
        // FUN_0216436c owns a separate 360-entry banner/model vector used
        // while constructing the Menu scene. Keep it aligned with the
        // expanded 810-slot layout so all 54 pages have backing
        // records too.
        {"banner-vector-capacity", 0x021643e0, 0x38800168,
         0x3880032a, nullptr, 0},
        {"banner-vector-construction-count", 0x021643ec, 0x3ba00168,
         0x3ba0032a, nullptr, 0},
        // PageMany has only 24 prebuilt page-marker children. Keep the real
        // requested page count stored in the widget, but stop its cosmetic
        // child-initialization loop at the vector capacity instead of taking
        // the stock panic path when pages beyond 24 are present.
        {"page-indicator-child-capacity-guard", 0x02001f5c, 0x40800060,
         0x4080004c, nullptr, 0}, // bge panic -> bge normal epilogue
        // FUN_020019e4 is the matching per-frame update for PageMany. Stop
        // both child walks at the 24-object vector and make marker selection
        // on pages beyond 24 a no-op while retaining the real selected index.
        {"page-indicator-update-primary-capacity-guard", 0x02001a40,
         0x4080001c, 0x40800074, nullptr, 0}, // bge panic -> loop tail
        {"page-indicator-update-secondary-capacity-guard", 0x02001a78,
         0x4080002c, 0x4080003c, nullptr, 0}, // bge panic -> loop tail
        {"page-indicator-select-out-of-range-noop", 0x02001b84,
         0x4842b281, 0x48000024, nullptr, 0}, // panic call -> store index
        {"page-indicator-deselect-out-of-range-noop", 0x02001be8,
         0x4842b21d, 0x48000024, nullptr, 0}, // panic call -> continue
        // FUN_02274a0c copies the active Menu page count into launch state but
        // asserts unless (pages - 4) is below 21, i.e. at most 24 pages.
        // Raise the exclusive bound to 51 so all 54 pages are accepted.
        {"launch-state-page-count-bound", 0x02274a60, 0x28050015,
         0x28050033, nullptr, 0},
        // FUN_02179a24 constructs the destination record vector consumed by
        // FUN_02179f8c/FUN_0217a120. Keep its size in lockstep with the
        // enlarged temporary title-id buffers below.
        {"title-record-vector-capacity", 0x02179a40, 0x38800130, 0x3880032e,
         nullptr, 0}, // li r4,304 -> 814
        {"title-record-construction-count", 0x02179a50, 0x3bc00130,
         0x3bc0032e, nullptr, 0}, // li r30,304 -> 814
        // FUN_02179f8c owns an inline 304-entry title-id array at sp+0x8.
        // Grow its frame by 510 * 8 bytes and move ABI save slots above it.
        {"refresh-frame-allocate", 0x02179f90, 0x9421f668, 0x9421e678,
         nullptr, 0}, // stwu r1,-0x998(r1) -> -0x1988
        {"refresh-save-r29", 0x02179f94, 0x93a1098c, 0x93a1197c,
         nullptr, 0},
        {"refresh-save-r30", 0x02179f98, 0x93c10990, 0x93c11980,
         nullptr, 0},
        {"refresh-save-r31", 0x02179f9c, 0x93e10994, 0x93e11984,
         nullptr, 0},
        {"refresh-save-lr", 0x02179fa4, 0x9001099c, 0x9001198c,
         nullptr, 0},
        {"refresh-buffer-capacity", 0x02179fd8, 0x38800130, 0x3880032e,
         nullptr, 0}, // li r4,304 -> 814
        {"refresh-restore-r29", 0x0217a078, 0x83a1098c, 0x83a1197c,
         nullptr, 0},
        {"refresh-restore-lr", 0x0217a07c, 0x8001099c, 0x8001198c,
         nullptr, 0},
        {"refresh-restore-r30", 0x0217a080, 0x83c10990, 0x83c11980,
         nullptr, 0},
        {"refresh-restore-r31", 0x0217a088, 0x83e10994, 0x83e11984,
         nullptr, 0},
        {"refresh-frame-release", 0x0217a08c, 0x38210998, 0x38211988,
         nullptr, 0},

        // FUN_021699f0 converts title IDs for the BOSS new-arrival service in
        // a third 304-entry stack array. Enlarge its frame and both bounds.
        {"new-arrival-frame-allocate", 0x021699f0, 0x9421f650, 0x9421e660,
         nullptr, 0}, // stwu r1,-0x9b0(r1) -> -0x19a0
        {"new-arrival-save-registers", 0x021699f8, 0xbf010990,
         0xbf011980, nullptr, 0},
        {"new-arrival-input-bound", 0x02169a04, 0x281c0130, 0x281c032e,
         nullptr, 0}, // cmplwi r28,304 -> 814
        {"new-arrival-save-lr", 0x02169a10, 0x900109b4, 0x900119a4,
         nullptr, 0},
        {"new-arrival-array-count", 0x02169a28, 0x38800130, 0x3880032e,
         nullptr, 0}, // li r4,304 -> 814
        {"new-arrival-restore-registers-failure", 0x02169b0c, 0xbb010990,
         0xbb011980, nullptr, 0},
        {"new-arrival-restore-lr-failure", 0x02169b10, 0x800109b4,
         0x800119a4, nullptr, 0},
        {"new-arrival-frame-release-failure", 0x02169b18, 0x382109b0,
         0x382119a0, nullptr, 0},
        {"new-arrival-restore-registers-success", 0x02169b24, 0xbb010990,
         0xbb011980, nullptr, 0},
        {"new-arrival-restore-lr-success", 0x02169b28, 0x800109b4,
         0x800119a4, nullptr, 0},
        {"new-arrival-frame-release-success", 0x02169b30, 0x382109b0,
         0x382119a0, nullptr, 0},

        // FUN_0217a120 owns a byte-per-title result array followed by the
        // title-id array. Shift the latter by 512 bytes, enlarge it by 4080,
        // and move the ABI save area above both expanded arrays.
        {"startup-frame-allocate", 0x0217a124, 0x9421f538, 0x9421e348,
         nullptr, 0}, // stwu r1,-0xac8(r1) -> -0x1cb8
        {"startup-save-r29", 0x0217a128, 0x93a10abc, 0x93a11cac,
         nullptr, 0},
        {"startup-save-r28", 0x0217a12c, 0x93810ab8, 0x93811ca8,
         nullptr, 0},
        {"startup-save-r30", 0x0217a130, 0x93c10ac0, 0x93c11cb0,
         nullptr, 0},
        {"startup-save-r31", 0x0217a134, 0x93e10ac4, 0x93e11cb4,
         nullptr, 0},
        {"startup-save-lr", 0x0217a13c, 0x90010acc, 0x90011cbc,
         nullptr, 0},
        {"startup-title-buffer-base", 0x0217a160, 0x3ba10138,
         0x3ba10338, nullptr, 0},
        {"startup-buffer-capacity", 0x0217a164, 0x38800130, 0x3880032e,
         nullptr, 0}, // li r4,304 -> 814
        {"startup-restore-r28", 0x0217a250, 0x83810ab8, 0x83811ca8,
         nullptr, 0},
        {"startup-restore-r29", 0x0217a254, 0x83a10abc, 0x83a11cac,
         nullptr, 0},
        {"startup-restore-lr", 0x0217a258, 0x80010acc, 0x80011cbc,
         nullptr, 0},
        {"startup-restore-r30", 0x0217a25c, 0x83c10ac0, 0x83c11cb0,
         nullptr, 0},
        {"startup-restore-r31", 0x0217a264, 0x83e10ac4, 0x83e11cb4,
         nullptr, 0},
        {"startup-frame-release", 0x0217a268, 0x38210ac8, 0x38211cb8,
         nullptr, 0},

        // FUN_0217a270 repeats the same byte-result/title-id stack layout for
        // refreshes. Apply the identical 512-byte split and 4080-byte growth.
        {"update-frame-allocate", 0x0217a270, 0x9421f530, 0x9421e340,
         nullptr, 0}, // stwu r1,-0xad0(r1) -> -0x1cc0
        {"update-save-registers", 0x0217a274, 0xbf610abc, 0xbf611cac,
         nullptr, 0},
        {"update-save-lr", 0x0217a27c, 0x90010ad4, 0x90011cc4,
         nullptr, 0},
        {"update-title-copy-base", 0x0217a2b4, 0x3ba10130, 0x3ba10330,
         nullptr, 0},
        {"update-title-buffer-argument", 0x0217a31c, 0x38c10138,
         0x38c10338, nullptr, 0},
        {"update-restore-registers-failure", 0x0217a33c, 0xbb610abc,
         0xbb611cac, nullptr, 0},
        {"update-restore-lr-failure", 0x0217a340, 0x80010ad4,
         0x80011cc4, nullptr, 0},
        {"update-frame-release-failure", 0x0217a348, 0x38210ad0,
         0x38211cc0, nullptr, 0},
        {"update-restore-registers-success", 0x0217a4b0, 0xbb610abc,
         0xbb611cac, nullptr, 0},
        {"update-restore-lr-success", 0x0217a4b4, 0x80010ad4,
         0x80011cc4, nullptr, 0},
        {"update-frame-release-success", 0x0217a4bc, 0x38210ad0,
         0x38211cc0, nullptr, 0},
}};

struct LoadedExecutable {
    uint32_t textAddress;
    uint32_t textOffset;
    uint32_t textSize;
};

struct CacheWork {
    void *target;
};

struct CacheThread {
    OSThread *thread = nullptr;
    uint8_t *stack = nullptr;
    bool created = false;
};

OSDynLoad_Module sKernelModule = nullptr;
KernelCopyDataFn sKernelCopyData = nullptr;
std::array<uint32_t, kPatchCount> sAppliedAddresses{};
std::array<bool, kPatchCount> sPatchState{};
PatchedFunctionHandle sIconEvictionPatchHandle = 0;
PatchedFunctionHandle sTitleListBuildPatchHandle = 0;
PatchedFunctionHandle sLayoutIngestPatchHandle = 0;
PatchedFunctionHandle sLayoutReconcilePatchHandle = 0;
PatchedFunctionHandle sLayoutFinalizePatchHandle = 0;
PatchedFunctionHandle sModelOwnerPatchHandle = 0;
PatchedFunctionHandle sSaveSyncPatchHandle = 0;
PatchedFunctionHandle sRootSaveSyncPatchHandle = 0;
PatchedFunctionHandle sFolderSaveSyncPatchHandle = 0;
PatchedFunctionHandle sUiPopulatePatchHandle = 0;
PatchedFunctionHandle sTitleUiOpPatchHandle = 0;
PatchedFunctionHandle sIconCheckPatchHandle = 0;
PatchedFunctionHandle sModelDispatchPatchHandle = 0;
PatchedFunctionHandle sUiOrchestratorPatchHandle = 0;
PatchedFunctionHandle sSceneUpdatePatchHandle = 0;
PatchedFunctionHandle sAssertCodePatchHandle = 0;
PatchedFunctionHandle sPanicPatchHandle = 0;
bool sFunctionPatcherInitialized = false;
uint32_t sIconEvictionCalls = 0;
uint32_t sIconEvictionFallbacks = 0;
uint32_t sTitleListBuildCalls = 0;
uint32_t sLayoutIngestCalls = 0;
uint32_t sLayoutReconcileCalls = 0;
uint32_t sLayoutFinalizeCalls = 0;
uint32_t sModelOwnerCalls = 0;
uint32_t sSaveSyncCalls = 0;
uint32_t sRootSaveSyncCalls = 0;
uint32_t sFolderSaveSyncCalls = 0;
uint32_t sUiPopulateCalls = 0;
uint32_t sTitleUiOpCalls = 0;
uint32_t sIconCheckCalls = 0;
uint32_t sModelDispatchCalls = 0;
uint32_t sUiOrchestratorCalls = 0;
uint32_t sSceneUpdateCalls = 0;
uint32_t sAssertCodeCalls = 0;
uint32_t sPanicCalls = 0;

bool shouldTrace(uint32_t &counter) {
    const uint32_t call = counter++;
    return call < 8;
}

DECL_FUNCTION(bool, TraceTitleListBuild, int param1, int param2, int param3) {
    const bool trace = shouldTrace(sTitleListBuildCalls);
    if (trace) {
        writeStatusLog("trace=title-list-build event=enter call=%u",
                       sTitleListBuildCalls - 1);
    }
    const bool result = real_TraceTitleListBuild(param1, param2, param3);
    if (trace) {
        writeStatusLog("trace=title-list-build event=exit call=%u result=%u",
                       sTitleListBuildCalls - 1, result ? 1u : 0u);
    }
    return result;
}

DECL_FUNCTION(void, TraceLayoutIngest, int param1) {
    const bool trace = shouldTrace(sLayoutIngestCalls);
    if (trace) {
        writeStatusLog("trace=layout-ingest event=enter call=%u",
                       sLayoutIngestCalls - 1);
    }
    real_TraceLayoutIngest(param1);
    if (trace) {
        writeStatusLog("trace=layout-ingest event=exit call=%u",
                       sLayoutIngestCalls - 1);
    }
}

DECL_FUNCTION(void, TraceLayoutReconcile, int param1, uint32_t param2,
              uint32_t param3, uint32_t param4, uint32_t param5, int param6) {
    const bool trace = shouldTrace(sLayoutReconcileCalls);
    if (trace) {
        writeStatusLog("trace=layout-reconcile event=enter call=%u",
                       sLayoutReconcileCalls - 1);
    }
    real_TraceLayoutReconcile(param1, param2, param3, param4, param5, param6);
    if (trace) {
        writeStatusLog("trace=layout-reconcile event=exit call=%u",
                       sLayoutReconcileCalls - 1);
    }
}

DECL_FUNCTION(void, TraceLayoutFinalize, int param1, uint32_t param2,
              int param3, int param4, int param5, uint32_t param6) {
    const bool trace = shouldTrace(sLayoutFinalizeCalls);
    if (trace) {
        writeStatusLog("trace=layout-finalize event=enter call=%u",
                       sLayoutFinalizeCalls - 1);
    }
    real_TraceLayoutFinalize(param1, param2, param3, param4, param5, param6);
    if (trace) {
        writeStatusLog("trace=layout-finalize event=exit call=%u",
                       sLayoutFinalizeCalls - 1);
    }
}

DECL_FUNCTION(void, TraceModelOwner, uint32_t param1, uint32_t param2,
              uint32_t param3, uint32_t param4) {
    const bool trace = shouldTrace(sModelOwnerCalls);
    if (trace) {
        writeStatusLog("trace=model-owner event=enter call=%u",
                       sModelOwnerCalls - 1);
    }
    real_TraceModelOwner(param1, param2, param3, param4);
    if (trace) {
        writeStatusLog("trace=model-owner event=exit call=%u",
                       sModelOwnerCalls - 1);
    }
}

DECL_FUNCTION(void, TraceSaveSync) {
    const bool trace = shouldTrace(sSaveSyncCalls);
    if (trace) {
        writeStatusLog("trace=save-sync event=enter call=%u", sSaveSyncCalls - 1);
    }
    real_TraceSaveSync();
    if (trace) {
        writeStatusLog("trace=save-sync event=exit call=%u", sSaveSyncCalls - 1);
    }
}

DECL_FUNCTION(void, TraceRootSaveSync, int param1) {
    const bool trace = shouldTrace(sRootSaveSyncCalls);
    if (trace) {
        writeStatusLog("trace=root-save-sync event=enter call=%u",
                       sRootSaveSyncCalls - 1);
    }
    real_TraceRootSaveSync(param1);
    if (trace) {
        writeStatusLog("trace=root-save-sync event=exit call=%u",
                       sRootSaveSyncCalls - 1);
    }
}

DECL_FUNCTION(void, TraceFolderSaveSync, int param1) {
    const bool trace = shouldTrace(sFolderSaveSyncCalls);
    if (trace) {
        writeStatusLog("trace=folder-save-sync event=enter call=%u",
                       sFolderSaveSyncCalls - 1);
    }
    real_TraceFolderSaveSync(param1);
    if (trace) {
        writeStatusLog("trace=folder-save-sync event=exit call=%u",
                       sFolderSaveSyncCalls - 1);
    }
}

DECL_FUNCTION(void, TraceUiPopulate, int param1, uint32_t param2,
              uint32_t param3, uint32_t param4, uint32_t param5,
              uint32_t param6) {
    const bool trace = shouldTrace(sUiPopulateCalls);
    if (trace) {
        writeStatusLog("trace=ui-populate event=enter call=%u changed=%u removed=%u",
                       sUiPopulateCalls - 1, param4, param6);
    }
    real_TraceUiPopulate(param1, param2, param3, param4, param5, param6);
    if (trace) {
        writeStatusLog("trace=ui-populate event=exit call=%u titleOps=%u",
                       sUiPopulateCalls - 1, sTitleUiOpCalls);
    }
}

DECL_FUNCTION(void, TraceTitleUiOp, uint32_t titleHigh, uint32_t titleLow) {
    const uint32_t call = sTitleUiOpCalls++;
    const bool milestone = call < 4 || (call % 25) == 0 || call >= 330;
    if (milestone) {
        writeStatusLog("trace=title-ui-op event=enter call=%u title=%08x%08x",
                       call, titleHigh, titleLow);
    }
    real_TraceTitleUiOp(titleHigh, titleLow);
    if (milestone) {
        writeStatusLog("trace=title-ui-op event=exit call=%u", call);
    }
}

DECL_FUNCTION(uint32_t, TraceIconCheck, int param1, int param2) {
    const bool trace = shouldTrace(sIconCheckCalls);
    if (trace) {
        writeStatusLog("trace=icon-check event=enter call=%u mode=%d",
                       sIconCheckCalls - 1, param2);
    }
    const uint32_t result = real_TraceIconCheck(param1, param2);
    if (trace) {
        writeStatusLog("trace=icon-check event=exit call=%u result=%u",
                       sIconCheckCalls - 1, result);
    }
    return result;
}

__attribute__((always_inline)) inline uint32_t returnAddress() {
    return reinterpret_cast<uint32_t>(__builtin_return_address(0));
}

DECL_FUNCTION(void, TraceModelDispatch) {
    const uint32_t call = sModelDispatchCalls++;
    writeStatusLog("trace=model-dispatch event=enter call=%u caller=%08x", call,
                   returnAddress());
    real_TraceModelDispatch();
    writeStatusLog("trace=model-dispatch event=exit call=%u", call);
}

DECL_FUNCTION(void, TraceUiOrchestrator, int param1, uint32_t param2,
              uint32_t param3, uint32_t param4, uint32_t param5,
              uint32_t param6) {
    const uint32_t call = sUiOrchestratorCalls++;
    writeStatusLog("trace=ui-orchestrator event=enter call=%u caller=%08x changed=%u removed=%u",
                   call, returnAddress(), param4, param6);
    real_TraceUiOrchestrator(param1, param2, param3, param4, param5, param6);
    writeStatusLog("trace=ui-orchestrator event=exit call=%u", call);
}

DECL_FUNCTION(void, TraceSceneUpdate, int param1) {
    const uint32_t call = sSceneUpdateCalls++;
    if (call < 8) {
        writeStatusLog("trace=scene-update event=enter call=%u caller=%08x", call,
                       returnAddress());
    }
    real_TraceSceneUpdate(param1);
    if (call < 8) {
        writeStatusLog("trace=scene-update event=exit call=%u", call);
    }
}

DECL_FUNCTION(void, TraceAssertCode, uint32_t code) {
    const uint32_t call = sAssertCodeCalls++;
    if (call < 32) {
        writeStatusLog("trace=assert-code call=%u code=%u caller=%08x", call,
                       code, returnAddress());
    }
    real_TraceAssertCode(code);
}

DECL_FUNCTION(uint32_t, TracePanic) {
    const uint32_t call = sPanicCalls++;
    writeStatusLog("trace=panic call=%u caller=%08x", call, returnAddress());
    return real_TracePanic();
}

int selectOldestIconRecordIgnoringPins(int cacheAddress) {
    if (!cacheAddress) {
        return -1;
    }

    int selected = -1;
    uint32_t oldestHigh = UINT32_MAX;
    uint32_t oldestLow = UINT32_MAX;
    for (uint32_t index = 0; index < kIconRecordCount; ++index) {
        auto *record = reinterpret_cast<volatile const uint32_t *>(
                cacheAddress + kIconRecordsOffset + index * kIconRecordSize);
        const uint32_t timestampHigh = record[4];
        const uint32_t timestampLow = record[5];
        if (timestampHigh < oldestHigh ||
            (timestampHigh == oldestHigh && timestampLow <= oldestLow)) {
            oldestHigh = timestampHigh;
            oldestLow = timestampLow;
            selected = static_cast<int>(index);
        }
    }
    return selected;
}

DECL_FUNCTION(int, IconCacheFindEvictionCandidate, int cacheAddress) {
    const int originalResult = real_IconCacheFindEvictionCandidate(cacheAddress);
    const uint32_t callIndex = sIconEvictionCalls++;
    if (callIndex < kIconEvictionTraceLimit) {
        writeStatusLog("icon-evict call=%u original=%d", callIndex,
                       originalResult);
    }

    if (originalResult >= 0) {
        return originalResult;
    }

    const int fallback = selectOldestIconRecordIgnoringPins(cacheAddress);
    ++sIconEvictionFallbacks;
    writeStatusLog("icon-evict fallback=%u selected=%d",
                   static_cast<uint32_t>(sIconEvictionFallbacks), fallback);
    return fallback;
}

function_replacement_data_t sIconEvictionPatch =
        REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(
                IconCacheFindEvictionCandidate, kTargetTitleIds,
                std::size(kTargetTitleIds), "men.rpx",
                kIconEvictionFunctionOffset, kUsaMenuVersion,
                kUsaMenuVersion);

function_replacement_data_t sTitleListBuildPatch =
        REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(
                TraceTitleListBuild, kTargetTitleIds,
                std::size(kTargetTitleIds), "men.rpx",
                kTitleListBuildFunctionOffset, kUsaMenuVersion,
                kUsaMenuVersion);

function_replacement_data_t sLayoutIngestPatch =
        REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(
                TraceLayoutIngest, kTargetTitleIds,
                std::size(kTargetTitleIds), "men.rpx",
                kLayoutIngestFunctionOffset, kUsaMenuVersion,
                kUsaMenuVersion);

function_replacement_data_t sLayoutReconcilePatch =
        REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(
                TraceLayoutReconcile, kTargetTitleIds,
                std::size(kTargetTitleIds), "men.rpx",
                kLayoutReconcileFunctionOffset, kUsaMenuVersion,
                kUsaMenuVersion);

function_replacement_data_t sLayoutFinalizePatch =
        REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(
                TraceLayoutFinalize, kTargetTitleIds,
                std::size(kTargetTitleIds), "men.rpx",
                kLayoutFinalizeFunctionOffset, kUsaMenuVersion,
                kUsaMenuVersion);

#define DEFINE_TRACE_PATCH(variable, function, offset)                         \
    function_replacement_data_t variable =                                    \
            REPLACE_FUNCTION_OF_EXECUTABLE_BY_ADDRESS_WITH_VERSION(           \
                    function, kTargetTitleIds, std::size(kTargetTitleIds),     \
                    "men.rpx", offset, kUsaMenuVersion, kUsaMenuVersion)

DEFINE_TRACE_PATCH(sModelOwnerPatch, TraceModelOwner, kModelOwnerFunctionOffset);
DEFINE_TRACE_PATCH(sSaveSyncPatch, TraceSaveSync, kSaveSyncFunctionOffset);
DEFINE_TRACE_PATCH(sRootSaveSyncPatch, TraceRootSaveSync,
                   kRootSaveSyncFunctionOffset);
DEFINE_TRACE_PATCH(sFolderSaveSyncPatch, TraceFolderSaveSync,
                   kFolderSaveSyncFunctionOffset);
DEFINE_TRACE_PATCH(sUiPopulatePatch, TraceUiPopulate, kUiPopulateFunctionOffset);
DEFINE_TRACE_PATCH(sTitleUiOpPatch, TraceTitleUiOp, kTitleUiOpFunctionOffset);
DEFINE_TRACE_PATCH(sIconCheckPatch, TraceIconCheck, kIconCheckFunctionOffset);
DEFINE_TRACE_PATCH(sModelDispatchPatch, TraceModelDispatch,
                   kModelDispatchFunctionOffset);
DEFINE_TRACE_PATCH(sUiOrchestratorPatch, TraceUiOrchestrator,
                   kUiOrchestratorFunctionOffset);
DEFINE_TRACE_PATCH(sSceneUpdatePatch, TraceSceneUpdate,
                   kSceneUpdateFunctionOffset);
DEFINE_TRACE_PATCH(sAssertCodePatch, TraceAssertCode,
                   kAssertCodeFunctionOffset);
DEFINE_TRACE_PATCH(sPanicPatch, TracePanic, kPanicFunctionOffset);

#undef DEFINE_TRACE_PATCH

bool installTracePatch(function_replacement_data_t *patch,
                       PatchedFunctionHandle *handle, const char *name) {
    bool hasBeenPatched = false;
    const FunctionPatcherStatus status = FunctionPatcher_AddFunctionPatch(
            patch, handle, &hasBeenPatched);
    writeStatusLog("trace=%s event=install code=%d patched=%u", name,
                   static_cast<int>(status), hasBeenPatched ? 1u : 0u);
    return status == FUNCTION_PATCHER_RESULT_SUCCESS && hasBeenPatched;
}

void removeTracePatches() {
    PatchedFunctionHandle *handles[] = {
            &sPanicPatchHandle,
            &sAssertCodePatchHandle,
            &sSceneUpdatePatchHandle,
            &sUiOrchestratorPatchHandle,
            &sModelDispatchPatchHandle,
            &sIconCheckPatchHandle,
            &sTitleUiOpPatchHandle,
            &sUiPopulatePatchHandle,
            &sFolderSaveSyncPatchHandle,
            &sRootSaveSyncPatchHandle,
            &sSaveSyncPatchHandle,
            &sModelOwnerPatchHandle,
            &sLayoutFinalizePatchHandle,
            &sLayoutReconcilePatchHandle,
            &sLayoutIngestPatchHandle,
            &sTitleListBuildPatchHandle,
    };
    for (auto *handle : handles) {
        if (*handle != 0) {
            FunctionPatcher_RemoveFunctionPatch(*handle);
            *handle = 0;
        }
    }
}

bool installTracePatches() {
    sTitleListBuildCalls = 0;
    sLayoutIngestCalls = 0;
    sLayoutReconcileCalls = 0;
    sLayoutFinalizeCalls = 0;
    sModelOwnerCalls = 0;
    sSaveSyncCalls = 0;
    sRootSaveSyncCalls = 0;
    sFolderSaveSyncCalls = 0;
    sUiPopulateCalls = 0;
    sTitleUiOpCalls = 0;
    sIconCheckCalls = 0;
    sModelDispatchCalls = 0;
    sUiOrchestratorCalls = 0;
    sSceneUpdateCalls = 0;
    sAssertCodeCalls = 0;
    sPanicCalls = 0;

    // FunctionPatcher registrations survive application transitions and are
    // automatically applied to the next men.rpx instance. Registering the
    // same replacements again on return from a title chains the wrappers and
    // recurses indefinitely, so reuse the existing atomic hook set.
    if (sTitleListBuildPatchHandle != 0) {
        bool isPatched = false;
        const FunctionPatcherStatus status = FunctionPatcher_IsFunctionPatched(
                sTitleListBuildPatchHandle, &isPatched);
        writeStatusLog("trace=hooks event=reuse code=%d patched=%u",
                       static_cast<int>(status), isPatched ? 1u : 0u);
        return status == FUNCTION_PATCHER_RESULT_SUCCESS && isPatched;
    }

    if (!installTracePatch(&sTitleListBuildPatch, &sTitleListBuildPatchHandle,
                           "title-list-build") ||
        !installTracePatch(&sLayoutIngestPatch, &sLayoutIngestPatchHandle,
                           "layout-ingest") ||
        !installTracePatch(&sLayoutReconcilePatch,
                           &sLayoutReconcilePatchHandle,
                           "layout-reconcile") ||
        !installTracePatch(&sLayoutFinalizePatch, &sLayoutFinalizePatchHandle,
                           "layout-finalize") ||
        !installTracePatch(&sModelOwnerPatch, &sModelOwnerPatchHandle,
                           "model-owner") ||
        !installTracePatch(&sSaveSyncPatch, &sSaveSyncPatchHandle,
                           "save-sync") ||
        !installTracePatch(&sRootSaveSyncPatch, &sRootSaveSyncPatchHandle,
                           "root-save-sync") ||
        !installTracePatch(&sFolderSaveSyncPatch, &sFolderSaveSyncPatchHandle,
                           "folder-save-sync") ||
        !installTracePatch(&sUiPopulatePatch, &sUiPopulatePatchHandle,
                           "ui-populate") ||
        !installTracePatch(&sTitleUiOpPatch, &sTitleUiOpPatchHandle,
                           "title-ui-op") ||
        !installTracePatch(&sIconCheckPatch, &sIconCheckPatchHandle,
                           "icon-check") ||
        !installTracePatch(&sModelDispatchPatch, &sModelDispatchPatchHandle,
                           "model-dispatch") ||
        !installTracePatch(&sUiOrchestratorPatch,
                           &sUiOrchestratorPatchHandle,
                           "ui-orchestrator") ||
        !installTracePatch(&sSceneUpdatePatch, &sSceneUpdatePatchHandle,
                           "scene-update") ||
        !installTracePatch(&sAssertCodePatch, &sAssertCodePatchHandle,
                           "assert-code") ||
        !installTracePatch(&sPanicPatch, &sPanicPatchHandle,
                           "panic")) {
        removeTracePatches();
        return false;
    }
    return true;
}

bool installIconEvictionPatch() {
    if (!sFunctionPatcherInitialized) {
        const FunctionPatcherStatus initResult = FunctionPatcher_InitLibrary();
        if (initResult != FUNCTION_PATCHER_RESULT_SUCCESS) {
            LOG_ERROR("FunctionPatcher initialization failed result=%d",
                      static_cast<int>(initResult));
            writeStatusLog("profile=%s icon-evict-hook result=init-failed code=%d",
                           kProfileName, static_cast<int>(initResult));
            return false;
        }
        sFunctionPatcherInitialized = true;
    }

    if (sIconEvictionPatchHandle != 0) {
        bool isPatched = false;
        const FunctionPatcherStatus status = FunctionPatcher_IsFunctionPatched(
                sIconEvictionPatchHandle, &isPatched);
        if (status == FUNCTION_PATCHER_RESULT_SUCCESS && isPatched) {
            return true;
        }
        LOG_ERROR("icon eviction hook was registered but is not active result=%d",
                  static_cast<int>(status));
        writeStatusLog("profile=%s icon-evict-hook result=inactive code=%d",
                       kProfileName, static_cast<int>(status));
        return false;
    }

    bool hasBeenPatched = false;
    const FunctionPatcherStatus status = FunctionPatcher_AddFunctionPatch(
            &sIconEvictionPatch, &sIconEvictionPatchHandle, &hasBeenPatched);
    if (status != FUNCTION_PATCHER_RESULT_SUCCESS || !hasBeenPatched) {
        LOG_ERROR("icon eviction hook installation failed result=%d patched=%d",
                  static_cast<int>(status), hasBeenPatched);
        writeStatusLog("profile=%s icon-evict-hook result=install-failed code=%d patched=%u",
                       kProfileName, static_cast<int>(status),
                       hasBeenPatched ? 1u : 0u);
        if (sIconEvictionPatchHandle != 0) {
            FunctionPatcher_RemoveFunctionPatch(sIconEvictionPatchHandle);
            sIconEvictionPatchHandle = 0;
        }
        return false;
    }

    sIconEvictionCalls = 0;
    sIconEvictionFallbacks = 0;
    writeStatusLog("profile=%s icon-evict-hook result=active offset=%08x",
                   kProfileName, kIconEvictionFunctionOffset);
    return true;
}

void removeIconEvictionPatch() {
    if (sIconEvictionPatchHandle != 0) {
        const FunctionPatcherStatus status = FunctionPatcher_RemoveFunctionPatch(
                sIconEvictionPatchHandle);
        writeStatusLog("profile=%s icon-evict-hook result=removed code=%d calls=%u fallbacks=%u",
                       kProfileName, static_cast<int>(status),
                       static_cast<uint32_t>(sIconEvictionCalls),
                       static_cast<uint32_t>(sIconEvictionFallbacks));
        sIconEvictionPatchHandle = 0;
    }
}

bool anyPatchApplied() {
    for (const bool applied : sPatchState) {
        if (applied) {
            return true;
        }
    }
    return false;
}

uint32_t effectiveToPhysical(const void *address) {
    const auto effective = reinterpret_cast<uint32_t>(address);
    if (effective >= 0x00800000 && effective < 0x01000000) {
        return effective + (0x30800000 - 0x00800000);
    }
    return OSEffectiveToPhysical(effective);
}

bool readInstruction(uint32_t address, uint32_t *outInstruction) {
    if (!outInstruction || !sKernelCopyData) {
        return false;
    }

    // Avoid direct data-cache maintenance on the executable page. Copy the word
    // through the kernel module into a dedicated cache line, then invalidate
    // that line before consuming it. This mirrors Aroma's FunctionPatcher read
    // path without touching the target page's data cache.
    alignas(0x20) volatile uint32_t readBuffer[8]{};
    auto *bufferAddress = const_cast<uint32_t *>(readBuffer);
    const uint32_t destinationPhysical = effectiveToPhysical(bufferAddress);
    const uint32_t sourcePhysical = effectiveToPhysical(
            reinterpret_cast<void *>(address));
    if (!destinationPhysical || !sourcePhysical) {
        LOG_ERROR("instruction read translation failed address=%08x dstPhys=%08x srcPhys=%08x",
                  address, destinationPhysical, sourcePhysical);
        return false;
    }

    DCFlushRange(bufferAddress, sizeof(readBuffer));
    sKernelCopyData(destinationPhysical, sourcePhysical, sizeof(uint32_t));
    DCFlushRange(bufferAddress, sizeof(readBuffer));
    OSMemoryBarrier();
    *outInstruction = readBuffer[0];
    return true;
}

int invalidateInstructionCache(int32_t argc, const char **argv) {
    (void) argc;
    auto *work = reinterpret_cast<CacheWork *>(argv);
    ICInvalidateRange(work->target, sizeof(uint32_t));
    OSMemoryBarrier();
    return 0;
}

void releaseCacheThreads(std::array<CacheThread, 3> &threads,
                         bool runCreatedThreads) {
    if (runCreatedThreads) {
        for (auto &entry : threads) {
            if (entry.created) {
                OSResumeThread(entry.thread);
            }
        }
        for (auto &entry : threads) {
            if (entry.created) {
                OSJoinThread(entry.thread, nullptr);
            }
        }
    }

    for (auto &entry : threads) {
        if (entry.stack) {
            MEMFreeToDefaultHeap(entry.stack);
        }
        if (entry.thread) {
            MEMFreeToDefaultHeap(entry.thread);
        }
        entry = {};
    }
}

bool prepareCacheThreads(std::array<CacheThread, 3> &threads, CacheWork *work) {
    constexpr uint32_t stackSize = 0x2000;

    if (!MEMAllocFromDefaultHeapEx || !MEMFreeToDefaultHeap) {
        LOG_ERROR("default heap exports unavailable for cache synchronization");
        return false;
    }

    for (uint32_t core = 0; core < threads.size(); ++core) {
        auto &entry = threads[core];
        entry.thread = static_cast<OSThread *>(
                MEMAllocFromDefaultHeapEx(sizeof(OSThread), 0x10));
        entry.stack = static_cast<uint8_t *>(
                MEMAllocFromDefaultHeapEx(stackSize, 0x20));
        if (!entry.thread || !entry.stack) {
            LOG_ERROR("cache thread allocation failed core=%u", core);
            releaseCacheThreads(threads, true);
            return false;
        }

        const auto attributes = static_cast<OSThreadAttributes>(1u << core);
        if (!OSCreateThread(entry.thread, invalidateInstructionCache, 1,
                            reinterpret_cast<char *>(work),
                            entry.stack + stackSize, stackSize, 16, attributes)) {
            LOG_ERROR("cache thread creation failed core=%u", core);
            releaseCacheThreads(threads, true);
            return false;
        }
        entry.created = true;
    }
    return true;
}

bool writeInstruction(uint32_t address, uint32_t expected, uint32_t replacement) {
    if (!sKernelCopyData) {
        LOG_ERROR("KernelCopyData is unavailable");
        return false;
    }

    uint32_t current = 0;
    if (!readInstruction(address, &current)) {
        LOG_ERROR("instruction read failed address=%08x", address);
        return false;
    }
    if (current != expected) {
        LOG_ERROR("instruction changed address=%08x expected=%08x actual=%08x",
                  address, expected, current);
        return false;
    }

    uint32_t sourceWord = replacement;
    const uint32_t sourcePhysical = effectiveToPhysical(&sourceWord);
    const uint32_t targetPhysical = effectiveToPhysical(
            reinterpret_cast<void *>(address));
    if (!sourcePhysical || !targetPhysical) {
        LOG_ERROR("physical translation failed address=%08x srcPhys=%08x dstPhys=%08x",
                  address, sourcePhysical, targetPhysical);
        return false;
    }

    CacheWork work{reinterpret_cast<void *>(address)};
    std::array<CacheThread, 3> threads{};
    if (!prepareCacheThreads(threads, &work)) {
        return false;
    }

    DCFlushRange(&sourceWord, sizeof(sourceWord));
    sKernelCopyData(targetPhysical, sourcePhysical, sizeof(sourceWord));
    releaseCacheThreads(threads, true);

    uint32_t verified = 0;
    if (!readInstruction(address, &verified)) {
        LOG_ERROR("instruction verification read failed address=%08x", address);
        return false;
    }
    if (verified != replacement) {
        LOG_ERROR("instruction verification failed address=%08x expected=%08x actual=%08x",
                  address, replacement, verified);
        return false;
    }
    return true;
}

bool findMenuExecutable(LoadedExecutable *outExecutable) {
    if (!outExecutable) {
        return false;
    }

    const int32_t count = OSDynLoad_GetNumberOfRPLs();
    if (count <= 0 || count > 512) {
        LOG_ERROR("unexpected RPL count=%d", count);
        return false;
    }

    auto infos = std::unique_ptr<OSDynLoad_NotifyData[]>(
            new (std::nothrow) OSDynLoad_NotifyData[count]);
    if (!infos) {
        LOG_ERROR("failed to allocate RPL information count=%d", count);
        return false;
    }

    if (!OSDynLoad_GetRPLInfo(0, static_cast<uint32_t>(count), infos.get())) {
        LOG_ERROR("OSDynLoad_GetRPLInfo failed count=%d", count);
        return false;
    }

    for (int32_t index = 0; index < count; ++index) {
        const auto &info = infos[index];
        if (!info.name || !std::string_view(info.name).ends_with("men.rpx")) {
            continue;
        }
        *outExecutable = LoadedExecutable{
                info.textAddr,
                info.textOffset,
                info.textSize,
        };
        return true;
    }

    LOG_ERROR("men.rpx was not present in the loaded RPL list");
    return false;
}

bool resolvePatchAddress(const LoadedExecutable &executable,
                         const InstructionPatch &patch, uint32_t *outAddress) {
    if (!outAddress || patch.canonicalAddress < kCanonicalTextBase) {
        return false;
    }

    const uint32_t offset = patch.canonicalAddress - kCanonicalTextBase;
    if (executable.textSize < sizeof(uint32_t) ||
        offset > executable.textSize - sizeof(uint32_t) ||
        executable.textAddress > UINT32_MAX - offset) {
        LOG_ERROR("patch outside text name=%s offset=%08x textSize=%08x",
                  patch.name, offset, executable.textSize);
        return false;
    }
    *outAddress = executable.textAddress + offset;
    return true;
}

bool validatePatchSignature(const LoadedExecutable &executable,
                            const InstructionPatch &patch, uint32_t *outAddress) {
    uint32_t address = 0;
    if (!resolvePatchAddress(executable, patch, &address)) {
        return false;
    }

    // Validate every site before any instruction is modified. The wider
    // signatures remain additional build fingerprints for the key limits.
    uint32_t actualSite = 0;
    if (!readInstruction(address, &actualSite) || actualSite != patch.expected) {
        LOG_ERROR("patch-site mismatch name=%s address=%08x expected=%08x actual=%08x",
                  patch.name, address, patch.expected, actualSite);
        writeStatusLog("profile=%s patch-site=%s address=%08x expected=%08x actual=%08x result=mismatch",
                       kProfileName, patch.name, address, patch.expected,
                       actualSite);
        return false;
    }

    const uint32_t siteOffset = patch.canonicalAddress - kCanonicalTextBase;
    for (size_t index = 0; index < patch.signatureCount; ++index) {
        const auto &word = patch.signature[index];
        const int64_t checkOffset = static_cast<int64_t>(siteOffset) + word.delta;
        if (checkOffset < 0 ||
            static_cast<uint64_t>(checkOffset) + sizeof(uint32_t) > executable.textSize) {
            LOG_ERROR("signature outside text name=%s index=%u", patch.name,
                      static_cast<uint32_t>(index));
            return false;
        }

        const uint32_t checkAddress = executable.textAddress +
                                      static_cast<uint32_t>(checkOffset);
        uint32_t actual = 0;
        if (!readInstruction(checkAddress, &actual)) {
            LOG_ERROR("signature read failed name=%s index=%u address=%08x",
                      patch.name, static_cast<uint32_t>(index), checkAddress);
            writeStatusLog("profile=%s signature=%s index=%u address=%08x result=read-failed",
                           kProfileName, patch.name, static_cast<uint32_t>(index),
                           checkAddress);
            return false;
        }
        if (actual != word.expected) {
            const uint32_t direct = *reinterpret_cast<volatile const uint32_t *>(
                    checkAddress);
            LOG_ERROR("signature mismatch name=%s index=%u address=%08x expected=%08x actual=%08x",
                      patch.name, static_cast<uint32_t>(index), checkAddress,
                      word.expected, actual);
            writeStatusLog("profile=%s signature=%s index=%u address=%08x expected=%08x kernel=%08x direct=%08x result=mismatch",
                           kProfileName, patch.name, static_cast<uint32_t>(index),
                           checkAddress, word.expected, actual, direct);
            return false;
        }
    }

    *outAddress = address;
    return true;
}

bool restoreAppliedPatch() {
    if (!anyPatchApplied()) {
        return true;
    }

    bool restored = true;
    for (size_t index = kPatchCount; index-- > 0;) {
        if (!sPatchState[index]) {
            continue;
        }
        const auto &patch = kPatches[index];
        uint32_t current = 0;
        if (readInstruction(sAppliedAddresses[index], &current) &&
            current == patch.expected) {
            sPatchState[index] = false;
            sAppliedAddresses[index] = 0;
        } else if (!writeInstruction(sAppliedAddresses[index], patch.replacement,
                                     patch.expected)) {
            LOG_ERROR("failed to restore patch name=%s address=%08x", patch.name,
                      sAppliedAddresses[index]);
            restored = false;
        } else {
            sPatchState[index] = false;
            sAppliedAddresses[index] = 0;
        }
    }

    if (restored) {
        LOG_INFO("title-limit patch restored profile=%s", kProfileName);
    }
    return restored;
}

bool applyProfile(const LoadedExecutable &executable) {
    std::array<uint32_t, kPatchCount> addresses{};
    for (size_t index = 0; index < kPatchCount; ++index) {
        if (!validatePatchSignature(executable, kPatches[index], &addresses[index])) {
            LOG_ERROR("profile rejected profile=%s signatures=%u/%u mode=fail-closed",
                      kProfileName, static_cast<uint32_t>(index),
                      static_cast<uint32_t>(kPatchCount));
            writeStatusLog("profile=%s signatures=%u/%u mode=fail-closed result=rejected",
                           kProfileName, static_cast<uint32_t>(index),
                           static_cast<uint32_t>(kPatchCount));
            return false;
        }
    }

#if TITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH
    if (!installIconEvictionPatch()) {
        LOG_ERROR("profile rejected profile=%s reason=icon-eviction-hook",
                  kProfileName);
        return false;
    }
    if (!installTracePatches()) {
        LOG_ERROR("profile rejected profile=%s reason=trace-hooks",
                  kProfileName);
        removeIconEvictionPatch();
        return false;
    }

    size_t appliedCount = 0;
    for (; appliedCount < kPatchCount; ++appliedCount) {
        const auto &patch = kPatches[appliedCount];
        if (!writeInstruction(addresses[appliedCount], patch.expected,
                              patch.replacement)) {
            LOG_ERROR("patch application failed name=%s address=%08x",
                      patch.name, addresses[appliedCount]);
            uint32_t current = 0;
            if (readInstruction(addresses[appliedCount], &current) &&
                current == patch.replacement) {
                sAppliedAddresses[appliedCount] = addresses[appliedCount];
                sPatchState[appliedCount] = true;
                ++appliedCount;
            }
            break;
        }
        sAppliedAddresses[appliedCount] = addresses[appliedCount];
        sPatchState[appliedCount] = true;
        LOG_INFO("patched name=%s address=%08x original=%08x replacement=%08x",
                 patch.name, addresses[appliedCount], patch.expected,
                 patch.replacement);
    }

    if (appliedCount != kPatchCount) {
        const bool rollbackComplete = restoreAppliedPatch();
        removeTracePatches();
        removeIconEvictionPatch();
        writeStatusLog("profile=%s mode=fail-closed result=apply-failed rollback=%s",
                       kProfileName, rollbackComplete ? "complete" : "incomplete");
        return false;
    }

    LOG_INFO("title-limit patch active profile=%s signatures=%u/%u slots=%u mode=production",
             kProfileName, static_cast<uint32_t>(kPatchCount),
             static_cast<uint32_t>(kPatchCount), kExtendedSlotCapacity);
    writeStatusLog("profile=%s signatures=%u/%u slots=%u mode=production result=active",
                   kProfileName, static_cast<uint32_t>(kPatchCount),
                   static_cast<uint32_t>(kPatchCount), kExtendedSlotCapacity);
#else
    LOG_INFO("profile supported profile=%s signatures=%u/%u slots=%u mode=dry-run",
             kProfileName, static_cast<uint32_t>(kPatchCount),
             static_cast<uint32_t>(kPatchCount), kExtendedSlotCapacity);
    writeStatusLog("profile=%s signatures=%u/%u slots=%u mode=dry-run result=supported",
                   kProfileName, static_cast<uint32_t>(kPatchCount),
                   static_cast<uint32_t>(kPatchCount), kExtendedSlotCapacity);
#endif
    return true;
}

const char *menuRegion(uint64_t titleId) {
    switch (titleId) {
    case 0x0005001010040000ULL:
        return "JPN";
    case 0x0005001010040100ULL:
        return "USA";
    case 0x0005001010040200ULL:
        return "EUR";
    default:
        return nullptr;
    }
}
} // namespace

void initializeRuntime() {
    if (sKernelModule) {
        return;
    }

    if (OSDynLoad_Acquire("homebrew_kernel", &sKernelModule) != OS_DYNLOAD_OK) {
        LOG_ERROR("homebrew_kernel is unavailable; behavioral patch disabled");
        sKernelModule = nullptr;
        return;
    }

    if (OSDynLoad_FindExport(sKernelModule, OS_DYNLOAD_EXPORT_FUNC,
                             "KernelCopyData",
                             reinterpret_cast<void **>(&sKernelCopyData)) !=
        OS_DYNLOAD_OK) {
        LOG_ERROR("homebrew_kernel missing KernelCopyData; behavioral patch disabled");
        OSDynLoad_Release(sKernelModule);
        sKernelModule = nullptr;
        sKernelCopyData = nullptr;
        return;
    }
    LOG_INFO("kernel write primitive resolved");
}

void deinitializeRuntime() {
    if (anyPatchApplied()) {
        restoreAppliedPatch();
    }

    removeIconEvictionPatch();
    removeTracePatches();
    if (sFunctionPatcherInitialized) {
        FunctionPatcher_DeInitLibrary();
        sFunctionPatcherInitialized = false;
    }

    if (sKernelModule) {
        OSDynLoad_Release(sKernelModule);
        sKernelModule = nullptr;
        sKernelCopyData = nullptr;
    }

}

void reportMenuBuild(uint64_t titleId, uint16_t titleVersion) {
    const char *region = menuRegion(titleId);
    if (!region) {
        return;
    }

    if (titleId != kUsaMenuTitleId || titleVersion != kUsaMenuVersion) {
        LOG_INFO("menu build region=%s titleId=%016llx version=%u profile=unsupported mode=fail-closed",
                 region, static_cast<unsigned long long>(titleId),
                 static_cast<uint32_t>(titleVersion));
        writeStatusLog("titleId=%016llx version=%u profile=unsupported mode=fail-closed",
                       static_cast<unsigned long long>(titleId),
                       static_cast<uint32_t>(titleVersion));
        return;
    }

    if (anyPatchApplied()) {
        LOG_WARN("menu profile already active profile=%s", kProfileName);
        return;
    }

    LoadedExecutable executable{};
    if (!findMenuExecutable(&executable)) {
        LOG_ERROR("menu profile unavailable profile=%s", kProfileName);
        writeStatusLog("profile=%s mode=fail-closed result=runtime-unavailable",
                       kProfileName);
        return;
    }

    LOG_INFO("menu build region=%s titleId=%016llx version=%u profile=%s text=%08x textOffset=%08x textSize=%08x",
             region, static_cast<unsigned long long>(titleId),
             static_cast<uint32_t>(titleVersion), kProfileName,
             executable.textAddress, executable.textOffset, executable.textSize);
    writeStatusLog("profile=%s titleId=%016llx version=%u text=%08x textOffset=%08x textSize=%08x",
                   kProfileName, static_cast<unsigned long long>(titleId),
                   static_cast<uint32_t>(titleVersion), executable.textAddress,
                   executable.textOffset, executable.textSize);

#if TITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH
    if (!sKernelCopyData) {
        LOG_ERROR("profile validated path unavailable profile=%s reason=no-kernel-writer",
                  kProfileName);
        writeStatusLog("profile=%s mode=fail-closed result=no-kernel-writer",
                       kProfileName);
        return;
    }
#endif
    applyProfile(executable);
}

void finishApplication() {
    // RPX memory is being discarded. Keep the patch active through final Menu
    // serialization, then forget the addresses instead of writing into teardown.
    if (anyPatchApplied()) {
        LOG_INFO("application ended with title-limit patch active profile=%s",
                 kProfileName);
    }
    sAppliedAddresses = {};
    sPatchState = {};
}

} // namespace patches
