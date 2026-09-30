#ifndef W98_EXTRAS_H
#define W98_EXTRAS_H
#include <windows.h>
#include <stdio.h>
void extra_start(const char *directory);
void extra_stop(void);
int extra_core_enabled(void);
void extra_refresh(void);
void extra_pause(int paused);
int device_action(const char *command);
void js_string(FILE *f, const char *s);
#endif
