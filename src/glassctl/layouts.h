/* Resolution profiles: only visibility/coordinates are restored from saved layouts.
   Desktop/Layout remains the active, shared-settings collector contract. */
static int layout_key(const char *key)
{
    int w, h;
    char tail;
    return sscanf(key, "%dx%d%c", &w, &h, &tail) == 2 && w >= 320 && h >= 200 && w <= 32000 && h <= 32000;
}
static void layout_split(char *s, char **a)
{
    int i;
    char *p = s;
    for (i = 0; i < FIELDS; i++)
    {
        a[i] = p;
        p = strchr(p, '|');
        if (p)
            *p++ = 0;
    }
}
static void layout_hidden(char *s)
{
    int i;
    memset(s, 0, PANELS + 1);
    GetPrivateProfileStringA("Desktop", "LayoutHidden", "", s, PANELS + 1, ini);
    for (i = 0; i < PANELS; i++)
        if (s[i] != '1')
            s[i] = '0';
    s[PANELS] = 0;
}
static int layout_sync(void)
{
    char key[40], section[64], config[4096], hidden[PANELS + 1], value[32];
    int ok;
    GetPrivateProfileStringA("Desktop", "Resolution", "", key, sizeof(key), ini);
    if (!layout_key(key))
        return 1;
    GetPrivateProfileStringA("Desktop", "Layout", defaults, config, sizeof(config), ini);
    if (!valid(config))
        return 0;
    sprintf(section, "Layout.%s", key);
    layout_hidden(hidden);
    ok = WritePrivateProfileStringA(section, "Layout", config, ini) &&
         WritePrivateProfileStringA(section, "Hidden", hidden, ini);
    GetPrivateProfileStringA("Desktop", "WorkWidth", "0", value, sizeof(value), ini);
    ok = WritePrivateProfileStringA(section, "WorkWidth", value, ini) && ok;
    GetPrivateProfileStringA("Desktop", "WorkHeight", "0", value, sizeof(value), ini);
    return WritePrivateProfileStringA(section, "WorkHeight", value, ini) && ok;
}
static void layout_clear_hidden(int index)
{
    char hidden[PANELS + 1];
    layout_hidden(hidden);
    hidden[index] = '0';
    WritePrivateProfileStringA("Desktop", "LayoutHidden", hidden, ini);
}
static int layout_guard(char *args)
{
    char *tag = strrchr(args, '/'), key[40], actual[40];
    if (!tag || !layout_key(tag + 1))
        return 1;
    *tag++ = 0;
    GetPrivateProfileStringA("Desktop", "Resolution", "", key, sizeof(key), ini);
    sprintf(actual, "%dx%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    return !strcmp(tag, key) && !strcmp(tag, actual);
}
static int layout_switch(int w, int h, int aw, int ah, int *sizes)
{
    char key[40], old[40], section[64], current[4096], saved[4096], result[4096], *a[FIELDS], *b[FIELDS],
         hidden[PANELS + 1], text[48];
    int on[PANELS], x[PANELS], y[PANELS], order[PANELS], bottom[PANELS], n = 0, i, j, k, t, cols, width, exists, initial,
                                                                         reflow = 0, ok = 1, oldaw, oldah;
    sprintf(key, "%dx%d", w, h);
    if (!layout_key(key) || aw < 1 || ah < 1 || aw > w || ah > h)
        return 0;
    GetPrivateProfileStringA("Desktop", "Resolution", "", old, sizeof(old), ini);
    GetPrivateProfileStringA("Desktop", "Layout", defaults, current, sizeof(current), ini);
    migrate(current);
    if (!valid(current))
        strcpy(current, defaults);
    if (!layout_sync())
        return 0;
    sprintf(section, "Layout.%s", key);
    GetPrivateProfileStringA(section, "Layout", "", saved, sizeof(saved), ini);
    exists = valid(saved);
    initial = !layout_key(old) && !exists;
    oldaw = GetPrivateProfileIntA(section, "WorkWidth", 0, ini);
    oldah = GetPrivateProfileIntA(section, "WorkHeight", 0, ini);
    if (!exists)
        strcpy(saved, current);
    memset(hidden, '0', PANELS);
    hidden[PANELS] = 0;
    if (exists)
    {
        char temp[PANELS + 1];
        memset(temp, 0, sizeof(temp));
        GetPrivateProfileStringA(section, "Hidden", "", temp, sizeof(temp), ini);
        for (i = 0; i < PANELS; i++)
            hidden[i] = temp[i] == '1' ? '1' : '0';
    }
    layout_split(current, a);
    layout_split(saved, b);
    width = atoi(a[1]);
    reflow = GetPrivateProfileIntA("Desktop", "Reflow", 0, ini) || (!exists && !initial) || (exists && (oldaw != aw ||
             oldah != ah || atoi(b[1]) != width));
    for (i = 0; i < PANELS; i++)
    {
        on[i] = atoi(b[5 + i * 8]);
        x[i] = atoi(b[6 + i * 8]);
        y[i] = atoi(b[7 + i * 8]);
        if (i == 5 || i == 21)
            on[i] = 0;
        if (initial && x[i] == 32000 && y[i] != 32000)
            x[i] = aw - width - 2;
        if (on[i] && (x[i] < 1 || x[i] + width > aw - 2 || y[i] < 1 || y[i] + sizes[i] > ah - 3))
            reflow = 1;
    }
    if (reflow)
    {
        for (i = 0; i < PANELS; i++)
            if (i != 5 && i != 21 && (on[i] || hidden[i] == '1'))
                order[n++] = i;
        for (i = 1; i < n; i++)
        {
            t = order[i];
            j = i;
            while (j > 0 && (on[order[j - 1]] < on[t] || (on[order[j - 1]] == on[t] && (x[order[j - 1]] < x[t] ||
                             (x[order[j - 1]] == x[t] && y[order[j - 1]] > y[t])))))
            {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = t;
        }
        cols = (aw + 7) / (width + 10);
        if (cols > PANELS)
            cols = PANELS;
        for (j = 0; j < cols; j++)
            bottom[j] = 1;
        for (k = 0; k < n; k++)
        {
            i = order[k];
            t = sizes[i];
            if (t > ah - 4)
                t = ah - 4;
            on[i] = 0;
            hidden[i] = '1';
            if (t < 40 || width > aw - 3)
                continue;
            for (j = 0; j < cols; j++)
                if (bottom[j] + t <= ah - 3)
                {
                    on[i] = 1;
                    hidden[i] = '0';
                    x[i] = aw - width - 2 - j * (width + 10);
                    y[i] = bottom[j];
                    bottom[j] += t + 10;
                    break;
                }
        }
    }
    result[0] = 0;
    for (i = 0; i < FIELDS; i++)
    {
        if (i)
            strcat(result, "|");
        if (i >= 5 && (i - 5) % 8 < 3)
        {
            j = (i - 5) / 8;
            k = (i - 5) % 8;
            sprintf(text, "%d", k == 0 ? on[j] : k == 1 ? x[j] : y[j]);
            strcat(result, text);
        }
        else
            strcat(result, a[i]);
    }
    if (!valid(result))
        return 0;
    ok = WritePrivateProfileStringA("Desktop", "Layout", result, ini) &&
         WritePrivateProfileStringA("Desktop", "Resolution", key, ini) &&
         WritePrivateProfileStringA("Desktop", "LayoutHidden", hidden, ini);
    sprintf(text, "%d", aw);
    ok = WritePrivateProfileStringA("Desktop", "WorkWidth", text, ini) && ok;
    sprintf(text, "%d", ah);
    ok = WritePrivateProfileStringA("Desktop", "WorkHeight", text, ini) && ok;
    WritePrivateProfileStringA("Desktop", "PlacementNotice", "", ini);
    WritePrivateProfileStringA("Desktop", "Reflow", NULL, ini);
    return ok && layout_sync();
}
static int layout_display(char *args)
{
    int w, h, aw, ah, sizes[PANELS], i, n = 0;
    char *p, *end;
    RECT r;
    if (sscanf(args, "%d/%d/%d/%d/%n", &w, &h, &aw, &ah, &n) != 4 || n <= 0)
        return 0;
    if (w != GetSystemMetrics(SM_CXSCREEN) || h != GetSystemMetrics(SM_CYSCREEN))
        return 1;
    if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0) || aw != r.right - r.left || ah != r.bottom - r.top)
        return 1;
    p = args + n;
    for (i = 0; i < PANELS; i++)
    {
        sizes[i] = (int)strtol(p, &end, 10);
        if (end == p || sizes[i] < 40 || sizes[i] > 32000 || (i < PANELS - 1 ? *end != ',' : *end != 0))
            return 0;
        p = end + 1;
    }
    return layout_switch(w, h, aw, ah, sizes);
}
