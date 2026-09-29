#ifndef GLASS98_CDCATALOG_H
#define GLASS98_CDCATALOG_H
#define CD_CATALOG_TEXT 512
typedef struct
{
    char album[CD_CATALOG_TEXT], artist[CD_CATALOG_TEXT];
    char tracks[99][CD_CATALOG_TEXT];
} CD_CATALOG_RESULT;
/* Relative track starts and total duration are in 1/75-second frames.
   Returns matching editions, zero for no match, or -1 for an invalid catalog.
   Text is UTF-8. The reader allocates no memory proportional to catalog size. */
int cd_catalog_lookup(const char *path, const unsigned long *offsets, int tracks,
                      unsigned long duration, int choice, CD_CATALOG_RESULT *result);
#endif
