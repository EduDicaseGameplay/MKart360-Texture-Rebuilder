#include "xbox360/race8.h"
#ifdef XBOX360_PORT
#include "xbox360/netplay.h"
#endif
#ifndef GCC
#define gRaceState_AS_U16
#endif
#include <ultra64.h>
#ifdef XBOX360_PORT
extern void x360_progress(int stage);
#else
#define x360_progress(stage) ((void)0)
#endif
#include <PR/os.h>
#include <PR/ucode.h>
#include <macros.h>
#include <decode.h>
#include <mk64.h>
#include <course.h>
#include <string.h>

#include "profiler.h"
#include "main.h"
#ifdef XBOX360_PORT
#include "xbox360/platform.h"
#endif
#include "racing/memory.h"
#include "menus.h"
#include <segments.h>
#include <common_structs.h>
#include <defines.h>
#include "buffers.h"
#include "camera.h"
#include "profiler.h"
#include "race_logic.h"
#include "skybox_and_splitscreen.h"
#include "render_objects.h"
#include "effects.h"
#include "code_80281780.h"
#include "audio/external.h"
#include "code_800029B0.h"
#include "code_80280000.h"
#include "podium_ceremony_actors.h"
#include "menu_items.h"
#include "code_80057C60.h"
#include "profiler.h"
#include "player_controller.h"
#include "render_player.h"
#include "render_courses.h"
#include "actors.h"
#include "objects.h"
#include "actor_types.h"
#include "bomb_kart.h"
#include "replays.h"
#include <debug.h>
#include "crash_screen.h"
#include "buffers/gfx_output_buffer.h"

void func_80091B78(void);
void audio_init(void);
void create_debug_thread(void);
void start_debug_thread(void);
struct SPTask* create_next_audio_frame_task(void);

struct VblankHandler* gVblankHandler1 = NULL;
struct VblankHandler* gVblankHandler2 = NULL;

struct SPTask* gActiveSPTask = NULL;
struct SPTask* sCurrentAudioSPTask = NULL;
struct SPTask* sCurrentDisplaySPTask = NULL;
struct SPTask* sNextAudioSPTask = NULL;
struct SPTask* sNextDisplaySPTask = NULL;

struct Controller gControllers[NUM_PLAYERS + 1];
struct Controller* gControllerOne = &gControllers[0];
struct Controller* gControllerTwo = &gControllers[1];
struct Controller* gControllerThree = &gControllers[2];
struct Controller* gControllerFour = &gControllers[3];
struct Controller* gControllerFive = &gControllers[8]; // All physical controllers combined.`
struct Controller* gControllerSix = &gControllers[5];
struct Controller* gControllerSeven = &gControllers[6];
struct Controller* gControllerEight = &gControllers[7];

Player gPlayers[NUM_PLAYERS];
Player* gPlayerOne = &gPlayers[0];
Player* gPlayerTwo = &gPlayers[1];
Player* gPlayerThree = &gPlayers[2];
Player* gPlayerFour = &gPlayers[3];
Player* gPlayerFive = &gPlayers[4];
Player* gPlayerSix = &gPlayers[5];
Player* gPlayerSeven = &gPlayers[6];
Player* gPlayerEight = &gPlayers[7];

#if defined(XBOX360_PORT)
/* MK64_CROSSPLAY_R22_DETERMINISM_PARITY
 * Crossplay is x86 vs PPC.  Keep local presentation from mutating the
 * deterministic simulation state, and make the 30 Hz network clock use an
 * explicitly-defined binary32 reciprocal instead of compiler-dependent /30.
 */
static Player sR22CrossplayPlayers[NUM_PLAYERS];
static Object sR22CrossplayObjects[OBJECT_LIST_SIZE];
static struct Actor sR22CrossplayActors[ACTOR_LIST_SIZE];
static BombKart sR22CrossplayBombs[NUM_BOMB_KARTS_MAX];
static u16 sR22CrossplayRandomSeed;
static s32 sR22CrossplayPresentationSaved;

/* R69: 60Hz network clock accumulates 1 tick/race frame, 2/30Hz-menu frame. */
static u32 sR69NetClockFrame = 0;
static u32 sR69NetClockTicks = 0;
extern int x360_net_60fps_session(void);
static f32 r22_crossplay_frame_time(unsigned int frame) {
    union { u32 bits; f32 value; } inv30;
    if(x360_net_60fps_session())return (f32)sR69NetClockTicks*(1.0f/60.0f);
    if (!x360_net_crossplay()) {
        return (f32) frame / 30.0f;
    }
    /* Exact IEEE-754 binary32 bits for the reciprocal used by the PPC build. */
    inv30.bits = 0x3D088889U;
    return (f32) frame * inv30.value;
}

static void r22_crossplay_present_begin(void) {
    if (!x360_net_crossplay() || sR22CrossplayPresentationSaved) return;
    memcpy(sR22CrossplayPlayers, gPlayers, sizeof(sR22CrossplayPlayers));
    memcpy(sR22CrossplayObjects, gObjectList, sizeof(sR22CrossplayObjects));
    memcpy(sR22CrossplayActors, gActorList, sizeof(sR22CrossplayActors));
    memcpy(sR22CrossplayBombs, gBombKarts, sizeof(sR22CrossplayBombs));
    sR22CrossplayRandomSeed = gRandomSeed16;
    sR22CrossplayPresentationSaved = 1;
}

static void r22_crossplay_present_end(void) {
    if (!sR22CrossplayPresentationSaved) return;
    memcpy(gPlayers, sR22CrossplayPlayers, sizeof(sR22CrossplayPlayers));
    memcpy(gObjectList, sR22CrossplayObjects, sizeof(sR22CrossplayObjects));
    memcpy(gActorList, sR22CrossplayActors, sizeof(sR22CrossplayActors));
    memcpy(gBombKarts, sR22CrossplayBombs, sizeof(sR22CrossplayBombs));
    gRandomSeed16 = sR22CrossplayRandomSeed;
    sR22CrossplayPresentationSaved = 0;
}
#endif

Player* gPlayerOneCopy = &gPlayers[0];
Player* gPlayerTwoCopy = &gPlayers[1];
UNUSED Player* gPlayerThreeCopy = &gPlayers[2];
UNUSED Player* gPlayerFourCopy = &gPlayers[3];

UNUSED s32 D_800FD850[3];
struct GfxPool gGfxPools[2];
struct GfxPool* gGfxPool;

UNUSED s32 gfxPool_padding; // is this necessary?
struct VblankHandler gGameVblankHandler;
struct VblankHandler sSoundVblankHandler;
OSMesgQueue gDmaMesgQueue, gGameVblankQueue, gGfxVblankQueue, unused_gMsgQueue, gIntrMesgQueue, gSPTaskMesgQueue;
OSMesgQueue sSoundMesgQueue;
OSMesg sSoundMesgBuf[1];
OSMesg gDmaMesgBuf[1], gGameMesgBuf;
OSMesg gGfxMesgBuf[1];
UNUSED OSMesg D_8014F010, D_8014F014;
OSMesg gIntrMesgBuf[16], gSPTaskMesgBuf[16];
OSMesg gMainReceivedMesg;
OSIoMesg gDmaIoMesg;
OSMesgQueue gSIEventMesgQueue;
OSMesg gSIEventMesgBuf[3];

OSContStatus gControllerStatuses[4];
OSContPad gControllerPads[8];
u8 gControllerBits;
// Contains a 32x32 grid of indices into gCollisionIndices containing indices into gCollisionMesh
CollisionGrid gCollisionGrid[1024];
u16 gNumActors;
u16 gMatrixObjectCount;
s32 gTickSpeed;
/* R69: replicated OG Xbox half-rate guards for stock 30Hz visual state. */
s16 gRun30hz = 1;
f32 D_80150118;

u16 wasSoftReset;
u16 D_8015011E;

s32 D_80150120;
s32 gGotoMode;
UNUSED s32 D_80150128;
UNUSED s32 D_8015012C;
f32 gCameraZoom[8]; // look like to be the fov of each character
UNUSED s32 D_80150140;
UNUSED s32 D_80150144;
f32 gScreenAspect;
f32 gCourseFarPersp;
f32 gCourseNearPersp;
UNUSED f32 D_80150154;

struct D_80150158 gD_80150158[16];
uintptr_t gSegmentTable[16];
Gfx* gDisplayListHead;

struct SPTask* gGfxSPTask;
s32 D_801502A0;
s32 D_801502A4;
u16* gPhysicalFramebuffers[3];
uintptr_t gPhysicalZBuffer;
UNUSED u32 D_801502B8;
UNUSED u32 D_801502BC;
Mat4 D_801502C0;

s32 padding[2048];

u16 D_80152300[8];
u16 D_80152308;

UNUSED OSThread paddingThread;
OSThread gIdleThread;
ALIGNED8 u8 gIdleThreadStack[STACKSIZE]; // Based on sm64 and padding between bss symbols.
OSThread gVideoThread;
ALIGNED8 u8 gVideoThreadStack[STACKSIZE];
UNUSED OSThread D_80156820;
UNUSED ALIGNED8 u8 D_8015680_Stack[STACKSIZE];
OSThread gGameLoopThread;
ALIGNED8 u8 gGameLoopThreadStack[STACKSIZE];
OSThread gAudioThread;
ALIGNED8 u8 gAudioThreadStack[STACKSIZE];
UNUSED OSThread D_8015CD30;
UNUSED ALIGNED8 u8 D_8015CD30_Stack[STACKSIZE / 2];

ALIGNED8 u8 gGfxSPTaskYieldBuffer[4352];
ALIGNED8 u32 gGfxSPTaskStack[256];
OSMesg gPIMesgBuf[32];
OSMesgQueue gPIMesgQueue;

s32 gGamestate = 0xFFFF;
// gRaceState is externed as an s32 in other files. D_800DC514 is only used in main.c, likely a developer mistake.
u16 gRaceState = RACE_NONE;

/* R14 crossplay: race_logic.c intentionally sees this historical symbol as
 * s32 while main.c declares the original u16 view.  On big-endian Xbox 360,
 * reading the u16 name returns the high half of the 32-bit race state, which
 * stays zero for states 0..7.  Read the same four bytes race_logic.c uses. */
static s32 x360_crossplay_race_state32(void) {
    s32 v = 0;
    memcpy(&v, (const void *)&gRaceState, sizeof(v));
    return v;
}

static int sXplayRaceReleaseSnapshotSent = 0;
u16 D_800DC514 = 0;
u16 creditsRenderMode = 0; // Renders the whole track. Displays red if used in normal race mode.
u16 gDemoMode = DEMO_MODE_INACTIVE;
u16 gEnableDebugMode = ENABLE_DEBUG_MODE;
s32 gGamestateNext = 7; // = COURSE_DATA_MENU?;
UNUSED s32 D_800DC528 = 1;
s32 gActiveScreenMode = SCREEN_MODE_1P;
s32 gScreenModeSelection = SCREEN_MODE_1P;
UNUSED s32 D_800DC534 = 0;
s32 gPlayerCountSelection1 = 2;

s32 gModeSelection = GRAND_PRIX;
s32 D_800DC540 = 0;
s32 D_800DC544 = 0;
s32 gCCSelection = CC_50;
s32 gGlobalTimer = 0;
UNUSED s32 D_800DC550 = 0;
UNUSED s32 D_800DC554 = 0;
UNUSED s32 D_800DC558 = 0;
// Framebuffer rendering values (max 3)
u16 sRenderedFramebuffer = 0;
u16 sRenderingFramebuffer = 0;
UNUSED u16 D_800DC564 = 0;
s32 D_800DC568 = 0;
s32 D_800DC56C[8] = { 0 };
s16 sNumVBlanks = 0;
UNUSED s16 D_800DC590 = 0;
f32 gVBlankTimer = 0.0f;
f32 gCourseTimer = 0.0f;

void create_thread(OSThread* thread, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri pri) {
    thread->next = NULL;
    thread->queue = NULL;
    osCreateThread(thread, id, entry, arg, sp, pri);
}
void isPrintfInit(void);
void main_func(void) {
#ifdef VERSION_EU
    osTvType = TV_TYPE_PAL;
#endif
    osInitialize();
#ifdef DEBUG
    isPrintfInit(); // init osSyncPrintf
#endif
    create_thread(&gIdleThread, 1, &thread1_idle, NULL, gIdleThreadStack + ARRAY_COUNT(gIdleThreadStack), 100);
    osStartThread(&gIdleThread);
}

/**
 * Initialize hardware, start main thread, then idle.
 */
