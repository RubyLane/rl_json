#ifndef _DEDUP_H
#define _DEDUP_H

#if DEDUP

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
