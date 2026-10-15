#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FG_BRIDGE_ABI 0x00030000u
#define FG_RGBA8_LINEAR 1u
#define FG_CPU_READY 1u
#define FG_RETAIN_UNTIL_RETURN 2u
// Capture timestamp uses the same steady clock as the engine; enables live deadlines.
#define FG_CLOCK_MONOTONIC 4u
#define FG_CONSUMED_OFFLINE 0
#define FG_CONFIRMED_FLIP 1
#define FG_DROPPED_BY_CONSUMER 2
#define FG_BAD_ARGUMENT -1
#define FG_UNSUPPORTED_SURFACE -2
#define FG_BUSY -3
#define FG_ENGINE_ERROR -4
#define FG_CALLBACK_ERROR -5
typedef struct FGContext FGContext;
typedef struct FGInput {
    const void* rgba;
    uint64_t capacity;
    uint32_t width,height,pitch,format,flags;
    double captured_seconds;
} FGInput;
typedef struct FGOutput {
    const void* rgba;
    uint32_t width,height,pitch,generated;
    float temporal_position;
    double due_seconds,expires_seconds; // zero for offline consumers / unavailable clock
} FGOutput;
// Return 1 only after an actual flip confirmation and provide its monotonic time.
// Return 0 for an offline/file consumer. Pointers are borrowed until callback return.
typedef int (*FGPresent)(void* user,const FGOutput* output,double* confirmed_seconds);
typedef struct FGConfiguration {
    uint32_t enabled,multiplier,profile,processing,output;
    uint32_t automatic,low_latency,filter,sharpness,fps_limit,scene_guard,safe_mode,diagnostics,source_budget_us;
} FGConfiguration;
typedef struct FGSnapshot {
    uint64_t captured,delivered,confirmed;
    uint32_t menu_visible,input_width,input_height,processing_width,processing_height,output_width,output_height;
    double cpu_ms,total_bridge_ms;
    FGConfiguration configuration;
    int last_error;
    uint64_t dropped;
    uint32_t adaptive_pressure,skipped_reason,deadline_available,effective_multiplier;
    double source_fps,presented_fps,frametime_mean_ms,frametime_max_ms,frametime_p95_ms;
    uint64_t rgba_buffer_bytes;
    double interpolation_ms,scaling_ms,latency_estimate_ms;
} FGSnapshot;
typedef struct FGBridgeAPI {
    uint32_t abi,size;
    uint32_t cooperative_capture,pad_edges,output_callback,game_hooks,gpu_backend;
    FGContext* (*create)(uint32_t threads);
    int (*destroy)(FGContext*);
    int (*configure)(FGContext*,const FGConfiguration*);
    int (*pad)(FGContext*,uint32_t portable_buttons);
    int (*capture)(FGContext*,const FGInput*,FGPresent,void*);
    int (*snapshot)(FGContext*,FGSnapshot*);
} FGBridgeAPI;
#if defined(_WIN32)
#ifdef FG_BRIDGE_BUILD
#define FG_EXPORT __declspec(dllexport)
#else
#define FG_EXPORT __declspec(dllimport)
#endif
#else
#define FG_EXPORT __attribute__((visibility("default")))
#endif
FG_EXPORT const FGBridgeAPI* fg_bridge_api(uint32_t abi);
#ifdef __cplusplus
}
#endif