void thread1_idle(void* arg) {
    osCreateViManager(OS_PRIORITY_VIMGR);
#ifdef VERSION_EU
    osViSetMode(&osViModeTable[OS_VI_PAL_LAN1]);
#else // VERSION_US
    if (osTvType == TV_TYPE_NTSC) {
        osViSetMode(&osViModeTable[OS_VI_NTSC_LAN1]);
    } else {
        osViSetMode(&osViModeTable[OS_VI_MPAL_LAN1]);
    }
#endif
    osViBlack(true);
    osViSetSpecialFeatures(OS_VI_GAMMA_OFF);
    osCreatePiManager(OS_PRIORITY_PIMGR, &gPIMesgQueue, gPIMesgBuf, ARRAY_COUNT(gPIMesgBuf));
    wasSoftReset = (s16) osResetType;
    create_debug_thread();
    start_debug_thread();
    create_thread(&gVideoThread, 3, &thread3_video, arg, gVideoThreadStack + ARRAY_COUNT(gVideoThreadStack), 100);
    osStartThread(&gVideoThread);
    osSetThreadPri(NULL, 0);

    // Idle without consuming an Xbox hardware thread.
    while (true) {
#ifdef XBOX360_PORT
        x360_sleep_ms(1000);
#else
        ;
#endif
    }
}

void setup_mesg_queues(void) {
    osCreateMesgQueue(&gDmaMesgQueue, gDmaMesgBuf, ARRAY_COUNT(gDmaMesgBuf));
    osCreateMesgQueue(&gSPTaskMesgQueue, gSPTaskMesgBuf, ARRAY_COUNT(gSPTaskMesgBuf));
    osCreateMesgQueue(&gIntrMesgQueue, gIntrMesgBuf, ARRAY_COUNT(gIntrMesgBuf));
    osViSetEvent(&gIntrMesgQueue, (OSMesg) MESG_VI_VBLANK, 1);
    osSetEventMesg(OS_EVENT_SP, &gIntrMesgQueue, (OSMesg) MESG_SP_COMPLETE);
    osSetEventMesg(OS_EVENT_DP, &gIntrMesgQueue, (OSMesg) MESG_DP_COMPLETE);
}

void start_sptask(s32 taskType) {
    if (taskType == M_AUDTASK) {
        gActiveSPTask = sCurrentAudioSPTask;
    } else {
        gActiveSPTask = sCurrentDisplaySPTask;
    }
    osSpTaskLoad(&gActiveSPTask->task);
    osSpTaskStartGo(&gActiveSPTask->task);
    gActiveSPTask->state = SPTASK_STATE_RUNNING;
}

/**
 * Initializes the Fast3D OSTask structure.
 * Loads F3DEX or F3DLX based on the number of players
 **/
void create_gfx_task_structure(void) {
    gGfxSPTask->msgqueue = &gGfxVblankQueue;
    gGfxSPTask->msg = (OSMesg) 2;
    gGfxSPTask->task.t.type = M_GFXTASK;
    gGfxSPTask->task.t.flags = OS_TASK_DP_WAIT;
    gGfxSPTask->task.t.ucode_boot = rspF3DBootStart;
    gGfxSPTask->task.t.ucode_boot_size = ((u8*) rspF3DBootEnd - (u8*) rspF3DBootStart);
    // The split-screen multiplayer racing state uses F3DLX which has a simple subpixel calculation.
    // Singleplayer race mode and all other game states use F3DEX.
    // http://n64devkit.square7.ch/n64man/ucode/gspF3DEX.htm
    if (gGamestate != RACING || gPlayerCountSelection1 == 1) {
        gGfxSPTask->task.t.ucode = gspF3DEXTextStart;
        gGfxSPTask->task.t.ucode_data = gspF3DEXDataStart;
    } else {
        gGfxSPTask->task.t.ucode = gspF3DLXTextStart;
        gGfxSPTask->task.t.ucode_data = gspF3DLXDataStart;
    }
    gGfxSPTask->task.t.flags = 0;
    gGfxSPTask->task.t.flags = OS_TASK_DP_WAIT;
    gGfxSPTask->task.t.ucode_size = SP_UCODE_SIZE;
    gGfxSPTask->task.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    gGfxSPTask->task.t.dram_stack = (u64*) &gGfxSPTaskStack;
    gGfxSPTask->task.t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    gGfxSPTask->task.t.output_buff = (u64*) &gGfxSPTaskOutputBuffer;
    gGfxSPTask->task.t.output_buff_size = (u64*) ((u8*) gGfxSPTaskOutputBuffer + sizeof(gGfxSPTaskOutputBuffer));
    gGfxSPTask->task.t.data_ptr = (u64*) gGfxPool->gfxPool;
    gGfxSPTask->task.t.data_size = (gDisplayListHead - gGfxPool->gfxPool) * sizeof(Gfx);
    func_8008C214();
    gGfxSPTask->task.t.yield_data_ptr = (u64*) &gGfxSPTaskYieldBuffer;
    gGfxSPTask->task.t.yield_data_size = OS_YIELD_DATA_SIZE;
}

void init_controllers(void) {
    osCreateMesgQueue(&gSIEventMesgQueue, &gSIEventMesgBuf[0], ARRAY_COUNT(gSIEventMesgBuf));
    osSetEventMesg(OS_EVENT_SI, &gSIEventMesgQueue, (OSMesg) 0x33333333);
    osContInit(&gSIEventMesgQueue, &gControllerBits, gControllerStatuses);
    if ((gControllerBits & 1) == 0) {
        sIsController1Unplugged = true;
    } else {
        sIsController1Unplugged = false;
    }
}

void update_controller(s32 index) {
    struct Controller* controller = &gControllers[index];
    u16 stick;

    if (sIsController1Unplugged) {
        return;
    }

    controller->rawStickX = gControllerPads[index].stick_x;
    controller->rawStickY = gControllerPads[index].stick_y;

    if ((gControllerPads[index].button & D_CBUTTONS) != 0) {
        gControllerPads[index].button |= Z_TRIG;
    }
    controller->buttonPressed = gControllerPads[index].button & (gControllerPads[index].button ^ controller->button);
    controller->buttonDepressed = controller->button & (gControllerPads[index].button ^ controller->button);
    controller->button = gControllerPads[index].button;

    stick = 0;
    if (controller->rawStickX < -50) {
        stick |= L_JPAD;
    }
    if (controller->rawStickX > 50) {
        stick |= R_JPAD;
    }
    if (controller->rawStickY < -50) {
        stick |= D_JPAD;
    }
    if (controller->rawStickY > 50) {
        stick |= U_JPAD;
    }
    controller->stickPressed = stick & (stick ^ controller->stickDirection);
    controller->stickDepressed = controller->stickDirection & (stick ^ controller->stickDirection);
    controller->stickDirection = stick;
}

void read_controllers(void) {
#ifndef XBOX360_PORT
    OSMesg msg;

    osContStartReadData(&gSIEventMesgQueue);
    osRecvMesg(&gSIEventMesgQueue, &msg, OS_MESG_BLOCK);
    osContGetReadData(gControllerPads);
#else
    if(x360_net_active()) gVBlankTimer=r22_crossplay_frame_time(x360_net_frame());
    x360_read_controllers(gControllerPads, x360_net8_active()?8:4);
#endif
    update_controller(0);
    update_controller(1);
    update_controller(2);
    update_controller(3);
#ifdef XBOX360_PORT
    if(x360_net8_active()){int pad;for(pad=4;pad<8;++pad)update_controller(pad);}
#endif
    gControllerFive->button = (s16) (((gControllerOne->button | gControllerTwo->button) | gControllerThree->button) |
                                     gControllerFour->button);
    gControllerFive->buttonPressed =
        (s16) (((gControllerOne->buttonPressed | gControllerTwo->buttonPressed) | gControllerThree->buttonPressed) |
               gControllerFour->buttonPressed);
    gControllerFive->buttonDepressed = (s16) (((gControllerOne->buttonDepressed | gControllerTwo->buttonDepressed) |
                                               gControllerThree->buttonDepressed) |
                                              gControllerFour->buttonDepressed);
    gControllerFive->stickDirection =
        (s16) (((gControllerOne->stickDirection | gControllerTwo->stickDirection) | gControllerThree->stickDirection) |
               gControllerFour->stickDirection);
    gControllerFive->stickPressed =
        (s16) (((gControllerOne->stickPressed | gControllerTwo->stickPressed) | gControllerThree->stickPressed) |
               gControllerFour->stickPressed);
    gControllerFive->stickDepressed =
        (s16) (((gControllerOne->stickDepressed | gControllerTwo->stickDepressed) | gControllerThree->stickDepressed) |
               gControllerFour->stickDepressed);
}

void func_80000BEC(void) {
    gPhysicalZBuffer = VIRTUAL_TO_PHYSICAL(&gZBuffer);
}

void dispatch_audio_sptask(struct SPTask* spTask) {
#ifdef XBOX360_PORT
    x360_dispatch_audio_task(spTask);
#else
    osWritebackDCacheAll();
    osSendMesg(&gSPTaskMesgQueue, spTask, OS_MESG_NOBLOCK);
#endif
}

void exec_display_list(struct SPTask* spTask) {
#ifdef XBOX360_PORT
    x360_exec_sp_task(spTask);
#else
    osWritebackDCacheAll();
    spTask->state = SPTASK_STATE_NOT_STARTED;
    if (sCurrentDisplaySPTask == NULL) {
        sCurrentDisplaySPTask = spTask;
        sNextDisplaySPTask = NULL;
        osSendMesg(&gIntrMesgQueue, (OSMesg) MESG_START_GFX_SPTASK, OS_MESG_NOBLOCK);
    } else {
        sNextDisplaySPTask = spTask;
    }
#endif
}

/**
 * Set default RCP (Reality Co-Processor) settings.
 */
void init_rcp(void) {
    move_segment_table_to_dmem();
    init_rdp();
    set_viewport();
    select_framebuffer();
    init_z_buffer();
}

/**
 * End the master display list and initialize the graphics task structure for the next frame to be rendered.
 */
void end_master_display_list(void) {
    gDPFullSync(gDisplayListHead++);
    gSPEndDisplayList(gDisplayListHead++);
    create_gfx_task_structure();
}

// clear_frame_buffer from SM64, with a few edits
//! @todo Why did void* work for matching
void* clear_framebuffer(s32 color) {
    gDPPipeSync(gDisplayListHead++);

    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);

    gDPSetFillColor(gDisplayListHead++, color);
    gDPFillRectangle(gDisplayListHead++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    gDPPipeSync(gDisplayListHead++);

    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
}

void rendering_init(void) {
    gGfxPool = &gGfxPools[0];
    set_segment_base_addr(1, gGfxPool);
    gGfxSPTask = &gGfxPool->spTask;
    gDisplayListHead = gGfxPool->gfxPool;
    init_rcp();
    clear_framebuffer(0);
    end_master_display_list();
    exec_display_list(&gGfxPool->spTask);
    sRenderingFramebuffer++;
    gGlobalTimer++;
}

void config_gfx_pool(void) {
    gGfxPool = &gGfxPools[gGlobalTimer & 1];
    set_segment_base_addr(1, gGfxPool);
    gDisplayListHead = gGfxPool->gfxPool;
    gGfxSPTask = &gGfxPool->spTask;
}

/**
 * Send current master display list for rendering.
 * Tell the VI which colour framebuffer to display.
 * Yields to the VI framerate twice, locking the game at 30 FPS.
 * Selects the next framebuffer to be rendered and displayed.
 */
void display_and_vsync(void) {
#ifdef XBOX360_PORT
    profiler_log_thread5_time(BEFORE_DISPLAY_LISTS);
    exec_display_list(&gGfxPool->spTask);
    profiler_log_thread5_time(AFTER_DISPLAY_LISTS);
    /* Native gfx_end_frame presents and paces exactly once. */
    profiler_log_thread5_time(THREAD5_END);

    if (++sRenderedFramebuffer == 3) sRenderedFramebuffer = 0;
    if (++sRenderingFramebuffer == 3) sRenderingFramebuffer = 0;
    gGlobalTimer++;
#else
    profiler_log_thread5_time(BEFORE_DISPLAY_LISTS);
    osRecvMesg(&gGfxVblankQueue, &gMainReceivedMesg, OS_MESG_BLOCK);
    exec_display_list(&gGfxPool->spTask);
    profiler_log_thread5_time(AFTER_DISPLAY_LISTS);
    osRecvMesg(&gGameVblankQueue, &gMainReceivedMesg, OS_MESG_BLOCK);
    osViSwapBuffer((void*) PHYSICAL_TO_VIRTUAL(gPhysicalFramebuffers[sRenderedFramebuffer]));
    profiler_log_thread5_time(THREAD5_END);
    osRecvMesg(&gGameVblankQueue, &gMainReceivedMesg, OS_MESG_BLOCK);
    crash_screen_set_framebuffer(gPhysicalFramebuffers[sRenderedFramebuffer]);

    if (++sRenderedFramebuffer == 3) sRenderedFramebuffer = 0;
    if (++sRenderingFramebuffer == 3) sRenderingFramebuffer = 0;
    gGlobalTimer++;
#endif
}

void init_segment_ending_sequences(void) {
#ifndef XBOX360_PORT
    bzero((void*) SEG_ENDING, SEG_ENDING_SIZE);
    osWritebackDCacheAll();
    dma_copy((u8*) SEG_ENDING, (u8*) SEG_ENDING_ROM_START, SEG_ENDING_ROM_SIZE);
    osInvalICache((void*) SEG_ENDING, SEG_ENDING_SIZE);
    osInvalDCache((void*) SEG_ENDING, SEG_ENDING_SIZE);
#endif
}

