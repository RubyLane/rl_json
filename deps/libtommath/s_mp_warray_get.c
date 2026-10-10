#include "tommath_private.h"
#ifdef S_MP_WARRAY_GET_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

void *s_mp_warray_get(void)
{
   if (s_mp_warray.w_used)
      return NULL;
   if (s_mp_warray.w_free == NULL) {
      s_mp_warray.w_free = MP_CALLOC(MP_WARRAY, sizeof(mp_word));
   }
   s_mp_warray.w_used = s_mp_warray.w_free;
   s_mp_warray.w_free = NULL;
   return s_mp_warray.w_used;
}

#endif
