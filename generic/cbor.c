#include "rl_jsonInt.h"

/*
 * Renamed enum values, see:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/hresult-values
 */
enum svalue_types {
    CBOR_FALSE = 20,
    CBOR_TRUE  = 21,
    CBOR_NULL  = 22,
    CBOR_UNDEF = 23
};

enum indexmode {
	IDX_ABS,
	IDX_ENDREL
};

#define CBOR_TRUNCATED(l, c) \
	do { \
		Tcl_SetErrorCode(interp, "CBOR", "TRUNCATED", NULL); \
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("CBOR value truncated")); \
		c = TCL_ERROR; \
		goto l; \
	} while(0);

#define CBOR_INVALID(l, c, fmtstr, ...) \
	do { \
		Tcl_SetErrorCode(interp, "CBOR", "INVALID", NULL); \
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("CBOR syntax error: " fmtstr, ##__VA_ARGS__)); \
		c = TCL_ERROR; \
		goto l; \
	} while(0);

#define CBOR_TRAILING(l, c) \
	do { \
		Tcl_SetErrorCode(interp, "CBOR", "TRAILING", NULL); \
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Excess bytes after CBOR value")); \
		c = TCL_ERROR; \
		goto l; \
	} while(0);


#define CIRCULAR_STATIC_SLOTS	20
static const uint8_t*	g_circular_buf[CIRCULAR_STATIC_SLOTS*sizeof(uint8_t*)];
static int cbor_matches(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, Tcl_Obj* pathElem, int* matchesPtr);

static float decode_float(const uint8_t* p) { //{{{
	float		val;
	uint32_t	uval = be32toh(*(uint32_t*)p);
	memcpy(&val, &uval, sizeof(float));
	return val;
}

//}}}
static double decode_double(const uint8_t* p) { //{{{
	double		val;
	uint64_t	uval = be64Itoh(*(uint64_t*)p);
	memcpy(&val, &uval, sizeof(double));
	return val;
}

//}}}

// Private API {{{
// From RFC8949 {{{
static int well_formed_indefinite(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, int breakable, uint8_t* mtPtr, uint8_t mt);

static double decode_half(const uint8_t* halfp) { //{{{
	uint32_t	half = (halfp[0] << 8) + halfp[1];
	uint32_t	exp = (half >> 10) & 0x1f;
	uint32_t	mant = half & 0x3ff;
	double		val;

	if (exp == 0)		val = ldexp(mant, -24);
	else if (exp != 31)	val = ldexp(mant + 1024, exp - 25);
	else				val = mant == 0 ? INFINITY : NAN;

	return half & 0x8000 ? -val : val;
}

//}}}
static int take(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* end, int len, const uint8_t** partPtr) //{{{
{
	int				code = TCL_OK;
	const uint8_t*	p = *pPtr;

	if (p + len > end) CBOR_TRUNCATED(finally, code);

	*partPtr = p;
	*pPtr = p + len;

finally:
	return code;
}

//}}}
static int well_formed(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, int breakable, uint8_t* mtPtr) //{{{
{
	int				code = TCL_OK;
	const uint8_t*	part = NULL;

	// process initial bytes
	TEST_OK_LABEL(finally, code, take(interp, pPtr, e, 1, &part));
	uint8_t		ib = *(uint8_t*)part;
	uint8_t		mt = ib >> 5;
	uint8_t		ai;
	uint64_t	val;

	val = ai = ib & 0x1f;

	switch (ai) {
		case 24: TEST_OK_LABEL(finally, code, take(interp, pPtr, e, 1, &part)); val =          *(uint8_t*)part;  break;
		case 25: TEST_OK_LABEL(finally, code, take(interp, pPtr, e, 2, &part)); val = be16toh(*(uint16_t*)part); break;
		case 26: TEST_OK_LABEL(finally, code, take(interp, pPtr, e, 4, &part)); val = be32toh(*(uint32_t*)part); break;
		case 27: TEST_OK_LABEL(finally, code, take(interp, pPtr, e, 8, &part)); val = be64Itoh(*(uint64_t*)part); break;
		case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", ai);
		case 31: return well_formed_indefinite(interp, pPtr, e, breakable, mtPtr, mt);
	}
	// process content
	switch (mt) {
		// case 0, 1, 7 do not have content; just use val
		case 2: case 3: TEST_OK_LABEL(finally, code, take(interp, pPtr, e, val, &part)); break; // bytes/UTF-8
		case 4: for (int i=0; i<val;   i++) TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 0, mtPtr)); break;
		case 5: for (int i=0; i<val*2; i++) TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 0, mtPtr)); break;
		case 6: TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 0, mtPtr)); break;     // 1 embedded data item
		case 7: if (ai == 24 && val < 32) CBOR_INVALID(finally, code, "bad simple value: %" PRIu64, val);
	}
	if (mtPtr) *mtPtr = mt; // definite-length data item

finally:
	return code;
}

//}}}
static int well_formed_indefinite(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, int breakable, uint8_t* mtPtr, uint8_t mt) //{{{
{
	int		code = TCL_OK;
	uint8_t	it;

	switch (mt) {
		case 2:
		case 3:
			for (;;) {
				TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 1, &it));
				if (it == -1) break;
				// need definite-length chunk of the same type
				if (it != mt) CBOR_INVALID(finally, code, "indefinite-length chunk type: %d doesn't match parent: %d", it, mt);
			}
			break;

		case 4:
			for (;;) {
				TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 1, &it));
				if (it == -1) break;
			}
			break;

		case 5:
			for (;;) {
				TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 1, &it));
				if (it == -1) break;
			}
			TEST_OK_LABEL(finally, code, well_formed(interp, pPtr, e, 0, &it));
			break;

		case 7:
			if (breakable) {
				if (mtPtr) *mtPtr = -1;	// signal break out
				goto finally;
			}
			CBOR_INVALID(finally, code, "break outside indefinite-length data item");

		default:
			CBOR_INVALID(finally, code, "indefinite-length data item with major type: %d", mt);
	}
	if (mtPtr) *mtPtr = 99;	// indefinite-length data item

