#ifdef DEBUG

#include <signal.h>
#include <stdio.h>

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, \
                "Failed assertion: %s\nin file: %s\non line: %u\nin function: %s\n", \
                #cond, __FILE__, __LINE__, __func__); \
            __asm__ volatile("int3"); \
        } \
    } while (0)

#else

#define ASSERT(cond)

#endif
