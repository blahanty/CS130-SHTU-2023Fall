#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* Define fixed-point type. */
typedef int fp;
/* Left shift 16 bits for fractional part. */
#define FP_SHIFT_BITS 16
/* Transfer integer A to fixed-point. */
#define INT_TO_FP(A) ((fp)(A << FP_SHIFT_BITS))
/* Transfer fixed-point A to integer by rounding down decimal places. */
#define FP_TO_INT(A) (A >> FP_SHIFT_BITS)
/* Add fixed-point A and B. */
#define FP_ADD(A, B) (A + B)
/* Add fixed-point A and integer B. */
#define FP_ADD_INT(A, B) (A + (B << FP_SHIFT_BITS))
/* Subtract fixed-point A from B. */
#define FP_SUB(A, B) (A - B)
/* Subtract integer B from fixed-point A. */
#define FP_SUB_INT(A, B) (A - (B << FP_SHIFT_BITS))
/* Multiply fixed-point A and B. */
#define FP_MUL(A, B) ((fp)(((int64_t)A) * B >> FP_SHIFT_BITS))
/* Multiply fixed-point A and integer B. */
#define FP_MUL_INT(A, B) (A * B)
/* Divide fixed-point A by B. */
#define FP_DIV(A, B) ((fp)((((int64_t)A) << FP_SHIFT_BITS) / B))
/* Divide fixed-point A by integer B. */
#define FP_DIV_INT(A, B) (A / B)
/* Round fixed-point A to integer. */
#define FP_ROUND(A) (A >= 0 ? ((A + (1 << (FP_SHIFT_BITS - 1))) >> FP_SHIFT_BITS) \
                            : ((A - (1 << (FP_SHIFT_BITS - 1))) >> FP_SHIFT_BITS))

#endif