finally:
	return code;
}

//}}}
// From RFC8948 }}}

static int parse_index(Tcl_Interp* interp, Tcl_Obj* obj, enum indexmode* mode, ssize_t* ofs) //{{{
{
	int				code = TCL_OK;
	const char*		str = Tcl_GetString(obj);
	char*			end;

	if (
		str[0] == 'e' &&
		str[1] == 'n' &&
		str[2] == 'd' &&
		str[3] == '-'
	) {
		const long long val = strtoll(str+4, &end, 10) * -1;
		if (end[0] != 0) THROW_ERROR_LABEL(finally, code, "Invalid index");

		*mode = IDX_ENDREL;
		*ofs = val;
	} else {
		const long long val = strtoll(str, &end, 10);
		if (end[0] != 0) THROW_ERROR_LABEL(finally, code, "Invalid index");
		*mode = IDX_ABS;
		*ofs = val;
	}

finally:
	return code;
}

//}}}
static int utf8_codepoint(Tcl_Interp* interp, const uint8_t** p, const uint8_t* e, uint32_t* codepointPtr) //{{{
{
	int				code = TCL_OK;
	const uint8_t*	c = *p;
	uint32_t		codepoint = 0;
	uint32_t		composed = 0;

part:
	if ((*c & 0x80) == 0) {
		if (c >= e) CBOR_TRUNCATED(finally, code);
		codepoint = *c++;
	} else if ((*c & 0xE0) == 0xC0) { // Two byte encoding
		if (c+1 >= e) CBOR_TRUNCATED(finally, code);
		codepoint  = (*c++ & 0x1F) << 6;
		codepoint |= (*c++ & 0x3F);
	} else if ((*c & 0xF0) == 0xE0) { // Three byte encoding
		if (c+2 >= e) CBOR_TRUNCATED(finally, code);
		codepoint  = (*c++ & 0x0F) << 12;
		codepoint |= (*c++ & 0x3F) << 6;
		codepoint |= (*c++ & 0x3F);
	} else if ((*c & 0xF8) == 0xF0) { // Four byte encoding
		if (c+3 >= e) CBOR_TRUNCATED(finally, code);
		codepoint  = (*c++ & 0x07) << 18;
		codepoint |= (*c++ & 0x3F) << 12;
		codepoint |= (*c++ & 0x3F) << 6;
		codepoint |= (*c++ & 0x3F);
	} else if ((*c & 0xFC) == 0xF8) { // Five byte encoding
		if (c+4 >= e) CBOR_TRUNCATED(finally, code);
		codepoint  = (*c++ & 0x03) << 24;
		codepoint |= (*c++ & 0x3F) << 18;
		codepoint |= (*c++ & 0x3F) << 12;
		codepoint |= (*c++ & 0x3F) << 6;
		codepoint |= (*c++ & 0x3F);
	} else if ((*c & 0xFE) == 0xFC) { // Six byte encoding
		if (c+5 >= e) CBOR_TRUNCATED(finally, code);
		codepoint  = (*c++ & 0x01) << 30;
		codepoint |= (*c++ & 0x3F) << 24;
		codepoint |= (*c++ & 0x3F) << 18;
		codepoint |= (*c++ & 0x3F) << 12;
		codepoint |= (*c++ & 0x3F) << 6;
		codepoint |= (*c++ & 0x3F);
	} else {	// Invalid encoding
		CBOR_INVALID(finally, code, "Invalid UTF-8 encoding");
	}

	if (composed) {
		if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) { // Low surrogate
			codepoint = composed + (codepoint - 0xDC00);
		} else {
			CBOR_INVALID(finally, code, "UTF-8 low surrogate missing");
		}
	} else {
		if (codepoint >= 0xD800 && codepoint <= 0xDBFF) { // High surrogate
			composed = 0x10000 + ((codepoint - 0xD800) << 16);
			goto part;
		} else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) { // Orphan low surrogate
			CBOR_INVALID(finally, code, "UTF-8 orphan low surrogate");
		}
	}

	*p = c;
	*codepointPtr = codepoint;

finally:
	return code;
}

//}}}
static Tcl_Obj* new_tcl_uint64(uint64_t val) //<<<
{
	Tcl_Obj*	res = NULL;

	if (val <= INT64_MAX) {
		// Tcl_WideInt is signed, so we can't use it for values >= 2^63
		res = Tcl_NewWideIntObj(val);
	} else {
		int		rc;
		mp_int	n = {0};
		const int	digits = (sizeof(uint64_t)*CHAR_BIT + MP_DIGIT_BIT-1) / (MP_DIGIT_BIT);	// Need at least 64 bits to represent val

		//if ((rc = mp_init_size(&n, digits)) != MP_OKAY) Tcl_Panic("mp_init_size failed: %s", mp_error_to_string(rc));
		if ((rc = mp_init_size(&n, digits)) != MP_OKAY) Tcl_Panic("mp_init_size failed");
		n.sign = MP_ZPOS;
		n.used = digits;
		for (int i=0; i<digits; i++) n.dp[i] = (val >> (i*MP_DIGIT_BIT)) & MP_MASK;
		res = Tcl_NewBignumObj(&n);
		mp_clear(&n);
	}

	return res;
}

