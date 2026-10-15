#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void boot_mark(const char* message);
int boot_optional_log(int stage); // Failure to write is nonfatal; never requires an existing file.
void boot_log_result(const char* operation,int result);
int boot_video_open(void);
int boot_video_present(const void* rgba); // fixed 1280x720 RGBA, waits for real flip
int boot_video_close(void); // memory retained on close failure
int boot_pad_open(void);
int boot_pad_options(void);
void boot_pad_close(void);
void boot_notify(const char* text);
void boot_sleep(unsigned milliseconds);
#ifdef __cplusplus
}
#endif
