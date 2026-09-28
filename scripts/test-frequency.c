#include <stdio.h>
#include "../src/bridge/telemetry.h"
int main(void)
{
    TELEMETRY a, b;
    DWORD first, second, tick;
    char text[512];
    tick = GetTickCount();
    telemetry_init(&a);
    first = GetTickCount() - tick;
    telemetry_stop(&a);
    tick = GetTickCount();
    telemetry_init(&b);
    second = GetTickCount() - tick;
    telemetry_text(&b, text);
    telemetry_stop(&b);
    printf("First init %lu ms; repeat init %lu ms; frequency %lu MHz; kind %lu\n%s\n", first, second, b.frequencyMHz,
           b.frequencyKind, text);
    if (a.frequencyMHz != b.frequencyMHz || a.frequencyKind != b.frequencyKind || second >= 500)
        return 1;
    puts("PASS: frequency cached across telemetry stop/start");
    return 0;
}