void init_segment_racing(void) {
#ifndef XBOX360_PORT
    bzero((void*) SEG_RACING, SEG_RACING_SIZE);
    osWritebackDCacheAll();
    dma_copy((u8*) SEG_RACING, (u8*) SEG_RACING_ROM_START, SEG_RACING_ROM_SIZE);
    osInvalICache((void*) SEG_RACING, SEG_RACING_SIZE);
    osInvalDCache((void*) SEG_RACING, SEG_RACING_SIZE);
#endif
}

void dma_copy(u8* dest, u8* romAddr, size_t size) {

    osInvalDCache(dest, size);
    while (size > 0x100) {
        osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) romAddr, dest, 0x100, &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, 1);
        size -= 0x100;
        romAddr += 0x100;
        dest += 0x100;
    }
    if (size != 0) {
        osPiStartDma(&gDmaIoMesg, 0, 0, (uintptr_t) romAddr, dest, size, &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, 1);
    }
}

/**
 * Setup main segments and framebuffers.
 */
void setup_game_memory(void) {
    UNUSED u32 pad[2];
    ptrdiff_t commonCourseDataSize; // Compressed mio0 size
    uintptr_t textureSegSize;
    ptrdiff_t textureSegStart;
    uintptr_t allocatedMemory;
    UNUSED s32 unknown_padding;

    init_segment_racing();
    gHeapEndPtr = SEG_RACING;
    set_segment_base_addr(0, (void*) SEG_START);

    // Memory pool size of 0xAB630
    initialize_memory_pool(MEMORY_POOL_START, MEMORY_POOL_END);

    func_80000BEC();

#ifndef XBOX360_PORT
    // Initialize trig tables segment
    osInvalDCache((void*) TRIG_TABLES, TRIG_TABLES_SIZE);
    osPiStartDma(&gDmaIoMesg, 0, 0, TRIG_TABLES_ROM_START, (void*) TRIG_TABLES, TRIG_TABLES_SIZE, &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, OS_MESG_BLOCK);

#endif

    set_segment_base_addr(2, (void*) load_data(SEG_DATA_START, SEG_DATA_END));

    commonCourseDataSize = COMMON_TEXTURES_SIZE;
    commonCourseDataSize = ALIGN16(commonCourseDataSize);

#ifdef AVOID_UB
    textureSegStart = (ptrdiff_t) SEG_RACING - commonCourseDataSize;
#else
    textureSegStart = SEG_RACING - commonCourseDataSize;
#endif
    osPiStartDma(&gDmaIoMesg, 0, 0, COMMON_TEXTURES_ROM_START, (void*) textureSegStart, commonCourseDataSize,
                 &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, &gMainReceivedMesg, OS_MESG_BLOCK);

    textureSegSize = *(uintptr_t*) (textureSegStart + 4);
    textureSegSize = ALIGN16(textureSegSize);
    allocatedMemory = gNextFreeMemoryAddress;
    mio0decode((u8*) textureSegStart, (u8*) allocatedMemory);
    set_segment_base_addr(0xD, (void*) allocatedMemory);

    gNextFreeMemoryAddress += textureSegSize;

    // Common course data does not get reloaded when the race state resets.
    // Therefore, only reset the memory ptr to after the common course data.
    gFreeMemoryResetAnchor = gNextFreeMemoryAddress;
}

/**
 * @brief
 *
 */
void game_init_clear_framebuffer(void) {
    gGamestateNext = 0; // = START_MENU_FROM_QUIT?
    clear_framebuffer(0);
}

static void r25_capture(u32 phase,u32 tick);
/* MK64_R63_OFFLINE_60FPS_TEST: experimental offline-only 60 Hz race path.
 * Native port source symbols, NOT N64 MIPS / GameShark memory addresses.
 * Keep Time Trials / replays, non-racing phases and all netplay at 30 Hz. */
#ifdef XBOX360_PORT
#ifndef MK64_OFFLINE_60FPS_TEST
#define MK64_OFFLINE_60FPS_TEST 1
#endif
static int mk64_r69_online_60fps_active(void) {
    return x360_net_60fps_session() && gGamestate == RACING &&
        x360_crossplay_race_state32() == RACE_IN_PROGRESS && gModeSelection != TIME_TRIALS &&
        gDemoMode == DEMO_MODE_INACTIVE && gIsGamePaused == 0 &&
        gIsInQuitToMenuTransition == 0;
}
int x360_net_60fps_racing(void){return mk64_r69_online_60fps_active();}
int mk64_offline_60fps_active(void) {
    return MK64_OFFLINE_60FPS_TEST && !x360_net_active() &&
        gGamestate == RACING && x360_crossplay_race_state32() == RACE_IN_PROGRESS &&
        gModeSelection != TIME_TRIALS && gDemoMode == DEMO_MODE_INACTIVE &&
        gIsGamePaused == 0 && gIsInQuitToMenuTransition == 0;
}
static int mk64_r63_offline_steps(s16 vblanks) {
    if (x360_net_active())return mk64_r69_online_60fps_active()?1:2;
    if (!mk64_offline_60fps_active()) return 2;
    return (vblanks < 1) ? 1 : ((vblanks > 4) ? 4 : vblanks);
}
#endif

void race_logic_loop(void) {
    s16 i;
    u16 rotY;

    if(x360_net8_active()) x360_race8_controls();
    gMatrixObjectCount = 0;
    gMatrixEffectCount = 0;
    if (gIsGamePaused != 0 && !x360_net8_active()) {
        func_80290B14();
    }
    if (gIsInQuitToMenuTransition != 0) {
        func_802A38B4();
        return;
    }

    if (sNumVBlanks >= 6) {
        sNumVBlanks = 5;
    }
    if (sNumVBlanks < 0) {
        sNumVBlanks = 1;
    }
    func_802A4EF4();
    r25_capture(1,0xFFFFFFFFU);

    switch (gActiveScreenMode) {
        case SCREEN_MODE_1P:
            gTickSpeed = mk64_r63_offline_steps(sNumVBlanks);
            if(!x360_net8_active()) replays_loop();
            if (gIsGamePaused == 0) {
                for (i = 0; i < gTickSpeed; i++) {
                    if (D_8015011E) {
                        gCourseTimer += COURSE_TIMER_ITER;
                    }
                    func_802909F0();
                    r25_capture(10,(u32)i);
                    evaluate_collision_for_players_and_actors();
                    r25_capture(11,(u32)i);
                    handle_a_press_for_all_players_during_race();
                    r25_capture(12,(u32)i);
                    if(x360_net8_active()) x360_race8_cameras();
                    else func_8001EE98(gPlayerOneCopy, camera1, 0);
                    r25_capture(13,(u32)i);
                    func_80028F70();
                    func_8028F474();
                    r25_capture(17,(u32)i);
                    func_80059AC8();
                    r25_capture(18,(u32)i);
                    update_course_actors();
                    r25_capture(19,(u32)i);
                    course_update_water();
                    r25_capture(20,(u32)i);
                    func_8028FCBC();
                    r25_capture(21,(u32)i);
                }
                func_80022744();
                r25_capture(30,0xFFFFFFFFU);
            }
            func_8005A070();
            r25_capture(31,0xFFFFFFFFU);
#if defined(XBOX360_PORT)
            /* R22: everything after func_8005A070() is presentation.  The item
             * window/object update above remains authoritative gameplay. */
            r22_crossplay_present_begin();
#endif
            sNumVBlanks = 0;
            profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);
            D_8015F788 = 0;
            if(x360_net8_active()) x360_race8_render();
            else render_player_one_1p_screen();
            if (!gEnableDebugMode) {
                D_800DC514 = false;
            } else {
                if (D_800DC514) {

                    if ((gControllerOne->buttonPressed & R_TRIG) && (gControllerOne->button & A_BUTTON) &&
                        (gControllerOne->button & B_BUTTON)) {
                        D_800DC514 = false;
                    }

                    rotY = camera1->rot[1];
                    gDebugPathCount = D_800DC5EC->pathCounter;
                    if (rotY < DEGREES(45)) {
                        func_80057A50(40, 100, "SOUTH  ", gDebugPathCount);
                    } else if (rotY < DEGREES(135)) {
                        func_80057A50(40, 100, "EAST   ", gDebugPathCount);
                    } else if (rotY < DEGREES(225)) {
                        func_80057A50(40, 100, "NORTH  ", gDebugPathCount);
                    } else if (rotY < DEGREES(315)) {
                        func_80057A50(40, 100, "WEST   ", gDebugPathCount);
                    } else {
                        func_80057A50(40, 100, "SOUTH  ", gDebugPathCount);
                    }

                } else {
                    if ((gControllerOne->buttonPressed & L_TRIG) && (gControllerOne->button & A_BUTTON) &&
                        (gControllerOne->button & B_BUTTON)) {
                        D_800DC514 = true;
                    }
                }
            }
            break;

        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
#ifdef XBOX360_PORT
            /*
             * Xbox presents this port at a fixed 30 Hz. Always run exactly two
             * 60 Hz MK64 logic ticks per presented frame. The original N64
             * DK Jungle 2P compensation used three ticks because that mode
             * rendered slower on N64 hardware; keeping it here makes the
             * entire race simulation run at 1.5x speed.
             */
            gTickSpeed = mk64_r63_offline_steps(sNumVBlanks);
#else
            if (gCurrentCourseId == COURSE_DK_JUNGLE) {
                gTickSpeed = 3;
            } else {
                gTickSpeed = 2;
            }
#endif
            if (gIsGamePaused == 0) {
                for (i = 0; i < gTickSpeed; i++) {
                    if (D_8015011E != 0) {
                        gCourseTimer += COURSE_TIMER_ITER;
                    }
                    func_802909F0();
                    r25_capture(10,(u32)i);
                    evaluate_collision_for_players_and_actors();
                    r25_capture(11,(u32)i);
                    handle_a_press_for_all_players_during_race();
                    r25_capture(12,(u32)i);
                    if(x360_net8_active()) x360_race8_cameras();
                    else func_8001EE98(gPlayerOneCopy, camera1, 0);
                    r25_capture(13,(u32)i);
                    func_80029060();
                    r25_capture(14,(u32)i);
                    func_8001EE98(gPlayerTwoCopy, camera2, 1);
                    r25_capture(15,(u32)i);
                    func_80029150();
                    r25_capture(16,(u32)i);
                    func_8028F474();
                    r25_capture(17,(u32)i);
                    func_80059AC8();
                    r25_capture(18,(u32)i);
                    update_course_actors();
                    r25_capture(19,(u32)i);
                    course_update_water();
                    r25_capture(20,(u32)i);
                    func_8028FCBC();
                    r25_capture(21,(u32)i);
                }
                func_80022744();
                r25_capture(30,0xFFFFFFFFU);
            }
            func_8005A070();
            r25_capture(31,0xFFFFFFFFU);
#if defined(XBOX360_PORT)
            /* R22: everything after func_8005A070() is presentation.  The item
             * window/object update above remains authoritative gameplay. */
            r22_crossplay_present_begin();
#endif
            profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);
            sNumVBlanks = 0;
            move_segment_table_to_dmem();
            init_rdp();
            if (D_800DC5B0 != 0) {
                select_framebuffer();
            }
            D_8015F788 = 0;
            if (gPlayerWinningIndex == 0) {
                render_player_two_2p_screen_vertical();
                render_player_one_2p_screen_vertical();
            } else {
                render_player_one_2p_screen_vertical();
                render_player_two_2p_screen_vertical();
            }
            break;

        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:

#ifdef XBOX360_PORT
            /*
             * Match the native 30 Hz Xbox presentation clock: two 60 Hz
             * simulation ticks for every course, including DK Jungle.
             */
            gTickSpeed = mk64_r63_offline_steps(sNumVBlanks);
#else
            if (gCurrentCourseId == COURSE_DK_JUNGLE) {
                gTickSpeed = 3;
            } else {
                gTickSpeed = 2;
            }
#endif

            if (gIsGamePaused == 0) {
                for (i = 0; i < gTickSpeed; i++) {
                    if (D_8015011E != 0) {
                        gCourseTimer += COURSE_TIMER_ITER;
                    }
                    func_802909F0();
                    r25_capture(10,(u32)i);
                    evaluate_collision_for_players_and_actors();
                    r25_capture(11,(u32)i);
                    handle_a_press_for_all_players_during_race();
                    r25_capture(12,(u32)i);
                    if(x360_net8_active()) x360_race8_cameras();
                    else func_8001EE98(gPlayerOneCopy, camera1, 0);
                    r25_capture(13,(u32)i);
                    func_80029060();
                    r25_capture(14,(u32)i);
                    func_8001EE98(gPlayerTwoCopy, camera2, 1);
                    r25_capture(15,(u32)i);
                    func_80029150();
                    r25_capture(16,(u32)i);
                    func_8028F474();
                    r25_capture(17,(u32)i);
                    func_80059AC8();
                    r25_capture(18,(u32)i);
                    update_course_actors();
                    r25_capture(19,(u32)i);
                    course_update_water();
                    r25_capture(20,(u32)i);
                    func_8028FCBC();
                    r25_capture(21,(u32)i);
                }
                func_80022744();
                r25_capture(30,0xFFFFFFFFU);
            }
            profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);
            sNumVBlanks = (u16) 0;
            func_8005A070();
            r25_capture(31,0xFFFFFFFFU);