//}}}
static Tcl_Obj* new_tcl_nint64(uint64_t val) //<<<
{
	Tcl_Obj*	res = NULL;

	if (val < INT64_MIN) {
		res = Tcl_NewWideIntObj(-1-val);
	} else {
		int		rc;
		mp_int	n = {0};
		mp_int	minusone = {0};
		const int	digits = (sizeof(uint64_t)*CHAR_BIT + MP_DIGIT_BIT-1) / (MP_DIGIT_BIT);	// Need at least 64 bits to represent val

		//if ((rc = mp_init_size(&n, digits)) != MP_OKAY) Tcl_Panic("mp_init_size failed: %s", mp_error_to_string(rc));
		//if ((rc = mp_init(&minusone)) != MP_OKAY) Tcl_Panic("mp_init failed: %s", mp_error_to_string(rc));
		if ((rc = mp_init_size(&n, digits)) != MP_OKAY) Tcl_Panic("mp_init_size failed");
		if ((rc = mp_init(&minusone)) != MP_OKAY) Tcl_Panic("mp_init failed");

		n.sign = MP_NEG;
		n.used = digits;
		for (int i=0; i<digits; i++) n.dp[i] = (val >> (i*MP_DIGIT_BIT)) & MP_MASK;

		mp_set(&minusone, -1);

		//if ((rc = mp_add(&n, &minusone, &n)) != MP_OKAY) Tcl_Panic("mp_add failed: %s", mp_error_to_string(rc));
		if ((rc = mp_add(&n, &minusone, &n)) != MP_OKAY) Tcl_Panic("mp_add failed");

		res = Tcl_NewBignumObj(&n);
		mp_clear(&n);
		mp_clear(&minusone);
	}

	return res;
}

//}}}
static int cbor_get_obj(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, Tcl_Obj** resPtr, Tcl_Obj** tagsPtr) //{{{
{
	int					code = TCL_OK;
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	const uint8_t*		p = *pPtr;
	Tcl_Obj*			res = NULL;
	Tcl_Obj*			tmp1 = NULL;
	Tcl_Obj*			tmp2 = NULL;

	const uint8_t*	valPtr;

	#define TAKE(n) do { \
		const size_t	nb = n; \
		if (e - p < nb) CBOR_TRUNCATED(finally, code); \
		valPtr = p; \
		p += nb; \
	} while (0)

read_dataitem:
	TAKE(1);
	const uint8_t		ib = valPtr[0];
	const enum cbor_mt	mt = ib >> 5;
	const uint8_t		ai = ib & 0x1f;
	uint64_t			val = ai;

	switch (ai) {
		case 24: TAKE(1); val = *(uint8_t*)valPtr;           break;
		case 25: TAKE(2); val = be16toh(*(uint16_t*)valPtr); break;
		case 26: TAKE(4); val = be32toh(*(uint32_t*)valPtr); break;
		case 27: TAKE(8); val = be64Itoh(*(uint64_t*)valPtr); break;
		case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", ai);
	}

	if (ai == 31) {
		switch (mt) {
			case M_UINT:
			case M_NINT:
			case M_TAG:
			case M_REAL:
				CBOR_INVALID(finally, code, "invalid indefinite length for major type %d", mt);

			case M_BSTR:
			case M_UTF8:
			case M_ARR:
			case M_MAP:
				break;
		}
	}

	switch (mt) {
		case M_TAG:
			if (tagsPtr)
				TEST_OK_LABEL(finally, code, Tcl_ListObjAppendElement(interp, *tagsPtr, Tcl_NewWideIntObj(val)));
			goto read_dataitem;

		case M_UINT:	replace_tclobj(&res, new_tcl_uint64(val));	break;
		case M_NINT:	replace_tclobj(&res, new_tcl_nint64(val));	break;

		case M_BSTR:
		case M_UTF8:
		{
			if (ai == 31) {
				size_t	ofs = 0;
				replace_tclobj(&res,
					mt == M_UTF8 ?
						Tcl_NewObj() :
						Tcl_NewByteArrayObj(NULL, 0));

				for (;;) {
					TAKE(1);
					if (valPtr[0] == 0xFF) {break;}
					const enum cbor_mt	chunk_mt = valPtr[0] >> 5;
					if (chunk_mt != mt) CBOR_INVALID(finally, code, "String chunk type: %d doesn't match parent: %d", chunk_mt, mt);
					const uint8_t		chunk_ai = valPtr[0] & 0x1f;
					uint64_t			chunk_val;

					switch (chunk_ai) {
					    //case 0 ... 23:
						case 0: case 1: case 2: case 3: case 4:
						case 5: case 6: case 7: case 8: case 9:
						case 10: case 11: case 12: case 13: case 14:
						case 15: case 16: case 17: case 18: case 19:
						case 20: case 21: case 22: case 23:
						  chunk_val = chunk_ai; break;
						case 24: TAKE(1); chunk_val = *(uint8_t*)valPtr;           break;
						case 25: TAKE(2); chunk_val = be16toh(*(uint16_t*)valPtr); break;
						case 26: TAKE(4); chunk_val = be32toh(*(uint32_t*)valPtr); break;
						case 27: TAKE(8); chunk_val = be64Itoh(*(uint64_t*)valPtr); break;
						default: CBOR_INVALID(finally, code, "invalid chunk additional info: %d", chunk_ai);
					}
					TAKE(chunk_val);

					if (chunk_mt == M_UTF8) {
						Tcl_AppendToObj(res, (const char*)valPtr, chunk_val);
					} else {
						uint8_t*	base = Tcl_SetByteArrayLength(res, ofs + chunk_val);
						memcpy(base + ofs, valPtr, chunk_val);
						ofs += chunk_val;
					}
				}
			} else {
				TAKE(val);
				replace_tclobj(&res,
					mt == M_UTF8 ?
						Tcl_NewStringObj((const char*)valPtr, val) :
						Tcl_NewByteArrayObj(valPtr, val));
			}
			break;
		}

		case M_ARR:
		{
			replace_tclobj(&res, Tcl_NewListObj(0, NULL));

			for (size_t i=0; ; i++) {
				if (ai == 31) {
					if (p >= e) CBOR_TRUNCATED(finally, code);
					if (*p == 0xFF) {p++; break;}
				} else {
					if (i >= val) break;
				}

				TEST_OK_LABEL(finally, code, cbor_get_obj(interp, &p, e, &tmp1, NULL));
				TEST_OK_LABEL(finally, code, Tcl_ListObjAppendElement(interp, res, tmp1));
			}
			break;
		}

		case M_MAP:
		{
			replace_tclobj(&res, Tcl_NewDictObj());
			for (size_t i=0; ; i++) {
				if (ai == 31) {
					if (p >= e) CBOR_TRUNCATED(finally, code);
					if (*p == 0xFF) {p++; break;}
				} else {
					if (i >= val) break;
				}

				TEST_OK_LABEL(finally, code, cbor_get_obj(interp, &p, e, &tmp1, NULL));
				TEST_OK_LABEL(finally, code, cbor_get_obj(interp, &p, e, &tmp2, NULL));
				TEST_OK_LABEL(finally, code, Tcl_DictObjPut(interp, res, tmp1, tmp2));
			}
			break;
		}

		case M_REAL:
		{
			switch (ai) {
			    //case 0 ... 19:
			  case 0: case 1: case 2: case 3: case 4:
			  case 5: case 6: case 7: case 8: case 9:
			  case 10: case 11: case 12: case 13: case 14:
			  case 15: case 16: case 17: case 18: case 19:
			    replace_tclobj(&res, Tcl_ObjPrintf("simple(%" PRIu64 ")", val));	break;
				case CBOR_FALSE:				replace_tclobj(&res, l->cbor_false);		break;
				case CBOR_TRUE:				replace_tclobj(&res, l->cbor_true);			break;
				case CBOR_NULL:				replace_tclobj(&res, l->cbor_null);			break;
				case CBOR_UNDEF:				replace_tclobj(&res, l->cbor_undefined);	break;
				case 24:	
#if 1
				  if (val >= 32 && val <= 255) {
				      replace_tclobj(&res, Tcl_ObjPrintf("simple(%" PRIu64 ")", val));
				  } else {
				      CBOR_INVALID(finally, code, "invalid simple value: %" PRIu64, val);
				  }
#else
					switch (val) {
						case 32 ... 255:	replace_tclobj(&res, Tcl_ObjPrintf("simple(%" PRIu64 ")", val));	break;
					  default:			CBOR_INVALID(finally, code, "invalid simple value: %" PRIu64, val);
					}
#endif
					break;
				case 25:	replace_tclobj(&res, Tcl_NewDoubleObj( decode_half(valPtr)   ));	break;
				case 26:	replace_tclobj(&res, Tcl_NewDoubleObj( decode_float(valPtr)  ));	break;
				case 27:	replace_tclobj(&res, Tcl_NewDoubleObj( decode_double(valPtr) ));	break;
				default:	CBOR_INVALID(finally, code, "invalid additional info: %d", ai);
			}
			break;
		}
	}

	replace_tclobj(resPtr, res);

finally:
	#undef TAKE
	replace_tclobj(&res, NULL);
	replace_tclobj(&tmp1, NULL);
	replace_tclobj(&tmp2, NULL);
	*pPtr = p;
	return code;
}

//}}}
static int cbor_match_map(Tcl_Interp* interp, uint8_t ai, uint64_t val, const uint8_t** pPtr, const uint8_t* e, Tcl_Obj* pathElem, int* matchesPtr) //{{{
{
	int				code = TCL_OK;
	Tcl_Obj*		tmp = NULL;
	Tcl_Obj*		key = NULL;
	Tcl_Obj*		cbor_val = NULL;
	Tcl_HashTable	remaining;
	const uint8_t*	p = *pPtr;
	int				size;
	int				skipping = 0;

	Tcl_InitHashTable(&remaining, TCL_ONE_WORD_KEYS);

	if (TCL_OK == Tcl_DictObjSize(NULL, pathElem, &size)) {
		replace_tclobj(&tmp, pathElem);	// Take a private copy to remove entries as we match them
	} else {
		// Not a dict, so we can't match (but it's not an error)
		skipping = 1;
	}

	if (!skipping && ai != 31 && val != size)
		skipping = 1;	// If there are different numbers of entries, we can't match

	for (size_t i=0; ; i++) {
		if (ai == 31) {
			if (p >= e) CBOR_TRUNCATED(finally, code);
			if (*p == 0xFF) {p++; break;}
		} else {
			if (i >= val) break;
		}

		if (skipping) {
			TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
			TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
			continue;
		}

		const uint8_t		key_mt = *p >> 5;
		switch (key_mt) {
			case M_TAG: continue;

			case M_UINT:
			case M_NINT:
			case M_BSTR:
			case M_UTF8:
			case M_REAL:
			{
				Tcl_Obj*	valobj = NULL;	// On loan from the dict
				int			matches;
				TEST_OK_LABEL(finally, code, cbor_get_obj(interp, &p, e, &key, NULL));
				TEST_OK_LABEL(finally, code, Tcl_DictObjGet(interp, tmp, key, &valobj));
				if (valobj == NULL) {
					skipping = 1;
					TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
					continue;
				}
				TEST_OK_LABEL(finally, code, cbor_matches(interp, &p, e, valobj, &matches));
				if (matches) {
					TEST_OK_LABEL(finally, code, Tcl_DictObjRemove(interp, tmp, key));
					valobj = NULL;
					size--;
				} else {
					skipping = 1;
				}
				continue;
			}

			case M_ARR:
			case M_MAP:
			{
				// Defer dealing with these until there is nothing else that could cause a mismatch
				Tcl_HashEntry*	he = NULL;
				Tcl_CreateHashEntry(&remaining, i, NULL);
				Tcl_SetHashValue(he, (void*)p);
				TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
				break;
			}
		}
	}

	if (skipping) goto mismatch;

	// Having knocked out the easier matches, try to match up the compound types
	Tcl_HashSearch	search;
	for (Tcl_HashEntry* he=Tcl_FirstHashEntry(&remaining, &search); he; he=Tcl_NextHashEntry(&search)) {
		if (size == 0) goto mismatch; // No more entries in the path, so we can't match

		const uint8_t*	rp = (const uint8_t*)Tcl_GetHashValue(he);
		Tcl_DictSearch	search;
		int				done;
		Tcl_Obj*		k = NULL;	// On loan from the dict
		Tcl_Obj*		v = NULL;	// On loan from the dict
		int				matches = 0;

		TEST_OK_LABEL(finally, code, Tcl_DictObjFirst(interp, tmp, &search, &k, &v, &done));
		for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
			const uint8_t*	rpc = rp;
			TEST_OK_LABEL(finally, code, cbor_matches(interp, &rpc, e, k, &matches));
			if (matches) {
				TEST_OK_LABEL(finally, code, cbor_matches(interp, &rpc, e, v, &matches));
				if (matches) {
					TEST_OK_LABEL(finally, code, Tcl_DictObjRemove(interp, tmp, k));
					k = NULL;
					v = NULL;
					size--;
					break;
				}
			} else {
				TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
			}
		}
		Tcl_DictObjDone(&search);
		if (!matches) goto mismatch;	// No matching entry found in dict for remaining entry in map
	}

	if (size > 0) goto mismatch;	// Unmatched keys remain in dict

	*matchesPtr = 1;

