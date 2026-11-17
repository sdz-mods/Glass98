#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "../src/bridge/ntstats.h"
int main(void)
{
    assert(glass_cpu_percent(100, 200, 200, 300) == 0);
    assert(glass_cpu_percent(100, 200, 100, 300) == 100);
    assert(glass_cpu_percent(100, 200, 150, 300) == 50);
    /* Two processors: one busy for a second, one idle. */
    assert(glass_cpu_percent(0, 0, 10000000, 20000000) == 50);
    assert(glass_cpu_percent(100, 200, 100, 200) == -1);
    assert(glass_cpu_percent(100, 200, 90, 300) == -1);
    assert(glass_cpu_percent(100, 200, 300, 300) == -1);
    puts("PASS: NT CPU idle, busy, multicore normalization and counter discontinuities");
    return 0;
}
