#ifndef __TAP_H__
#define __TAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef va_copy
#define va_copy(d,s) ((d) = (s))
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int     vok_at_loc      (const char *file, int line, int test, const char *fmt,
                         va_list args);
int     ok_at_loc       (const char *file, int line, int test, const char *fmt,
                         ...);
int     is_at_loc       (const char *file, int line, const char *got,
                         const char *expected, const char *fmt, ...);
int     isnt_at_loc     (const char *file, int line, const char *got,
                         const char *expected, const char *fmt, ...);
int     cmp_ok_at_loc   (const char *file, int line, int a, const char *op,
                         int b, const char *fmt, ...);
int     bail_out        (int ignore, const char *fmt, ...);
void    cplan           (int tests, const char *fmt, ...);
int     diag            (const char *fmt, ...);
int     note            (const char *fmt, ...);
int     exit_status     (void);
void    skippy          (int n, const char *fmt, ...);
void    ctodo           (int ignore, const char *fmt, ...);
void    cendtodo        (void);

#define NO_PLAN          -1
#define SKIP_ALL         -2
#define ok(...)          ok_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define is(...)          is_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define isnt(...)        isnt_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define cmp_ok(...)      cmp_ok_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define plan(...)        cplan(__VA_ARGS__, NULL)
#define done_testing     return exit_status()
#define BAIL_OUT(...)    bail_out(0, ## __VA_ARGS__, NULL)

#define skip(test, ...)  do {if (test) {skippy(__VA_ARGS__, NULL); break;}
#define endskip          } while (0)

#define todo(...)        ctodo(0, ## __VA_ARGS__, NULL)
#define endtodo          cendtodo()

#define dies_ok(...)     dies_ok_common(1, ## __VA_ARGS__)
#define lives_ok(...)    dies_ok_common(0, ## __VA_ARGS__)

#ifdef _WIN32
#define pass(...)        ok_at_loc(__FILE__, __LINE__, 1, __VA_ARGS__, NULL)
#define fail(...)        ok_at_loc(__FILE__, __LINE__, 0, __VA_ARGS__, NULL)
#define like(...)        skippy(1, "like is not implemented on MSWin32")
#define unlike(...)      like()
#define dies_ok_common(...) \
    skippy(1, "Death detection is not supported on MSWin32")
#else
#define pass(...)        ok(1, ## __VA_ARGS__)
#define fail(...)        ok(0, ## __VA_ARGS__)
#define like(...)        like_at_loc(1, __FILE__, __LINE__, __VA_ARGS__, NULL)
#define unlike(...)      like_at_loc(0, __FILE__, __LINE__, __VA_ARGS__, NULL)
int     like_at_loc     (int for_match, const char *file, int line,
                         const char *got, const char *expected,
                         const char *fmt, ...);
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
int tap_test_died (int status);
#define dies_ok_common(for_death, code, ...)                \
    do {                                                    \
        tap_test_died(1);                                   \
        int cpid = fork();                                  \
        switch (cpid) {                                     \
        case -1:                                            \
            perror("fork error");                           \
            exit(1);                                        \
        case 0:                                             \
            close(1);                                       \
            close(2);                                       \
            code                                            \
            tap_test_died(0);                               \
            exit(0);                                        \
        }                                                   \
        if (waitpid(cpid, NULL, 0) < 0) {                   \
            perror("waitpid error");                        \
            exit(1);                                        \
        }                                                   \
        int it_died = tap_test_died(0);                     \
        if (!it_died)                                       \
            {code}                                          \
        ok(for_death ? it_died : !it_died, ## __VA_ARGS__); \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif

