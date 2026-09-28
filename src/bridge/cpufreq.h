/* Pure validation shared by the once-per-process timer calibration and tests. */
#ifndef W98_CPUFREQ_H
#define W98_CPUFREQ_H
static unsigned long frequency_result(double a, double b, double c)
{
    double t;
    if (!(a >= 10 && a <= 20000 && b >= 10 && b <= 20000 && c >= 10 && c <= 20000))
        return 0;
    if (a > b)
    {
        t = a;
        a = b;
        b = t;
    }
    if (b > c)
    {
        t = b;
        b = c;
        c = t;
    }
    if (a > b)
    {
        t = a;
        a = b;
        b = t;
    }
    if (c - a > b * 0.05)
        return 0;
    return (unsigned long)(b + 0.5);
}
#endif
