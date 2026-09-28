#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "autotheme.h"
static const IID pictureIID = {0x7BF80980, 0xBF32, 0x101A, {0x8B, 0xBB, 0x00, 0xAA, 0x00, 0x30, 0x0C, 0xAB}};
static unsigned long le32(const unsigned char *p)
{
    return p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
/* IPicture rejects some valid V4/V5 BMPs. Sample uncompressed BGR directly,
   respecting the file's pixel offset, row padding and top-down orientation.
   BI_RGB's fourth byte is unused, not alpha. Color profiles are not applied. */
static int sample_rgb_bmp(const unsigned char *p, unsigned long n, unsigned long w, unsigned long h,
                          unsigned long *pixels)
{
    unsigned long header, offset, stride, x, y, sx, sy;
    int depth, step;
    const unsigned char *pixel;
    if (n < 54 || p[0] != 'B' || p[1] != 'M')
        return 0;
    header = le32(p + 14);
    depth = p[28] | (p[29] << 8);
    if ((header != 40 && header != 108 && header != 124) || le32(p + 30) != BI_RGB || (depth != 24 && depth != 32))
        return 0;
    offset = le32(p + 10);
    step = depth / 8;
    stride = (w * step + 3) & ~3UL;
    if (header > n - 14 || p[26] != 1 || p[27] != 0 || offset < 14 + header || offset > n || stride > (n - offset) / h)
        return -1;
    for (y = 0; y < WALL_SAMPLE; y++)
    {
        sy = ((2 * y + 1) * h) / (2 * WALL_SAMPLE);
        if (!(le32(p + 22) & 0x80000000UL))
            sy = h - 1 - sy;
        for (x = 0; x < WALL_SAMPLE; x++)
        {
            sx = ((2 * x + 1) * w) / (2 * WALL_SAMPLE);
            pixel = p + offset + sy * stride + sx * step;
            pixels[y * WALL_SAMPLE + x] = pixel[0] | ((unsigned long)pixel[1] << 8) | ((unsigned long)pixel[2] << 16);
        }
    }
    return 1;
}
/* Inspect dimensions before handing compressed data to the legacy decoder. */
static int dimensions(const unsigned char *p, unsigned long n, unsigned long *w, unsigned long *h)
{
    unsigned long off, len;
    int marker;
    if (n >= 26 && p[0] == 'B' && p[1] == 'M')
    {
        if (le32(p + 14) == 12)
        {
            *w = p[18] | (p[19] << 8);
            *h = p[20] | (p[21] << 8);
        }
        else if (n >= 54 && le32(p + 14) >= 40)
        {
            *w = le32(p + 18);
            *h = le32(p + 22);
            if (*h & 0x80000000UL)
                *h = 0 - *h;
        }
        else
            return 0;
        return 1;
    }
    if (n >= 10 && (!memcmp(p, "GIF87a", 6) || !memcmp(p, "GIF89a", 6)))
    {
        *w = p[6] | (p[7] << 8);
        *h = p[8] | (p[9] << 8);
        return 1;
    }
    if (n < 4 || p[0] != 255 || p[1] != 216)
        return 0;
    off = 2;
    while (off + 4 <= n)
    {
        if (p[off++] != 255)
            return 0;
        while (off < n && p[off] == 255)
            off++;
        if (off >= n)
            return 0;
        marker = p[off++];
        if (marker == 217 || marker == 218)
            return 0;
        if (marker == 1 || (marker >= 208 && marker <= 215))
            continue;
        if (off + 2 > n)
            return 0;
        len = (p[off] << 8) | p[off + 1];
        if (len < 2 || len > n - off)
            return 0;
        if (marker >= 192 && marker <= 207 && marker != 196 && marker != 200 && marker != 204)
        {
            if (len < 8)
                return 0;
            *h = (p[off + 3] << 8) | p[off + 4];
            *w = (p[off + 5] << 8) | p[off + 6];
            return 1;
        }
        off += len;
    }
    return 0;
}
int wall_analyze(const char *path, int mode, int margins, WALL_PALETTE *out, char *error, int capacity)
{
    HANDLE file = INVALID_HANDLE_VALUE;
    HGLOBAL memory = NULL;
    unsigned char *bytes = NULL, *dib = NULL;
    DWORD size, read;
    unsigned long w, h, *pixels = NULL;
    IStream *stream = NULL;
    IPicture *picture = NULL;
    HDC dc = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ old = NULL;
    BITMAPINFO bi;
    LONG pw, ph;
    HRESULT hr, init;
    int i, ok = 0;
    const char *why = "Cannot read the wallpaper image.";
    init = CoInitialize(NULL);
    if (FAILED(init))
    {
        lstrcpynA(error, "Could not initialize the image decoder.", capacity);
        return 0;
    }
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto done;
    size = GetFileSize(file, NULL);
    why = "Use a BMP, JPEG or GIF no larger than 16 MiB and 12 megapixels.";
    if (!size || size > 16UL * 1024 * 1024)
        goto done;
    why = "Not enough memory to load the wallpaper.";
    memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory)
        goto done;
    bytes = (unsigned char*)GlobalLock(memory);
    if (!bytes)
        goto done;
    why = "Cannot read the wallpaper image.";
    if (!ReadFile(file, bytes, size, &read, NULL) || read != size)
        goto done;
    why = "Use a BMP, JPEG or GIF up to 8192 pixels per side and 12 megapixels.";
    if (!dimensions(bytes, size, &w, &h) || !w || !h || w > 8192 || h > 8192 || w * h > 12000000UL)
        goto done;
    why = "Not enough memory to sample the wallpaper.";
    pixels = (unsigned long*)malloc(WALL_SAMPLE * WALL_SAMPLE * sizeof(unsigned long));
    if (!pixels)
        goto done;
    i = sample_rgb_bmp(bytes, size, w, h, pixels);
    if (i < 0)
    {
        why = "The BMP header or pixel data is incomplete or invalid.";
        goto done;
    }
    if (i > 0)
        goto analyze;
    GlobalUnlock(memory);
    bytes = NULL;
    why = "Windows could not decode this image. Try a BMP, JPEG or GIF.";
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)))
        goto done;
    memory = NULL;
    hr = OleLoadPicture(stream, size, FALSE, &pictureIID, (void**)&picture);
    if (FAILED(hr) || !picture)
        goto done;
    if (FAILED(picture->lpVtbl->get_Width(picture, &pw)) || FAILED(picture->lpVtbl->get_Height(picture, &ph)) || pw <= 0 ||
            ph <= 0)
        goto done;
    dc = CreateCompatibleDC(NULL);
    if (!dc)
        goto done;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = WALL_SAMPLE;
    bi.bmiHeader.biHeight = -WALL_SAMPLE;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&dib, NULL, 0);
    if (!bitmap)
        goto done;
    old = SelectObject(dc, bitmap);
    if (!old || old == HGDI_ERROR)
    {
        old = NULL;
        goto done;
    }
    for (i = 0; i < WALL_SAMPLE * WALL_SAMPLE; i++)
    {
        dib[i * 4] = 0x20;
        dib[i * 4 + 1] = 0x18;
        dib[i * 4 + 2] = 0x0B;
        dib[i * 4 + 3] = 0;
    }
    SetStretchBltMode(dc, COLORONCOLOR);
    if (FAILED(picture->lpVtbl->Render(picture, dc, 0, 0, WALL_SAMPLE, WALL_SAMPLE, 0, ph, pw, -ph, NULL)))
        goto done;
    GdiFlush();
    for (i = 0; i < WALL_SAMPLE * WALL_SAMPLE; i++)
        pixels[i] = dib[i * 4] | ((unsigned long)dib[i * 4 + 1] << 8) | ((unsigned long)dib[i * 4 + 2] << 16);
analyze:
    ok = wall_palette(pixels, WALL_SAMPLE, WALL_SAMPLE, mode, margins, out);
    if (!ok)
        why = "Not enough memory to analyze this wallpaper.";
done:
    if (pixels)
        free(pixels);
    if (old)
        SelectObject(dc, old);
    if (bitmap)
        DeleteObject(bitmap);
    if (dc)
        DeleteDC(dc);
    if (picture)
        picture->lpVtbl->Release(picture);
    if (stream)
        stream->lpVtbl->Release(stream);
    if (bytes)
        GlobalUnlock(memory);
    if (memory)
        GlobalFree(memory);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    CoUninitialize();
    lstrcpynA(error, ok ? "" : why, capacity);
    return ok;
}