#if defined(XBOX360_PORT)
            /* R22: everything after func_8005A070() is presentation.  The item
             * window/object update above remains authoritative gameplay. */
            r22_crossplay_present_begin();
#endif
            move_segment_table_to_dmem();
            init_rdp();
            if (D_800DC5B0 != 0) {
                select_framebuffer();
            }
            D_8015F788 = 0;
            if (gPlayerWinningIndex == 0) {
                render_player_two_2p_screen_horizontal();
                render_player_one_2p_screen_horizontal();
            } else {
                render_player_one_2p_screen_horizontal();
                render_player_two_2p_screen_horizontal();
            }

            break;

        case SCREEN_MODE_3P_4P_SPLITSCREEN:
#ifdef XBOX360_PORT
            /*
             * B23C: the native Xbox renderer is explicitly paced at 30 FPS.
             * The original N64 3P/4P branches raise gTickSpeed to 3 (or 4 on
             * DK Jungle) to compensate for the lower render rate those modes
             * could sustain on N64 hardware. Keeping those compensation values
             * while rendering at a fixed 30 FPS makes simulation/race time run
             * at 1.5x (or 2x) speed. Two 1/60-second logic ticks per 30-Hz
             * frame preserves normal game speed while keeping the proven
             * 30-FPS Xbox presentation and netplay pacing unchanged.
             */
            gTickSpeed = mk64_r63_offline_steps(sNumVBlanks);
#else
            if (gPlayerCountSelection1 == 3) {
                switch (gCurrentCourseId) {
                    case COURSE_BOWSER_CASTLE:
                    case COURSE_MOO_MOO_FARM:
                    case COURSE_SKYSCRAPER:
                    case COURSE_DK_JUNGLE:
                        gTickSpeed = 3;
                        break;
                    default:
                        gTickSpeed = 2;
                        break;
                }
            } else {
                // Four players
                switch (gCurrentCourseId) {
                    case COURSE_BLOCK_FORT:
                    case COURSE_DOUBLE_DECK:
                    case COURSE_BIG_DONUT:
                        gTickSpeed = 2;
                        break;
                    case COURSE_DK_JUNGLE:
                        gTickSpeed = 4;
                        break;
                    default:
                        gTickSpeed = 3;
                        break;
                }
            }
#endif
            if (gIsGamePaused == 0) {
                for (i = 0; i < gTickSpeed; i++) {
                    if (D_8015011E != 0) {
                        gCourseTimer += COURSE_TIMER_ITER;
                    }
                    func_802909F0();
                    r25_capture(10,(u32)i);
                    evaluate_collision_for_players_and_actors();
                    r25_capture(11,(u32)i);
                    handle_a_press_for_all_players_during_race();
                    r25_capture(12,(u32)i);
                    if(x360_net8_active()) x360_race8_cameras();
                    else func_8001EE98(gPlayerOneCopy, camera1, 0);
                    r25_capture(13,(u32)i);
                    func_80029158();
                    func_8001EE98(gPlayerTwo, camera2, 1);
                    func_800291E8();
                    func_8001EE98(gPlayerThree, camera3, 2);
                    func_800291F0();
                    func_8001EE98(gPlayerFour, camera4, 3);
                    func_800291F8();
                    func_8028F474();
                    r25_capture(17,(u32)i);
                    func_80059AC8();
                    r25_capture(18,(u32)i);
                    update_course_actors();
                    r25_capture(19,(u32)i);
                    course_update_water();
                    r25_capture(20,(u32)i);
                    func_8028FCBC();
                    r25_capture(21,(u32)i);
                }
                func_80022744();
                r25_capture(30,0xFFFFFFFFU);
            }
            func_8005A070();
            r25_capture(31,0xFFFFFFFFU);
#if defined(XBOX360_PORT)
            /* R22: everything after func_8005A070() is presentation.  The item
             * window/object update above remains authoritative gameplay. */
            r22_crossplay_present_begin();
#endif
            sNumVBlanks = 0;
            profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);
            move_segment_table_to_dmem();
            init_rdp();
            if (D_800DC5B0 != 0) {
                select_framebuffer();
            }
            D_8015F788 = 0;
            if (gPlayerWinningIndex == 0) {
                render_player_two_3p_4p_screen();
                render_player_three_3p_4p_screen();
                render_player_four_3p_4p_screen();
                render_player_one_3p_4p_screen();
            } else if (gPlayerWinningIndex == 1) {
                render_player_one_3p_4p_screen();
                render_player_three_3p_4p_screen();
                render_player_four_3p_4p_screen();
                render_player_two_3p_4p_screen();
            } else if (gPlayerWinningIndex == 2) {
                render_player_one_3p_4p_screen();
                render_player_two_3p_4p_screen();
                render_player_four_3p_4p_screen();
                render_player_three_3p_4p_screen();
            } else {
                render_player_one_3p_4p_screen();
                render_player_two_3p_4p_screen();
                render_player_three_3p_4p_screen();
                render_player_four_3p_4p_screen();
            }
            break;
    }

    if (!gEnableDebugMode) {
        gEnableResourceMeters = 0;
    } else {
        if (gEnableResourceMeters) {
            resource_display();
            if ((!(gControllerOne->button & L_TRIG)) && (gControllerOne->button & R_TRIG) &&
                (gControllerOne->buttonPressed & B_BUTTON)) {
                gEnableResourceMeters = 0;
            }
        } else {
            if ((!(gControllerOne->button & L_TRIG)) && (gControllerOne->button & R_TRIG) &&
                (gControllerOne->buttonPressed & B_BUTTON)) {
                gEnableResourceMeters = 1;
            }
        }
    }
    func_802A4300();
#ifdef XBOX360_PORT
    /*
     * MK64_ALL_ONLINE_HUD_AUDIO_PHASE_V9
     * Never select native 2P/3P/4P HUD layout from total ONLINE player count.
     * x360_race8_render_hud() maps the actual local slot(s) to a native 1P or
     * horizontal-2P presentation.
     */
    if (x360_net_active()) {
        x360_race8_render_hud();
    } else {
        func_800591B4();
    }
#else
    func_800591B4();
#endif
    func_80093E20();
#if DVDL
    display_dvdl();
#endif
#if defined(XBOX360_PORT)
    /* Discard render/particle/culling side effects before the next lockstep frame. */
    r22_crossplay_present_end();
    r25_capture(32,0xFFFFFFFFU);
#endif
    gDPFullSync(gDisplayListHead++);
    gSPEndDisplayList(gDisplayListHead++);
}

/**
 * mk64's game loop depends on a series of states.
 * It runs a wide branching series of code based on these states.
 * State 1) Clear framebuffer
 * State 2) Run menus
 * State 3) Process race related logic
 * State 4) Ending sequence
 * State 5) Credits
 *
 * Note that the state doesn't flip-flop at random but is permanent
 * until the state changes (ie. Exit menus and start a race).
 */

void game_state_handler(void) {
#if DVDL
    if ((gControllerOne->button & L_TRIG) && (gControllerOne->button & R_TRIG) && (gControllerOne->button & Z_TRIG) &&
        (gControllerOne->button & A_BUTTON)) {
        gGamestateNext = CREDITS_SEQUENCE;
    } else if ((gControllerOne->button & L_TRIG) && (gControllerOne->button & R_TRIG) &&
               (gControllerOne->button & Z_TRIG) && (gControllerOne->button & B_BUTTON)) {
        gGamestateNext = ENDING;
    }
#endif

    switch (gGamestate) {
        case 7:
            game_init_clear_framebuffer();
            break;
        case START_MENU_FROM_QUIT:
        case MAIN_MENU_FROM_QUIT:
        case PLAYER_SELECT_MENU_FROM_QUIT:
        case COURSE_SELECT_MENU_FROM_QUIT:
            // Display black
            osViBlack(0);
            update_menus();
            init_rcp();
            func_80094A64(gGfxPool);
#if DVDL
            display_dvdl();
#endif
            break;
        case RACING:
            race_logic_loop();
            break;
        case ENDING:
            podium_ceremony_loop();
            break;
        case CREDITS_SEQUENCE:
            credits_loop();
            break;
    }
}

void interrupt_gfx_sptask(void) {
    if (gActiveSPTask->task.t.type == M_GFXTASK) {
        gActiveSPTask->state = SPTASK_STATE_INTERRUPTED;
        osSpTaskYield();
    }
}

void receive_new_tasks(void) {
    UNUSED s32 pad;
    struct SPTask* spTask;

    while (osRecvMesg(&gSPTaskMesgQueue, (OSMesg*) &spTask, OS_MESG_NOBLOCK) != -1) {
        spTask->state = SPTASK_STATE_NOT_STARTED;
        switch (spTask->task.t.type) {
            case 2:
                sNextAudioSPTask = spTask;
                break;
            case 1:
                sNextDisplaySPTask = spTask;
                break;
        }
    }

    if (sCurrentAudioSPTask == NULL && sNextAudioSPTask != NULL) {
        sCurrentAudioSPTask = sNextAudioSPTask;
        sNextAudioSPTask = NULL;
    }
    if (sCurrentDisplaySPTask == NULL && sNextDisplaySPTask != NULL) {
        sCurrentDisplaySPTask = sNextDisplaySPTask;
        sNextDisplaySPTask = NULL;
    }
}

void set_vblank_handler(s32 index, struct VblankHandler* handler, OSMesgQueue* queue, OSMesg* msg) {
    handler->queue = queue;
    handler->msg = msg;
    switch (index) {
        case 1:
            gVblankHandler1 = handler;
            break;
        case 2:
            gVblankHandler2 = handler;
            break;
    }
}

void start_gfx_sptask(void) {
    if (gActiveSPTask == NULL && sCurrentDisplaySPTask != NULL &&
        sCurrentDisplaySPTask->state == SPTASK_STATE_NOT_STARTED) {
        profiler_log_gfx_time(TASKS_QUEUED);
        start_sptask(M_GFXTASK);
    }
}

void handle_vblank(void) {
    if(!x360_net_active()) gVBlankTimer += V_BlANK_TIMER_ITER;
    if(!x360_net_crossplay()) sNumVBlanks++;

    receive_new_tasks();

    // First try to kick off an audio task. If the gfx task is currently
    // running, we need to asynchronously interrupt it -- handle_sp_complete
    // will pick up on what we're doing and start the audio task for us.
    // If there is already an audio task running, there is nothing to do.
    // If there is no audio task available, try a gfx task instead.
    if (sCurrentAudioSPTask != NULL) {
        if (gActiveSPTask != NULL) {
            interrupt_gfx_sptask();
        } else {
            profiler_log_vblank_time();
            start_sptask(M_AUDTASK);
        }
    } else {
        if (gActiveSPTask == NULL && sCurrentDisplaySPTask != NULL &&
            sCurrentDisplaySPTask->state != SPTASK_STATE_FINISHED) {
            profiler_log_gfx_time(TASKS_QUEUED);
            start_sptask(M_GFXTASK);
        }
    }

/* This is where I would put my rumble code... If I had any. */
#if ENABLE_RUMBLE
    rumble_thread_update_vi();
#endif

    if (gVblankHandler1 != NULL) {
        osSendMesg(gVblankHandler1->queue, gVblankHandler1->msg, OS_MESG_NOBLOCK);
    }
    if (gVblankHandler2 != NULL) {
        osSendMesg(gVblankHandler2->queue, gVblankHandler2->msg, OS_MESG_NOBLOCK);
    }
}

void handle_dp_complete(void) {
    // Gfx SP task is completely done.
    if (sCurrentDisplaySPTask->msgqueue != NULL) {
        osSendMesg(sCurrentDisplaySPTask->msgqueue, sCurrentDisplaySPTask->msg, OS_MESG_NOBLOCK);
    }
    profiler_log_gfx_time(RDP_COMPLETE);
    sCurrentDisplaySPTask->state = SPTASK_STATE_FINISHED_DP;
    sCurrentDisplaySPTask = NULL;
}

