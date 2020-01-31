#ifndef _DEDUP_H
#define _DEDUP_H

#if DEDUP

#define KC_ENTRIES		384		// Must be an integer multiple of 8*sizeof(long long)

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

#define STRING_DEDUP_MAX	16

void free_cache(struct interp_cx* l);
Tcl_Obj* new_stringobj_dedup(struct interp_cx* l, const char* bytes, int length);
#	define get_string(l, bytes, length)	new_stringobj_dedup(l, bytes, length)
#else
#	define free_cache(l)	// nop
//#	define new_stringobj_dedup(l, bytes, length)	Tcl_NewStringObj(bytes, length)
#	define get_string(l, bytes, length) Tcl_NewStringObj(bytes, length)
#endif


#endif
