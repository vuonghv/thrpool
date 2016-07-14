/*
 * The MACROs in this file strengthen assert() of standar assert.h
 * by printing the value of its arguments when assert() failed.
 * 
 * Author: Vuong Hoang <vuonghv.cs@gmail.com>
 */
#ifndef _THRPOOL_ASSERT_H
#define _THRPOOL_ASSERT_H

#include <assert.h>
#include <stdio.h>

#define XSTR(arg) STR(arg)
#define STR(arg) #arg
#define ASSERT(expr) assert(expr)

/* integer: assert(a rop b) */
#define __ASSERT_ROP_INT(a, b, rop)                                         \
    do {                                                                    \
        if (!((a) rop (b))) {                                               \
            fprintf(stderr, "[ASSERT FAILED]\n"                             \
                    "left  side: " STR(a) " = %d\n"                         \
                    "right side: " STR(b) " = %d\n", a, b);                 \
        }                                                                   \
        assert((a) rop (b));                                                \
    } while (0)

/* float: assert(a rop b) */
#define __ASSERT_ROP_FLOAT(a, b, rop)                                       \
    do {                                                                    \
        if (!((a) rop (b))) {                                               \
            fprintf(stderr, "[ASSERT FAILED]\n"                             \
                    "left  side: " STR(a) " = %f\n"                         \
                    "right side: " STR(b) " = %f\n", a, b);                 \
        }                                                                   \
        assert((a) rop (b));                                                \
    } while (0)

/* pointer: assert(a rop b) */
#define __ASSERT_ROP_ADDR(a, b, rop)                                        \
    do {                                                                    \
        if (!((a) rop (b))) {                                               \
            fprintf(stderr, "[ASSERT FAILED]\n"                             \
                    "left  side: " STR(a) " = %p\n"                         \
                    "right side: " STR(b) " = %p\n", a, b);                 \
        }                                                                   \
        assert((a) rop (b));                                                \
    } while (0)

/* pointer: assert(a rop NULL) */
#define __ASSERT_ROP_NULL(a, rop)                                           \
    do {                                                                    \
        if (!((a) rop NULL)) {                                              \
            fprintf(stderr, "[ASSERT FAILED]\n" STR(a) " = %p\n", a);       \
        }                                                                   \
        assert((a) rop NULL);                                               \
    } while (0)

#ifdef NDEBUG

#define ASSERT_EQ_INT(a, b) do {} while(0)
#define ASSERT_NE_INT(a, b) do {} while(0)
#define ASSERT_LE_INT(a, b) do {} while(0)
#define ASSERT_LT_INT(a, b) do {} while(0)
#define ASSERT_GE_INT(a, b) do {} while(0)
#define ASSERT_GT_INT(a, b) do {} while(0)

#define ASSERT_EQ_FLOAT(a, b) do {} while(0)
#define ASSERT_NE_FLOAT(a, b) do {} while(0)
#define ASSERT_LE_FLOAT(a, b) do {} while(0)
#define ASSERT_LT_FLOAT(a, b) do {} while(0)
#define ASSERT_GE_FLOAT(a, b) do {} while(0)
#define ASSERT_GT_FLOAT(a, b) do {} while(0)

#define ASSERT_EQ_ADDR(a, b) do {} while(0)
#define ASSERT_NE_ADDR(a, b) do {} while(0)
#define ASSERT_IS_NULL(a) do {} while(0)
#define ASSERT_NOT_NULL(a) do {} while(0)

#else   /* Not NDEBUG */

#define ASSERT_EQ_INT(a, b) __ASSERT_ROP_INT(a, b, ==) /* assert(a == b) */
#define ASSERT_NE_INT(a, b) __ASSERT_ROP_INT(a, b, !=) /* assert(a != b) */
#define ASSERT_LE_INT(a, b) __ASSERT_ROP_INT(a, b, <=) /* assert(a <= b) */
#define ASSERT_LT_INT(a, b) __ASSERT_ROP_INT(a, b, <)  /* assert(a < b)  */
#define ASSERT_GE_INT(a, b) __ASSERT_ROP_INT(a, b, >=) /* assert(a >= b) */
#define ASSERT_GT_INT(a, b) __ASSERT_ROP_INT(a, b, >)  /* assert(a > b)  */

#define ASSERT_EQ_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, ==) /* assert(a == b) */
#define ASSERT_NE_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, !=) /* assert(a != b) */
#define ASSERT_LE_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, <=) /* assert(a <= b) */
#define ASSERT_LT_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, <)  /* assert(a < b)  */
#define ASSERT_GE_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, >=) /* assert(a >= b) */
#define ASSERT_GT_FLOAT(a, b) __ASSERT_ROP_FLOAT(a, b, >)  /* assert(a > b)  */

#define ASSERT_EQ_ADDR(a, b) __ASSERT_ROP_ADDR(a, b, ==) /* assert(a == b) */
#define ASSERT_NE_ADDR(a, b) __ASSERT_ROP_ADDR(a, b, !=) /* assert(a != b) */
#define ASSERT_IS_NULL(a) __ASSERT_ROP_NULL(a, ==)       /* assert(a == NULL) */
#define ASSERT_NOT_NULL(a) __ASSERT_ROP_NULL(a, !=)      /* assert(a != NULL) */

#endif  /* NDEBUG */

#endif  /* _THRPOOL_ASSERT_H */