void handle_sp_complete(void) {
    struct SPTask* curSPTask = gActiveSPTask;

    gActiveSPTask = NULL;

    if (curSPTask->state == SPTASK_STATE_INTERRUPTED) {
        // handle_vblank tried to start an audio task while there was already a
        // gfx task running, so it had to interrupt the gfx task. That interruption
        // just finished.
        if (osSpTaskYielded((OSTask*) curSPTask) == 0) {
            // The gfx task completed before we had time to interrupt it.
            // Mark it finished, just like below.
            curSPTask->state = SPTASK_STATE_FINISHED;
            profiler_log_gfx_time(RSP_COMPLETE);
        }
        // Start the audio task, as expected by handle_vblank.
        profiler_log_vblank_time();
        start_sptask(M_AUDTASK);
    } else {
        curSPTask->state = SPTASK_STATE_FINISHED;
        if (curSPTask->task.t.type == M_AUDTASK) {
            // After audio tasks come gfx tasks.
            profiler_log_vblank_time();
            if (sCurrentDisplaySPTask != NULL) {
                if (sCurrentDisplaySPTask->state != SPTASK_STATE_FINISHED) {
                    if (sCurrentDisplaySPTask->state != SPTASK_STATE_INTERRUPTED) {
                        profiler_log_gfx_time(TASKS_QUEUED);
                    }
                    start_sptask(M_GFXTASK);
                }
            }
            sCurrentAudioSPTask = NULL;
            if (curSPTask->msgqueue != NULL) {
                osSendMesg(curSPTask->msgqueue, curSPTask->msg, OS_MESG_NOBLOCK);
            }
        } else {
            // The SP process is done, but there is still a Display Processor notification
            // that needs to arrive before we can consider the task completely finished and
            // null out sCurrentDisplaySPTask. That happens in handle_dp_complete.
            profiler_log_gfx_time(RSP_COMPLETE);
        }
    };
}

#ifdef XBOX360_PORT
static OSMesgQueue x360AudioReadyQueue;
static OSMesg x360AudioReadyMessage[1];
#endif

void thread3_video(UNUSED void* arg0) {
    s32 i;
    u64* framebuffer1;
    OSMesg msg;
    UNUSED s32 pad[4];

    gPhysicalFramebuffers[0] = (u16*) &gFramebuffer0;
    gPhysicalFramebuffers[1] = (u16*) &gFramebuffer1;
    gPhysicalFramebuffers[2] = (u16*) &gFramebuffer2;

    // Clear framebuffer.
    framebuffer1 = (u64*) &gFramebuffer1;
    for (i = 0; i < 19200; i++) {
        framebuffer1[i] = 0;
    }
    setup_mesg_queues();
    x360_log("MK64: setting up game memory\n");
    setup_game_memory();
    x360_log("MK64: game memory ready\n");

#ifdef XBOX360_PORT
    osCreateMesgQueue(&x360AudioReadyQueue,x360AudioReadyMessage,1);
#endif
    create_thread(&gAudioThread, 4, &thread4_audio, 0, gAudioThreadStack + ARRAY_COUNT(gAudioThreadStack), 20);
    osStartThread(&gAudioThread);
#ifdef XBOX360_PORT
    /* N64 priority ordering does not serialize initialization on native cores. */
    osRecvMesg(&x360AudioReadyQueue,NULL,OS_MESG_BLOCK);
    x360_log("MK64: audio ready; starting game loop\n");
#endif

    create_thread(&gGameLoopThread, 5, &thread5_game_loop, 0, gGameLoopThreadStack + ARRAY_COUNT(gGameLoopThreadStack),
                  10);
    osStartThread(&gGameLoopThread);

    while (true) {
        osRecvMesg(&gIntrMesgQueue, &msg, OS_MESG_BLOCK);
        switch ((u32) msg) {
            case MESG_VI_VBLANK:
                handle_vblank();
                break;
            case MESG_SP_COMPLETE:
                handle_sp_complete();
                break;
            case MESG_DP_COMPLETE:
                handle_dp_complete();
                break;
            case MESG_START_GFX_SPTASK:
                start_gfx_sptask();
                break;
        }
    }
}

void func_800025D4(void) {
    func_80091B78();
    gActiveScreenMode = SCREEN_MODE_1P;
    set_perspective_and_aspect_ratio();
}

void func_80002600(void) {
    func_80091B78();
    gActiveScreenMode = SCREEN_MODE_1P;
    set_perspective_and_aspect_ratio();
}

void func_8000262C(void) {
    func_80091B78();
    gActiveScreenMode = SCREEN_MODE_1P;
    set_perspective_and_aspect_ratio();
}

void func_80002658(void) {
    func_80091B78();
    gActiveScreenMode = SCREEN_MODE_1P;
    set_perspective_and_aspect_ratio();
}

/**
 * Sets courseId to NULL if
 *
 *
 */
void update_gamestate(void) {
    switch (gGamestate) {
        case START_MENU_FROM_QUIT:
            func_80002658();
            gCurrentlyLoadedCourseId = COURSE_NULL;
            break;
        case MAIN_MENU_FROM_QUIT:
            func_800025D4();
            gCurrentlyLoadedCourseId = COURSE_NULL;
            break;
        case PLAYER_SELECT_MENU_FROM_QUIT:
            func_80002600();
            gCurrentlyLoadedCourseId = COURSE_NULL;
            break;
        case COURSE_SELECT_MENU_FROM_QUIT:
            func_8000262C();
            gCurrentlyLoadedCourseId = COURSE_NULL;
            break;
        case RACING:
            /**
             * @bug Reloading this segment makes random_u16() deterministic for player spawn order.
             * In laymens terms, random_u16() outputs the same value every time.
             */
            init_segment_racing();
            setup_race();
            break;
        case ENDING:
            gCurrentlyLoadedCourseId = COURSE_NULL;
            init_segment_ending_sequences();
            load_ceremony_cutscene();
            break;
        case CREDITS_SEQUENCE:
            gCurrentlyLoadedCourseId = COURSE_NULL;
            init_segment_racing();
            init_segment_ending_sequences();
            load_credits();
            break;
    }
}

void thread5_game_loop(UNUSED void* arg) {
    osCreateMesgQueue(&gGfxVblankQueue, gGfxMesgBuf, 1);
    osCreateMesgQueue(&gGameVblankQueue, &gGameMesgBuf, 1);
    init_controllers();
    if (!wasSoftReset) {
        clear_nmi_buffer();
    }

    set_vblank_handler(2, &gGameVblankHandler, &gGameVblankQueue, (OSMesg) OS_EVENT_SW2);
    // These variables track stats such as player wins.
    // In the event of a console reset, it remembers them.
    nmi_gVersusResults2P = &pAppNmiBuffer[0]; // 2  u8's, tracks number of times player 1/2 won a VS race
    nmi_gVersusResults3P =
        &pAppNmiBuffer[2]; // 9  u8's, 3x3, tracks number of times player 1/2/3   has placed in 1st/2nd/3rd in a VS race
    nmi_gVersusResults4P = &pAppNmiBuffer[11]; // 12 u8's, 4x3, tracks number of times player 1/2/3/4 has placed in 1st/2nd/3rd
                                       // in a VS race
    gNmiUnknown4 = &pAppNmiBuffer[23]; // 2  u8's, tracking number of Battle mode wins by player 1/2
    gNmiUnknown5 = &pAppNmiBuffer[25]; // 3  u8's, tracking number of Battle mode wins by player 1/2/3
    gNmiUnknown6 = &pAppNmiBuffer[28]; // 4  u8's, tracking number of Battle mode wins by player 1/2/3/4
    rendering_init();
    x360_log("MK64: initial controller read begins\n");
    read_controllers();
    x360_log("MK64: game audio state initialization begins\n");
    func_800C5CB8();
    x360_log("MK64: game audio state ready\n");
    if(x360_net8_active()) x360_race8_prepare();

    while (true) {
#ifdef XBOX360_PORT
        /*
         * MK64_R45_CONTROLS_PREMENU
         * Physical L3+R3+LT+RT (0.4s) or a peer GOODBYE returns here instead
         * of rebooting. Reopen the same Host/Join/Options premenu in-process.
         */
        {extern void x360_net_solo_tick(void);x360_net_solo_tick();}
        if(x360_return_chord_pressed() || x360_net_return_requested()){
            /* R57: full title relaunch avoids stale game/network state after
             * returning from an online session. */
            x360_net_full_restart();
            continue;
        }
#endif
        /*
         * MK64_ALL_ONLINE_HUD_AUDIO_PHASE_V9
         *
         * Offline keeps the original ordering. Online defers this game-audio
         * state update until immediately after read_controllers(), because that
         * function is the lockstep frame barrier. This prevents a faster peer
         * from advancing its next audio state before waiting for a slower
         * split-screen peer.
         */
#ifdef XBOX360_PORT
        if (!x360_net_active()) {
#endif
            if(gGlobalTimer<5)x360_log("MK64: loop audio update begins\n");
            x360_progress(1);
            func_800CB2C4();
            if(gGlobalTimer<5)x360_log("MK64: loop audio update complete\n");
#ifdef XBOX360_PORT
        }
#endif

        if(x360_net8_active()) x360_race8_service();

        /* R10: only cross-platform sessions move MK64 state transitions behind
         * the host-committed controller-frame barrier.  360<->360 retains the
         * proven historical scheduling byte-for-byte in behavior. */
#ifdef XBOX360_PORT
        if (!x360_net_crossplay())
#endif
        {
            if (gGamestateNext != gGamestate) {
                gGamestate = gGamestateNext;
                if(gGlobalTimer<5)x360_log("MK64: game state transition begins\n");
                x360_progress(2);
                update_gamestate();
                if(gGlobalTimer<5)x360_log("MK64: game state transition complete\n");
            }
        }
        profiler_log_thread5_time(THREAD5_START);
        config_gfx_pool();
        x360_progress(3);
#ifdef XBOX360_PORT
        if (x360_net_active()) {
            if (x360_net_crossplay() && gGamestate == RACING && gDemoMode == DEMO_MODE_INACTIVE) {
                s32 rs32 = x360_crossplay_race_state32();
                /* R32: race simulation uses the low-latency input-only path only
                 * while it is genuinely in-progress.  Pause/quit/result/cup
                 * lifecycle work must use strict host snapshots on BOTH peers. */
                if (rs32 != 3 || gIsGamePaused != 0 ||
                    gIsInQuitToMenuTransition != 0 || gGamestateNext != gGamestate) {
                    sXplayRaceReleaseSnapshotSent = 0;
                    x360_net_set_menu_sync(1);
                } else if (!sXplayRaceReleaseSnapshotSent) {
                    /* Keep strict state sync for exactly this first state-3
                     * controller frame so the OG receives the authoritative
                     * GO/race-state snapshot before both sides switch to the
                     * normal low-latency race stream on the next frame. */
                    x360_net_set_menu_sync(1);
                    sXplayRaceReleaseSnapshotSent = 1;
                    x360_net_trace("MK64NET16: host final GO snapshot+hash rs=3 frame=%u\n", (unsigned)x360_net_frame());
                } else {
                    x360_net_set_menu_sync(0);
                }
            } else {
                int strictSync;
                sXplayRaceReleaseSnapshotSent = 0;
                strictSync = (gDemoMode != DEMO_MODE_INACTIVE || gGamestate != RACING ||
                              x360_crossplay_race_state32() != 3 ||
                              gIsGamePaused != 0 || gIsInQuitToMenuTransition != 0 ||
                              gGamestateNext != gGamestate) ? 1 : 0;
                x360_net_set_menu_sync(strictSync);
            }
        }
#endif
        /* Match the OG 30Hz visual/gameplay cadence in experimental V11. */
#ifdef XBOX360_PORT
        if (x360_net_60fps_session()) {
            /* Before read_controllers: match the OG Xbox hash/sample phase. */
            gRun30hz=mk64_r69_online_60fps_active()?((x360_net_frame()&1U)==0U):1;
            gGlobalTimer=(s32)x360_net_frame();
            gVBlankTimer=(f32)sR69NetClockTicks*(1.0f/60.0f);
            sNumVBlanks=mk64_r69_online_60fps_active()?1:2;
        } else {
            /* Also preserve stock effect life when R63 offline 60Hz is active. */
            gRun30hz=mk64_offline_60fps_active()?((gGlobalTimer&1)==0):1;
        }
#endif
        read_controllers();

#ifdef XBOX360_PORT
        if (x360_net_60fps_session()) {
            const u32 nextFrame=(u32)x360_net_frame();
            /* R72.1: at first observed active network frame, the lockstep
             * stream may already be beyond frame 1 (early/lobby prefill).
             * An unanchored clock otherwise stays zero for the whole session,
             * while the R72 OG guest begins counting at the final GO state.
             * Bootstrap only V11 at its initial observation; keep the normal
             * 1-tick racing / 2-tick countdown cadence thereafter. */
            if(sR69NetClockFrame==0U && sR69NetClockTicks==0U && nextFrame>1U){
                sR69NetClockFrame=nextFrame-1U;
                sR69NetClockTicks=(nextFrame-1U)*2U;
                x360_net_trace("R72_1_CLOCK_BOOTSTRAP frame=%u ticks=%u\n",
                         (unsigned)nextFrame,(unsigned)sR69NetClockTicks);
            }
            if(nextFrame==sR69NetClockFrame+1U){
                sR69NetClockTicks+=mk64_r69_online_60fps_active()?1U:2U;
                sR69NetClockFrame=nextFrame;
            }
        }
        if (x360_net_crossplay()) {
            gGlobalTimer=(s32)x360_net_frame();
            gVBlankTimer=r22_crossplay_frame_time(x360_net_frame());
            sNumVBlanks=mk64_r69_online_60fps_active()?1:2;
            if (gGamestateNext != gGamestate) {
                gGamestate = gGamestateNext;
                x360_log("MK64NET8: crossplay post-barrier gamestate transition\n");
                x360_progress(2);
                update_gamestate();
            }
        }

        /*
         * read_controllers() has now consumed the same complete network frame
         * on every peer. Advance the game-owned audio state from this common
         * phase point before race simulation/rendering begins.
         */
        if (x360_net_active()) {
            if(gGlobalTimer<5)x360_log("MK64: online post-barrier audio update begins\n");
            x360_progress(1);
            func_800CB2C4();
            if(gGlobalTimer<5)x360_log("MK64: online post-barrier audio update complete\n");
        }
#endif

        if(gGlobalTimer<5)x360_log("MK64: game state handler begins\n");
        x360_progress(4);
        game_state_handler();
        if(gGlobalTimer<5)x360_log("MK64: game state handler complete\n");
        end_master_display_list();
        if(gGlobalTimer<5)x360_log("MK64: loop display list begins\n");
        x360_progress(5);
        display_and_vsync();
        x360_progress(6);
        if(gGlobalTimer<6)x360_log("MK64: loop frame complete\n");
    }
}

