#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void play_sdcard_mp3_files(const char *path, bool loop);

void touch_task(void *param);

#ifdef __cplusplus
}
#endif
