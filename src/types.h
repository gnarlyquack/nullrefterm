#include <limits.h>

#define nullptr ((void *)0)

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define CAST(type, value) ((type)(value))

#define EXPR_MAX(expr) _Generic((expr), \
    char: CHAR_MAX, \
    short: SHRT_MAX, \
    int: INT_MAX, \
    long: LONG_MAX, \
    long long: LLONG_MAX, \
    unsigned char: UCHAR_MAX, \
    unsigned short: USHRT_MAX, \
    unsigned int: UINT_MAX, \
    unsigned long: ULONG_MAX, \
    unsigned long long: ULLONG_MAX)

#define EXPR_MIN(expr) _Generic((expr), \
    char: CHAR_MIN, \
    short: SHRT_MIN, \
    int: INT_MIN, \
    long: LONG_MIN, \
    long long: LLONG_MIN, \
    unsigned char: 0, \
    unsigned short: 0, \
    unsigned int: 0, \
    unsigned long: 0, \
    unsigned long long: 0)

#define TYPE_MAX(type) EXPR_MAX((type)0)

#define TYPE_MIN(type) EXPR_MIN((type)0)

typedef unsigned char byte;
