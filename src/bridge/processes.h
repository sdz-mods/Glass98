#ifndef GLASS98_PROCESSES_H
#define GLASS98_PROCESSES_H
void processes_init(void);
void processes_pause(int paused);
void processes_update(const char *ini, int enabled, int seconds);
void processes_write(FILE *file);
void processes_stop(void);
#endif