finally:
	*pPtr = p;
	replace_tclobj(&tmp, NULL);
	replace_tclobj(&key, NULL);
	replace_tclobj(&cbor_val, NULL);
	Tcl_DeleteHashTable(&remaining);
	return code;

mismatch:
	*matchesPtr = 0;
	goto finally;
}

//}}}
static int cbor_matches(Tcl_Interp* interp, const uint8_t** pPtr, const uint8_t* e, Tcl_Obj* pathElem, int* matchesPtr) //{{{
{
	int						code = TCL_OK;
	const uint8_t*			p = *pPtr;
	int						matches = 0;
	//Tcl_DString			tags;

	//Tcl_DStringInit(&tags);

	const uint8_t*	valPtr;

	#define TAKE(n) do { \
		const size_t	nb = n; \
		if (e - p < nb) CBOR_TRUNCATED(finally, code); \
		valPtr = p; \
		p += nb; \
	} while (0)

data_item: // loop: read off tags
	// Read head {{{
	TAKE(1);
	const uint8_t		ib = valPtr[0];
	const enum cbor_mt	mt = ib >> 5;
	const uint8_t		ai = ib & 0x1f;
	uint64_t			val = ai;

	switch (ai) {
		case 24: TAKE(1); val = *(uint8_t*)valPtr;           break;
		case 25: TAKE(2); val = be16toh(*(uint16_t*)valPtr); break;
		case 26: TAKE(4); val = be32toh(*(uint32_t*)valPtr); break;
		case 27: TAKE(8); val = be64Itoh(*(uint64_t*)valPtr); break;
		case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", ai);
	}
	//}}}

	if (ai == 31) {
		switch (mt) {
			case M_UINT:
			case M_NINT:
			case M_TAG:
			case M_REAL:
				CBOR_INVALID(finally, code, "invalid indefinite length for major type %d", mt);

			case M_BSTR:
			case M_UTF8:
			case M_ARR:
			case M_MAP:
				break;
		}
	}

	switch (mt) {
		//case M_TAG:	Tcl_DStringAppend(&tags, (uint64_t*)&val, sizeof(uint64_t*)); goto data_item;
		case M_TAG:	goto data_item;	// Skip tags
		case M_UINT: case M_NINT: // Compare as integers {{{
		{
			// TODO: fix range issues: if val is in the range [-2^63, 2^63-1] then Tcl_WideInt is fine, otherwise we need to use bignums
			Tcl_WideInt		pathval;
			TEST_OK_LABEL(finally, code, Tcl_GetWideIntFromObj(interp, pathElem, &pathval));
			if (pathval == (mt == M_UINT ? val : -1-val)) goto matches;
			break;
		}
		//}}}
		case M_BSTR: // Compare as byte strings {{{
		{
			size_t				pathlen;
			const uint8_t*		pathval = (const uint8_t*)Tcl_GetBytesFromObj(interp, pathElem, &pathlen);
			const uint8_t*const	pathend = pathval + pathlen;
			const uint8_t*const	pe = p + val;

			if (ai == 31) { // Indefinite length bytes: comprised of a sequence of definite-length byte dataitems {{{
				int skipping = 0;

				for (;;) {
					const uint8_t		chunk_ib = *p++;
					const enum cbor_mt	chunk_mt = chunk_ib >> 5;
					const uint8_t		chunk_ai = chunk_ib & 0x1f;
					uint64_t			chunk_val = ai;

					if (chunk_ib == 0xFF) break;
					if (chunk_mt != M_BSTR) CBOR_INVALID(finally, code, "wrong type for binary chunk: %d", chunk_mt);

					switch (chunk_ai) {
						case 24: TAKE(1); chunk_val = *(uint8_t*)valPtr;           break;
						case 25: TAKE(2); chunk_val = be16toh(*(uint16_t*)valPtr); break;
						case 26: TAKE(4); chunk_val = be32toh(*(uint32_t*)valPtr); break;
						case 27: TAKE(8); chunk_val = be64Itoh(*(uint64_t*)valPtr); break;
						case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", chunk_ai);
						case 31: CBOR_INVALID(finally, code, "cannot nest indefinite length chunks");
					}

					const uint8_t*	chunk_pe = p + chunk_val;

					if (skipping) {
						p = chunk_pe;
					} else {
						for (; pathval < pathend && p < chunk_pe;) {
							if (*pathval++ != *p++) {
								p = pe;
								skipping = 1;
								break;
							}
						}
					}
				}
				if (!skipping && (pathval == pathend && p == pe)) goto matches;
				goto mismatch;
				//}}}
			} else { // Definite length bytes {{{
				for (; pathval < pathend && p < pe;) {
					if (*pathval++ != *p++) {
						p = pe;
						goto mismatch;
					}
				}
				if (pathval == pathend && p == pe) goto matches;
				goto mismatch;
				//}}}
			}
			break;
		}
		//}}}
		case M_UTF8: // Compare as UTF-8 strings {{{
		{
			int					s_pathlen;
			const uint8_t*		s_pathval = (const uint8_t*)Tcl_GetStringFromObj(pathElem, &s_pathlen);
			const uint8_t*const	s_pathend = s_pathval + s_pathlen;
			const uint8_t*const	s_pe = p + val;

			if (ai == 31) { // Indefinite length UTF-8 string: comprised of a sequence of definite-length utf-8 dataitems {{{
				int skipping = 0;

				for (;;) {
					const uint8_t		chunk_ib = *p++;
					const enum cbor_mt	chunk_mt = chunk_ib >> 5;
					const uint8_t		chunk_ai = chunk_ib & 0x1f;
					uint64_t			chunk_val = ai;

					if (chunk_ib == 0xFF) break;
					if (chunk_mt != M_UTF8) CBOR_INVALID(finally, code, "wrong type for UTF-8 chunk: %d", chunk_mt);

					switch (chunk_ai) {
						case 24: TAKE(1); chunk_val = *(uint8_t*)valPtr;           break;
						case 25: TAKE(2); chunk_val = be16toh(*(uint16_t*)valPtr); break;
						case 26: TAKE(4); chunk_val = be32toh(*(uint32_t*)valPtr); break;
						case 27: TAKE(8); chunk_val = be64Itoh(*(uint64_t*)valPtr); break;
						case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", chunk_ai);
						case 31: CBOR_INVALID(finally, code, "cannot nest indefinite length chunks");
					}

					const uint8_t*	c_pe = p + chunk_val;

					if (skipping) {
						p = c_pe;
					} else {
						for (; s_pathval < s_pathend && p < c_pe;) {
							uint32_t	path_c;
							uint32_t	di_c;

							TEST_OK_LABEL(finally, code, utf8_codepoint(interp, &s_pathval, s_pathend, &path_c));
							TEST_OK_LABEL(finally, code, utf8_codepoint(interp, &p,         c_pe,      &di_c));
							if (path_c != di_c) {
								p = c_pe;
								skipping = 1;
								break;
							}
						}
					}
				}
				if (!skipping && (s_pathval == s_pathend && p == s_pe)) goto matches;
				goto mismatch;
				//}}}
			} else { // Definite length UTF-8 string {{{
				for (; s_pathval < s_pathend && p < s_pe;) {
					uint32_t	path_c;
					uint32_t	di_c;

					TEST_OK_LABEL(finally, code, utf8_codepoint(interp, &s_pathval, s_pathend, &path_c));
					TEST_OK_LABEL(finally, code, utf8_codepoint(interp, &p,       s_pe,      &di_c));
					if (path_c != di_c) {
						p = s_pe;
						goto mismatch;
					}
				}
				if (s_pathval == s_pathend && p == s_pe) goto matches;
				goto mismatch;
				//}}}
			}
			break;
		}
		//}}}
		case M_ARR:  // Compare as a list {{{
		{
			int			oc;
			Tcl_Obj**	ov;
			if (TCL_OK != Tcl_ListObjGetElements(NULL, pathElem, &oc, &ov)) {
				// Skip remaining elements {{{
				for (;;) {
					uint8_t		elem_mt;
					TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, &elem_mt));
					if (elem_mt == -1) break;
				}
				//}}}
				goto mismatch;
			}
			if (ai == 31) { // Indefinite length array {{{
				for (int i=0; i<oc; i++) {
					if (*p == 0xFF) { // End of indefinite length array before end of pathelem
						p++;
						goto mismatch;
					}
					TEST_OK_LABEL(finally, code, cbor_matches(interp, &p, e, ov[i], &matches));
					if (!matches) {
						// Skip remaining elements {{{
						for (;;) {
							uint8_t		elem_mt;
							TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, &elem_mt));
							if (elem_mt == -1) break;
						}
						//}}}
						goto mismatch;
					}
				}
				if (*p != 0xFF) { // End of pathelem before end of indefinite length array
					// Skip remaining elements {{{
					for (;;) {
						uint8_t		elem_mt;
						TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, &elem_mt));
						if (elem_mt == -1) break;
					}
					//}}}
					goto mismatch;
				}
				goto matches;
				//}}}
			} else { // Definite length array {{{
				int		skipping = oc != val;
				for (int i=0; i<val; i++) {
					if (skipping) {
						TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
					} else {
						TEST_OK_LABEL(finally, code, cbor_matches(interp, &p, e, ov[i], &matches));
						if (!matches) {
							skipping = 1;
							continue;
						}
					}
				}
				if (!skipping) goto matches;
				//}}}
			}
			goto mismatch;
		}
		//}}}
		case M_MAP:  // Compare as dictionary {{{
		{
			TEST_OK_LABEL(finally, code, cbor_match_map(interp, ai, val, &p, e, pathElem, &matches));
			if (matches) goto matches;
			goto mismatch;
		}
		//}}}
		case M_REAL: // Compare as real values / simple values {{{
		{
			double rvalue;

			switch (ai) {
				case 20: case 21:	// Simple value: false / true
				{
					int boolval;
					TEST_OK_LABEL(finally, code, Tcl_GetBooleanFromObj(interp, pathElem, &boolval));
					if (boolval == (ai == 21)) goto matches;
					goto mismatch;
				}
				case 22: case 23:	// Simple value: null / undefined - treat zero length string as matching
				{
					int len;
					Tcl_GetStringFromObj(pathElem, &len);
					if (len == 0) goto matches;
					goto mismatch;
				}
				case 25:	rvalue = decode_half(valPtr);	break;
				case 26:	rvalue = decode_float(valPtr);	break;
				case 27:	rvalue = decode_double(valPtr);	break;
				default:	CBOR_INVALID(finally, code, "invalid additional info: %d", ai);
			}

			double		pathval;
			TEST_OK_LABEL(finally, code, Tcl_GetDoubleFromObj(interp, pathElem, &pathval));
			if (pathval == rvalue) goto matches;
			goto mismatch;
		}
		//}}}
	}

finally:
	*pPtr = p;
	#undef TAKE
	//Tcl_DStringFree(&tags);
	return code;

matches:
	*matchesPtr = 1;
	goto finally;
mismatch:
	*matchesPtr = 0;
	goto finally;
}

//}}}
// Private API }}}
// Stubs API {{{
int CBOR_GetDataItemFromPath(Tcl_Interp* interp, Tcl_Obj* cborObj, Tcl_Obj* pathObj, const uint8_t** dataitemPtr, const uint8_t** ePtr, Tcl_DString* tagsPtr) //{{{
{
	int						code = TCL_OK;
	Tcl_Obj**				pathv = NULL;
	int						pathc = 0;
	const uint8_t*			p = NULL;
	size_t					byteslen = 0;
	const uint8_t*			bytes = NULL;
	const uint8_t**			circular = g_circular_buf;

	bytes = Tcl_GetBytesFromObj(interp, cborObj, &byteslen);
	if (bytes == NULL) {code = TCL_ERROR; goto finally;}
	TEST_OK_LABEL(finally, code, Tcl_ListObjGetElements(interp, pathObj, &pathc, &pathv));

	const uint8_t*const	e = bytes + byteslen;
	p = bytes;
	*ePtr = e;

	const uint8_t*	valPtr;

	#define TAKE(n) do { \
		const size_t	nb = n; \
		if (e - p < nb) CBOR_TRUNCATED(finally, code); \
		valPtr = p; \
		p += nb; \
	} while (0)


	for (int pathi=0; pathi<pathc; pathi++) {
		Tcl_Obj*	pathElem = pathv[pathi];
		Tcl_DStringSetLength(tagsPtr, 0);

	data_item: // loop: read off tags
		// Read head {{{
		TAKE(1);
		const uint8_t	ib = valPtr[0];
		const uint8_t	mt = ib >> 5;
		const uint8_t	ai = ib & 0x1f;
		uint64_t		val = ai;

		#define BAD_PATH() do { \
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("Cannot index into atomic value type %d with path element %d: \"%s\"", mt, pathi, Tcl_GetString(pathElem))); \
			code = TCL_ERROR; \
			goto finally; \
		} while (0)

		switch (ai) {
			case 24: TAKE(1); val = *(uint8_t*)valPtr;           break;
			case 25: TAKE(2); val = be16toh(*(uint16_t*)valPtr); break;
			case 26: TAKE(4); val = be32toh(*(uint32_t*)valPtr); break;
			case 27: TAKE(8); val = be64Itoh(*(uint64_t*)valPtr); break;
			case 28: case 29: case 30: CBOR_INVALID(finally, code, "reserved additional info value: %d", ai);
		}

		if (ai == 31) {
			switch (mt) {
				case M_UINT:
				case M_NINT:
				case M_TAG:
				case M_REAL:
					CBOR_INVALID(finally, code, "invalid indefinite length for major type %d", mt);

				case M_BSTR:
				case M_UTF8:
				case M_ARR:
				case M_MAP:
					break;
			}
		}
		//}}}

		switch (mt) {
			case M_ARR: //{{{
			{
				enum indexmode	mode;
				ssize_t			ofs;

				TEST_OK_LABEL(finally, code, parse_index(interp, pathElem, &mode, &ofs));
				const size_t	absofs = ofs < 0 ? ofs*-1 : ofs;
				if (mode == IDX_ENDREL) { //{{{
					if (ai == 31) { // end-x, indefinite length array {{{
						uint8_t		elem_mt;

						// Skip absofs elements
						for (ssize_t i=0; i<absofs; i++) {
							TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, &elem_mt));
							if (elem_mt == -1) goto not_found;	// Index before start: reached end of indefinite array before skipping enough
						}

						// Start recording the offsets so that when we reach the end we can look back ofs elements
						if (circular != g_circular_buf) {
							ckfree(circular);
							circular = g_circular_buf;
						}
						if (absofs > CIRCULAR_STATIC_SLOTS) circular = ckalloc(absofs * sizeof(uint8_t*));
						for (ssize_t c=0; ; c = (c+1)%absofs) {
							circular[c] = p;
							TEST_OK_LABEL(finally, code, well_formed_indefinite(interp, &p, e, 1, &elem_mt, mt));
							if (elem_mt == -1) {
								// Found the end: c-1 is the index of the last element, circular[(c-1+ofs)%absofs] is the indexed element
								p = circular[((c-1+ofs)%absofs + absofs)%absofs];
								break;
							}
						}
						//}}}
					} else { // end-x, known length array {{{
						if (val-1 - ofs < 0) goto not_found;	// Index before start
						// Skip val-1+ofs elements
						const size_t skip = val-1+ofs;
						for (ssize_t i=0; i<skip; i++) TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, NULL));
					}
					//}}}
					//}}}
				} else { //{{{
					if (ai == 31) { // ofs x, indefinite length array {{{
						const uint8_t*	last_p = p;
						for (ssize_t i=0; i<ofs+1; i++) { // Need to visit the referenced elem to be sure it isn't the break symbol (end of array)
							uint8_t	elem_mt;
							last_p = p;
							TEST_OK_LABEL(finally, code, well_formed_indefinite(interp, &p, e, 1, &elem_mt, mt));
							if (elem_mt == -1) goto not_found;	// Index beyond end
						}
						p = last_p;
						//}}}
					} else { // ofs x, known length array {{{
						if (ofs > val-1) goto not_found;	// Index beyond end
						for (ssize_t i=0; i<ofs; i++) TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 1, NULL));
						//}}}
					}
					//}}}
				}
				goto next_path_elem;
			}
			//}}}
			case M_MAP: //{{{
				for (size_t i=0; ; i++) {
					if (ai == 31) {
						if (p >= e) CBOR_TRUNCATED(finally, code);
						if (*p == 0xFF) {p++; break;}
					} else {
						if (i >= val) break;
					}

					int matches;
					TEST_OK_LABEL(finally, code, cbor_matches(interp, &p, e, pathElem, &matches));
					if (matches) goto next_path_elem;
					TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));	// Skip value
				}
				goto not_found;
			//}}}
			case M_TAG: //{{{
				Tcl_DStringAppend(tagsPtr, (const char*)&val, sizeof(val));
				goto data_item;
				//}}}
			default: BAD_PATH(); // Can only index into arrays and maps
		}

		#undef BAD_PATH
	next_path_elem:
		continue;
	}

	*dataitemPtr = p;

