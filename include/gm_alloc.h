#ifndef _GM_ALLOC_H
#define _GM_ALLOC_H

#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <include/utils.h>

void *gm_malloc(size_t size);
void *gm_realloc(void *ptr, size_t size);
void *gm_strdup(const char *s);
void *gm_strndup(const char *s, size_t size);
void gm_asprintf(char **strp, const char *fmt, ...);
#define gm_free(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0)
#endif
