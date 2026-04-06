/*
 * bignum_ops.h -- Thin interface for libtommath operations that must be
 * compiled separately from Tcl headers.
 *
 * Tcl 8.6's tcl.h forward-declares mp_int/mp_digit with types that conflict
 * with standalone libtommath 1.3.0 at the C type level (anonymous vs named
 * struct tag).  The struct layouts are binary-identical when compiled with
 * matching digit width (MP_32BIT for Tcl 8.6).
 *
 * bignum_ops.c includes tommath.h (but NOT tcl.h) and provides helpers.
 * Callers include this header after tcl.h — both sides see mp_int as a
 * typedef, and the pointer is valid across the boundary.
 *
 * Requires: mp_int must be defined before including this header
 *           (via either <tcl.h> or <tommath.h>).
 */

#ifndef BIGNUM_OPS_H
#define BIGNUM_OPS_H

#include <stdint.h>
#include <stddef.h>

/* Returns 0 (MP_OKAY) on success, negative on error.
 * Caller must eventually call bignum_clear() on success. */

int bignum_from_uint64(mp_int *mp, uint64_t val);
int bignum_from_nint64(mp_int *mp, uint64_t val);  /* result: -1 - val */
int bignum_from_ubin(mp_int *mp, const uint8_t *bytes, size_t len);
int bignum_from_nbin(mp_int *mp, const uint8_t *bytes, size_t len);  /* result: -1 - n */

int bignum_cmp(const mp_int *a, const mp_int *b);

void bignum_clear(mp_int *mp);

#endif /* BIGNUM_OPS_H */
