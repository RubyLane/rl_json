#include "tommath_private.h"
#ifdef S_MP_WARRAY_PUT_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

void s_mp_warray_put(void *w)
{
   if (s_mp_warray.w_free || s_mp_warray.w_used != w)
      return;
   s_mp_warray.w_free = w;
   s_mp_warray.w_used = NULL;
}

#endif
