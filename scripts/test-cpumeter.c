/* Counter rollover, invalid baselines and timer-independent CPU arithmetic. */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/vxd/g98cpu.h"
#include "../src/bridge/cpumeter.h"

int main(void)
{
    G98CPU_SAMPLE a, b;
    memset(&a, 0, sizeof(a));
    a.size = sizeof(a);
    a.version = G98CPU_VERSION;
    b = a;
    b.pitLow = 1193182;
    assert(cpu_meter(&a, &b) == 100);
    b.systemThreadMs = 500;
    assert(cpu_meter(&a, &b) == 50);
    b.systemThreadMs = 1000;
    assert(cpu_meter(&a, &b) == 0);
    b.systemThreadMs = 1001;
    assert(cpu_meter(&a, &b) == 0); /* sampling skew */
    b.systemThreadMs = 1100;
    assert(cpu_meter(&a, &b) == -1);

    a.systemThreadMs = 0xffffff00UL;
    b.systemThreadMs = a.systemThreadMs + 500;
    assert(cpu_meter(&a, &b) == 50); /* idle counter wraps at 49.7 days */
    a.pitLow = 0xffff0000UL;
    b.pitHigh = 1;
    b.pitLow = a.pitLow + 1193182UL;
    assert(cpu_meter(&a, &b) == 50); /* low PIT word wraps roughly hourly */
    b.version++;
    assert(cpu_meter(&a, &b) == -1);
    b.version = a.version;
    b.size--;
    assert(cpu_meter(&a, &b) == -1);
    b = a;
    assert(cpu_meter(&a, &b) == -1); /* zero interval */
    b.pitLow--;
    assert(cpu_meter(&a, &b) == -1); /* counter reset */
    a.pitLow = 0;
    b = a;
    b.pitLow = 1193182UL * 61;
    assert(cpu_meter(&a, &b) == -1); /* resume after a long gap */
    puts("PASS: CPU meter loads, protocol, rollover, skew and rebaseline");
    return 0;
}