/**
 * Sound processing thread. Runs at 50 or 60 FPS according to osTvType.
 */
void thread4_audio(UNUSED void* arg) {
    UNUSED u32 unused[3];
    x360_log("MK64: audio initialization begins\n");
    audio_init();
    x360_log("MK64: audio initialization complete\n");
    osCreateMesgQueue(&sSoundMesgQueue, sSoundMesgBuf, ARRAY_COUNT(sSoundMesgBuf));
    set_vblank_handler(1, &sSoundVblankHandler, &sSoundMesgQueue, (OSMesg) 512);
#ifdef XBOX360_PORT
    osSendMesg(&x360AudioReadyQueue,(OSMesg)1,OS_MESG_BLOCK);
#endif

    while (true) {
        OSMesg msg;
        struct SPTask* spTask;

        osRecvMesg(&sSoundMesgQueue, &msg, OS_MESG_BLOCK);

        profiler_log_thread4_time();

        spTask = create_next_audio_frame_task();
        if (spTask != NULL) {
            dispatch_audio_sptask(spTask);
        }
        profiler_log_thread4_time();
    }
}

#ifdef XBOX360_PORT

#define XPLAY_STATE_BYTES 1440
#define XPLAY_MENU_ITEM_BASE 160
#define XPLAY_MENU_ITEM_STRIDE 40
static void xplay_state_put32(u8 *p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
static u32 xplay_state_get32(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static u32 xplay_state_f32_bits(f32 v){u32 b=0;memcpy(&b,&v,sizeof(b));return b;}
static f32 xplay_state_bits_f32(u32 b){f32 v=0.0f;memcpy(&v,&b,sizeof(v));return v;}

int x360_crossplay_state_pack(unsigned char *out,int cap){
    extern u16 gRandomSeed16;
    int i,j,k=0;
    if(!out||cap<XPLAY_STATE_BYTES)return 0;
    memset(out,0,XPLAY_STATE_BYTES);
#define XP32(off,v) xplay_state_put32(out+(off),(u32)(v))
    XP32(0,gGlobalTimer);
    XP32(4,gGamestate);
    XP32(8,gGamestateNext);
    XP32(12,gMenuSelection);
    XP32(16,gFadeModeSelection);
    XP32(20,gMenuFadeType);
    XP32(24,gMenuTimingCounter);
    XP32(28,gMenuDelayTimer);
    XP32(32,gPlayerCountSelection1);
    XP32(36,gScreenModeSelection);
    XP32(40,gModeSelection);
    XP32(44,gCCSelection);
    XP32(48,gCurrentCourseId);
    XP32(52,gCupSelection);
    XP32(56,gCourseIndexInCup);
    XP32(60,gMainMenuSelection);
    XP32(64,gPlayerSelectMenuSelection);
    XP32(68,gSubMenuSelection);
    XP32(72,gPlayerCount);
    XP32(76,gScreenModeListIndex);
    XP32(80,gDemoMode);
    XP32(84,gDemoUseController);
    XP32(88,unref_8018EE0C);
    XP32(92,gDebugMenuSelection);
    XP32(96,x360_crossplay_race_state32());
    XP32(100,gRandomSeed16);
    XP32(104,xplay_state_f32_bits(gVBlankTimer));
    XP32(108,xplay_state_f32_bits(gCourseTimer));
#undef XP32
    for(i=0;i<4;++i)out[112+i]=(u8)gCharacterSelections[i];
    for(i=0;i<4;++i)out[116+i]=(u8)gCharacterGridSelections[i];
    for(i=0;i<4;++i)out[120+i]=(u8)gCharacterGridIsSelected[i];
    for(i=0;i<5;++i)out[124+i]=(u8)gTransitionType[i];
    for(i=0;i<4;++i)out[129+i]=(u8)gGameModeMenuColumn[i];
    for(i=0;i<4;++i)for(j=0;j<3;++j)out[133+k++]=(u8)gGameModeSubMenuColumn[i][j];
    out[145]=gControllerBits;
    /* R32 lifecycle extension. Bytes 146..159 were unused/reserved in the
     * existing 1440-byte wire image, so protocol size and MenuItem layout stay
     * unchanged. Keep the result/cup/quit transition itself host-authoritative,
     * not only the menu drawn around it. */
    {
        extern s32 gDemoTimer;
        u16 demo=(u16)(s16)gDemoTimer;
        out[146]=(u8)(demo>>8);
        out[147]=(u8)demo;
    }
    out[148]=(u8)gGotoMode;
    out[149]=(u8)gIsGamePaused;
    out[150]=(u8)gIsInQuitToMenuTransition;
    out[151]=(u8)D_80150120;
    xplay_state_put32(out+152,(u32)D_800DC544);
    /* 156..159 remain reserved for a future lifecycle field without moving
     * XPLAY_MENU_ITEM_BASE. */
    /* The stock menu engine stores most transition timers/selection animation
     * state in gMenuItems[].  Both ports use the same logical MenuItem fields.
     * Serialize them explicitly so the 360 host is authoritative without
     * transmitting native structs, pointers or CPU-endian memory. */
    for(i=0;i<MENU_ITEMS_MAX;++i){
        const MenuItem *m=&gMenuItems[i];
        int o=XPLAY_MENU_ITEM_BASE+i*XPLAY_MENU_ITEM_STRIDE;
        xplay_state_put32(out+o+0,(u32)m->type);
        xplay_state_put32(out+o+4,(u32)m->state);
        xplay_state_put32(out+o+8,(u32)m->subState);
        xplay_state_put32(out+o+12,(u32)m->column);
        xplay_state_put32(out+o+16,(u32)m->row);
        out[o+20]=(u8)m->priority;
        out[o+21]=(u8)m->visible;
        out[o+22]=(u8)(((u16)m->unused)>>8);
        out[o+23]=(u8)((u16)m->unused);
        xplay_state_put32(out+o+24,(u32)m->D_8018DEE0_index);
        xplay_state_put32(out+o+28,(u32)m->param1);
        xplay_state_put32(out+o+32,(u32)m->param2);
        xplay_state_put32(out+o+36,xplay_state_f32_bits(m->paramf));
    }
    return XPLAY_STATE_BYTES;
}

void x360_crossplay_state_apply(const unsigned char *in,int len){
    extern u16 gRandomSeed16;
    int i,j,k=0;
    if(!in||len!=XPLAY_STATE_BYTES)return;
#define XG32(off) ((s32)xplay_state_get32(in+(off)))
    gGlobalTimer=XG32(0);
    gGamestate=XG32(4);
    gGamestateNext=XG32(8);
    gMenuSelection=XG32(12);
    gFadeModeSelection=XG32(16);
    gMenuFadeType=XG32(20);
    gMenuTimingCounter=XG32(24);
    gMenuDelayTimer=XG32(28);
    gPlayerCountSelection1=XG32(32);
    gScreenModeSelection=XG32(36);
    gModeSelection=XG32(40);
    gCCSelection=XG32(44);
    gCurrentCourseId=(s16)XG32(48);
    gCupSelection=(s8)XG32(52);
    gCourseIndexInCup=(s8)XG32(56);
    gMainMenuSelection=(s8)XG32(60);
    gPlayerSelectMenuSelection=(s8)XG32(64);
    gSubMenuSelection=(s8)XG32(68);
    gPlayerCount=(s8)XG32(72);
    gScreenModeListIndex=(s8)XG32(76);
    gDemoMode=(u16)XG32(80);
    gDemoUseController=(s8)XG32(84);
    unref_8018EE0C=(s8)XG32(88);
    gDebugMenuSelection=(s8)XG32(92);
    gRaceState=(u16)XG32(96);
    gRandomSeed16=(u16)XG32(100);
#undef XG32
    gVBlankTimer=xplay_state_bits_f32(xplay_state_get32(in+104));
    gCourseTimer=xplay_state_bits_f32(xplay_state_get32(in+108));
    for(i=0;i<4;++i)gCharacterSelections[i]=(s8)in[112+i];
    for(i=0;i<4;++i)gCharacterGridSelections[i]=(s8)in[116+i];
    for(i=0;i<4;++i)gCharacterGridIsSelected[i]=(s8)in[120+i];
    for(i=0;i<5;++i)gTransitionType[i]=(s8)in[124+i];
    for(i=0;i<4;++i)gGameModeMenuColumn[i]=(s8)in[129+i];
    for(i=0;i<4;++i)for(j=0;j<3;++j)gGameModeSubMenuColumn[i][j]=(s8)in[133+k++];
    gControllerBits=in[145];
    {
        extern s32 gDemoTimer;
        gDemoTimer=(s32)(s16)(((u16)in[146]<<8)|in[147]);
    }
    gGotoMode=(s32)(u8)in[148];
    gIsGamePaused=(s32)(u8)in[149];
    gIsInQuitToMenuTransition=(s32)(u8)in[150];
    D_80150120=(s32)(u8)in[151];
    D_800DC544=(s32)xplay_state_get32(in+152);
    for(i=0;i<MENU_ITEMS_MAX;++i){
        MenuItem *m=&gMenuItems[i];
        int o=XPLAY_MENU_ITEM_BASE+i*XPLAY_MENU_ITEM_STRIDE;
        m->type=(s32)xplay_state_get32(in+o+0);
        m->state=(s32)xplay_state_get32(in+o+4);
        m->subState=(s32)xplay_state_get32(in+o+8);
        m->column=(s32)xplay_state_get32(in+o+12);
        m->row=(s32)xplay_state_get32(in+o+16);
        m->priority=(s8)in[o+20];
        m->visible=(bool8)in[o+21];
        m->unused=(s16)(((u16)in[o+22]<<8)|in[o+23]);
        m->D_8018DEE0_index=(s32)xplay_state_get32(in+o+24);
        m->param1=(s32)xplay_state_get32(in+o+28);
        m->param2=(s32)xplay_state_get32(in+o+32);
        m->paramf=xplay_state_bits_f32(xplay_state_get32(in+o+36));
    }
}

static u32 cross_hash_u8(u32 h,u8 v){return (h^v)*16777619U;}
static u32 cross_hash_u16(u32 h,u16 v){h=cross_hash_u8(h,(u8)(v>>8));return cross_hash_u8(h,(u8)v);}
static u32 cross_hash_u32(u32 h,u32 v){h=cross_hash_u8(h,(u8)(v>>24));h=cross_hash_u8(h,(u8)(v>>16));h=cross_hash_u8(h,(u8)(v>>8));return cross_hash_u8(h,(u8)v);}
static u32 cross_hash_f32(u32 h,f32 v){u32 bits=0;memcpy(&bits,&v,sizeof(bits));return cross_hash_u32(h,bits);}
static u32 cross_phase_hash(u32 h){
    int i;
    h=cross_hash_u32(h,(u32)gGamestate);
    h=cross_hash_u32(h,(u32)gGamestateNext);
    if(gGamestate==RACING){
        h=cross_hash_u32(h,(u32)gModeSelection);
        h=cross_hash_u16(h,(u16)x360_crossplay_race_state32());
        h=cross_hash_u32(h,(u32)gPlayerCountSelection1);
        h=cross_hash_u32(h,(u32)gScreenModeSelection);
    }else{
        h=cross_hash_u32(h,(u32)gMenuSelection);
        h=cross_hash_u8(h,(u8)gMainMenuSelection);
        h=cross_hash_u8(h,(u8)gPlayerSelectMenuSelection);
        h=cross_hash_u8(h,(u8)gSubMenuSelection);
        h=cross_hash_u8(h,(u8)gPlayerCount);
        h=cross_hash_u32(h,(u32)gPlayerCountSelection1);
        h=cross_hash_u32(h,(u32)gScreenModeSelection);
        h=cross_hash_u32(h,(u32)gModeSelection);
    }
    h=cross_hash_u8(h,(u8)gCupSelection);
    h=cross_hash_u8(h,(u8)gCourseIndexInCup);
    /* R32: hash the state that selects/times the next lifecycle boundary. */
    {
        extern s32 gDemoTimer;
        h=cross_hash_u16(h,(u16)(s16)gDemoTimer);
    }
    h=cross_hash_u8(h,(u8)gGotoMode);
    h=cross_hash_u8(h,(u8)gIsGamePaused);
    h=cross_hash_u8(h,(u8)gIsInQuitToMenuTransition);
    h=cross_hash_u8(h,(u8)D_80150120);
    h=cross_hash_u32(h,(u32)D_800DC544);
    for(i=0;i<4;++i)h=cross_hash_u8(h,(u8)gCharacterSelections[i]);
    return h;
}


/* R18 cross-platform race hash.
 * PPC 360 and x86 OG can differ by a few IEEE-754 LSBs even when the actual
 * kart state is equivalent. 360<->360 keeps the original exact hash.
 * Cross-platform racing uses a canonical quantized hash; raw RNG/timers/floats
 * stay available in the R17 diagnostics. */
static s32 cross_r18_qpos(f32 v){return v>=0.0f?(s32)(v*8.0f+0.5f):(s32)(v*8.0f-0.5f);}
static s32 cross_r18_qvel(f32 v){return v>=0.0f?(s32)(v*64.0f+0.5f):(s32)(v*64.0f-0.5f);}
static u32 cross_r18_race_hash(u32 h,int players){
    int i,j;
    h=cross_hash_u32(h,(u32)gGlobalTimer);
    h=cross_phase_hash(h);
    for(i=0;i<players;++i){
        h=cross_hash_u16(h,gPlayers[i].type);
        h=cross_hash_u16(h,(u16)gPlayers[i].lapCount);
        h=cross_hash_u32(h,gPlayers[i].effects);
        for(j=0;j<3;++j)h=cross_hash_u32(h,(u32)cross_r18_qpos(gPlayers[i].pos[j]));
        for(j=0;j<3;++j)h=cross_hash_u32(h,(u32)cross_r18_qvel(gPlayers[i].velocity[j]));
    }
    return h;
}

/* MK64_ASTRA_TRACE_R17: RAM-only rolling deterministic state trace. */
#ifdef XBOX360_PORT
#define ASTRA_FRAME_HISTORY 512U
extern unsigned int mk64_astra_rng_call_count(void);
typedef struct {u32 h0,h1,h2,pos[3],oldPos[3],vel[3],speed,currentSpeed,size,previousSpeed,effects,triggers;u16 type,rank,lap,surface,path,character,kartProps,alpha;} AstraPlayerFrameR17;
typedef struct {u32 valid,frame,globalTimer,gamestate,raceState,mode,screenMode,course,players,seed,rngCalls,courseTimerBits,vblankTimerBits,tickSpeed;AstraPlayerFrameR17 p[4];} AstraFrameR17;
static AstraFrameR17 sAstraFrameR17[ASTRA_FRAME_HISTORY];
static u32 astra_r17_u32(const void *p){u32 v;memcpy(&v,p,4);return v;} static u16 astra_r17_u16(const void *p){u16 v;memcpy(&v,p,2);return v;}
static u32 astra_r17_hash(const unsigned char *p,unsigned n){u32 h=2166136261U;while(n--){h^=*p++;h*=16777619U;}return h;}
static void astra_r17_player(AstraPlayerFrameR17 *o,const void *vp){const unsigned char *p=(const unsigned char*)vp;memset(o,0,sizeof(*o));o->h0=astra_r17_hash(p,0x100);o->h1=astra_r17_hash(p+0x100,0x100);o->h2=astra_r17_hash(p+0x200,0x58);o->type=astra_r17_u16(p);o->rank=astra_r17_u16(p+4);o->lap=astra_r17_u16(p+8);o->triggers=astra_r17_u32(p+0xC);o->pos[0]=astra_r17_u32(p+0x14);o->pos[1]=astra_r17_u32(p+0x18);o->pos[2]=astra_r17_u32(p+0x1C);o->oldPos[0]=astra_r17_u32(p+0x20);o->oldPos[1]=astra_r17_u32(p+0x24);o->oldPos[2]=astra_r17_u32(p+0x28);o->vel[0]=astra_r17_u32(p+0x34);o->vel[1]=astra_r17_u32(p+0x38);o->vel[2]=astra_r17_u32(p+0x3C);o->kartProps=astra_r17_u16(p+0x44);o->speed=astra_r17_u32(p+0x94);o->currentSpeed=astra_r17_u32(p+0x9C);o->effects=astra_r17_u32(p+0xBC);o->alpha=astra_r17_u16(p+0xC6);o->surface=astra_r17_u16(p+0xF8);o->path=astra_r17_u16(p+0x220);o->size=astra_r17_u32(p+0x224);o->previousSpeed=astra_r17_u32(p+0x22C);o->character=astra_r17_u16(p+0x254);}
static void mk64_astra_diag_capture(void){extern u16 gRandomSeed16;extern int x360_net_diagnostics_enabled(void);u32 f;AstraFrameR17 *r;int i;if(!x360_net_active()||!x360_net_diagnostics_enabled())return;f=x360_net_frame();r=&sAstraFrameR17[f&(ASTRA_FRAME_HISTORY-1U)];memset(r,0,sizeof(*r));r->valid=1;r->frame=f;r->globalTimer=(u32)gGlobalTimer;r->gamestate=(u32)gGamestate;r->raceState=(u32)x360_crossplay_race_state32();r->mode=(u32)gModeSelection;r->screenMode=(u32)gScreenModeSelection;r->course=(u32)gCurrentCourseId;r->players=(u32)x360_net_player_count();r->seed=(u32)gRandomSeed16;r->rngCalls=mk64_astra_rng_call_count();r->courseTimerBits=astra_r17_u32(&gCourseTimer);r->vblankTimerBits=astra_r17_u32(&gVBlankTimer);r->tickSpeed=(u32)gTickSpeed;for(i=0;i<4;i++)astra_r17_player(&r->p[i],&gPlayers[i]);}
void mk64_astra_diag_dump(void){u32 cur,first,f;int i;if(!x360_net_active())return;cur=x360_net_frame();first=cur>32U?cur-32U:0;x360_net_trace("ASTRA_FRAME_BEGIN SIDE=360 FIRST=%lu LAST=%lu\n",(unsigned long)first,(unsigned long)cur);for(f=first;f<=cur;f++){AstraFrameR17 *r=&sAstraFrameR17[f&(ASTRA_FRAME_HISTORY-1U)];if(!r->valid||r->frame!=f)continue;x360_net_trace("ASTRA_FRAME SIDE=360 F=%lu GT=%lu GS=%lu RS=%lu MODE=%lu SM=%lu COURSE=%lu PC=%lu SEED=%04lX RNG=%lu CT=%08lX VT=%08lX TICK=%lu\n",(unsigned long)r->frame,(unsigned long)r->globalTimer,(unsigned long)r->gamestate,(unsigned long)r->raceState,(unsigned long)r->mode,(unsigned long)r->screenMode,(unsigned long)r->course,(unsigned long)r->players,(unsigned long)r->seed,(unsigned long)r->rngCalls,(unsigned long)r->courseTimerBits,(unsigned long)r->vblankTimerBits,(unsigned long)r->tickSpeed);for(i=0;i<2;i++){AstraPlayerFrameR17 *p=&r->p[i];x360_net_trace("ASTRA_PLAYER SIDE=360 F=%lu P=%d H0=%08lX H1=%08lX H2=%08lX TYPE=%04X RANK=%04X LAP=%04X EFF=%08lX TRIG=%08lX POS=%08lX,%08lX,%08lX OLD=%08lX,%08lX,%08lX VEL=%08lX,%08lX,%08lX SPD=%08lX CUR=%08lX PREV=%08lX SIZE=%08lX SURF=%04X PATH=%04X CHAR=%04X KPROP=%04X ALPHA=%04X\n",(unsigned long)r->frame,i+1,(unsigned long)p->h0,(unsigned long)p->h1,(unsigned long)p->h2,p->type,p->rank,p->lap,(unsigned long)p->effects,(unsigned long)p->triggers,(unsigned long)p->pos[0],(unsigned long)p->pos[1],(unsigned long)p->pos[2],(unsigned long)p->oldPos[0],(unsigned long)p->oldPos[1],(unsigned long)p->oldPos[2],(unsigned long)p->vel[0],(unsigned long)p->vel[1],(unsigned long)p->vel[2],(unsigned long)p->speed,(unsigned long)p->currentSpeed,(unsigned long)p->previousSpeed,(unsigned long)p->size,p->surface,p->path,p->character,p->kartProps,p->alpha);}}x360_net_trace("ASTRA_FRAME_END SIDE=360\n");}
#endif

/* MK64_CROSSPLAY_R25_ASTRA_GOLD_TRACE
 * ASTRA handoff trace. Diagnostic only: no RNG calls, no file I/O during
 * gameplay, and no writes to simulation state. Title/demo races are excluded.
 */
#define R25_RING 2048U
#define R25_WORDS 88
typedef struct { u32 w[R25_WORDS]; } R25Rec;
static R25Rec sR25Ring[R25_RING];
static u32 sR25Total;
static u32 r25_bits(f32 v){u32 b=0;memcpy(&b,&v,sizeof(b));return b;}
static u32 r25_h32(u32 h,u32 v){h=(h^(u8)(v>>24))*16777619U;h=(h^(u8)(v>>16))*16777619U;h=(h^(u8)(v>>8))*16777619U;return(h^(u8)v)*16777619U;}
static u32 r25_kin_hash(Player *p,int index){
    u32 h=2166136261U;int j;
    for(j=0;j<3;++j)h=r25_h32(h,r25_bits(p->pos[j]));
    for(j=0;j<3;++j)h=r25_h32(h,r25_bits(p->velocity[j]));
    h=r25_h32(h,r25_bits(p->speed));h=r25_h32(h,r25_bits(p->currentSpeed));h=r25_h32(h,r25_bits(p->previousSpeed));
    for(j=0;j<3;++j)h=r25_h32(h,r25_bits(p->oldPos[j]));
    h=r25_h32(h,(u32)(u16)p->rotation[1]);h=r25_h32(h,(u32)(u16)p->slopeAccel);
    h=r25_h32(h,r25_bits(p->unk_098));h=r25_h32(h,r25_bits(p->unk_08C));h=r25_h32(h,r25_bits(p->boundingBoxSize));
    h=r25_h32(h,r25_bits(p->collision.surfaceDistance[2]));
    for(j=0;j<3;++j)h=r25_h32(h,r25_bits(p->collision.orientationVector[j]));
    return h;
}
static u32 r25_logic_hash(Player *p,int index){
    u32 h=2166136261U;(void)index;
    h=r25_h32(h,(u32)(u16)p->type);h=r25_h32(h,(u32)(u16)p->lapCount);h=r25_h32(h,p->effects);
    h=r25_h32(h,(u32)p->triggers);h=r25_h32(h,(u32)(u16)p->kartProps);h=r25_h32(h,(u32)(u16)p->currentRank);
    h=r25_h32(h,(u32)(u16)p->nearestPathPointId);h=r25_h32(h,(u32)(u16)p->currentItemCopy);return h;
}
static void r25_detail(u32 *w,Player *p,struct Controller *c,int index){
    w[0]=(u32)(u16)p->type;
    w[1]=((u32)(u16)p->lapCount<<16)|(u16)p->currentRank;
    w[2]=((u32)(u16)p->nearestPathPointId<<16)|(u16)p->currentItemCopy;
    w[3]=p->effects;w[4]=(u32)p->triggers;w[5]=(u32)(u16)p->kartProps;
    w[6]=r25_bits(p->pos[0]);w[7]=r25_bits(p->pos[1]);w[8]=r25_bits(p->pos[2]);
    w[9]=r25_bits(p->velocity[0]);w[10]=r25_bits(p->velocity[1]);w[11]=r25_bits(p->velocity[2]);
    w[12]=r25_bits(p->speed);w[13]=r25_bits(p->currentSpeed);w[14]=r25_bits(p->previousSpeed);
    w[15]=r25_bits(p->oldPos[0]);w[16]=r25_bits(p->oldPos[1]);w[17]=r25_bits(p->oldPos[2]);
    w[18]=((u32)(u16)p->rotation[1]<<16)|(u16)p->slopeAccel;
    w[19]=r25_bits(p->unk_098);w[20]=r25_bits(p->unk_08C);w[21]=r25_bits(p->boundingBoxSize);
    w[22]=r25_bits(p->collision.surfaceDistance[2]);w[23]=r25_bits(p->collision.orientationVector[0]);
    w[24]=r25_bits(p->collision.orientationVector[1]);w[25]=r25_bits(p->collision.orientationVector[2]);
    w[26]=r25_bits(p->unk_090);w[27]=0; /* reserved */
    w[28]=((u32)(u16)c->button<<16)|((u32)(u8)c->rawStickX<<8)|(u8)c->rawStickY;
    w[29]=((u32)(u16)c->buttonPressed<<16)|(u16)c->buttonDepressed;
}
/* R27 CPU trace: retain the first 1024 checkpoints plus the latest 2048.
 * This preserves the first post-GO split even if the watchdog fires later.
 * 672 KiB, no allocation, I/O, RNG calls or simulation writes during racing. */
#define R27_ANCHOR 1024U
#define R27_RECENT 2048U
#define R27_WORDS 56U
static u32 sR27Cpu[R27_ANCHOR + R27_RECENT][R27_WORDS];
static u32 sR27CpuTotal, sR27Tick, sR27LastFrame;
static int sR27RaceActive;
static void r27_trace_frame(u32 tick) {
    extern int x360_net_diagnostics_enabled(void);
    int active = x360_net_diagnostics_enabled() && x360_net_crossplay() && gDemoMode == DEMO_MODE_INACTIVE &&
                 gGamestate == RACING && gModeSelection == GRAND_PRIX && x360_crossplay_race_state32() == 3;
    u32 frame = (u32)x360_net_frame();
    sR27Tick = tick;
    if (active && (!sR27RaceActive || frame < sR27LastFrame)) sR27CpuTotal = 0;
    sR27RaceActive = active;
    sR27LastFrame = frame;
}
void mk64_r27_cpu_checkpoint(unsigned int stage, int playerId) {
    extern u16 gRandomSeed16;
    extern int x360_net_diagnostics_enabled(void);
    u32 slot, *w; Player *p;
    extern void mk64_r27_cpu_ai(unsigned int *w, int playerId);
    if (!x360_net_diagnostics_enabled() || !sR27RaceActive || playerId < 0 || playerId >= 8) return;
    slot = sR27CpuTotal < R27_ANCHOR ? sR27CpuTotal :
           R27_ANCHOR + ((sR27CpuTotal - R27_ANCHOR) & (R27_RECENT - 1U));
    w = sR27Cpu[slot]; p = &gPlayers[playerId];
    w[0] = (u32)x360_net_frame(); w[1] = sR27Tick; w[2] = stage; w[3] = (u32)playerId;
    w[4] = gRandomSeed16; w[5] = r25_bits(gCourseTimer); w[6] = r25_bits(gVBlankTimer);
    w[7] = r25_kin_hash(p, playerId); w[8] = r25_logic_hash(p, playerId);
    r25_detail(w + 9, p, &gControllers[0], playerId); /* CPU has no controller input. */
    w[37] = 0; w[38] = 0;
    mk64_r27_cpu_ai(w, playerId);
    ++sR27CpuTotal;
}
unsigned int mk64_crossplay_r27_count(void) {
    return sR27CpuTotal < R27_ANCHOR + R27_RECENT ? sR27CpuTotal : R27_ANCHOR + R27_RECENT;
}
int mk64_crossplay_r27_get(unsigned int index, unsigned int *out, int outCount) {
    u32 slot, start;
    if (!out || outCount < R27_WORDS || index >= mk64_crossplay_r27_count()) return 0;
    if (index < R27_ANCHOR) slot = index;
    else {
        start = sR27CpuTotal > R27_ANCHOR + R27_RECENT ? sR27CpuTotal - R27_RECENT : R27_ANCHOR;
        slot = R27_ANCHOR + ((start + index - 2U * R27_ANCHOR) & (R27_RECENT - 1U));
    }
    memcpy(out, sR27Cpu[slot], sizeof(sR27Cpu[slot])); return R27_WORDS;
}

static void r25_capture(u32 phase,u32 tick){
    extern u16 gRandomSeed16;extern int x360_net_diagnostics_enabled(void);R25Rec *r;u32 *w;int i;
    if(!x360_net_diagnostics_enabled())return;
    r27_trace_frame(tick);
    if(!x360_net_crossplay() || gDemoMode!=DEMO_MODE_INACTIVE || gGamestate!=RACING || gModeSelection!=GRAND_PRIX)return;
    r=&sR25Ring[sR25Total&(R25_RING-1U)];w=r->w;
    w[0]=(u32)x360_net_frame();w[1]=phase;w[2]=tick;w[3]=(u32)gGlobalTimer;w[4]=(u32)x360_crossplay_race_state32();w[5]=(u32)gRandomSeed16;
    w[6]=r25_bits(gCourseTimer);w[7]=r25_bits(gVBlankTimer);w[8]=(u32)gDemoMode;w[9]=(u32)(u16)gTickSpeed;
    w[10]=(u32)gPlayerCountSelection1;w[11]=((u32)(u16)gActiveScreenMode<<16)|(u16)gModeSelection;
    for(i=0;i<8;++i)w[12+i]=r25_kin_hash(&gPlayers[i],i);
    for(i=0;i<8;++i)w[20+i]=r25_logic_hash(&gPlayers[i],i);
    r25_detail(&w[28],&gPlayers[0],&gControllers[0],0);r25_detail(&w[58],&gPlayers[1],&gControllers[1],1);
    ++sR25Total;
}
unsigned int x360_crossplay_r25_count(void){return sR25Total<R25_RING?sR25Total:R25_RING;}
int x360_crossplay_r25_get(unsigned int index,unsigned int *out,int outCount){
    u32 n,start,pos;if(!out||outCount<R25_WORDS)return 0;n=x360_crossplay_r25_count();if(index>=n)return 0;
    start=sR25Total-n;pos=(start+index)&(R25_RING-1U);memcpy(out,sR25Ring[pos].w,sizeof(sR25Ring[pos].w));return R25_WORDS;
}
unsigned int x360_net_state_hash(void){
    extern u16 gRandomSeed16;
    u32 h=2166136261U;int i,j,players=x360_net_player_count();
    mk64_astra_diag_capture();
    if(players<2)players=2;if(players>8)players=8;
    if(x360_net_crossplay() && gDemoMode==DEMO_MODE_INACTIVE && gGamestate==RACING && gModeSelection==GRAND_PRIX)players=8; /* R25: real GP only */
    if(x360_net_crossplay() && (gDemoMode!=DEMO_MODE_INACTIVE || x360_net_menu_sync_active())){
        unsigned char state[XPLAY_STATE_BYTES];
        int n=x360_crossplay_state_pack(state,sizeof(state));
        for(i=0;i<n;++i)h=cross_hash_u8(h,state[i]);
        return h;
    }
    if(x360_net_crossplay() && gGamestate==RACING){
        /* R27: retain existing checks and add exact gameplay bits for real GP.
         * A one-ULP movement difference must reach the watchdog immediately. */
        h = cross_r18_race_hash(h,players);
        if (gModeSelection == GRAND_PRIX && x360_crossplay_race_state32() == 3 && gDemoMode == DEMO_MODE_INACTIVE) {
            h = r25_h32(h, (u32)gRandomSeed16);
            h = r25_h32(h, r25_bits(gCourseTimer));
            h = r25_h32(h, r25_bits(gVBlankTimer));
            for (i = 0; i < 8; ++i) {
                h = r25_h32(h, r25_kin_hash(&gPlayers[i], i));
                h = r25_h32(h, r25_logic_hash(&gPlayers[i], i));
            }
        }
        return h;
    }
    h=cross_hash_u32(h,(u32)gGlobalTimer);
    h=cross_phase_hash(h);
    if(gGamestate==RACING){
        h=cross_hash_f32(h,gCourseTimer);
        h=cross_hash_f32(h,gVBlankTimer);
        h=cross_hash_u16(h,gRandomSeed16);
        for(i=0;i<players;++i){
            h=cross_hash_u16(h,gPlayers[i].type);
            h=cross_hash_u16(h,(u16)gPlayers[i].lapCount);
            h=cross_hash_u32(h,gPlayers[i].effects);
            for(j=0;j<3;++j)h=cross_hash_f32(h,gPlayers[i].pos[j]);
            for(j=0;j<3;++j)h=cross_hash_f32(h,gPlayers[i].velocity[j]);
        }
    }
    return x360_net_extended_lobby()?x360_race8_hash(h):h;
}
#endif

/* MK64_CROSSPLAY_COMPONENT_DIAG_R15_MAIN
 * Cross-platform, read-only diagnostic snapshot.
 * Hashes integers in an explicit byte order so PPC/x86 host endianness
 * cannot itself create a mismatch.
 */
static u32 mkdiag_fnv_u32(u32 h, u32 v) {
    h = (h ^ ((v >> 24) & 0xFFU)) * 16777619U;
    h = (h ^ ((v >> 16) & 0xFFU)) * 16777619U;
    h = (h ^ ((v >>  8) & 0xFFU)) * 16777619U;
    h = (h ^ ( v        & 0xFFU)) * 16777619U;
    return h;
}
static u32 mkdiag_float_bits(f32 v) {
    union { f32 f; u32 u; } x;
    x.f = v;
    return x.u;
}
static u32 mkdiag_hash_vec3_raw(const f32 *v) {
    u32 h = 2166136261U;
    h = mkdiag_fnv_u32(h, mkdiag_float_bits(v[0]));
    h = mkdiag_fnv_u32(h, mkdiag_float_bits(v[1]));
    h = mkdiag_fnv_u32(h, mkdiag_float_bits(v[2]));
    return h;
}
static u32 mkdiag_quant_float_bits(f32 v) {
    u32 b = mkdiag_float_bits(v);
    /* Diagnostic only: discard the lowest 12 mantissa/storage bits.
     * This never changes gameplay state. */
    return b & 0xFFFFF000U;
}
static u32 mkdiag_hash_vec3_quant(const f32 *v) {
    u32 h = 2166136261U;
    h = mkdiag_fnv_u32(h, mkdiag_quant_float_bits(v[0]));
    h = mkdiag_fnv_u32(h, mkdiag_quant_float_bits(v[1]));
    h = mkdiag_fnv_u32(h, mkdiag_quant_float_bits(v[2]));
    return h;
}
static u32 mkdiag_player_meta_hash(int i) {
    u32 h = 2166136261U;
    h = mkdiag_fnv_u32(h, (u32)gPlayers[i].type);
    h = mkdiag_fnv_u32(h, (u32)(s32)gPlayers[i].lapCount);
    h = mkdiag_fnv_u32(h, (u32)gPlayers[i].effects);
    return h;
}

void mk64_crossplay_component_diag(unsigned int *out, int cap) {
    extern u16 gRandomSeed16;
    u32 core, full;
    int i;
    if (!out || cap < 36) return;
    for (i = 0; i < cap; ++i) out[i] = 0;

    out[0] = 0x4D4B4431U; /* MKD1 */
    out[1] = (u32)gGlobalTimer;
    out[2] = (u32)gGamestate;
    out[3] = (u32)gModeSelection;
    out[4] = (u32)gRandomSeed16;

    core = 2166136261U;
    core = mkdiag_fnv_u32(core, out[2]);
    core = mkdiag_fnv_u32(core, out[3]);
    core = mkdiag_fnv_u32(core, out[4]);
    out[5] = core;

    out[6]  = mkdiag_player_meta_hash(0);
    out[7]  = mkdiag_hash_vec3_raw(gPlayers[0].pos);
    out[8]  = mkdiag_hash_vec3_raw(gPlayers[0].velocity);
    out[9]  = mkdiag_hash_vec3_quant(gPlayers[0].pos);
    out[10] = mkdiag_hash_vec3_quant(gPlayers[0].velocity);

    out[11] = mkdiag_player_meta_hash(1);
    out[12] = mkdiag_hash_vec3_raw(gPlayers[1].pos);
    out[13] = mkdiag_hash_vec3_raw(gPlayers[1].velocity);
    out[14] = mkdiag_hash_vec3_quant(gPlayers[1].pos);
    out[15] = mkdiag_hash_vec3_quant(gPlayers[1].velocity);

    out[16] = mkdiag_float_bits(gPlayers[0].pos[0]);
    out[17] = mkdiag_float_bits(gPlayers[0].pos[1]);
    out[18] = mkdiag_float_bits(gPlayers[0].pos[2]);
    out[19] = mkdiag_float_bits(gPlayers[0].velocity[0]);
    out[20] = mkdiag_float_bits(gPlayers[0].velocity[1]);
    out[21] = mkdiag_float_bits(gPlayers[0].velocity[2]);

    out[22] = mkdiag_float_bits(gPlayers[1].pos[0]);
    out[23] = mkdiag_float_bits(gPlayers[1].pos[1]);
    out[24] = mkdiag_float_bits(gPlayers[1].pos[2]);
    out[25] = mkdiag_float_bits(gPlayers[1].velocity[0]);
    out[26] = mkdiag_float_bits(gPlayers[1].velocity[1]);
    out[27] = mkdiag_float_bits(gPlayers[1].velocity[2]);

    full = core;
    for (i = 6; i <= 15; ++i) full = mkdiag_fnv_u32(full, out[i]);
    out[28] = full;

    out[30] = (u32)gPlayers[0].type;
    out[31] = (u32)(s32)gPlayers[0].lapCount;
    out[32] = (u32)gPlayers[0].effects;
    out[33] = (u32)gPlayers[1].type;
    out[34] = (u32)(s32)gPlayers[1].lapCount;
    out[35] = (u32)gPlayers[1].effects;
}