finally:
	#undef TAKE
	if (circular != g_circular_buf) {
		ckfree(circular);
		circular = g_circular_buf;
	}
	return code;

not_found:
	*dataitemPtr = NULL;
	goto finally;
}

//}}}
int CBOR_Length(Tcl_Interp* interp, const uint8_t* p, const uint8_t* e, size_t* lenPtr) //{{{
{
	int				code = TCL_OK;
	const uint8_t*	pl = p;

	TEST_OK_LABEL(finally, code, well_formed(interp, &p, e, 0, NULL));
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(p-pl));

finally:
	return code;
}

//}}}
// Stubs API }}}
// Script API {{{
static int cbor_nr_cmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj*const objv[]) //{{{
{
	int					code = TCL_OK;
	struct interp_cx*	l = cdata;
	static const char* ops[] = {
		"get",
		"extract",
		"wellformed",
		NULL
	};
	enum {
		OP_GET,
		OP_EXTRACT,
		OP_WELLFORMED,
	};
	int			op;
	Tcl_Obj*	res = NULL;
	Tcl_Obj*	path = NULL;
	Tcl_DString	tags;

	Tcl_DStringInit(&tags);

	enum {A_cmd, A_OP, A_args};
	CHECK_MIN_ARGS_LABEL(finally, code, "op ?args ...?");

	TEST_OK_LABEL(finally, code, Tcl_GetIndexFromObj(interp, objv[A_OP], ops, "op", TCL_EXACT, &op));

	switch (op) {
		case OP_GET: //{{{
		{
			enum {A_cmd=A_OP, A_CBOR, A_args};
			const int A_PATH = A_args;
			CHECK_MIN_ARGS_LABEL(finally, code, "cbor ?key ...?");

			replace_tclobj(&path, Tcl_NewListObj(objc-A_PATH, objv+A_PATH));

			const uint8_t*	dataitem = NULL;
			const uint8_t*	e = NULL;
			TEST_OK_LABEL(finally, code, CBOR_GetDataItemFromPath(interp, objv[A_CBOR], path, &dataitem, &e, &tags));
			if (dataitem == NULL) {
				Tcl_SetErrorCode(interp, "CBOR", "NOTFOUND", Tcl_GetString(path), NULL);
				THROW_ERROR_LABEL(finally, code, "path not found");
			}
			TEST_OK_LABEL(finally, code, cbor_get_obj(interp, &dataitem, e, &res, NULL));

			/*
			const int tagcount = Tcl_DStringLength(&tags)/sizeof(uint64_t);
			const uint64_t* tag = (const uint64_t*)Tcl_DStringValue(&tags);
			if (tagcount > 0) {
				switch (tag[tagcount-1]) {
					case 2: // unsigned bignum
					{
						size_t	bytelen;
						const uint8_t*	bytes = Tcl_GetBytesFromObj(res, &bytelen);
						int		rc;
						mp_int	n = {0};
						const int	digits = (bytelen*CHAR_BIT + MP_DIGIT_BIT-1) / (MP_DIGIT_BIT);	// Need at least 64 bits to represent val

						if ((rc = mp_init_size(&n, digits)) != MP_OKAY) Tcl_Panic("mp_init_size failed");
						n.sign = MP_ZPOS;
						n.used = digits;
						//for (int i=0; i<digits; i++) n.dp[i] = (val >> (i*MP_DIGIT_BIT)) & MP_MASK;
						res = Tcl_NewBignumObj(&n);
						mp_clear(&n);
						break;
					}

					case 3: // negative bignum
					{
					}
				}
			}
			*/

			Tcl_SetObjResult(interp, res);
			break;
		}
		//}}}
		case OP_EXTRACT: //{{{
		{
			enum {A_cmd=A_OP, A_CBOR, A_args};
			const int A_PATH = A_args;
			CHECK_MIN_ARGS_LABEL(finally, code, "cbor ?key ...?");

			replace_tclobj(&path, Tcl_NewListObj(objc-A_PATH, objv+A_PATH));

			const uint8_t*	dataitem = NULL;
			const uint8_t*	e = NULL;
			TEST_OK_LABEL(finally, code, CBOR_GetDataItemFromPath(interp, objv[A_CBOR], path, &dataitem, &e, &tags));
			if (dataitem == NULL) {
				Tcl_SetErrorCode(interp, "CBOR", "NOTFOUND", Tcl_GetString(path), NULL);
				THROW_ERROR_LABEL(finally, code, "path not found");
			}

			size_t		len = 0;
			TEST_OK_LABEL(finally, code, CBOR_Length(interp, dataitem, e, &len));
			Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(dataitem, len));
			break;
		}
		//}}}
		case OP_WELLFORMED: //{{{
		{
			enum {A_cmd=A_OP, A_BYTES, A_objc};
			CHECK_ARGS_LABEL(finally, code, "bytes");

			int				len;
			const uint8_t*	bytes = Tcl_GetByteArrayFromObj(objv[A_BYTES], &len);
			const uint8_t*	p = bytes;

			TEST_OK_LABEL(finally, code, well_formed(interp, &p, bytes+len, 0, NULL));
			if (p != bytes+len) CBOR_TRAILING(finally, code);

			Tcl_SetObjResult(interp, l->tcl_true);
			break;
		}
		//}}}
		default: THROW_ERROR_LABEL(finally, code, "op not implemented yet");
	}

finally:
	replace_tclobj(&path, NULL);
	replace_tclobj(&res, NULL);
	Tcl_DStringFree(&tags);
	return code;
}

//}}}
static int cbor_cmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj*const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, cbor_nr_cmd, cdata, objc, objv);
}

//}}}
// Script API }}}

int cbor_init(Tcl_Interp* interp, struct interp_cx* l) //{{{
{
	int		code = TCL_OK;

	Tcl_NRCreateCommand(interp, NS "::cbor", cbor_cmd, cbor_nr_cmd, l, NULL);

	return code;
}

//}}}
void cbor_release(Tcl_Interp* interp) //{{{
{
	// Called on unload.  Namespace changes (like commands) are handled automatically
}

//}}}

// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
