#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void quote(FILE *f, const char *s)
{
    unsigned char c;
    fputc('\'', f);
    while ((c = *s++) != 0)
    {
        if (c < 32 || c >= 127 || c == '\'' || c == '\\' || c == '<')
            fprintf(f, "\\x%02x", c);
        else
            fputc(c, f);
    }
    fputc('\'', f);
}
static void plain(char *out, const char *p, const char *end)
{
    int n = 0, tag = 0;
    WCHAR wide[512];
    char ansi[512];
    while (p < end && n < 400)
    {
        if (!strncmp(p, "<![CDATA[", 9))
        {
            p += 9;
            continue;
        }
        if (!strncmp(p, "]]>", 3))
        {
            p += 3;
            continue;
        }
        if (*p == '<')
        {
            tag = 1;
            p++;
            continue;
        }
        if (*p == '>')
        {
            tag = 0;
            p++;
            continue;
        }
        if (tag)
        {
            p++;
            continue;
        }
        if (!strncmp(p, "&amp;", 5))
        {
            out[n++] = '&';
            p += 5;
        }
        else if (!strncmp(p, "&lt;", 4))
        {
            out[n++] = '<';
            p += 4;
        }
        else if (!strncmp(p, "&gt;", 4))
        {
            out[n++] = '>';
            p += 4;
        }
        else if (!strncmp(p, "&quot;", 6))
        {
            out[n++] = '"';
            p += 6;
        }
        else
        {
            out[n++] = (unsigned char) * p < 32 ? ' ' : *p;
            p++;
        }
    }
    out[n] = 0;
    if (MultiByteToWideChar(CP_UTF8, 0, out, -1, wide, 512) &&
            WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, 512, NULL, NULL))
        strcpy(out, ansi);
}
int rss_fetch(const char *ini, const char *dir)
{
    char url[1024], dest[MAX_PATH], temp[MAX_PATH], status[160] = "No feed configured", title[512], *buf, *p, *t, *end,
            *close;
    HINTERNET session = NULL, request = NULL;
    DWORD timeout = 10000, read = 0, code = 0, size = sizeof(code), length = 0, error = 0;
    int count = 0;
    FILE *f;
    FILETIME ft;
    double now;
    GetPrivateProfileStringA("Options", "rss", "", url, sizeof(url), ini);
    sprintf(dest, "%s\\RSS.JS", dir);
    sprintf(temp, "%s\\RSS.TMP", dir);
    buf = malloc(262145);
    if (!buf)
        return 0;
    buf[0] = 0;
    if (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8))
    {
        session = InternetOpenA("Glass98", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (session)
        {
            InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
            InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
            InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
            request = InternetOpenUrlA(session, url, NULL, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0);
            if (request && HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &size, NULL) &&
                    code == 200)
            {
                while (length < 262144)
                {
                    if (!InternetReadFile(request, buf + length, 262144 - length, &read))
                    {
                        error = GetLastError();
                        break;
                    }
                    if (!read)
                        break;
                    length += read;
                }
                buf[length] = 0;
                if (error)
                {
                    sprintf(status, "Feed read failed (error %lu)", error);
                    length = 0;
                    buf[0] = 0;
                }
                else
                    strcpy(status, length == 262144 ? "Feed exceeds 256 KiB" : "No RSS/Atom entries found");
            }
            else
            {
                error = GetLastError();
                sprintf(status, "Feed unavailable (HTTP %lu / error %lu)", code, error);
            }
        }
        else
            strcpy(status, "Internet transport unavailable");
    }
    if (request)
        InternetCloseHandle(request);
    if (session)
        InternetCloseHandle(session);
    f = fopen(temp, "wb");
    if (!f)
    {
        free(buf);
        return 0;
    }
    fputs("var rssItems=[", f);
    p = buf;
    if (length && length < 262144)
        while (count < 12)
        {
            t = strstr(p, "<item");
            if (!t)
                t = strstr(p, "<entry");
            if (!t)
                break;
            end = strstr(t, "</item>");
            if (!end)
                end = strstr(t, "</entry>");
            if (!end)
                break;
            p = end + 7;
            t = strstr(t, "<title");
            if (!t || t > end)
                continue;
            t = strchr(t, '>');
            if (!t || t > end)
                continue;
            t++;
            close = strstr(t, "</title>");
            if (!close || close > end)
                continue;
            plain(title, t, close);
            if (count)
                fputc(',', f);
            quote(f, title);
            count++;
        }
    if (count)
        strcpy(status, "Updated");
    GetSystemTimeAsFileTime(&ft);
    now = ((double)ft.dwHighDateTime * 4294967296.0 + ft.dwLowDateTime) / 10000.0 - 11644473600000.0;
    fputs("];var rssStatus=", f);
    quote(f, status);
    fprintf(f, ";var rssTime=%.0f;\n", now);
    error = ferror(f);
    if (fclose(f))
        error = 1;
    free(buf);
    if (error)
        return 0;
    if (!DeleteFileA(dest) && GetLastError() != ERROR_FILE_NOT_FOUND)
        return 0;
    return MoveFileA(temp, dest) != 0;
}
