#include "tommath_private.h"
#ifdef MP_WARRAY_FREE_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* static check that the multiplication won't overflow */
MP_STATIC_ASSERT(warray_free_sz_does_not_overflow, (sizeof(mp_word) * MP_WARRAY) >= MP_WARRAY)

static int s_warray_free(void)
{
   int ret = 0;
   if (s_mp_warray.w_used)
      return -2;
   if (s_mp_warray.w_free) {
      s_mp_zero_buf(s_mp_warray.w_free, sizeof(mp_word) * MP_WARRAY);
      MP_FREE(s_mp_warray.w_free, sizeof(mp_word) * MP_WARRAY);
      s_mp_warray.w_free = NULL;
   }
   return ret;
}

int mp_warray_free(void)
{
   if (MP_HAS(MP_SMALL_STACK_SIZE)) return s_warray_free();
   return -1;
}

#endif
