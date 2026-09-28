#ifndef W98_THEMES_H
#define W98_THEMES_H
#include <stdio.h>
int theme_action(const char *command, const char *ini, char *layout);
void theme_publish(FILE *f, const char *ini, const char *layout);
#endif
