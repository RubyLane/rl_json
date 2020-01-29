#include "rl_json.h"
#include "rl_jsonInt.h"

#define ENSEMBLE	0

#if defined(_WIN32)
#define snprintf _snprintf
#endif

#ifdef WIN32
#define _DLLEXPORT extern DLLEXPORT
#else
#define _DLLEXPORT
#endif

static const char* dyn_prefix[] = {
	NULL,	// JSON_UNDEF
	NULL,	// JSON_OBJECT
	NULL,	// JSON_ARRAY
	NULL,	// JSON_STRING
	NULL,	// JSON_NUMBER
	NULL,	// JSON_BOOL
	NULL,	// JSON_NULL

	"~S:",	// JSON_DYN_STRING
	"~N:",	// JSON_DYN_NUMBER
	"~B:",	// JSON_DYN_BOOL
	"~J:",	// JSON_DYN_JSON
	"~T:",	// JSON_DYN_TEMPLATE
	"~L:"	// JSON_DYN_LITERAL
};

static const enum json_types from_dyn[] = {
	JSON_UNDEF,	// JSON_UNDEF
	JSON_UNDEF,	// JSON_OBJECT
	JSON_UNDEF,	// JSON_ARRAY
	JSON_UNDEF,	// JSON_STRING
	JSON_UNDEF,	// JSON_NUMBER
	JSON_UNDEF,	// JSON_BOOL
	JSON_UNDEF,	// JSON_NULL

	JSON_STRING,		// JSON_DYN_STRING
	JSON_NUMBER,		// JSON_DYN_NUMBER
	JSON_BOOL,			// JSON_DYN_BOOL
	JSON_DYN_JSON,		// JSON_DYN_JSON
	JSON_DYN_TEMPLATE,	// JSON_DYN_TEMPLATE
	JSON_STRING			// JSON_DYN_LITERAL
};

static const char* type_names[] = {
	"undefined",	// JSON_UNDEF
	"object",		// JSON_OBJECT
	"array",		// JSON_ARRAY
	"string",		// JSON_STRING
	"number",		// JSON_NUMBER
	"boolean",		// JSON_BOOL
	"null",			// JSON_NULL

	"string",		// JSON_DYN_STRING
	"string",		// JSON_DYN_NUMBER
	"string",		// JSON_DYN_BOOL
	"string",		// JSON_DYN_JSON
	"string",		// JSON_DYN_TEMPLATE
	"string"		// JSON_DYN_LITERAL
};
const char* type_names_int[] = {	// Must match the order of the json_types enum
	"JSON_UNDEF",
	"JSON_OBJECT",
	"JSON_ARRAY",
	"JSON_STRING",
	"JSON_NUMBER",
	"JSON_BOOL",
	"JSON_NULL",

	"JSON_DYN_STRING",
	"JSON_DYN_NUMBER",
	"JSON_DYN_BOOL",
	"JSON_DYN_JSON",
	"JSON_DYN_TEMPLATE",
	"JSON_DYN_LITERAL"
};

static const char* action_opcode_str[] = { // Must match the order of the action_opcode enum
	"NOP",
	"ALLOCATE",
	"FETCH_VALUE",
	"DECLARE_LITERAL",
	"STORE_STRING",
	"STORE_NUMBER",
	"STORE_BOOLEAN",
	"STORE_JSON",
	"STORE_TEMPLATE",
	"PUSH_TARGET",
	"POP_TARGET",
	"REPLACE_VAL",
	"REPLACE_ARR",
	"REPLACE_ATOM",
	"REPLACE_KEY",

	(char*)NULL
};

static int new_json_value_from_list(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], Tcl_Obj** res);
static int NRforeach_next_loop_bottom(ClientData cdata[], Tcl_Interp* interp, int retcode);
static int json_pretty_dbg(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent, Tcl_Obj* pad, Tcl_DString* ds);

#ifdef _GNU_SOURCE
#define FFSLL	ffsll
#else
#define FFSLL	ffsll_polyfill
static int ffsll_polyfill(long long x) //{{{
{
	int i=0;
	long long mask = 1;
	for(i=0; i<sizeof(long long)*8;++i, mask <<= 1) {
		if(x & mask) {
			return i+1;
		}
	}
	return 0;
}

//}}}
#endif

static int first_free(long long* freemap) //{{{
{
	int	i=0, bit, res;
	while ((bit = FFSLL(freemap[i])) == 0) {
		i++;
	}
	res = i * (sizeof(long long)*8) + (bit-1);
	return res;
}

//}}}
static void mark_used(long long* freemap, int idx) //{{{
{
	int	i = idx / (sizeof(long long)*8);
	int bit = idx - (i * (sizeof(long long)*8));
	freemap[i] &= ~(1LL << bit);
}

//}}}
static void mark_free(long long* freemap, int idx) //{{{
{
	int	i = idx / (sizeof(long long)*8);
	int bit = idx - (i * (sizeof(long long)*8));
	freemap[i] |= 1LL << bit;
}

//}}}
static void free_cache(struct interp_cx* l) //{{{
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	he = Tcl_FirstHashEntry(&l->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
		//printf("age_cache: kc_count: %d", l->kc_count);
		e = &l->kc_entries[idx];

		Tcl_DeleteHashEntry(he);
		Tcl_DecrRefCount(e->val);
		Tcl_DecrRefCount(e->val);	// Two references - one for the cache table and one on loan to callers' interim processing
		mark_free(l->freemap, idx);
		e->val = NULL;
		he = Tcl_NextHashEntry(&search);
	}
	l->kc_count = 0;
}

//}}}
static void age_cache(struct interp_cx* l) //{{{
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	he = Tcl_FirstHashEntry(&l->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
		//printf("age_cache: kc_count: %d", l->kc_count);
		e = &l->kc_entries[idx];

		if (e->hits < 1) {
			Tcl_DeleteHashEntry(he);
			Tcl_DecrRefCount(e->val);
			Tcl_DecrRefCount(e->val);	// Two references - one for the cache table and one on loan to callers' interim processing
			mark_free(l->freemap, idx);
			e->val = NULL;
		} else {
			e->hits >>= 1;
		}
		he = Tcl_NextHashEntry(&search);
	}
	l->kc_count = 0;
}

//}}}
Tcl_Obj* new_stringobj_dedup(struct interp_cx* l, const char* bytes, int length) //{{{
{
	char				buf[STRING_DEDUP_MAX + 1];
	const char			*keyname;
	int					is_new;
	struct kc_entry*	kce;
	Tcl_Obj*			out;
	Tcl_HashEntry*		entry = NULL;

	if (length == 0) {
		return l->tcl_empty;
	} else if (length < 0) {
		length = strlen(bytes);
	}

	if (length > STRING_DEDUP_MAX || l == NULL)
		return Tcl_NewStringObj(bytes, length);

	if (likely(bytes[length] == 0)) {
		keyname = bytes;
	} else {
		memcpy(buf, bytes, length);
		buf[length] = 0;
		keyname = buf;
	}
	entry = Tcl_CreateHashEntry(&l->kc, keyname, &is_new);

	if (is_new) {
		ptrdiff_t	idx = first_free(l->freemap);

		if (unlikely(idx >= KC_ENTRIES)) {
			// Cache overflow
			Tcl_DeleteHashEntry(entry);
			age_cache(l);
			return Tcl_NewStringObj(bytes, length);
		}

		kce = &l->kc_entries[idx];
		kce->hits = 0;
		out = kce->val = Tcl_NewStringObj(bytes, length);
		Tcl_IncrRefCount(out);	// Two references - one for the cache table and one on loan to callers' interim processing.
		Tcl_IncrRefCount(out);	// Without this, values not referenced elsewhere could reach callers with refCount 1, allowing
								// the value to be mutated in place and corrupt the state of the cache (hash key not matching obj value)

		mark_used(l->freemap, idx);

		Tcl_SetHashValue(entry, (void*)idx);
		l->kc_count++;

		if (unlikely(l->kc_count > (int)(KC_ENTRIES/2.5))) {
			kce->hits++; // Prevent the just-created entry from being pruned
			age_cache(l);
		}
	} else {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(entry);

		kce = &l->kc_entries[idx];
		out = kce->val;
		if (kce->hits < 255) kce->hits++;
	}

	return out;
}

//}}}

static Tcl_Obj* as_json(Tcl_Interp* interp, Tcl_Obj* from) //{{{
{
	Tcl_ObjIntRep*	ir = NULL;
	enum json_types	type;

	if (JSON_GetIntrepFromObj(interp, from, &type, &ir) == TCL_OK) {
		// Already a JSON value, use it directly
		return from;
	} else {
		// Anything else, use it as a JSON string
		return JSON_NewJvalObj(JSON_STRING, from);		// EIAS, so we can use whatever $from is as the intrep for a JSON_STRING value
	}
}

//}}}

static int force_json_number(Tcl_Interp* interp, struct interp_cx* l, Tcl_Obj* obj, Tcl_Obj** forced) //{{{
{
	int	res = TCL_OK;

	// TODO: investigate a direct bytecode version?

	/*
display *obj
	 */
	if (l) {
		// Attempt to snoop on the intrep to verify that it is one of the numeric types
		if (
			obj->typePtr && (
				(l->typeInt    && Tcl_FetchIntRep(obj, l->typeInt) != NULL) ||
				(l->typeDouble && Tcl_FetchIntRep(obj, l->typeDouble) != NULL) ||
				(l->typeBignum && Tcl_FetchIntRep(obj, l->typeBignum) != NULL)
		   )
		) {
			// It's a known number type, we can safely use it directly
			//fprintf(stderr, "force_json_number fastpath, verified obj to be a number type\n");
			if (forced == NULL) return TCL_OK;

			if (Tcl_HasStringRep(obj)) { // Has a string rep already, make sure it's not hex or octal, and not padded with whitespace
				const char* s;
				int			len, start=0;

				s = Tcl_GetStringFromObj(obj, &len);
				if (len >= 1 && s[0] == '-')
					start++;

				if (unlikely(
					(len+start >= 1 && (
						(s[start] == '0' && len+start >= 2 && s[start+1] != '.') || // Octal or hex, or double with leading zero not immediately followed by .)
						s[start] == ' '  ||
						s[start] == '\n' ||
						s[start] == '\t' ||
						s[start] == '\v' ||
						s[start] == '\r' ||
						s[start] == '\f'
					)) ||
					(len-start >= 2 && (
						s[len-1] == ' '  ||
						s[len-1] == '\n' ||
						s[len-1] == '\t' ||
						s[len-1] == '\v' ||
						s[len-1] == '\r' ||
						s[len-1] == '\f'
					))
				)) {
					// The existing string rep is one of the cases
					// (octal / hex / whitespace padded) that is not a
					// valid JSON number.  Duplicate the obj and
					// invalidate the string rep
					Tcl_IncrRefCount(*forced = Tcl_DuplicateObj(obj));
					Tcl_InvalidateStringRep(*forced);
				} else {
					// String rep is safe as a JSON number
					Tcl_IncrRefCount(*forced = obj);
					//fprintf(stderr, "force_json_number obj stringrep is safe json number: (%s), intrep: (%s)\n", Tcl_GetString(obj), obj->typePtr->name);
				}
			} else {
				// Pure number - no string rep
				Tcl_IncrRefCount(*forced = obj);
			}

			return TCL_OK;
		} else {
			// Could be a string that is a valid number representation, or
			// something that will convert to a valid number.  Add 0 to it to
			// check (all valid numbers succeed at this, and are unchanged by
			// it).  Use the cached objs
			Tcl_IncrRefCount(l->force_num_cmd[2] = obj);
			res = Tcl_EvalObjv(interp, 3, l->force_num_cmd, TCL_EVAL_DIRECT);
			Tcl_DecrRefCount(l->force_num_cmd[2]);
			l->force_num_cmd[2] = NULL;
		}

	} else {
		Tcl_Obj*	cmd;

		cmd = Tcl_NewListObj(0, NULL);
		TEST_OK(Tcl_ListObjAppendElement(interp, cmd, Tcl_NewStringObj("::tcl::mathop::+", -1)));
		TEST_OK(Tcl_ListObjAppendElement(interp, cmd, Tcl_NewIntObj(0)));
		TEST_OK(Tcl_ListObjAppendElement(interp, cmd, obj));

		Tcl_IncrRefCount(cmd);
		res = Tcl_EvalObjEx(interp, cmd, TCL_EVAL_DIRECT);
		Tcl_DecrRefCount(cmd);
	}

	if (res == TCL_OK) {
		if (forced != NULL)
			Tcl_IncrRefCount(*forced = Tcl_GetObjResult(interp));

		Tcl_ResetResult(interp);
	}

	return res;
}

//}}}
static void append_json_string(const struct serialize_context* scx, Tcl_Obj* obj) //{{{
{
	int				len;
	const char*		chunk;
	const char*		p;
	const char*		e;
	Tcl_DString*	ds = scx->ds;
	Tcl_UniChar		c;
	ptrdiff_t		adv;
	char			ustr[23];		// Actually only need 7 bytes, allocating enough to avoid overrun if Tcl_UniChar somehow holds a 64bit value

	Tcl_DStringAppend(ds, "\"", 1);

	p = chunk = Tcl_GetStringFromObj(obj, &len);
	e = p + len;

	while (p < e) {
		adv = Tcl_UtfToUniChar(p, &c);
		if (unlikely(c <= 0x1f || c == '\\' || c == '"')) {
			Tcl_DStringAppend(ds, chunk, p-chunk);
			switch (c) {
				case '"':	Tcl_DStringAppend(ds, "\\\"", 2); break;
				case '\\':	Tcl_DStringAppend(ds, "\\\\", 2); break;
				case 0x8:	Tcl_DStringAppend(ds, "\\b", 2); break;
				case 0xC:	Tcl_DStringAppend(ds, "\\f", 2); break;
				case 0xA:	Tcl_DStringAppend(ds, "\\n", 2); break;
				case 0xD:	Tcl_DStringAppend(ds, "\\r", 2); break;
				case 0x9:	Tcl_DStringAppend(ds, "\\t", 2); break;

				default:
					snprintf(ustr, 7, "\\u%04X", c);
					Tcl_DStringAppend(ds, ustr, 6);
					break;
			}
			p += adv;
			chunk = p;
		} else {
			p += adv;
		}
	}

	if (likely(p > chunk))
		Tcl_DStringAppend(ds, chunk, p-chunk);

	Tcl_DStringAppend(ds, "\"", 1);
}

//}}}
static int serialize_json_val(Tcl_Interp* interp, struct serialize_context* scx, const int type, Tcl_Obj* val) //{{{
{
	Tcl_DString*	ds = scx->ds;
	int				res = TCL_OK;

	switch (type) {
		case JSON_STRING: //{{{
			append_json_string(scx, val);
			break;
			//}}}
		case JSON_OBJECT: //{{{
			{
				int				done, first=1;
				Tcl_DictSearch	search;
				Tcl_Obj*		k;
				Tcl_Obj*		v;
				enum json_types	v_type = JSON_UNDEF;
				Tcl_Obj*		iv = NULL;

				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));

				Tcl_DStringAppend(ds, "{", 1);
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					if (!first) {
						Tcl_DStringAppend(ds, ",", 1);
					} else {
						first = 0;
					}

					// Have to do the template subst here rather than at
					// parse time since the dict keys would be broken otherwise
					if (scx->serialize_mode == SERIALIZE_TEMPLATE) {
						int			l, stype;
						const char*	s;

						s = Tcl_GetStringFromObj(k, &l);

						if (
								l >= 3 &&
								s[0] == '~' &&
								s[2] == ':'
						) {
							switch (s[1]) {
								case 'S': stype = JSON_DYN_STRING; break;
								case 'L': stype = JSON_DYN_LITERAL; break;

								case 'N':
								case 'B':
								case 'J':
								case 'T':
									Tcl_SetObjResult(interp, Tcl_NewStringObj(
												"Only strings allowed as object keys", -1));
									res = TCL_ERROR;
									goto done;

								default:  stype = JSON_UNDEF; break;
							}

							if (stype != JSON_UNDEF) {
								int hold = scx->allow_null;
								scx->allow_null = 0;
								if (serialize_json_val(interp, scx, stype, Tcl_GetRange(k, 3, l-1)) != TCL_OK) {
									scx->allow_null = hold;
									res = TCL_ERROR;
									break;
								}
								scx->allow_null = hold;
							} else {
								append_json_string(scx, k);
							}
						} else {
							append_json_string(scx, k);
						}
					} else {
						append_json_string(scx, k);
					}

					Tcl_DStringAppend(ds, ":", 1);
					JSON_GetJvalFromObj(interp, v, &v_type, &iv);
					if (serialize_json_val(interp, scx, v_type, iv) != TCL_OK) {
						res = TCL_ERROR;
						break;
					}
				}
				Tcl_DStringAppend(ds, "}", 1);
				Tcl_DictObjDone(&search);
			}
			break;
			//}}}
		case JSON_ARRAY: //{{{
			{
				int				i, oc, first=1;
				Tcl_Obj**		ov;
				Tcl_Obj*		iv = NULL;
				enum json_types	v_type = JSON_UNDEF;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));

				Tcl_DStringAppend(ds, "[", 1);
				for (i=0; i<oc; i++) {
					if (!first) {
						Tcl_DStringAppend(ds, ",", 1);
					} else {
						first = 0;
					}
					JSON_GetJvalFromObj(interp, ov[i], &v_type, &iv);
					TEST_OK(serialize_json_val(interp, scx, v_type, iv));
				}
				Tcl_DStringAppend(ds, "]", 1);
			}
			break;
			//}}}
		case JSON_NUMBER: //{{{
			{
				const char*	bytes;
				int			len;

				bytes = Tcl_GetStringFromObj(val, &len);
				Tcl_DStringAppend(ds, bytes, len);
			}
			break;
			//}}}
		case JSON_BOOL: //{{{
			{
				int		boolval;

				TEST_OK(Tcl_GetBooleanFromObj(NULL, val, &boolval));

				if (boolval) {
					Tcl_DStringAppend(ds, "true", 4);
				} else {
					Tcl_DStringAppend(ds, "false", 5);
				}
			}
			break;
			//}}}
		case JSON_NULL: //{{{
			Tcl_DStringAppend(ds, "null", 4);
			break;
			//}}}

		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL: //{{{
			if (scx->serialize_mode == SERIALIZE_NORMAL) {
				Tcl_Obj*	tmp = Tcl_ObjPrintf("%s%s", dyn_prefix[type], Tcl_GetString(val));

				Tcl_IncrRefCount(tmp);
				append_json_string(scx, tmp);
				Tcl_DecrRefCount(tmp);
			} else {
				Tcl_Obj*		subst_val = NULL;
				enum json_types	subst_type;
				int				reset_mode = 0;

				if (type == JSON_DYN_LITERAL) {
					append_json_string(scx, val);
					break;
				}

				if (scx->fromdict != NULL) {
					TEST_OK(Tcl_DictObjGet(interp, scx->fromdict, val, &subst_val));
				} else {
					subst_val = Tcl_ObjGetVar2(interp, val, NULL, 0);
				}

				if (subst_val == NULL) {
					subst_type = JSON_NULL;
				} else {
					subst_type = from_dyn[type];
					Tcl_IncrRefCount(subst_val);
				}

				if (subst_type == JSON_DYN_JSON) {
					if (subst_val != NULL) Tcl_DecrRefCount(subst_val);
					res = JSON_GetJvalFromObj(interp, subst_val, &subst_type, &subst_val);
					if (subst_val != NULL) Tcl_IncrRefCount(subst_val);
					scx->serialize_mode = SERIALIZE_NORMAL;
					reset_mode = 1;
				} else if (subst_type == JSON_DYN_TEMPLATE) {
					if (subst_val != NULL) Tcl_DecrRefCount(subst_val);
					res = JSON_GetJvalFromObj(interp, subst_val, &subst_type, &subst_val);
					if (subst_val != NULL) Tcl_IncrRefCount(subst_val);
				} else if (subst_type == JSON_NUMBER) {
					Tcl_Obj*	forced = NULL;

					if ((res = force_json_number(interp, scx->l, subst_val, &forced)) == TCL_OK)
						REPLACE(subst_val, forced);

					RELEASE(forced);
					if (res != TCL_OK) {
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error substituting value from \"%s\" into template, not a number: \"%s\"", Tcl_GetString(val), Tcl_GetString(subst_val)));
						return TCL_ERROR;
					}
				}

				if (subst_type == JSON_NULL && !scx->allow_null)
					THROW_ERROR("Only strings allowed as object keys");

				if (res == TCL_OK)
					res = serialize_json_val(interp, scx, subst_type, subst_val);

				if (subst_val != NULL)
					Tcl_DecrRefCount(subst_val);

				if (reset_mode)
					scx->serialize_mode = SERIALIZE_TEMPLATE;
			}
			break;
			//}}}

		default: //{{{
			THROW_ERROR("Corrupt internal rep: invalid type ", Tcl_NewIntObj(type));
			break; //}}}
	}

done:
	return res;
}

//}}}

void append_to_cx(struct parse_context* cx, Tcl_Obj* val) //{{{
{
	Tcl_ObjIntRep*	ir = NULL;

	/*
	fprintf(stderr, "append_to_cx, storing %s: \"%s\"\n",
			type_names[val->internalRep.ptrAndLongRep.value],
			val->internalRep.ptrAndLongRep.ptr == NULL ? "NULL" :
			Tcl_GetString((Tcl_Obj*)val->internalRep.ptrAndLongRep.ptr));
	*/
	switch (cx->container) {
		case JSON_OBJECT:
			//fprintf(stderr, "append_to_cx, cx->hold_key->refCount: %d (%s)\n", cx->hold_key->refCount, Tcl_GetString(cx->hold_key));
			ir = Tcl_FetchIntRep(cx->val, cx->objtype);
			if (ir == NULL) Tcl_Panic("Can't get intrep for container");
			Tcl_DictObjPut(NULL, ir->twoPtrValue.ptr1, cx->hold_key, val);
			if (ir->twoPtrValue.ptr2) {Tcl_DecrRefCount((Tcl_Obj*)ir->twoPtrValue.ptr2); ir->twoPtrValue.ptr2=NULL;}
			Tcl_InvalidateStringRep(cx->val);
			Tcl_DecrRefCount(cx->hold_key);
			cx->hold_key = NULL;
			break;

		case JSON_ARRAY:
			ir = Tcl_FetchIntRep(cx->val, cx->objtype);
			if (ir == NULL) Tcl_Panic("Can't get intrep for container");
			//fprintf(stderr, "append_to_cx, appending to list: (%s)\n", Tcl_GetString(val));
			Tcl_ListObjAppendElement(NULL, ir->twoPtrValue.ptr1, val);
			if (ir->twoPtrValue.ptr2) {Tcl_DecrRefCount((Tcl_Obj*)ir->twoPtrValue.ptr2); ir->twoPtrValue.ptr2=NULL;}
			Tcl_InvalidateStringRep(cx->val);
			break;

		default:
			cx->val = val;
			Tcl_IncrRefCount(cx->val);
	}
}

//}}}

int serialize(Tcl_Interp* interp, struct serialize_context* scx, Tcl_Obj* obj) //{{{
{
	enum json_types	type = JSON_UNDEF;
	int				res;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, obj, &type, &val));

	res = serialize_json_val(interp, scx, type, val);

	// The result of the serialization is left in scx->ds.  Once the caller
	// is done with this value it must be freed with Tcl_DStringFree()
	return res;
}

//}}}

static int get_modifier(Tcl_Interp* interp, Tcl_Obj* modobj, enum modifiers* modifier) //{{{
{
	// This must be kept in sync with the modifiers enum
	static CONST char *modstrings[] = {
		"",
		"?length",
		"?size",
		"?type",
		"?keys",
		(char*)NULL
	};
	int	index;

	TEST_OK(Tcl_GetIndexFromObj(interp, modobj, modstrings, "modifier", TCL_EXACT, &index));
	*modifier = index;

	return TCL_OK;
}

//}}}
static Tcl_Obj* get_unshared_val(Tcl_ObjIntRep* ir) //{{{
{
	if (ir->twoPtrValue.ptr1 != NULL && Tcl_IsShared((Tcl_Obj*)ir->twoPtrValue.ptr1)) {
		replace_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr1, Tcl_DuplicateObj(ir->twoPtrValue.ptr1));
	}

	if (ir->twoPtrValue.ptr2) {
		// The caller wants val unshared, which implies that they intend to
		// change it, which would invalidate our cached template actions, so
		// release those if we have them
		release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
	}

	return ir->twoPtrValue.ptr1;
}

//}}}
int JSON_Set(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc, Tcl_Obj* replacement) //{{{
{
	int				i;
	enum json_types	type, newtype;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;
	Tcl_Obj*		step;
	Tcl_Obj*		src;
	Tcl_Obj*		target;
	Tcl_Obj*		newval;
	Tcl_Obj*		rep = NULL;

	src = Tcl_ObjGetVar2(interp, srcvar, NULL, 0);
	if (src == NULL) {
		src = Tcl_ObjSetVar2(interp, srcvar, NULL, JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj()), TCL_LEAVE_ERR_MSG);
	}

	if (Tcl_IsShared(src)) {
		src = Tcl_ObjSetVar2(interp, srcvar, NULL, Tcl_DuplicateObj(src), TCL_LEAVE_ERR_MSG);
		if (src == NULL)
			return TCL_ERROR;
	}

	/*
	fprintf(stderr, "JSON_Set, srcvar: \"%s\", src: \"%s\"\n",
			Tcl_GetString(srcvar), Tcl_GetString(src));
			*/
	target = src;

	TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
	val = get_unshared_val(ir);

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_int[type]);

		switch (type) {
			case JSON_UNDEF: //{{{
				THROW_ERROR("Found JSON_UNDEF type jval following path");
				//}}}
			case JSON_OBJECT: //{{{
				TEST_OK(Tcl_DictObjGet(interp, val, step, &target));
				if (target == NULL) {
					//fprintf(stderr, "Path element %d: \"%s\" doesn't exist creating a new key for it and storing a null\n",
					//		i, Tcl_GetString(step));
					target = JSON_NewJvalObj(JSON_NULL, NULL);
					TEST_OK(Tcl_DictObjPut(interp, val, step, target));
					i++;
					goto followed_path;
				}
				if (Tcl_IsShared(target)) {
					//fprintf(stderr, "Path element %d: \"%s\" exists but the TclObj is shared (%d), replacing it with an unshared duplicate\n",
					//		i, Tcl_GetString(step), target->refCount);
					target = Tcl_DuplicateObj(target);
					TEST_OK(Tcl_DictObjPut(interp, val, step, target));
				}
				break;
				//}}}
			case JSON_ARRAY: //{{{
				{
					int			ac, index_str_len, ok=1;
					long		index;
					const char*	index_str;
					char*		end;
					Tcl_Obj**	av;

					TEST_OK(Tcl_ListObjGetElements(interp, val, &ac, &av));
					//fprintf(stderr, "descending into array of length %d\n", ac);

					if (Tcl_GetLongFromObj(NULL, step, &index) != TCL_OK) {
						// Index isn't an integer, check for end(+/-int)?
						index_str = Tcl_GetStringFromObj(step, &index_str_len);
						if (index_str_len < 3 || strncmp("end", index_str, 3) != 0)
							ok = 0;

						if (ok) {
							index = ac-1;
							if (index_str_len >= 4) {
								if (index_str[3] != '-' && index_str[3] != '+') {
									ok = 0;
								} else {
									// errno is magically thread-safe on POSIX
									// systems (it's thread-local)
									errno = 0;
									index += strtol(index_str+3, &end, 10);
									if (errno != 0 || *end != 0)
										ok = 0;
								}
							}
						}

						if (!ok)
							THROW_ERROR("Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

						//fprintf(stderr, "Resolved index of %ld from \"%s\"\n", index, index_str);
					} else {
						//fprintf(stderr, "Explicit index: %ld\n", index);
					}

					if (index < 0) {
						// Prepend element to the array
						target = JSON_NewJvalObj(JSON_NULL, NULL);
						TEST_OK(Tcl_ListObjReplace(interp, val, -1, 0, 1, &target));

						i++;
						goto followed_path;
					} else if (index >= ac) {
						int			new_i;
						for (new_i=ac; new_i<index; new_i++) {
							TEST_OK(Tcl_ListObjAppendElement(interp, val,
										JSON_NewJvalObj(JSON_NULL, NULL)));
						}
						target = JSON_NewJvalObj(JSON_NULL, NULL);
						TEST_OK(Tcl_ListObjAppendElement(interp, val, target));

						i++;
						goto followed_path;
					} else {
						target = av[index];
						if (Tcl_IsShared(target)) {
							target = Tcl_DuplicateObj(target);
							TEST_OK(Tcl_ListObjReplace(interp, val, index, 1, 1, &target));
						}
						//fprintf(stderr, "extracted index %ld: (%s)\n", index, Tcl_GetString(target));
					}
				}
				break;
				//}}}
			case JSON_STRING:
			case JSON_NUMBER:
			case JSON_BOOL:
			case JSON_NULL:
			case JSON_DYN_STRING:
			case JSON_DYN_NUMBER:
			case JSON_DYN_BOOL:
			case JSON_DYN_JSON:
			case JSON_DYN_TEMPLATE:
			case JSON_DYN_LITERAL:
				THROW_ERROR("Attempt to index into atomic type ", type_names[type], " at path key \"", Tcl_GetString(step), "\"");
				/*
				i++;
				goto followed_path;
				*/
			default:
				THROW_ERROR("Unhandled type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

		TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
		val = get_unshared_val(ir);
	}

	goto set_val;

followed_path:
	TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
	val = get_unshared_val(ir);

	// target points at the (first) object to replace.  It and its internalRep
	// are unshared

	// If any path elements remain then they need to be created as object
	// keys
	//fprintf(stderr, "After walking path, %d elements remain to be created\n", pathc-i);
	for (; i<pathc; i++) {
		//fprintf(stderr, "create walk %d: %s, cx type: %s\n", i, Tcl_GetString(pathv[i]), type_names_int[type]);
		if (type != JSON_OBJECT) {
			//fprintf(stderr, "Type isn't JSON_OBJECT: %s, replacing with a JSON_OBJECT\n", type_names_int[type]);
			if (val != NULL)
				Tcl_DecrRefCount(val);
			val = Tcl_NewDictObj();
			TEST_OK(JSON_SetIntRep(target, JSON_OBJECT, val));
		}

		target = JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj());
		//fprintf(stderr, "Adding key \"%s\"\n", Tcl_GetString(pathv[i]));
		TEST_OK(Tcl_DictObjPut(interp, val, pathv[i], target));
		TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
		//fprintf(stderr, "Newly added key \"%s\" is of type %s\n", Tcl_GetString(pathv[i]), type_names_int[type]);
		// This was just created - it can't be shared
	}

set_val:
	//fprintf(stderr, "Reached end of path, calling JSON_SetIntRep for replacement value %s (%s), target is %s\n",
	//		type_names_int[newtype], Tcl_GetString(replacement), type_names_int[type]);
	rep = as_json(interp, replacement);

	TEST_OK(JSON_GetJvalFromObj(interp, rep, &newtype, &newval));
	TEST_OK(JSON_SetIntRep(target, newtype, newval));

	Tcl_InvalidateStringRep(src);

	if (interp)
		Tcl_SetObjResult(interp, src);

	return TCL_OK;
}

//}}}
int JSON_Unset(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc) //{{{
{
	enum json_types	type;
	int				i;
	Tcl_Obj*		val;
	Tcl_Obj*		step;
	Tcl_Obj*		src;
	Tcl_Obj*		target;

	src = Tcl_ObjGetVar2(interp, srcvar, NULL, TCL_LEAVE_ERR_MSG);
	if (src == NULL)
		return TCL_ERROR;

	if (pathc == 0) {
		Tcl_SetObjResult(interp, src);
		return TCL_OK;	// Do Nothing Gracefully
	}

	if (Tcl_IsShared(src)) {
		src = Tcl_ObjSetVar2(interp, srcvar, NULL, Tcl_DuplicateObj(src), TCL_LEAVE_ERR_MSG);
		if (src == NULL)
			return TCL_ERROR;
	}

	/*
	fprintf(stderr, "JSON_Set, srcvar: \"%s\", src: \"%s\"\n",
			Tcl_GetString(srcvar), Tcl_GetString(src));
			*/
	target = src;

	{
		Tcl_ObjIntRep*	ir = NULL;
		TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
		val = get_unshared_val(ir);
	}

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc-1; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_int[type]);

		switch (type) {
			case JSON_UNDEF: //{{{
				THROW_ERROR("Found JSON_UNDEF type jval following path");
				//}}}
			case JSON_OBJECT: //{{{
				TEST_OK(Tcl_DictObjGet(interp, val, step, &target));
				if (target == NULL) {
					goto bad_path;
				}
				if (Tcl_IsShared(target)) {
					//fprintf(stderr, "Path element %d: \"%s\" exists but the TclObj is shared (%d), replacing it with an unshared duplicate\n",
					//		i, Tcl_GetString(step), target->refCount);
					target = Tcl_DuplicateObj(target);
					TEST_OK(Tcl_DictObjPut(interp, val, step, target));
				}
				break;
				//}}}
			case JSON_ARRAY: //{{{
				{
					int			ac, index_str_len, ok=1;
					long		index;
					const char*	index_str;
					char*		end;
					Tcl_Obj**	av;

					TEST_OK(Tcl_ListObjGetElements(interp, val, &ac, &av));
					//fprintf(stderr, "descending into array of length %d\n", ac);

					if (Tcl_GetLongFromObj(NULL, step, &index) != TCL_OK) {
						// Index isn't an integer, check for end(+/-int)?
						index_str = Tcl_GetStringFromObj(step, &index_str_len);
						if (index_str_len < 3 || strncmp("end", index_str, 3) != 0)
							ok = 0;

						if (ok) {
							index = ac-1;
							if (index_str_len >= 4) {
								if (index_str[3] != '-' && index_str[3] != '+') {
									ok = 0;
								} else {
									// errno is magically thread-safe on POSIX
									// systems (it's thread-local)
									errno = 0;
									index += strtol(index_str+3, &end, 10);
									if (errno != 0 || *end != 0)
										ok = 0;
								}
							}
						}

						if (!ok)
							THROW_ERROR("Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

						//fprintf(stderr, "Resolved index of %ld from \"%s\"\n", index, index_str);
					} else {
						//fprintf(stderr, "Explicit index: %ld\n", index);
					}

					if (index < 0) {
						goto bad_path;
					} else if (index >= ac) {
						goto bad_path;
					} else {
						target = av[index];
						if (Tcl_IsShared(target)) {
							target = Tcl_DuplicateObj(target);
							TEST_OK(Tcl_ListObjReplace(interp, val, index, 1, 1, &target));
						}
						//fprintf(stderr, "extracted index %ld: (%s)\n", index, Tcl_GetString(target));
					}
				}
				break;
				//}}}
			case JSON_STRING:
			case JSON_NUMBER:
			case JSON_BOOL:
			case JSON_NULL:
			case JSON_DYN_STRING:
			case JSON_DYN_NUMBER:
			case JSON_DYN_BOOL:
			case JSON_DYN_JSON:
			case JSON_DYN_TEMPLATE:
			case JSON_DYN_LITERAL:
				THROW_ERROR("Attempt to index into atomic type ", type_names[type], " at path key \"", Tcl_GetString(step), "\"");
				/*
				i++;
				goto bad_path;
				*/
			default:
				THROW_ERROR("Unhandled type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

		{
			Tcl_ObjIntRep*	ir = NULL;
			TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
			val = get_unshared_val(ir);
		}
		//fprintf(stderr, "Walked on to new type %s\n", type_names[type]);
	}

	//fprintf(stderr, "Reached end of path, calling JSON_SetIntRep for replacement value %s (%s), target is %s\n",
	//		type_names_int[newtype], Tcl_GetString(replacement), type_names_int[type]);

	step = pathv[i];	// This names the key / element to unset
	//fprintf(stderr, "To replace: path step %d: \"%s\"\n", i, Tcl_GetString(step));
	switch (type) {
		case JSON_UNDEF: //{{{
			THROW_ERROR("Found JSON_UNDEF type jval following path");
			//}}}
		case JSON_OBJECT: //{{{
			TEST_OK(Tcl_DictObjRemove(interp, val, step));
			break;
			//}}}
		case JSON_ARRAY: //{{{
			{
				int			ac, index_str_len, ok=1;
				long		index;
				const char*	index_str;
				char*		end;
				Tcl_Obj**	av;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &ac, &av));
				//fprintf(stderr, "descending into array of length %d\n", ac);

				if (Tcl_GetLongFromObj(NULL, step, &index) != TCL_OK) {
					// Index isn't an integer, check for end(+/-int)?
					index_str = Tcl_GetStringFromObj(step, &index_str_len);
					if (index_str_len < 3 || strncmp("end", index_str, 3) != 0)
						ok = 0;

					if (ok) {
						index = ac-1;
						if (index_str_len >= 4) {
							if (index_str[3] != '-' && index_str[3] != '+') {
								ok = 0;
							} else {
								// errno is magically thread-safe on POSIX
								// systems (it's thread-local)
								errno = 0;
								index += strtol(index_str+3, &end, 10);
								if (errno != 0 || *end != 0)
									ok = 0;
							}
						}
					}

					if (!ok)
						THROW_ERROR("Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

					//fprintf(stderr, "Resolved index of %ld from \"%s\"\n", index, index_str);
				} else {
					//fprintf(stderr, "Explicit index: %ld\n", index);
				}
				//fprintf(stderr, "Removing array index %d of %d\n", index, ac);

				if (index < 0) {
					break;
				} else if (index >= ac) {
					break;
				} else {
					TEST_OK(Tcl_ListObjReplace(interp, val, index, 1, 0, NULL));
					//fprintf(stderr, "extracted index %ld: (%s)\n", index, Tcl_GetString(target));
				}
			}
			break;
			//}}}
		case JSON_STRING:
		case JSON_NUMBER:
		case JSON_BOOL:
		case JSON_NULL:
		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			{
				Tcl_Obj* bad_path = NULL;

				Tcl_IncrRefCount(bad_path = Tcl_NewListObj(i+1, pathv));
				Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", Tcl_GetString(bad_path), NULL);
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Attempt to index into atomic type %s at path \"%s\"", type_names[type], Tcl_GetString(bad_path)));
				Tcl_DecrRefCount(bad_path); bad_path = NULL;
				return TCL_ERROR;
			}
		default:
			THROW_ERROR("Unhandled type: ", Tcl_GetString(Tcl_NewIntObj(type)));
	}

	Tcl_InvalidateStringRep(src);

	if (interp)
		Tcl_SetObjResult(interp, src);

	return TCL_OK;

bad_path:
	{
		Tcl_Obj* bad_path = NULL;

		Tcl_IncrRefCount(bad_path = Tcl_NewListObj(i+1, pathv));
		Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", Tcl_GetString(bad_path), NULL);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Path element \"%s\" doesn't exist", Tcl_GetString(bad_path)));
		Tcl_DecrRefCount(bad_path); bad_path = NULL;
		return TCL_ERROR;
	}
}

//}}}
static int resolve_path(Tcl_Interp* interp, Tcl_Obj* src, Tcl_Obj *const pathv[], int pathc, Tcl_Obj** target, const int exists, const int modifiers) //{{{
{
	int					i, modstrlen;
	enum json_types		type;
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	const char*			modstr;
	enum modifiers		modifier;
	Tcl_Obj*			val;
	Tcl_Obj*			step;

#define EXISTS(bool) \
	if (exists) { \
		Tcl_SetObjResult(interp, (bool) ? l->tcl_true : l->tcl_false); \
		return TCL_OK; \
	}

	*target = src;

	if (unlikely(JSON_GetJvalFromObj(interp, *target, &type, &val) != TCL_OK)) {
		if (exists) {
			Tcl_ResetResult(interp);
			// [dict exists] considers any test to be false when applied to an invalid value, so we do the same
			EXISTS(0);
		}
		return TCL_ERROR;
	}

	//fprintf(stderr, "resolve_path, initial type %s\n", type_names[type]);
	for (i=0; i<pathc; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s\n", Tcl_GetString(step));

		if (i == pathc-1) {
			modstr = Tcl_GetStringFromObj(step, &modstrlen);
			if (modifiers && modstrlen >= 1 && modstr[0] == '?') {
				// Allow escaping the modifier char by doubling it
				if (modstrlen >= 2 && modstr[1] == '?') {
					step = Tcl_GetRange(step, 1, modstrlen-1);
					//fprintf(stderr, "escaped modifier, interpreting as step %s\n", Tcl_GetString(step));
				} else {
					TEST_OK(get_modifier(interp, step, &modifier));

					switch (modifier) {
						case MODIFIER_LENGTH: //{{{
							switch (type) {
								case JSON_ARRAY:
									{
										int			ac;
										Tcl_Obj**	av;
										TEST_OK(Tcl_ListObjGetElements(interp, val, &ac, &av));
										EXISTS(1);
										*target = Tcl_NewIntObj(ac);
									}
									break;
								case JSON_STRING:
									EXISTS(1);
									*target = Tcl_NewIntObj(Tcl_GetCharLength(val));
									break;
								case JSON_DYN_STRING:
								case JSON_DYN_NUMBER:
								case JSON_DYN_BOOL:
								case JSON_DYN_JSON:
								case JSON_DYN_TEMPLATE:
								case JSON_DYN_LITERAL:
									EXISTS(1);
									*target = Tcl_NewIntObj(Tcl_GetCharLength(val) + 3);
									break;
								default:
									EXISTS(0);
									THROW_ERROR(Tcl_GetString(step), " modifier is not supported for type ", type_names[type]);
							}
							break;
							//}}}
						case MODIFIER_SIZE: //{{{
							if (type != JSON_OBJECT) {
								EXISTS(0);
								THROW_ERROR(Tcl_GetString(step), " modifier is not supported for type ", type_names[type]);
							}
							{
								int	size;
								TEST_OK(Tcl_DictObjSize(interp, val, &size));
								EXISTS(1);
								*target = Tcl_NewIntObj(size);
							}
							break;
							//}}}
						case MODIFIER_TYPE: //{{{
							EXISTS(1);
							*target = l->type[type];
							break;
							//}}}
						case MODIFIER_KEYS: //{{{
							if (type != JSON_OBJECT) {
								EXISTS(0);
								THROW_ERROR(Tcl_GetString(step), " modifier is not supported for type ", type_names[type]);
							}
							{
								Tcl_DictSearch	search;
								Tcl_Obj*		k;
								Tcl_Obj*		v;
								int				done;
								Tcl_Obj*		res = NULL;

								TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
								if (exists) {
									Tcl_DictObjDone(&search);
									EXISTS(1);
								}

								res = Tcl_NewListObj(0, NULL);
								for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
									if (Tcl_ListObjAppendElement(interp, res, k) != TCL_OK) {
										Tcl_DictObjDone(&search);
										Tcl_IncrRefCount(res); Tcl_DecrRefCount(res); res=NULL; // res is ref 0, free it or it will leak
										return TCL_ERROR;
									}
								}
								Tcl_DictObjDone(&search);
								*target = res;
							}
							break;
							//}}}
						default:
							THROW_ERROR("Unhandled modifier type: ", Tcl_GetString(Tcl_NewIntObj(modifier)));
					}
					//fprintf(stderr, "Handled modifier, skipping descent check\n");
					break;
				}
			}
		}
		switch (type) {
			case JSON_UNDEF: //{{{
				THROW_ERROR("Found JSON_UNDEF type jval following path");
				//}}}
			case JSON_OBJECT: //{{{
				TEST_OK(Tcl_DictObjGet(interp, val, step, target));
				if (*target == NULL) {
					EXISTS(0);
					Tcl_SetObjResult(interp, Tcl_ObjPrintf("Path element %d: \"%s\" not found", pathc+1, Tcl_GetString(step)));
					return TCL_ERROR;
				}

				//TEST_OK(JSON_GetJvalFromObj(interp, src, &type, &val));
				//fprintf(stderr, "Descended into object, new type: %s, val: (%s)\n", type_names[type], Tcl_GetString(val));
				break;
				//}}}
			case JSON_ARRAY: //{{{
				{
					int			ac, index_str_len, ok=1;
					long		index;
					const char*	index_str;
					char*		end;
					Tcl_Obj**	av;

					TEST_OK(Tcl_ListObjGetElements(interp, val, &ac, &av));
					//fprintf(stderr, "descending into array of length %d\n", ac);

					if (Tcl_GetLongFromObj(NULL, step, &index) != TCL_OK) {
						// Index isn't an integer, check for end(-int)?
						index_str = Tcl_GetStringFromObj(step, &index_str_len);
						if (index_str_len < 3 || strncmp("end", index_str, 3) != 0) {
							ok = 0;
						}

						if (ok) {
							index = ac-1;
							if (index_str_len >= 4) {
								if (index_str[3] != '-') {
									ok = 0;
								} else {
									// errno is magically thread-safe on POSIX
									// systems (it's thread-local)
									errno = 0;
									index += strtol(index_str+3, &end, 10);
									if (errno != 0 || *end != 0)
										ok = 0;
								}
							}
						}

						if (!ok)
							THROW_ERROR("Expected an integer index or end(-integer)?, got ", Tcl_GetString(step));

						//fprintf(stderr, "Resolved index of %ld from \"%s\"\n", index, index_str);
					} else {
						//fprintf(stderr, "Explicit index: %ld\n", index);
					}

					if (index < 0 || index >= ac) {
						// Soft error - set target to an NULL object in
						// keeping with [lindex] behaviour
						EXISTS(0);
						*target = JSON_NewJvalObj(JSON_NULL, NULL);
						//fprintf(stderr, "index %ld is out of range [0, %d], setting target to a synthetic null\n", index, ac);
					} else {
						*target = av[index];
						//fprintf(stderr, "extracted index %ld: (%s)\n", index, Tcl_GetString(*target));
					}
				}
				break;
				//}}}
			case JSON_STRING:
			case JSON_NUMBER:
			case JSON_BOOL:
			case JSON_NULL:
			case JSON_DYN_STRING:
			case JSON_DYN_NUMBER:
			case JSON_DYN_BOOL:
			case JSON_DYN_JSON:
			case JSON_DYN_TEMPLATE:
			case JSON_DYN_LITERAL:
				{
					EXISTS(0);
					Tcl_SetObjResult(interp, Tcl_ObjPrintf(
						"Cannot descend into atomic type \"%s\" with path element %d: \"%s\"",
						type_names[type],
						pathc,
						Tcl_GetString(step)
					));
					return TCL_ERROR;
				}
			default:
				THROW_ERROR("Unhandled type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

		TEST_OK(JSON_GetJvalFromObj(interp, *target, &type, &val));
		//fprintf(stderr, "Walked on to new type %s\n", type_names[type]);
	}

	//fprintf(stderr, "Returning target: (%s)\n", Tcl_GetString(*target));
	EXISTS(type != JSON_NULL);
	return TCL_OK;
}

//}}}
static int convert_to_tcl(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj** out) //{{{
{
	enum json_types	type;
	int				res = TCL_OK;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, obj, &type, &val));
	/*
	fprintf(stderr, "Retrieved internal rep of jval: type: %s, intrep Tcl_Obj type: %s, object: %p\n",
			type_names[type], val && val->typePtr ? val->typePtr->name : "<no type>",
			val);
	*/

	switch (type) {
		case JSON_OBJECT:
			{
				int				done;
				Tcl_DictSearch	search;
				Tcl_Obj*		k;
				Tcl_Obj*		v;
				Tcl_Obj*		vo;

				*out = Tcl_NewDictObj();

				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));

				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					if (
							convert_to_tcl(interp, v, &vo) != TCL_OK ||
							Tcl_DictObjPut(interp, *out, k, vo) != TCL_OK
					) {
						res = TCL_ERROR;
						break;
					}
				}
				Tcl_DictObjDone(&search);
			}
			break;

		case JSON_ARRAY:
			{
				int			i, oc;
				Tcl_Obj**	ov;
				Tcl_Obj*	elem;

				*out = Tcl_NewListObj(0, NULL);

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));

				for (i=0; i<oc; i++) {
					TEST_OK(convert_to_tcl(interp, ov[i], &elem));
					TEST_OK(Tcl_ListObjAppendElement(interp, *out, elem));
				}
			}
			break;

		case JSON_STRING:
		case JSON_NUMBER:
		case JSON_BOOL:
			*out = val;
			break;

		case JSON_NULL:
			*out = Tcl_NewObj();
			break;

		// These are all just semantically normal JSON string values in this
		// context
		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			*out = Tcl_ObjPrintf("%s%s", dyn_prefix[type], Tcl_GetString(val));
			break;

		default:
			THROW_ERROR("Invalid value type");
	}

	return res;
}

//}}}
static int _new_object(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], Tcl_Obj** res) //{{{
{
	int			i, ac, retval = TCL_OK;
	Tcl_Obj**	av;
	Tcl_Obj*	k;
	Tcl_Obj*	v;
	Tcl_Obj*	new_val;
	Tcl_Obj*	val;

	if (objc % 2 != 0)
		THROW_ERROR("json new object needs an even number of arguments");

	Tcl_IncrRefCount(val = Tcl_NewDictObj());

	for (i=0; i<objc; i+=2) {
		k = objv[i];
		v = objv[i+1];
		TEST_OK_LABEL(end, retval, Tcl_ListObjGetElements(interp, v, &ac, &av));
		TEST_OK_LABEL(end, retval, new_json_value_from_list(interp, ac, av, &new_val));
		TEST_OK_LABEL(end, retval, Tcl_DictObjPut(interp, val, k, new_val));
	}

	Tcl_IncrRefCount(*res = JSON_NewJvalObj(JSON_OBJECT, val));
end:
	RELEASE(val);
	return retval;
}

//}}}
static void foreach_state_free(struct foreach_state* state) //{{{
{
	unsigned int i, j;

	Tcl_DecrRefCount(state->script);
	state->script = NULL;

	// Close any pending searches
	for (i=0; i<state->iterators; i++) {
		if (state->it[i].search.dictionaryPtr != NULL)
			Tcl_DictObjDone(&state->it[i].search);

		for (j=0; j < state->it[i].var_c; j++)
			Tcl_DecrRefCount(state->it[i].var_v[j]);

		if (state->it[i].varlist != NULL)
			Tcl_DecrRefCount(state->it[i].varlist);
	}

	if (state->it != NULL) {
		Tcl_Free((char*)state->it);
		state->it = NULL;
	}

	if (state->res != NULL) {
		Tcl_DecrRefCount(state->res);
		state->res = NULL;
	}
}

//}}}
static int NRforeach_next_loop_top(Tcl_Interp* interp, struct foreach_state* state) //{{{
{
	struct interp_cx* l = Tcl_GetAssocData(interp, "rl_json", NULL);
	unsigned int j, k;

	//fprintf(stderr, "Starting iteration %d/%d\n", i, max_loops);
	// Set the iterator variables
	for (j=0; j < state->iterators; j++) {
		struct foreach_iterator* this_it = &state->it[j];

		if (this_it->is_array) { // Iterating over a JSON array
			//fprintf(stderr, "Array iteration, data_i: %d, length %d\n", this_it->data_i, this_it->data_c);
			for (k=0; k<this_it->var_c; k++) {
				Tcl_Obj* it_val;

				if (this_it->data_i < this_it->data_c) {
					//fprintf(stderr, "Pulling next element %d off the data list (length %d)\n", this_it->data_i, this_it->data_c);
					it_val = this_it->data_v[this_it->data_i++];
				} else {
					//fprintf(stderr, "Ran out of elements in this data list, setting null\n");
					it_val = l->json_null;
				}
				//fprintf(stderr, "pre  Iteration %d, this_it: %p, setting var %p, varname ref: %d\n",
				//		i, this_it, this_it->var_v[k]/*, Tcl_GetString(this_it->var_v[k])*/, this_it->var_v[k]->refCount);
				//fprintf(stderr, "varname: \"%s\"\n", Tcl_GetString(it[j].var_v[k]));
				//Tcl_ObjSetVar2(interp, this_it->var_v[k], NULL, it_val, 0);
				Tcl_ObjSetVar2(interp, this_it->var_v[k], NULL, it_val, 0);
				//Tcl_ObjSetVar2(interp, Tcl_NewStringObj("elem", 4), NULL, it_val, 0);
				//fprintf(stderr, "post Iteration %d, this_it: %p, setting var %p, varname ref: %d\n",
				//		i, this_it, this_it->var_v[k]/*, Tcl_GetString(this_it->var_v[k])*/, this_it->var_v[k]->refCount);
			}
		} else { // Iterating over a JSON object
			//fprintf(stderr, "Object iteration\n");
			if (!this_it->done) {
				// We check that this_it->var_c == 2 in the setup
				Tcl_ObjSetVar2(interp, this_it->var_v[0], NULL, this_it->k, 0);
				Tcl_ObjSetVar2(interp, this_it->var_v[1], NULL, this_it->v, 0);
				Tcl_DictObjNext(&this_it->search, &this_it->k, &this_it->v, &this_it->done);
			}
		}
	}

	Tcl_NRAddCallback(interp, NRforeach_next_loop_bottom, state, NULL, NULL, NULL);
	return Tcl_NREvalObj(interp, state->script, 0);
}

//}}}
static int NRforeach_next_loop_bottom(ClientData cdata[], Tcl_Interp* interp, int retcode) //{{{
{
	struct foreach_state*	state = (struct foreach_state*)cdata[0];
	struct interp_cx*		l = Tcl_GetAssocData(interp, "rl_json", NULL);
	Tcl_Obj*				it_res = NULL;

	switch (retcode) {
		case TCL_OK:
			switch (state->collecting) {
				case COLLECT_NONE: break;
				case COLLECT_LIST:
					Tcl_IncrRefCount(it_res = Tcl_GetObjResult(interp));
					Tcl_ResetResult(interp);
					TEST_OK_LABEL(done, retcode, Tcl_ListObjAppendElement(interp, state->res, it_res));
					Tcl_DecrRefCount(it_res); it_res = NULL;
					break;

				case COLLECT_ARRAY:
				case COLLECT_OBJECT:
					{
						enum json_types	type;
						Tcl_Obj*		val = NULL;		// Intrep of state->res

						if (Tcl_IsShared(state->res)) {
							Tcl_Obj*	new = NULL;
							Tcl_IncrRefCount(new = Tcl_DuplicateObj(state->res));
							Tcl_DecrRefCount(state->res);
							state->res = new;	// Transfers ref from new to state->res
						}
						TEST_OK_LABEL(done, retcode, JSON_GetJvalFromObj(interp, state->res, &type, &val));

						Tcl_IncrRefCount(it_res = Tcl_GetObjResult(interp));
						Tcl_ResetResult(interp);

						switch (state->collecting) {
							case COLLECT_ARRAY:
								TEST_OK_LABEL(done, retcode, Tcl_ListObjAppendElement(interp, val, as_json(interp, it_res)));
								break;

							case COLLECT_OBJECT:
								if (it_res->typePtr == l->typeDict) { // Iterate over it_res as a dictionary {{{
									Tcl_DictSearch	search;
									Tcl_Obj*		k = NULL;
									Tcl_Obj*		v = NULL;
									int				done;

									TEST_OK_LABEL(done, retcode, Tcl_DictObjFirst(interp, it_res, &search, &k, &v, &done));
									for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
										TEST_OK_LABEL(cleanup_search, retcode, Tcl_DictObjPut(interp, val, k, as_json(interp, v)));
									}

cleanup_search:
									Tcl_DictObjDone(&search);
									if (retcode != TCL_OK) goto done;
									break;
									//}}}
								} else { // Iterate over it_res as a list {{{
									int			oc, i;
									Tcl_Obj**	ov = NULL;

									TEST_OK_LABEL(done, retcode, Tcl_ListObjGetElements(interp, it_res, &oc, &ov));

									if (oc % 2 != 0)
										THROW_ERROR_LABEL(done, retcode, "Iteration result must be a list with an even number of elements");

									for (i=0; i<oc; i+=2)
										TEST_OK_LABEL(done, retcode, Tcl_DictObjPut(interp, val, ov[i], as_json(interp, ov[i+1])));
									//}}}
								}
								break;

							default:
								THROW_ERROR_LABEL(done, retcode, "Unexpect value for collecting");
						}

						if (it_res)
							Tcl_DecrRefCount(it_res); it_res = NULL;
					}
					break;
			}

		case TCL_CONTINUE:
			retcode = TCL_OK;
			break;

		case TCL_BREAK:
			retcode = TCL_OK;
			// falls through
		default:
			goto done;
	}

	state->loop_num++;

	if (state->loop_num < state->max_loops) {
		return NRforeach_next_loop_top(interp, state);
	} else {
		if (state->res != NULL) {
			Tcl_SetObjResult(interp, state->res);
		}
	}

done:
	//fprintf(stderr, "done\n");
	if (it_res != NULL) {
		Tcl_DecrRefCount(it_res);
		it_res = NULL;
	}

	if (retcode == TCL_OK && state->res != NULL /* collecting */)
		Tcl_SetObjResult(interp, state->res);

	foreach_state_free(state);
	Tcl_Free((char*)state);
	state = NULL;

	return retcode;
}

//}}}
static int foreach(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], enum collecting_mode collecting) //{{{
{
	// Caller must ensure that objc is valid
	unsigned int			i;
	int						retcode=TCL_OK;
	struct foreach_state*	state = NULL;

	state = (struct foreach_state*)Tcl_Alloc(sizeof(*state));
	state->iterators = (objc-1)/2;
	state->it = (struct foreach_iterator*)Tcl_Alloc(sizeof(struct foreach_iterator) * state->iterators);
	state->max_loops = 0;
	state->loop_num = 0;
	state->collecting = collecting;

	Tcl_IncrRefCount(state->script = objv[objc-1]);

	switch (state->collecting) {
		case COLLECT_NONE:
			state->res = NULL;
			break;
		case COLLECT_LIST:
			Tcl_IncrRefCount(state->res = Tcl_NewListObj(0, NULL));
			break;
		case COLLECT_ARRAY:
			Tcl_IncrRefCount(state->res = JSON_NewJvalObj(JSON_ARRAY, Tcl_NewListObj(0, NULL)));
			break;
		case COLLECT_OBJECT:
			Tcl_IncrRefCount(state->res = JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj()));
			break;
		default:
			THROW_ERROR_LABEL(done, retcode, "Unhandled value for collecting");
	}

	for (i=0; i<state->iterators; i++) {
		state->it[i].search.dictionaryPtr = NULL;
		state->it[i].data_v = NULL;
		state->it[i].is_array = 0;
		state->it[i].var_v = NULL;
		state->it[i].varlist = NULL;
	}

	for (i=0; i<state->iterators; i++) {
		int				loops, j;
		enum json_types	type;
		Tcl_Obj*		val;
		Tcl_Obj*		varlist = objv[i*2];

		if (Tcl_IsShared(varlist))
			varlist = Tcl_DuplicateObj(varlist);

		Tcl_IncrRefCount(state->it[i].varlist = varlist);

		TEST_OK_LABEL(done, retcode, Tcl_ListObjGetElements(interp, state->it[i].varlist, &state->it[i].var_c, &state->it[i].var_v));
		for (j=0; j < state->it[i].var_c; j++)
			Tcl_IncrRefCount(state->it[i].var_v[j]);

		if (state->it[i].var_c == 0)
			THROW_ERROR_LABEL(done, retcode, "foreach varlist is empty");

		TEST_OK_LABEL(done, retcode, JSON_GetJvalFromObj(interp, objv[i*2+1], &type, &val));
		switch (type) {
			case JSON_ARRAY:
				TEST_OK_LABEL(done, retcode,
						Tcl_ListObjGetElements(interp, val, &state->it[i].data_c, &state->it[i].data_v));
				state->it[i].data_i = 0;
				state->it[i].is_array = 1;
				loops = (int)ceil(state->it[i].data_c / (double)state->it[i].var_c);

				break;

			case JSON_OBJECT:
				if (state->it[i].var_c != 2)
					THROW_ERROR_LABEL(done, retcode, "When iterating over a JSON object, varlist must be a pair of varnames (key value)");

				TEST_OK_LABEL(done, retcode, Tcl_DictObjSize(interp, val, &loops));
				TEST_OK_LABEL(done, retcode, Tcl_DictObjFirst(interp, val, &state->it[i].search, &state->it[i].k, &state->it[i].v, &state->it[i].done));
				break;

			case JSON_NULL:
				state->it[i].data_c = 0;
				state->it[i].data_v = NULL;
				state->it[i].data_i = 0;
				state->it[i].is_array = 1;
				loops = 0;
				break;

			default:
				THROW_ERROR_LABEL(done, retcode, "Cannot iterate over JSON type ", type_names[type]);
		}

		if (loops > state->max_loops)
			state->max_loops = loops;
	}

	if (state->loop_num < state->max_loops)
		return NRforeach_next_loop_top(interp, state);

done:
	//fprintf(stderr, "done\n");
	if (retcode == TCL_OK && state->collecting != COLLECT_NONE)
		Tcl_SetObjResult(interp, state->res);

	foreach_state_free(state);
	Tcl_Free((char*)state);
	state = NULL;

	return retcode;
}

//}}}
static int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent, Tcl_Obj* pad, Tcl_DString* ds) //{{{
{
	int							indent_len, pad_len, next_pad_len, count;
	enum json_types				type;
	const char*					pad_str;
	const char*					next_pad_str;
	Tcl_Obj*					next_pad;
	Tcl_Obj*					val;
	struct serialize_context	scx;

	scx.ds = ds;
	scx.serialize_mode = SERIALIZE_NORMAL;
	scx.fromdict = NULL;
	scx.l = Tcl_GetAssocData(interp, "rl_json", NULL);
	scx.allow_null = 1;

	TEST_OK(JSON_GetJvalFromObj(interp, json, &type, &val));

	Tcl_GetStringFromObj(indent, &indent_len);
	pad_str = Tcl_GetStringFromObj(pad, &pad_len);

	switch (type) {
		case JSON_OBJECT: //{{{
			{
				int				done, k_len, max=0, size;
				Tcl_DictSearch	search;
				Tcl_Obj*		k;
				Tcl_Obj*		v;
				const char*		key_pad_buf = "                    ";	// Must be at least 20 chars long (max cap below)

				TEST_OK(Tcl_DictObjSize(interp, val, &size));
				if (size == 0) {
					Tcl_DStringAppend(ds, "{}", 2);
					break;
				}

				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));

				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					Tcl_GetStringFromObj(k, &k_len);
					if (k_len <= 20 && k_len > max)
						max = k_len;
				}
				Tcl_DictObjDone(&search);

				if (max > 20)
					max = 20;		// If this cap is changed be sure to adjust the key_pad_buf length above

				next_pad = Tcl_DuplicateObj(pad);
				Tcl_AppendObjToObj(next_pad, indent);

				next_pad_str = Tcl_GetStringFromObj(next_pad, &next_pad_len);

				Tcl_DStringAppend(ds, "{\n", 2);

				count = 0;
				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					Tcl_DStringAppend(ds, next_pad_str, next_pad_len);
					append_json_string(&scx, k);
					Tcl_DStringAppend(ds, ": ", 2);

					Tcl_GetStringFromObj(k, &k_len);
					if (k_len < max)
						Tcl_DStringAppend(ds, key_pad_buf, max-k_len);

					if (json_pretty(interp, v, indent, next_pad, ds) != TCL_OK) {
						Tcl_DictObjDone(&search);
						return TCL_ERROR;
					}

					if (++count < size) {
						Tcl_DStringAppend(ds, ",\n", 2);
					} else {
						Tcl_DStringAppend(ds, "\n", 1);
					}
				}
				Tcl_DictObjDone(&search);

				Tcl_DStringAppend(ds, pad_str, pad_len);
				Tcl_DStringAppend(ds, "}", 1);
			}
			break;
			//}}}

		case JSON_ARRAY: //{{{
			{
				int			i, oc;
				Tcl_Obj**	ov;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));

				next_pad = Tcl_DuplicateObj(pad);
				Tcl_AppendObjToObj(next_pad, indent);
				next_pad_str = Tcl_GetStringFromObj(next_pad, &next_pad_len);

				if (oc == 0) {
					Tcl_DStringAppend(ds, "[]", 2);
				} else {
					Tcl_DStringAppend(ds, "[\n", 2);
					count = 0;
					for (i=0; i<oc; i++) {
						Tcl_DStringAppend(ds, next_pad_str, next_pad_len);
						TEST_OK(json_pretty(interp, ov[i], indent, next_pad, ds));
						if (++count < oc) {
							Tcl_DStringAppend(ds, ",\n", 2);
						} else {
							Tcl_DStringAppend(ds, "\n", 1);
						}
					}
					Tcl_DStringAppend(ds, pad_str, pad_len);
					Tcl_DStringAppend(ds, "]", 1);
				}
			}
			break;
			//}}}

		default:
			serialize(interp, &scx, json);
	}

	return TCL_OK;
}

//}}}
static int json_pretty_dbg(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent, Tcl_Obj* pad, Tcl_DString* ds) //{{{
{
	int							indent_len, pad_len, next_pad_len, count;
	enum json_types				type;
	const char*					pad_str;
	const char*					next_pad_str;
	Tcl_Obj*					next_pad;
	Tcl_Obj*					val;
	struct serialize_context	scx;

	scx.ds = ds;
	scx.serialize_mode = SERIALIZE_NORMAL;
	scx.fromdict = NULL;
	scx.l = Tcl_GetAssocData(interp, "rl_json", NULL);
	scx.allow_null = 1;

	TEST_OK(JSON_GetJvalFromObj(interp, json, &type, &val));

	Tcl_GetStringFromObj(indent, &indent_len);
	pad_str = Tcl_GetStringFromObj(pad, &pad_len);

	if (type == JSON_NULL) {
		Tcl_DStringAppend(ds, Tcl_GetString(Tcl_ObjPrintf("(0x%lx[%d]/NULL)",
						(unsigned long)(ptrdiff_t)json, json->refCount)), -1);
	} else {
		Tcl_DStringAppend(ds, Tcl_GetString(Tcl_ObjPrintf("(0x%lx[%d]/0x%lx[%d] %s)",
						(unsigned long)(ptrdiff_t)json, json->refCount,
						(unsigned long)(ptrdiff_t)val, val->refCount, val->typePtr ? val->typePtr->name : "pure string")), -1);
	}

	switch (type) {
		case JSON_OBJECT: //{{{
			{
				int				done, k_len, max=0, size;
				Tcl_DictSearch	search;
				Tcl_Obj*		k;
				Tcl_Obj*		v;
				const char*		key_pad_buf = "                    ";	// Must be at least 20 chars long (max cap below)

				TEST_OK(Tcl_DictObjSize(interp, val, &size));
				if (size == 0) {
					Tcl_DStringAppend(ds, "{}", 2);
					break;
				}

				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));

				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					Tcl_GetStringFromObj(k, &k_len);
					if (k_len <= 20 && k_len > max)
						max = k_len;
				}
				Tcl_DictObjDone(&search);

				if (max > 20)
					max = 20;		// If this cap is changed be sure to adjust the key_pad_buf length above

				next_pad = Tcl_DuplicateObj(pad);
				Tcl_AppendObjToObj(next_pad, indent);

				next_pad_str = Tcl_GetStringFromObj(next_pad, &next_pad_len);

				Tcl_DStringAppend(ds, "{\n", 2);

				count = 0;
				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					Tcl_DStringAppend(ds, next_pad_str, next_pad_len);
					append_json_string(&scx, k);
					Tcl_DStringAppend(ds, ": ", 2);

					Tcl_GetStringFromObj(k, &k_len);
					if (k_len < max)
						Tcl_DStringAppend(ds, key_pad_buf, max-k_len);

					if (json_pretty_dbg(interp, v, indent, next_pad, ds) != TCL_OK) {
						Tcl_DictObjDone(&search);
						return TCL_ERROR;
					}

					if (++count < size) {
						Tcl_DStringAppend(ds, ",\n", 2);
					} else {
						Tcl_DStringAppend(ds, "\n", 1);
					}
				}
				Tcl_DictObjDone(&search);

				Tcl_DStringAppend(ds, pad_str, pad_len);
				Tcl_DStringAppend(ds, "}", 1);
			}
			break;
			//}}}

		case JSON_ARRAY: //{{{
			{
				int			i, oc;
				Tcl_Obj**	ov;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));

				next_pad = Tcl_DuplicateObj(pad);
				Tcl_AppendObjToObj(next_pad, indent);
				next_pad_str = Tcl_GetStringFromObj(next_pad, &next_pad_len);

				if (oc == 0) {
					Tcl_DStringAppend(ds, "[]", 2);
				} else {
					Tcl_DStringAppend(ds, "[\n", 2);
					count = 0;
					for (i=0; i<oc; i++) {
						Tcl_DStringAppend(ds, next_pad_str, next_pad_len);
						TEST_OK(json_pretty_dbg(interp, ov[i], indent, next_pad, ds));
						if (++count < oc) {
							Tcl_DStringAppend(ds, ",\n", 2);
						} else {
							Tcl_DStringAppend(ds, "\n", 1);
						}
					}
					Tcl_DStringAppend(ds, pad_str, pad_len);
					Tcl_DStringAppend(ds, "]", 1);
				}
			}
			break;
			//}}}

		default:
			serialize_json_val(interp, &scx, type, val);
	}

	return TCL_OK;
}

//}}}
#if 0
static int merge(Tcl_Interp* interp, int deep, Tcl_Obj *const orig, Tcl_Obj *const patch, Tcl_Obj **const res) //{{{
{
	Tcl_Obj*		val;
	Tcl_Obj*		pval;
	int				type, ptype, done, retcode=TCL_OK;
	Tcl_DictSearch	search;
	Tcl_Obj*		k;
	Tcl_Obj*		v;
	Tcl_Obj*		orig_v;
	Tcl_Obj*		new_v;

	*res = orig;

	if (*res == NULL) {
		*res = patch;
		return TCL_OK;
	}

	TEST_OK(JSON_GetJvalFromObj(interp, *res, &type, &val));
	TEST_OK(JSON_GetJvalFromObj(interp, patch, &ptype, &pval));

	// In all cases, if the types don't match the patch completely
	// replaces the destination
	if (type != ptype) {
		*res = patch;
		return TCL_OK;
	}

	switch (type) {
		case JSON_UNDEF:
			THROW_ERROR("Tried to merge into JSON_UNDEF");

		case JSON_ARRAY:
		case JSON_STRING:
		case JSON_NUMBER:
		case JSON_BOOL:
		case JSON_NULL:
		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			// For all types other than object, the patch replaces the
			// destination.
			/* Probably not worth it:
			if (type == ptype == JSON_NULL) return TCL_OK;
			*/
			*res = patch;
			return TCL_OK;

		case JSON_OBJECT:
			if (Tcl_IsShared(*res)) {
				*res = Tcl_DuplicateObj(*res);
				TEST_OK(JSON_GetJvalFromObj(interp, *res, &type, &val));
			}

			TEST_OK(Tcl_DictObjFirst(interp, pval, &search, &k, &v, &done));
			for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
				TEST_OK_LABEL(done, retcode,
						Tcl_DictObjGet(interp, val, k, &orig_v));
				TEST_OK_LABEL(done, retcode,
						merge(interp, deep>0? deep-1:deep, orig_v, v, &new_v));

				if (new_v != orig_v)
					TEST_OK_LABEL(done, retcode,
							Tcl_DictObjPut(interp, val, k, new_v));
			}
done:
			Tcl_DictObjDone(&search);
			return retcode;

		default:
			THROW_ERROR("Unsupported JSON type: ", Tcl_GetString(Tcl_NewIntObj(type)));
	}
}

//}}}
#endif
static int prev_opcode(const struct template_cx *const cx) //{{{
{
	int			len, opcode;
	Tcl_Obj*	last = NULL;

	TEST_OK(Tcl_ListObjLength(cx->interp, cx->actions, &len));

	if (len == 0) return NOP;

	TEST_OK(Tcl_ListObjIndex(cx->interp, cx->actions, len-3, &last));
	TEST_OK(Tcl_GetIndexFromObj(cx->interp, last, action_opcode_str, "opcode", TCL_EXACT, &opcode));

	return opcode;
}

//}}}
static int emit_action(const struct template_cx* cx, enum action_opcode opcode, Tcl_Obj *const a, Tcl_Obj *const slot) // TODO: inline? {{{
{
	/*
	fprintf(stderr, "opcode %s: %s %s\n",
			Tcl_GetString(cx->l->action[opcode]),
			a == NULL ? "NULL" : Tcl_GetString(a),
			b == NULL ? "NULL" : Tcl_GetString(b));
			*/

	TEST_OK(Tcl_ListObjAppendElement(cx->interp, cx->actions, cx->l->action[opcode]));
	TEST_OK(Tcl_ListObjAppendElement(cx->interp, cx->actions, a==NULL ? cx->l->tcl_empty : a));
	TEST_OK(Tcl_ListObjAppendElement(cx->interp, cx->actions, slot==NULL ? cx->l->tcl_empty : slot));
	
	return TCL_OK;
}

//}}}
static int emit_fetches(const struct template_cx *const cx) //{{{
{
	Tcl_DictSearch	search;
	Tcl_Obj*		elem;
	Tcl_Obj*		v;
	int				done, retcode=TCL_OK;

	TEST_OK(Tcl_DictObjFirst(cx->interp, cx->map, &search, &elem, &v, &done));
	for (; !done; Tcl_DictObjNext(&search, &elem, &v, &done)) {
		int				len, fetch_idx, types_search_done=0, used_fetch=0;
		Tcl_DictSearch	types_search;
		Tcl_Obj*		type;
		Tcl_Obj*		slot;

		TEST_OK_LABEL(done, retcode,	emit_action(cx, FETCH_VALUE, elem, NULL)				);
		TEST_OK_LABEL(done, retcode,	Tcl_ListObjLength(cx->interp, cx->actions, &len)		);
		fetch_idx = len-3;		// Record the position of the fetch, in case we need to remove it later (DYN_LITERAL)

		TEST_OK_LABEL(done, retcode,	Tcl_DictObjFirst(cx->interp, v, &types_search, &type, &slot, &done));
		for (; !types_search_done; Tcl_DictObjNext(&types_search, &type, &slot, &types_search_done)) {
			int		subst_type;
			TEST_OK_LABEL(done2, retcode,	lookup_type(cx->interp, type, &subst_type));

			if (subst_type != JSON_DYN_LITERAL)
				used_fetch = 1;

			// Each of these actions checks for NULL in value and inserts a JSON null in that case
			switch (subst_type) {
				case JSON_DYN_STRING:
					TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_STRING, NULL, slot)	);
					break;
				case JSON_DYN_JSON:
					TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_JSON, NULL, slot)		);
					break;
				case JSON_DYN_TEMPLATE:
					TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_TEMPLATE, NULL, slot)	);
					break;
				case JSON_DYN_NUMBER:
					TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_NUMBER, NULL, slot)	);
					break;
				case JSON_DYN_BOOL:
					TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_BOOLEAN, NULL, slot)	);
					break;
				case JSON_DYN_LITERAL:
					{
						const char*		s;
						int				len;
						enum json_types	type;

						s = Tcl_GetStringFromObj(elem, &len);
						TEMPLATE_TYPE(s, len, type);	// s is advanced past prefix
						if (type == JSON_STRING) {
							TEST_OK_LABEL(done2, retcode,	emit_action(cx, DECLARE_LITERAL, elem, NULL)	);
							TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_STRING, NULL, slot)		);
						} else {
							TEST_OK_LABEL(done2, retcode,	emit_action(cx, DECLARE_LITERAL,
										JSON_NewJvalObj(type, get_string(cx->l, s, len-3)), NULL)	);
							TEST_OK_LABEL(done2, retcode,	emit_action(cx, STORE_JSON, NULL, slot)			);
						}
					}
					break;
				default:
					Tcl_SetObjResult(cx->interp, Tcl_ObjPrintf("Invalid type \"%s\"", Tcl_GetString(type)));
					retcode = TCL_ERROR;
					goto done2;
			}
		}

		if (!used_fetch)	// Value from fetch wasn't used, drop the FETCH_VALUE opcode
			TEST_OK_LABEL(done, retcode,	Tcl_ListObjReplace(cx->interp, cx->actions, fetch_idx, 3, 0, NULL)	);

done2:
		Tcl_DictObjDone(&types_search);
		if (retcode != TCL_OK) break;
	}

done:
	Tcl_DictObjDone(&search);
	return retcode;
}

//}}}
static int get_subst_slot(struct template_cx* cx, Tcl_Obj* elem, Tcl_Obj* type, int subst_type, Tcl_Obj** slot) //{{{
{
	int			retcode = TCL_OK;
	Tcl_Obj*	keydict = NULL;
	Tcl_Obj*	slotobj = NULL;

	// Find the map for this key
	TEST_OK(Tcl_DictObjGet(cx->interp, cx->map, elem, &keydict));
	if (keydict == NULL) {
		keydict = Tcl_NewDictObj();
		TEST_OK_LABEL(finally, retcode, Tcl_DictObjPut(cx->interp, cx->map, elem, keydict));
	}
	//fprintf(stderr, "get_subst_slot (%s) %s:\n%s\n", Tcl_GetString(elem), Tcl_GetString(type), Tcl_GetString(keydict));

	// Find the allocated slot for this type for this key
	TEST_OK_LABEL(finally, retcode, Tcl_DictObjGet(cx->interp, keydict, type, &slotobj));
	if (slotobj != NULL) {
		Tcl_IncrRefCount(slotobj);
	} else {
		replace_tclobj(&slotobj, Tcl_NewIntObj(cx->slots_used++));
		TEST_OK_LABEL(finally, retcode, Tcl_DictObjPut(cx->interp, keydict, type, slotobj));
		/*
		fprintf(stderr, "Allocated new slot for %s %s: %s\n", Tcl_GetString(elem), Tcl_GetString(type), Tcl_GetString(*slot));
	} else {
		fprintf(stderr, "Found slot for %s %s: %s\n", Tcl_GetString(elem), Tcl_GetString(type), Tcl_GetString(*slot));
		*/
	}

	if (retcode == TCL_OK)
		replace_tclobj(slot, slotobj);

finally:
	release_tclobj(&slotobj);

	return retcode;
}

//}}}
/*
static int record_subst_location(Tcl_Interp* interp, Tcl_Obj* parent, Tcl_Obj* elem, Tcl_Obj* registry, Tcl_Obj* slot) //{{{
{
	Tcl_Obj*	path_info = NULL;

	TEST_OK(Tcl_DictObjGet(interp, registry, parent, &path_info));
	if (path_info == NULL) {
		path_info = Tcl_NewDictObj();
		TEST_OK(Tcl_DictObjPut(interp, registry, parent, path_info));
	}

	TEST_OK(Tcl_DictObjPut(interp, path_info, elem, slot));

	return TCL_OK;
}

//}}}
*/
static int remove_action(Tcl_Interp* interp, struct template_cx* cx, int idx) //{{{
{
	idx *= 3;
	if (idx < 0) {
		int	len;

		TEST_OK(Tcl_ListObjLength(interp, cx->actions, &len));
		idx += len;
	}
	return Tcl_ListObjReplace(interp, cx->actions, idx, 3, 0, NULL);
}

//}}}
static int template_actions(struct template_cx* cx, Tcl_Obj* template, enum action_opcode rep_action, Tcl_Obj* elem) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;
	Tcl_Interp*		interp = cx->interp;
	int				retval = TCL_OK;

	TEST_OK(JSON_GetJvalFromObj(interp, template, &type, &val));

	switch (type) {
		case JSON_STRING:
		case JSON_NUMBER:
		case JSON_BOOL:
		case JSON_NULL:
			break;

		case JSON_OBJECT:
			{
				int				done, retval = TCL_OK;
				Tcl_DictSearch	search;
				Tcl_Obj*		k;
				Tcl_Obj*		v;

				TEST_OK(emit_action(cx, PUSH_TARGET, template, NULL));
				TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					int				len;
					enum json_types	stype;
					const char*		s = Tcl_GetStringFromObj(k, &len);

					TEST_OK_LABEL(free_search, retval, template_actions(cx, v, REPLACE_VAL, k));

					TEMPLATE_TYPE(s, len, stype);	// s is advanced past prefix
					switch (stype) {
						case JSON_STRING:
							break;

						case JSON_DYN_STRING:
						case JSON_DYN_LITERAL:
							{
								Tcl_Obj*	slot = NULL;
								//fprintf(stderr, "Found key subst at \"%s\": (%s) %s %s, allocated slot %s\n", Tcl_GetString(path), Tcl_GetString(k), type_names_int[stype], s+3, Tcl_GetString(slot));
								retval = get_subst_slot(cx, get_string(cx->l, s, len-3), cx->l->type_int[stype], stype, &slot);
								if (retval == TCL_OK)
									retval = emit_action(cx, REPLACE_KEY, k, slot);

								RELEASE(slot);
								if (retval != TCL_OK) goto free_search;
							}
							break;

						default:
							THROW_ERROR("Only strings allowed as object keys");
					}
				}
free_search:
				Tcl_DictObjDone(&search);
				if (prev_opcode(cx) == PUSH_TARGET) {
					remove_action(interp, cx, -1);
				} else {
					TEST_OK(emit_action(cx, POP_TARGET, NULL, NULL));
					TEST_OK(emit_action(cx, rep_action, elem, cx->l->tcl_zero));
				}
				if (retval != TCL_OK) return retval;
			}
			break;

		case JSON_ARRAY:
			{
				int			i, oc;
				Tcl_Obj**	ov;
				Tcl_Obj*	arr_elem = NULL;

				TEST_OK(emit_action(cx, PUSH_TARGET, template, NULL));

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));
				for (i=0; i<oc; i++) {
					REPLACE(arr_elem, Tcl_NewIntObj(i));
					if (TCL_OK != (retval = template_actions(cx, ov[i], REPLACE_ARR, arr_elem)))
						break;
				}

				RELEASE(arr_elem);
				if (retval != TCL_OK) return retval;
				if (prev_opcode(cx) == PUSH_TARGET) {
					remove_action(interp, cx, -1);
				} else {
					TEST_OK(emit_action(cx, POP_TARGET, NULL, NULL));
					TEST_OK(emit_action(cx, rep_action, elem, cx->l->tcl_zero));
				}
			}
			break;

		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			{
				Tcl_Obj*	slot = NULL;

				//fprintf(stderr, "Found value subst at \"%s\": (%s) %s: %s, allocated slot %s\n", Tcl_GetString(parent), Tcl_GetString(elem), type_names_int[type], Tcl_GetString(val), Tcl_GetString(slot));

				retval = get_subst_slot(cx, val, cx->l->type_int[type], type, &slot);
				if (retval == TCL_OK)
					retval = emit_action(cx, rep_action, elem, slot);
				release_tclobj(&slot);
			}
			break;

		default:
			THROW_ERROR("unhandled type: %d", type);
	}

	return retval;
}

//}}}
static int build_template_actions(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj** actions) //{{{
{
	int					retcode=TCL_OK;
	struct template_cx	cx = {	// Unspecified members are initialized to 0
		.interp		= interp,
		.l			= Tcl_GetAssocData(interp, "rl_json", NULL),
		.slots_used	= 1	// slot 0 is the scratch space, where completed targets go when popped
	};

	release_tclobj(actions);

	replace_tclobj(&cx.map,     Tcl_NewDictObj());
	replace_tclobj(&cx.actions, Tcl_NewListObj(0, NULL));

	TEST_OK_LABEL(done, retcode,
		template_actions(&cx, template, REPLACE_ATOM, cx.l->tcl_empty)
	);

	if (cx.slots_used > 1) { // Prepend the template action to allocate the slots
		Tcl_Obj*	actions_tail=NULL;
		int			maxdepth=0;

		// Save the current actions (containing the interpolate and traversal
		// actions), and swap in a new one that we will populate with some
		// prefix actions (allocation and fetching)
		replace_tclobj(&actions_tail, cx.actions);
		replace_tclobj(&cx.actions, Tcl_NewListObj(0, NULL));

		{ // Find max cx stack depth
			int			depth=0, actionc, i;
			Tcl_Obj**	actionv;

			TEST_OK_LABEL(actions_done, retcode,
				Tcl_ListObjGetElements(interp, actions_tail, &actionc, &actionv)
			);

			for (i=0; i<actionc; i+=3) {
				int			opcode;

				TEST_OK_LABEL(actions_done, retcode,
					Tcl_GetIndexFromObj(interp, actionv[i], action_opcode_str, "opcode", TCL_EXACT, &opcode)
				);
				switch (opcode) {
					case PUSH_TARGET:
						if (++depth > maxdepth) maxdepth = depth;
						break;

					case POP_TARGET:
						depth--;
						break;
				}
			}
		}

		TEST_OK_LABEL(actions_done, retcode,
			emit_action(&cx, ALLOCATE, Tcl_NewIntObj(maxdepth), Tcl_NewIntObj(cx.slots_used))
		);

		TEST_OK_LABEL(actions_done, retcode,
			emit_fetches(&cx)
		);

		// Add back on the actions tail
		TEST_OK_LABEL(actions_done, retcode,
			Tcl_ListObjAppendList(interp, cx.actions, actions_tail)
		);

actions_done:
		release_tclobj(&actions_tail);
		if (retcode != TCL_OK) goto done;
	}

	replace_tclobj(actions, cx.actions);
	Tcl_IncrRefCount(*actions);	// DEBUG: trigger obj leak

done:
	release_tclobj(&cx.map);
	release_tclobj(&cx.actions);

	return retcode;
}

//}}}
int lookup_type(Tcl_Interp* interp, Tcl_Obj* typeobj, int* type) //{{{
{
	return Tcl_GetIndexFromObj(interp, typeobj, type_names_int, "type", TCL_EXACT, type);
}

//}}}
/*
static int type_is_dynamic(const enum json_types type) //{{{
{
	switch (type) {
		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			return 1;
		default:
			return 0;
	}
}

//}}}
*/
static inline void fill_slot(Tcl_Obj** slots, int slot, Tcl_Obj* value) //{{{
{
	replace_tclobj(&slots[slot], value);
}

//}}}
static int apply_template_actions(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* actions, Tcl_Obj* dict, Tcl_Obj** res) // dict may be null, which means lookup vars {{{
{
	struct interp_cx* l = NULL;
#define STATIC_SLOTS	10
	Tcl_Obj*	stackslots[STATIC_SLOTS];
	Tcl_Obj**	slots = NULL;
	int			slotslen = 0;
	int			retcode = TCL_OK;
	Tcl_Obj**	actionv;
	int			actionc, i;
#define STATIC_STACK	8
	Tcl_Obj*	stackstack[STATIC_STACK];
	Tcl_Obj**	stack = NULL;
	int			stacklevel = 0;
	Tcl_Obj*	subst_val = NULL;
	Tcl_Obj*	key = NULL;
	int			slot, stacklevels=0;
	Tcl_Obj*	target = NULL;

	TEST_OK_LABEL(finally, retcode, Tcl_ListObjGetElements(interp, actions, &actionc, &actionv));
	if (actionc == 0) {
		replace_tclobj(res, Tcl_DuplicateObj(template));
		Tcl_InvalidateStringRep(*res);		// Some code relies on the fact that the result of the template command is a normalized json doc (no unnecessary whitespace / newlines)
		return TCL_OK;
	}

	if (actionc % 3 != 0)
		THROW_ERROR_LABEL(finally, retcode, "Invalid actions (odd number of elements)");

	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	for (i=0; i<actionc; i+=3) {
		int					tmp;
		enum action_opcode	opcode;
		Tcl_Obj*			a = actionv[i+1];
		Tcl_Obj*			b = actionv[i+2];

		TEST_OK_LABEL(finally, retcode, Tcl_GetIndexFromObj(interp, actionv[i], action_opcode_str, "opcode", TCL_EXACT, &tmp));
		opcode = tmp;
		//fprintf(stderr, "%s (%s) (%s)\n", Tcl_GetString(actionv[i]), Tcl_GetString(a), Tcl_GetString(b));
		switch (opcode) {
			case ALLOCATE: //{{{
				{
					// slots is in b, stack is in a
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slotslen));
					if (slotslen > STATIC_SLOTS) {
						slots = ckalloc(sizeof(Tcl_Obj*) * slotslen);
					} else {
						slots = stackslots;
					}
					if (slotslen > 0)
						memset(slots, 0, sizeof(Tcl_Obj*) * slotslen);

					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, a, &stacklevels));
					if (stacklevels > STATIC_STACK) {
						stack = ckalloc(sizeof(struct Tcl_Obj*) * stacklevels);
					} else {
						stack = stackstack;	// Use the space allocated on the c stack
					}
					if (stacklevels > 0)
						memset(stack, 0, sizeof(Tcl_Obj*) * stacklevels);
				}
				break;
				//}}}
			case FETCH_VALUE: //{{{
				replace_tclobj(&key, a);	// Keep a reference in case we need it for an error message shortly
				if (dict) {
					Tcl_Obj*	new = NULL;
					TEST_OK_LABEL(finally, retcode, Tcl_DictObjGet(interp, dict, a, &new));
					replace_tclobj(&subst_val, new);
				} else {
					replace_tclobj(&subst_val, Tcl_ObjGetVar2(interp, a, NULL, 0));
				}
				break;
				//}}}
			case DECLARE_LITERAL: //{{{
				replace_tclobj(&subst_val, a);
				break;
				//}}}
			case STORE_STRING: //{{{
				TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
				if (subst_val == NULL) {
					fill_slot(slots, slot, l->json_null);
				} else {
					const char*	str;
					int			len;
					Tcl_Obj*	jval=NULL;

					str = Tcl_GetStringFromObj(subst_val, &len);
					if (len == 0) {
						replace_tclobj(&jval, l->json_empty_string);
					} else if (len < 3) {
						replace_tclobj(&jval, JSON_NewJvalObj(JSON_STRING, subst_val));
					} else {
						enum json_types	type;

						TEMPLATE_TYPE(str, len, type);	// str is advanced to after the prefix

						if (type == JSON_STRING) {
							replace_tclobj(&jval, JSON_NewJvalObj(JSON_STRING, subst_val));
						} else {
							replace_tclobj(&jval, JSON_NewJvalObj(type, get_string(l, str, len-3)));
						}
					}
					fill_slot(slots, slot, jval);
					release_tclobj(&jval);
				}
				break;
				//}}}
			case STORE_NUMBER: //{{{
				TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
				if (subst_val == NULL) {
					fill_slot(slots, slot, l->json_null);
				} else {
					Tcl_Obj* forced = NULL;
					
					if (likely((retcode = force_json_number(interp, l, subst_val, &forced)) == TCL_OK))
						fill_slot(slots, slot, JSON_NewJvalObj(JSON_NUMBER, forced));

					release_tclobj(&forced);

					if (unlikely(retcode != TCL_OK)) {
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error substituting value from \"%s\" into template, not a number: \"%s\"", Tcl_GetString(key), Tcl_GetString(subst_val)));
						retcode = TCL_ERROR;
						goto finally;
					}
				}
				break;
				//}}}
			case STORE_BOOLEAN: //{{{
				TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
				if (subst_val == NULL) {
					fill_slot(slots, slot, l->json_null);
				} else {
					int is_true;

					TEST_OK_LABEL(finally, retcode, Tcl_GetBooleanFromObj(interp, subst_val, &is_true));

					fill_slot(slots, slot, is_true ? l->json_true : l->json_false);
				}
				break;
				//}}}
			case STORE_JSON: //{{{
				TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
				if (subst_val == NULL) {
					fill_slot(slots, slot, l->json_null);
				} else {
					TEST_OK_LABEL(finally, retcode, JSON_ForceJSON(interp, subst_val));
					fill_slot(slots, slot, subst_val);
				}
				break;
				//}}}
			case STORE_TEMPLATE: //{{{
				{
					Tcl_Obj*	sub_template_actions = NULL;
					Tcl_Obj*	new = NULL;
					int			slot;

					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
					if (subst_val == NULL) {
						fill_slot(slots, slot, l->json_null);
					} else {
						// recursively fill out sub template
						if (
							TCL_OK == (retcode = build_template_actions(interp, subst_val, &sub_template_actions)) &&
							TCL_OK == (retcode = apply_template_actions(interp, subst_val, sub_template_actions, dict, &new))
						) {
							// Result of a template substitution is guaranteed to be JSON if the return was TCL_OK
							//TEST_OK_LABEL(finally, retcode, JSON_ForceJSON(interp, new));
							fill_slot(slots, slot, new);
							release_tclobj(&new);
						}
						release_tclobj(&sub_template_actions);
						if (retcode != TCL_OK) goto finally;
					}
					break;
				}
				//}}}

			case PUSH_TARGET:
				if (target) Tcl_IncrRefCount(stack[stacklevel++] = target);
				/*
				if (Tcl_IsShared(a))
					a = Tcl_DuplicateObj(a);

				replace_tclobj(&target, a);
					*/
				replace_tclobj(&target, Tcl_DuplicateObj(a));
				break;

			case POP_TARGET:	// save target to slot[0] and pop the parent target off the stack
				fill_slot(slots, 0, target);
				if (stacklevel > 0) {
					Tcl_Obj*	popped = stack[--stacklevel];

					replace_tclobj(&target, popped);
					release_tclobj(&stack[stacklevel]);
				} else {
					release_tclobj(&target);
				}
				break;

			case REPLACE_ARR:
				{
					int				slot, idx;
					Tcl_ObjIntRep*	ir = NULL;
					Tcl_Obj*		ir_obj = NULL;

					// a is idx, b is slot
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, a, &idx));
					if (Tcl_IsShared(target)) {
						THROW_ERROR_LABEL(finally, retcode, "target is shared for REPLACE_ARR");
					}
					ir = Tcl_FetchIntRep(target, g_objtype_for_type[JSON_ARRAY]);
					if (ir == NULL) {
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not fetch array intrep for target array %s", Tcl_GetString(target)));
						retcode = TCL_ERROR;
						goto finally;
					}
					ir_obj = get_unshared_val(ir);
					TEST_OK_LABEL(finally, retcode, Tcl_ListObjReplace(interp, ir_obj, idx, 1, 1, &slots[slot]));
					Tcl_InvalidateStringRep(target);
					release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
				}
				break;

			case REPLACE_VAL:
				{
					int				slot;
					Tcl_ObjIntRep*	ir = NULL;
					Tcl_Obj*		ir_obj = NULL;

					// a is key, b is slot
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
					ir = Tcl_FetchIntRep(target, g_objtype_for_type[JSON_OBJECT]);
					if (ir == NULL) {
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not fetch array intrep for target object %s", Tcl_GetString(target)));
						retcode = TCL_ERROR;
						goto finally;
					}
					ir_obj = get_unshared_val(ir);
					TEST_OK_LABEL(finally, retcode, Tcl_DictObjPut(interp, ir_obj, a, slots[slot]));
					Tcl_InvalidateStringRep(target);
					release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
				}
				break;

			case REPLACE_ATOM:
				{
					int		slot;

					// b is slot
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
					replace_tclobj(&target, slots[slot]);
				}
				break;

			case REPLACE_KEY:
				{
					int				slot;
					Tcl_ObjIntRep*	ir = NULL;
					Tcl_Obj*		ir_obj = NULL;
					Tcl_Obj*		hold = NULL;

					// a is key, b is slot (which holds the new key name)
					TEST_OK_LABEL(finally, retcode, Tcl_GetIntFromObj(interp, b, &slot));
					ir = Tcl_FetchIntRep(target, g_objtype_for_type[JSON_OBJECT]);
					if (ir == NULL) {
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not fetch array intrep for target object %s", Tcl_GetString(target)));
						retcode = TCL_ERROR;
						goto finally;
					}
					ir_obj = get_unshared_val(ir);
					TEST_OK_LABEL(finally, retcode, Tcl_DictObjGet(interp, ir_obj, a, &hold));
					Tcl_IncrRefCount(hold);
					TEST_OK_LABEL(finally, retcode, Tcl_DictObjRemove(interp, ir_obj, a));
					{
						Tcl_Obj*		key_ir_obj = NULL;
						enum json_types	key_type;

						// The value in the slot is a JSON value (JSON_STRING or JSON_DYN_LITERAL), so we need to
						// fetch it's Tcl string (from its intrep)
						TEST_OK_LABEL(finally, retcode, JSON_GetJvalFromObj(interp, slots[slot], &key_type, &key_ir_obj));

						switch (key_type) {
							case JSON_STRING:
								TEST_OK_LABEL(finally, retcode, Tcl_DictObjPut(interp, ir_obj, key_ir_obj, hold));
								break;

							case JSON_DYN_STRING:
							case JSON_DYN_NUMBER:
							case JSON_DYN_BOOL:
							case JSON_DYN_JSON:
							case JSON_DYN_TEMPLATE:
							case JSON_DYN_LITERAL:
									TEST_OK_LABEL(finally, retcode, Tcl_DictObjPut(interp, ir_obj,
												Tcl_ObjPrintf("%s%s", dyn_prefix[key_type], Tcl_GetString(key_ir_obj)), hold));
									break;

							default:
								Tcl_SetObjResult(interp, Tcl_ObjPrintf(
											"Only strings allowed as object keys, got: %s for key \"%s\"",
											Tcl_GetString(slots[slot]),
											Tcl_GetString(a) ));
								//Tcl_SetObjErrorCode(interp, actions);
								retcode = TCL_ERROR;
								goto finally;
						}
					}
					release_tclobj(&hold);
					Tcl_InvalidateStringRep(target);
					release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
				}
				break;

			default:
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unhandled opcode: %s", Tcl_GetString(actionv[i])));
				retcode = TCL_ERROR;
				goto finally;
		}
	}

	replace_tclobj(res, target);

finally:
	if (slots) {
		for (i=0; i<slotslen; i++) release_tclobj(&slots[i]);
		if (slots != stackslots) ckfree(slots);
		slots = NULL;
	}

	release_tclobj(&key);
	release_tclobj(&subst_val);

	if (stack) {
		for (i=0; i<stacklevel; i++) release_tclobj(&stack[i]);
		if (stack != stackstack) ckfree(stack);
		stack = NULL;
	}

	return retcode;
}

//}}}
int JSON_Template(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* dict, Tcl_Obj** res) //{{{
{
	//struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	Tcl_Obj*			actions = NULL;
	int					retcode = TCL_OK;
	Tcl_ObjIntRep*		ir;
	enum json_types		type;

	TEST_OK(JSON_GetIntrepFromObj(interp, template, &type, &ir));

	replace_tclobj(&actions, ir->twoPtrValue.ptr2);
	if (actions == NULL) {
		TEST_OK(build_template_actions(interp, template, &actions));
		replace_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2, actions);
	}

	retcode = apply_template_actions(interp, template, actions, dict, res);
	release_tclobj(&actions);

	return retcode;
}

//}}}

// Ensemble subcommands
static int jsonParse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		res = NULL;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val");
		return TCL_ERROR;
	}

	TEST_OK(JSON_ForceJSON(interp, objv[1]));	// Force parsing objv[1] as JSON
	TEST_OK(convert_to_tcl(interp, objv[1], &res));
	Tcl_SetObjResult(interp, res);

	return TCL_OK;
}

//}}}
static int jsonNormalize(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		json = NULL;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val");
		return TCL_ERROR;
	}

	json = objv[1];

	if (Tcl_IsShared(json))
		json = Tcl_DuplicateObj(json);

	TEST_OK(JSON_ForceJSON(interp, json));
	Tcl_InvalidateStringRep(json);

	// Defer string rep generation to our caller
	Tcl_SetObjResult(interp, json);

	return TCL_OK;
}

//}}}
static int jsonType(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	enum json_types		type;
	Tcl_Obj*			val;
	Tcl_Obj*			target = NULL;
	struct interp_cx*	l = (struct interp_cx*)cdata;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 0));
	} else {
		target = objv[1];
	}

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	Tcl_SetObjResult(interp, l->type[type]);

	return TCL_OK;
}

//}}}
static int jsonLength(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val;
	Tcl_Obj*		target = NULL;

	if (objc < 2) CHECK_ARGS(2, "length json_val ?path ...?");

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 0));
	} else {
		target = objv[1];
	}

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	switch (type) {
		case JSON_ARRAY:
			{
				int			oc;
				Tcl_Obj**	ov = NULL;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));
				Tcl_SetObjResult(interp, Tcl_NewIntObj(oc));
			}
			break;
		case JSON_OBJECT:
			{
				int			size;
				TEST_OK(Tcl_DictObjSize(interp, val, &size));
				Tcl_SetObjResult(interp, Tcl_NewIntObj(size));
			}
			break;
		case JSON_STRING:
			{
				int len;

				Tcl_GetStringFromObj(val, &len);
				Tcl_SetObjResult(interp, Tcl_NewIntObj(len));
			}
			break;
		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:
			{
				int len;

				Tcl_GetStringFromObj(val, &len);
				Tcl_SetObjResult(interp, Tcl_NewIntObj(len+3));
			}
			break;

		default:
			THROW_ERROR("Named JSON value type isn't supported: ", type_names[type]);
	}

	return TCL_OK;
}

//}}}
static int jsonKeys(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	enum json_types	type;
	int				done;
	Tcl_Obj*		val;
	Tcl_Obj*		target = NULL;
	Tcl_Obj*		res = NULL;
	Tcl_Obj*		k = NULL;
	Tcl_Obj*		v = NULL;
	Tcl_DictSearch	search;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 0));
	} else {
		target = objv[1];
	}

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	if (type != JSON_OBJECT)
		THROW_ERROR("Named JSON value type isn't supported: ", type_names[type]);

	Tcl_IncrRefCount(res = Tcl_NewListObj(0, NULL));

	TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
	for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
		if (Tcl_ListObjAppendElement(interp, res, k) != TCL_OK) {
			Tcl_DictObjDone(&search);
			Tcl_DecrRefCount(res); res = NULL;
			return TCL_ERROR;
		}
	}
	Tcl_SetObjResult(interp, res);
	Tcl_DecrRefCount(res); res = NULL;

	return TCL_OK;
}

//}}}
static int jsonExists(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		target = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 1, 1));
		// resolve_path sets the interp result in exists mode
	} else {
		enum json_types	type;
		Tcl_Obj*		val;
		TEST_OK(JSON_GetJvalFromObj(interp, objv[1], &type, &val));
		Tcl_SetObjResult(interp, Tcl_NewBooleanObj(type != JSON_NULL));
	}

	return TCL_OK;
}

//}}}
static int jsonGet(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*	target = NULL;
	Tcl_Obj*	res = NULL;
	int			convert = 1;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		const char*		s = NULL;
		int				l;

		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 1));
		s = Tcl_GetStringFromObj(objv[objc-1], &l);
		if (s[0] == '?' && s[1] != '?') {
			// If the last element of the path is an unquoted
			// modifier, we need to skip the conversion from JSON
			// (it won't be json, but the modifier result)
			convert = 0;
		}
	} else {
		enum json_types	type;
		Tcl_ObjIntRep*	ir;
		target = objv[1];
		TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));	// Force parsing objv[2] as JSON
	}

	if (convert) {
		TEST_OK(convert_to_tcl(interp, target, &res));
	} else {
		res = target;
	}

	Tcl_SetObjResult(interp, res);

	return TCL_OK;
}

//}}}
static int jsonExtract(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		target = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 0));
	} else {
		enum json_types	type;
		Tcl_Obj*		val;
		TEST_OK(JSON_GetJvalFromObj(interp, objv[1], &type, &val));	// Just a validation, keeps the contract that we return JSON
		target = objv[1];
	}

	Tcl_SetObjResult(interp, target);

	return TCL_OK;
}

//}}}
static int jsonSet(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "varname ?path ...? json_val");
		return TCL_ERROR;
	}

	TEST_OK(JSON_Set(interp, objv[1], objv+2, objc-3, objv[objc-1]));

	return TCL_OK;
}

//}}}
static int jsonUnset(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "varname ?path ...?");
		return TCL_ERROR;
	}

	TEST_OK(JSON_Unset(interp, objv[1], objv+2, objc-2));

	return TCL_OK;
}

//}}}
static int jsonNew(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "type ?val?");
		return TCL_ERROR;
	}

	Tcl_Obj*	res = NULL;

	TEST_OK(new_json_value_from_list(interp, objc-1, objv+1, &res));
	Tcl_SetObjResult(interp, res);
	RELEASE(res);

	return TCL_OK;
}

//}}}
static int jsonString(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct interp_cx*	l = (struct interp_cx*)cdata;
	int					len;
	const char*			s;
	enum json_types		type;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}

	s = Tcl_GetStringFromObj(objv[1], &len);
	TEMPLATE_TYPE(s, len, type);	// s is advanced past prefix

	if (type == JSON_STRING) {
		Tcl_SetObjResult(interp, JSON_NewJvalObj(JSON_STRING, get_string(l, s, len)));
	} else {
		Tcl_SetObjResult(interp, JSON_NewJvalObj(type, get_string(l, s, len-3)));
	}

	return TCL_OK;
}

//}}}
static int jsonNumber(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*			forced = NULL;
	struct interp_cx*	l = (struct interp_cx*)cdata;
	int					res = TCL_OK;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}

	if (likely((res = force_json_number(interp, l, objv[1], &forced)) == TCL_OK)) {
		Tcl_SetObjResult(interp, JSON_NewJvalObj(JSON_NUMBER, forced));
	}
	RELEASE(forced);

	return res;
}

//}}}
static int jsonBoolean(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int b;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}

	TEST_OK(Tcl_GetBooleanFromObj(interp, objv[1], &b));
	Tcl_SetObjResult(interp, JSON_NewJvalObj(JSON_BOOL, Tcl_NewBooleanObj(b)));

	return TCL_OK;
}

//}}}
static int jsonObject(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int			oc;
	Tcl_Obj**	ov;
	Tcl_Obj*	res = NULL;

	if (objc == 2) {
		TEST_OK(Tcl_ListObjGetElements(interp, objv[1], &oc, &ov));
		TEST_OK(_new_object(interp, oc, ov, &res));
	} else {
		TEST_OK(_new_object(interp, objc-1, objv+1, &res));
	}
	Tcl_SetObjResult(interp, res);
	RELEASE(res);

	return TCL_OK;
}

//}}}
static int jsonArray(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int			i, ac, retval = TCL_OK;;
	Tcl_Obj**	av;
	Tcl_Obj*	elem = NULL;
	Tcl_Obj*	val = NULL;

	Tcl_IncrRefCount(val = Tcl_NewListObj(0, NULL));

	for (i=1; i<objc; i++) {
		TEST_OK_LABEL(end_new_array, retval, Tcl_ListObjGetElements(interp, objv[i], &ac, &av));
		TEST_OK_LABEL(end_new_array, retval, new_json_value_from_list(interp, ac, av, &elem));
		TEST_OK_LABEL(end_new_array, retval, Tcl_ListObjAppendElement(interp, val, elem));
	}
	Tcl_SetObjResult(interp, JSON_NewJvalObj(JSON_ARRAY, val));

end_new_array:
	RELEASE(val);

	return retval;
}

//}}}
static int jsonDecode(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct interp_cx*	l = (struct interp_cx*)cdata;
	Tcl_Obj*			ov[4];
	int					i, retval;

	if (objc < 2 || objc > 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "bytes ?encoding?");
		return TCL_ERROR;
	}

	ov[0] = l->apply;
	ov[1] = l->decode_bytes;
	ov[2] = objv[1];
	ov[3] = (objc >= 3) ? objv[2] : NULL;

	for (i=0; i<4 && ov[i]; i++) if (ov[i]) Tcl_IncrRefCount(ov[i]);
	retval = Tcl_EvalObjv(interp, i, ov, TCL_EVAL_GLOBAL);
	for (i=0; i<4 && ov[i]; i++) RELEASE(ov[i]);

	return retval;
}

//}}}
static int jsonIsNull(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		target = NULL;
	Tcl_Obj*		val;
	enum json_types	type;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_val ?path ...?");
		return TCL_ERROR;
	}

	if (objc >= 3) {
		TEST_OK(resolve_path(interp, objv[1], objv+2, objc-2, &target, 0, 0));
	} else {
		target = objv[1];
	}

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));

	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(type == JSON_NULL));

	return TCL_OK;
}

//}}}
static int jsonTemplate(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int							res;
	struct serialize_context	scx;
	Tcl_DString					ds;

	if (objc < 2 || objc > 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_template ?source_dict?");
		return TCL_ERROR;
	}

	Tcl_DStringInit(&ds);

	scx.ds = &ds;
	scx.serialize_mode = SERIALIZE_TEMPLATE;
	scx.fromdict = NULL;
	scx.l = (struct interp_cx*)cdata;
	scx.allow_null = 1;

	if (objc >= 3)
		Tcl_IncrRefCount(scx.fromdict = objv[2]);

	res = serialize(interp, &scx, objv[1]);

	RELEASE(scx.fromdict);

	if (res == TCL_OK)
		Tcl_DStringResult(interp, scx.ds);

	Tcl_DStringFree(scx.ds); scx.ds = NULL;

	return res == TCL_OK ? TCL_OK : TCL_ERROR;
}

//}}}
static int jsonTemplateNew(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*	res = NULL;

	if (objc < 2 || objc > 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_template ?source_dict?");
		return TCL_ERROR;
	}

	TEST_OK(JSON_Template(interp, objv[1], objc >= 3 ? objv[2] : NULL, &res));

	if (res == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("res is NULL from JSON_Template!"));
		return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, res);
	RELEASE(res);

	return TCL_OK;
}

//}}}
static int _foreach(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], enum collecting_mode mode) //{{{
{
	if (objc < 4 || (objc-4) % 2 != 0) {
		Tcl_WrongNumArgs(interp, 1, objv, "?varlist datalist ...? script");
		return TCL_ERROR;
	}

	return foreach(interp, objc-1, objv+1, mode);
}

//}}}
static int jsonNRForeach(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return _foreach(cdata, interp, objc, objv, COLLECT_NONE);
}

//}}}
static int jsonForeach(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNRForeach, cdata, objc, objv);
}

//}}}
static int jsonNRLmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return _foreach(cdata, interp, objc, objv, COLLECT_LIST);
}

//}}}
static int jsonLmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNRLmap, cdata, objc, objv);
}

//}}}
static int jsonNRAmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return _foreach(cdata, interp, objc, objv, COLLECT_ARRAY);
}

//}}}
static int jsonAmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNRAmap, cdata, objc, objv);
}

//}}}
static int jsonNROmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return _foreach(cdata, interp, objc, objv, COLLECT_OBJECT);
}

//}}}
static int jsonOmap(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNROmap, cdata, objc, objv);
}

//}}}
static int jsonFreeCache(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct interp_cx*	l = (struct interp_cx*)cdata;

	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}

	free_cache(l);

	return TCL_OK;
}

//}}}
static int jsonNop(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return TCL_OK;
}

//}}}
static int jsonPretty(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int			retval = TCL_OK;
	Tcl_DString	ds;
	Tcl_Obj*	indent = NULL;
	Tcl_Obj*	pad = NULL;

	if (objc < 2 || objc > 3)
		CHECK_ARGS(2, "pretty json_val ?indent?");

	if (objc >= 3) {
		indent = objv[2];
	} else {
		indent = Tcl_NewStringObj("    ", 4);
	}
	Tcl_IncrRefCount(indent);

	Tcl_IncrRefCount(pad = Tcl_NewStringObj("", 0));
	Tcl_DStringInit(&ds);
	if ((retval = json_pretty(interp, objv[1], indent, pad, &ds)) == TCL_OK)
		Tcl_DStringResult(interp, &ds);

	RELEASE(pad);
	RELEASE(indent);
	Tcl_DStringFree(&ds);

	return retval;
}

//}}}
static int jsonDebug(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int			retval = TCL_OK;
	Tcl_DString	ds;
	Tcl_Obj*	indent = NULL;
	Tcl_Obj*	pad = NULL;

	if (objc < 2 || objc > 3)
		CHECK_ARGS(2, "pretty json_val ?indent?");

	if (objc >= 3) {
		indent = objv[2];
	} else {
		indent = Tcl_NewStringObj("    ", 4);
	}
	Tcl_IncrRefCount(indent);

	Tcl_IncrRefCount(pad = Tcl_NewStringObj("", 0));
	Tcl_DStringInit(&ds);
	if ((retval = json_pretty_dbg(interp, objv[1], indent, pad, &ds)) == TCL_OK)
		Tcl_DStringResult(interp, &ds);

	RELEASE(pad);
	RELEASE(indent);
	Tcl_DStringFree(&ds);

	return retval;
}

//}}}
static int jsonTemplateActions(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	Tcl_Obj*		actions = NULL;
	Tcl_ObjIntRep*	ir;
	enum json_types	type;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "json_template");
		return TCL_ERROR;
	}

	TEST_OK(JSON_GetIntrepFromObj(interp, objv[1], &type, &ir));

	replace_tclobj(&actions, ir->twoPtrValue.ptr2);
	if (actions == NULL) {
		TEST_OK(build_template_actions(interp, objv[1], &actions));
		replace_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2, actions);
	}

	Tcl_SetObjResult(interp, actions);
	release_tclobj(&actions);

	return TCL_OK;
}

//}}}
#if 0
static int jsonMerge(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int		i=2, deep=0, checking_flags=1, str_len;
	const char*	str;
	Tcl_Obj*	res = NULL;
	Tcl_Obj*	patch;
	Tcl_Obj*	new;
	static const char* flags[] = {
		"--",
		"-deep",
		(char*)NULL
	};
	enum {
		FLAG_ENDARGS,
		FLAG_DEEP
	};
	int	index;

	THROW_ERROR("merge method is not functional yet, sorry");

	if (objc < 1) CHECK_ARGS(0, "?flag ...? ?json_val ...?");

	while (i < objc) {
		patch = objv[i++];

		// Nasty optimization - prevent generating string rep of
		// a pure JSON value to check if it is a flag (can never
		// be: "-" isn't valid as the first char of a JSON value)
		if (patch->typePtr == &json_type)
			checking_flags = 0;

		if (checking_flags) {
			str = Tcl_GetStringFromObj(patch, &str_len);
			if (str_len > 0 && str[0] == '-') {
				TEST_OK(Tcl_GetIndexFromObj(interp, patch, flags,
							"flag", TCL_EXACT, &index));
				switch (index) {
					case FLAG_ENDARGS: checking_flags = 0; break;
					case FLAG_DEEP:    deep = 1;           break;
					default: THROW_ERROR("Invalid flag");
				}
				continue;
			}
		}

		if (res == NULL) {
			res = patch;
		} else {
			TEST_OK(merge(interp, deep, res, patch, &new));
			if (new != res)
				res = new;
		}
	}

	if (res != NULL)
		Tcl_SetObjResult(interp, res);
}

//}}}
#endif

static int new_json_value_from_list(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], Tcl_Obj** res) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	int		new_type, retval=TCL_OK;
	static const char* types[] = {
		"string",
		"object",
		"array",
		"number",
		"true",
		"false",
		"null",
		"boolean",
		"json",
		(char*)NULL
	};
	enum {
		NEW_STRING,
		NEW_OBJECT,
		NEW_ARRAY,
		NEW_NUMBER,
		NEW_TRUE,
		NEW_FALSE,
		NEW_NULL,
		NEW_BOOL,
		NEW_JSON
	};

	if (objc < 1) CHECK_ARGS(0, "type ?val?");

	TEST_OK(Tcl_GetIndexFromObj(interp, objv[0], types, "type", 0, &new_type));

	switch (new_type) {
		case NEW_STRING:	retval = jsonString(l, interp, objc, objv); break;
		case NEW_OBJECT:	retval = jsonObject(l, interp, objc, objv); break;
		case NEW_ARRAY:		retval = jsonArray( l, interp, objc, objv); break;
		case NEW_NUMBER:	retval = jsonNumber(l, interp, objc, objv); break;
		case NEW_TRUE: //{{{
			CHECK_ARGS(0, "true");
			Tcl_IncrRefCount(*res = l->json_true);
			return TCL_OK;
			//}}}
		case NEW_FALSE: //{{{
			CHECK_ARGS(0, "false");
			Tcl_IncrRefCount(*res = l->json_false);
			return TCL_OK;
			//}}}
		case NEW_NULL: //{{{
			CHECK_ARGS(0, "null");
			Tcl_IncrRefCount(*res = l->json_null);
			return TCL_OK;
			//}}}
		case NEW_BOOL: //{{{
			{
				int b;

				CHECK_ARGS(1, "boolean val");
				TEST_OK(Tcl_GetBooleanFromObj(interp, objv[1], &b));
				Tcl_IncrRefCount(*res = b ? l->json_true : l->json_false);
			}
			return TCL_OK;
			//}}}
		case NEW_JSON: //{{{
			CHECK_ARGS(1, "json val");
			TEST_OK(JSON_ForceJSON(interp, objv[1]));
			Tcl_IncrRefCount(*res = objv[1]);
			return TCL_OK;
			//}}}
		default:
			THROW_ERROR("Invalid new_type: ", Tcl_GetString(Tcl_NewIntObj(new_type)));
	}

	if (retval == TCL_OK) {
		Tcl_IncrRefCount(*res = Tcl_GetObjResult(interp));
		Tcl_ResetResult(interp);
	}
	return retval;
}

//}}}
static int jsonNRObj(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int subcommand;
	static const char *subcommands[] = {
		"parse",		// DEPRECATED
		"normalize",
		"extract",
		"type",
		"length",
		"keys",
		"exists",
		"get",
		"set",
		"unset",
		"new",			// DEPRECATED
		"fmt",			// DEPRECATED
		"isnull",
		"template",
		"_template",
		"foreach",
		"lmap",
		"amap",
		"omap",
		"pretty",
		"debug",
//		"merge",

		// Create json types
		"string",
		"number",
		"boolean",
		"object",
		"array",

		"decode",

		// Debugging
		"free_cache",
		"nop",
		"_leak_obj",
		"_leak_info",
		"template_actions",
		(char*)NULL
	};
	enum {
		M_PARSE,
		M_NORMALIZE,
		M_EXTRACT,
		M_TYPE,
		M_LENGTH,
		M_KEYS,
		M_EXISTS,
		M_GET,
		M_SET,
		M_UNSET,
		M_NEW,
		M_FMT,
		M_ISNULL,
		M_TEMPLATE,
		M_TEMPLATE_NEW,
		M_FOREACH,
		M_LMAP,
		M_AMAP,
		M_OMAP,
		M_PRETTY,
		M_DEBUG,
//		M_MERGE,
		M_STRING,
		M_NUMBER,
		M_BOOLEAN,
		M_OBJECT,
		M_ARRAY,
		M_DECODE,
		// Debugging
		M_FREE_CACHE,
		M_NOP,
		M_LEAK_OBJ,
		M_LEAK_INFO,
		M_TEMPLATE_ACTIONS
	};

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
		return TCL_ERROR;
	}

	TEST_OK(Tcl_GetIndexFromObj(interp, objv[1], subcommands, "subcommand", TCL_EXACT, &subcommand));

	switch (subcommand) {
		case M_PARSE:		return jsonParse(cdata, interp, objc-1, objv+1);
		case M_NORMALIZE:	return jsonNormalize(cdata, interp, objc-1, objv+1);
		case M_TYPE:		return jsonType(cdata, interp, objc-1, objv+1);
		case M_LENGTH:		return jsonLength(cdata, interp, objc-1, objv+1);
		case M_KEYS:		return jsonKeys(cdata, interp, objc-1, objv+1);
		case M_EXISTS:		return jsonExists(cdata, interp, objc-1, objv+1);
		case M_GET:			return jsonGet(cdata, interp, objc-1, objv+1);
		case M_EXTRACT:		return jsonExtract(cdata, interp, objc-1, objv+1);
		case M_SET:			return jsonSet(cdata, interp, objc-1, objv+1);
		case M_UNSET:		return jsonUnset(cdata, interp, objc-1, objv+1);
		case M_FMT:
		case M_NEW:			return jsonNew(cdata, interp, objc-1, objv+1);
		case M_STRING:		return jsonString(cdata, interp, objc-1, objv+1);
		case M_NUMBER:		return jsonNumber(cdata, interp, objc-1, objv+1);
		case M_BOOLEAN:		return jsonBoolean(cdata, interp, objc-1, objv+1);
		case M_OBJECT:		return jsonObject(cdata, interp, objc-1, objv+1);
		case M_ARRAY:		return jsonArray(cdata, interp, objc-1, objv+1);
		case M_DECODE:		return jsonDecode(cdata, interp, objc-1, objv+1);
		case M_ISNULL:		return jsonIsNull(cdata, interp, objc-1, objv+1);
		case M_TEMPLATE:	return jsonTemplate(cdata, interp, objc-1, objv+1);
		case M_TEMPLATE_NEW:return jsonTemplateNew(cdata, interp, objc-1, objv+1);
		case M_FOREACH:		return jsonForeach(cdata, interp, objc-1, objv+1);
		case M_LMAP:		return jsonLmap(cdata, interp, objc-1, objv+1);
		case M_AMAP:		return jsonAmap(cdata, interp, objc-1, objv+1);
		case M_OMAP:		return jsonOmap(cdata, interp, objc-1, objv+1);
		case M_FREE_CACHE:	return jsonFreeCache(cdata, interp, objc-1, objv+1);
		case M_NOP:			return jsonNop(cdata, interp, objc-1, objv+1);
		case M_PRETTY:		return jsonPretty(cdata, interp, objc-1, objv+1);
		case M_DEBUG:		return jsonDebug(cdata, interp, objc-1, objv+1);
	//	case M_MERGE:		return jsonMerge(cdata, interp, objc-1, objv+1);
		case M_TEMPLATE_ACTIONS:	return jsonTemplateActions(cdata, interp, objc-1, objv+1);
		case M_LEAK_OBJ:	Tcl_NewObj(); break;
		case M_LEAK_INFO:
			{
				unsigned long	addr;
				Tcl_Obj*		obj = NULL;
				const char*		s;
				int				len;

				CHECK_ARGS(2, "addr");
				TEST_OK(Tcl_GetLongFromObj(interp, objv[2], (long*)&addr));
				obj = (Tcl_Obj*)addr;
				s = Tcl_GetStringFromObj(obj, &len);
				fprintf(stderr, "\tLeaked obj: %p[%d] len %d: \"%s\"\n", obj, obj->refCount, len, len < 256 ? s : "<too long>");

				break;
			}

		default:
			// Should be impossible to reach
			THROW_ERROR("Invalid subcommand");
	}

	return TCL_OK;
}

//}}}
static int jsonObj(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNRObj, cdata, objc, objv);
}

//}}}

void free_interp_cx(ClientData cdata, Tcl_Interp* interp) //{{{
{
	struct interp_cx* l = cdata;
	int					i;

	l->interp = NULL;

	RELEASE(l->tcl_true);
	RELEASE(l->tcl_false);
	RELEASE(l->tcl_one);
	RELEASE(l->tcl_zero);

	Tcl_DecrRefCount(l->tcl_empty);
	RELEASE(l->tcl_empty);

	RELEASE(l->json_true);
	RELEASE(l->json_false);
	RELEASE(l->json_null);

	free_cache(l);

	for (i=0; i<2; i++)
		RELEASE(l->force_num_cmd[i]);

	for (i=0; i<JSON_TYPE_MAX; i++) {
		RELEASE(l->type_int[i]);
		RELEASE(l->type[i]);
	}

	for (i=0; i<TEMPLATE_ACTIONS_END; i++)
		RELEASE(l->action[i]);

	Tcl_DeleteHashTable(&l->kc);

	RELEASE(l->apply);
	RELEASE(l->decode_bytes);

	free(l); l = NULL;
}

//}}}
static int checkmem(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int					retcode = TCL_OK;
	FILE*				h = NULL;
	char				line[1024];
	Tcl_HashTable		seen;
	Tcl_Obj*			res = NULL;
	char*				before = NULL;
	char*				after = NULL;
#ifdef DEDUP
	struct interp_cx*	l = (struct interp_cx*)cdata;
#endif

	CHECK_ARGS(2, "cmd newactive");

	before = strdup(tmpnam(NULL));
	after = strdup(tmpnam(NULL));

#ifdef DEDUP
	free_cache(l);
#endif
	Tcl_DumpActiveMemory(before);
	//retcode = Tcl_EvalObjEx(interp, objv[1], TCL_EVAL_DIRECT);
	retcode = Tcl_EvalEx(interp, Tcl_GetString(objv[1]), -1, TCL_EVAL_DIRECT);
#ifdef DEDUP
	free_cache(l);
#endif
	Tcl_DumpActiveMemory(after);

	Tcl_InitHashTable(&seen, TCL_STRING_KEYS);
	h = fopen(before, "r");
	while (!feof(h)) {
		int				new;

		fgets(line, 1024, h);
		if (strstr(line, " @ ./") == NULL) continue;
		Tcl_CreateHashEntry(&seen, line, &new);
	}
	fclose(h); h = NULL;
	if (remove(before) != 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error removing before tmp file: %s", Tcl_ErrnoMsg(Tcl_GetErrno())));
		retcode = TCL_ERROR;
		goto finally;
	}

	replace_tclobj(&res, Tcl_NewListObj(0, NULL));

	h = fopen(after, "r");
	while (!feof(h)) {
		int				new, len;

		fgets(line, 1024, h);
		if (strstr(line, " @ ./") == NULL) continue;
		len = strnlen(line, 1024);
		if (line[len-1] == '\n') len--;
		Tcl_CreateHashEntry(&seen, line, &new);
		if (new) {
			retcode = Tcl_ListObjAppendElement(interp, res, Tcl_NewStringObj(line, len));
			if (retcode != TCL_OK) break;
		}
	}
	fclose(h); h = NULL;
	if (remove(after) != 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error removing before tmp file: %s", Tcl_ErrnoMsg(Tcl_GetErrno())));
		retcode = TCL_ERROR;
		goto finally;
	}

	if (retcode == TCL_OK)
		if (Tcl_ObjSetVar2(interp, objv[2], NULL, res, TCL_LEAVE_ERR_MSG) == NULL)
			retcode = TCL_ERROR;

finally:
	if (before) {free(before); before=NULL;}
	if (after) {free(after); after=NULL;}
	release_tclobj(&res);
	if (h) {fclose(h); h = NULL;}
	Tcl_DeleteHashTable(&seen);

	return retcode;
}

//}}}

extern Rl_jsonStubs rl_jsonStubs;
_DLLEXPORT int Rl_json_Init(Tcl_Interp* interp) //{{{
{
	int					i;
	struct interp_cx*	l = NULL;

#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.5", 0) == NULL)
		return TCL_ERROR;
#endif // USE_TCL_STUBS

	TEST_OK(init_types(interp));

	l = (struct interp_cx*)malloc(sizeof *l);
	l->interp = interp;
	Tcl_IncrRefCount(l->tcl_true   = Tcl_NewStringObj("1", 1));
	Tcl_IncrRefCount(l->tcl_false  = Tcl_NewStringObj("0", 1));

	Tcl_IncrRefCount(l->tcl_empty  = Tcl_NewStringObj("", 0));
	// Ensure the empty string rep is considered "shared"
	Tcl_IncrRefCount(l->tcl_empty);

	Tcl_IncrRefCount(l->tcl_one    = Tcl_NewIntObj(1));
	Tcl_IncrRefCount(l->tcl_zero   = Tcl_NewIntObj(0));
	Tcl_IncrRefCount(l->json_true  = JSON_NewJvalObj(JSON_BOOL, l->tcl_true));
	Tcl_IncrRefCount(l->json_false = JSON_NewJvalObj(JSON_BOOL, l->tcl_false));
	Tcl_IncrRefCount(l->json_null  = JSON_NewJvalObj(JSON_NULL, NULL));
	Tcl_IncrRefCount(l->json_empty_string  = JSON_NewJvalObj(JSON_STRING, l->tcl_empty));

	// Hack to ensure a value is a number (could be any of the Tcl number types: double, int, wide, bignum)
	Tcl_IncrRefCount(l->force_num_cmd[0] = Tcl_NewStringObj("::tcl::mathop::+", -1));
	Tcl_IncrRefCount(l->force_num_cmd[1] = Tcl_NewIntObj(0));
	l->force_num_cmd[2] = NULL;

	// Const type name objects
	for (i=0; i<JSON_TYPE_MAX; i++) {
		Tcl_IncrRefCount(l->type_int[i] = Tcl_NewStringObj(type_names_int[i], -1));
		Tcl_IncrRefCount(l->type[i]     = Tcl_NewStringObj(type_names[i], -1));
	}

	// Const template action objects
	for (i=0; i<TEMPLATE_ACTIONS_END; i++)
		Tcl_IncrRefCount(l->action[i] = Tcl_NewStringObj(action_opcode_str[i], -1));

	Tcl_InitHashTable(&l->kc, TCL_STRING_KEYS);
	l->kc_count = 0;
	memset(&l->freemap, 0xFF, sizeof(l->freemap));

	l->typeDict   = Tcl_GetObjType("dict");
	l->typeInt    = Tcl_GetObjType("int");
	l->typeDouble = Tcl_GetObjType("double");
	l->typeBignum = Tcl_GetObjType("bignum");
	if (l->typeDict == NULL) THROW_ERROR("Can't retrieve objType for dict");
	if (l->typeInt == NULL) THROW_ERROR("Can't retrieve objType for int");
	if (l->typeDouble == NULL) THROW_ERROR("Can't retrieve objType for double");
	//if (l->typeBignum == NULL) THROW_ERROR("Can't retrieve objType for bignum");

	Tcl_IncrRefCount(l->apply = Tcl_NewStringObj("apply", 5));
	Tcl_IncrRefCount(l->decode_bytes = Tcl_NewStringObj( // Tcl lambda to decode raw bytes to a unicode string {{{
		"{bytes {encoding auto}} {\n"
		//"		puts \"Decoding using $encoding: [regexp -all -inline .. [binary encode hex $bytes]]\"\n"
		"	set decode_utf16 {{bytes encoding} {\n"
		//"		puts \"Decoding using $encoding: [regexp -all -inline .. [binary encode hex $bytes]]\"\n"
		"		set process_utf16_word {char {\n"
		"			upvar 1 w1 w1  w2 w2\n"
		"\n"
		"			set t	[expr {$char & 0b1111110000000000}]\n"
		"			if {$t == 0b1101100000000000} { # high surrogate\n"
		"				set w1	[expr {$char & 0b0000001111111111}]\n"
		"			} elseif {$t == 0b1101110000000000} { # low surrogate\n"
		"				set w2	[expr {$char & 0b0000001111111111}]\n"
		"			} else {\n"
		//"puts \"emitting [format %x $char]: ([format %c $char])\"\n"
		"				return [format %c $char]\n"
		"			}\n"
		"\n"
		"			if {[info exists w1] && [info exists w2]} {\n"
		"				set char	[expr {($w1 << 10) | $w2 | 0x10000}]\n"
		//"puts [format {W1: %04x, W2: %04x, char: %x} $w1 $w2 $char]\n"
		"				unset -nocomplain w1 w2\n"
		//"puts \"emitting [format %x $char]: ([format %c $char])\"\n"
		"				return [format %c $char]\n"
		"			}\n"
		"\n"
		"			return\n"
		"		}}\n"
		"\n"
		"		if {[string range $encoding 0 1] eq {x }} {\n"
		"			set encoding [string range $encoding 2 end]\n"	// Hack to allow the test suite to force manual decoding
		"		} elseif {$encoding in [encoding names]} {\n"
		"			return [encoding convertfrom $encoding $bytes]\n"
		"		}\n"
		//"		puts \"Manual $encoding decode\"\n"
		"		binary scan $bytes [expr {$encoding eq {utf-16le} ? {su*} : {Su*}}] chars\n"
		//"		puts \"chars:\n\t[join [lmap e $chars {format %04x $e}] \\n\\t]\"\n"
		"		set res	{}\n"
		"		foreach char $chars {\n"
		"			append res [apply $process_utf16_word $char]\n"
		"		}\n"
		"		set res\n"
		"	}}\n"
		"\n"
		"	set decode_utf32 {{bytes encoding} {\n"
		//"		puts \"Decoding using $encoding: [regexp -all -inline .. [binary encode hex $bytes]]\"\n"
		"		if {[string range $encoding 0 1] eq {x }} {\n"
		"			set encoding [string range $encoding 2 end]\n"	// Hack to allow the test suite to force manual decoding
		"		} elseif {$encoding in [encoding names]} {\n"
		"			puts \"$encoding is in \\[encoding names\\], using native\"\n"
		"			return [encoding convertfrom $encoding $bytes]\n"
		"		}\n"
		//"		puts \"Manual $encoding decode\"\n"
		"		binary scan $bytes [expr {$encoding eq {utf-32le} ? {iu*} : {Iu*}}] chars\n"
		//"		puts \"chars:\n\t[join [lmap e $chars {format %04x $e}] \\n\\t]\"\n"
		"		set res	{}\n"
		"		foreach char $chars {\n"
		"			append res [format %c $char]\n"
		"		}\n"
		"		set res\n"
		"	}}\n"
		"\n"
		"	if {$encoding eq {auto}} {\n"
		"		set bom	[binary encode hex [string range $bytes 0 3]]\n"
		"		switch -glob -- $bom {\n"
		"			0000feff { set encoding utf-32be }\n"
		"			fffe0000 { set encoding utf-32le }\n"
		"			feff*    { set encoding utf-16be }\n"
		"			fffe*    { set encoding utf-16le }\n"
		"\n"
		"			efbbbf -\n"
		"			default { # No BOM, or UTF-8 BOM\n"
		"				set encoding utf-8\n"
		"			}\n"
		"		}\n"
		"	}\n"
		"\n"
		"	switch -- $encoding {\n"
		"		utf-8 {\n"
		"			encoding convertfrom utf-8 $bytes\n"
		"		}\n"
		"\n"
		"		{x utf-16le} -\n"
		"		{x utf-16be} -\n"
		"		utf-16le -\n"
		"		utf-16be { apply $decode_utf16 $bytes $encoding }\n"
		"\n"
		"		{x utf-32le} -\n"
		"		{x utf-32be} -\n"
		"		utf-32le -\n"
		"		utf-32be { apply $decode_utf32 $bytes $encoding }\n"
		"\n"
		"		default {\n"
		"			error \"Unsupported encoding \\\"$encoding\\\"\"\n"
		"		}\n"
		"	}\n"
		"}\n" , -1));
	//}}}

	Tcl_SetAssocData(interp, "rl_json", free_interp_cx, l);

	{
		Tcl_Namespace*	ns = NULL;
#if ENSEMBLE
		Tcl_Namespace*	ns_cmd = NULL;
		Tcl_Command		ens_cmd = NULL;
#endif

#define NS	"::rl_json"

		ns = Tcl_CreateNamespace(interp, NS, NULL, NULL);
		TEST_OK(Tcl_Export(interp, ns, "*", 0));

#if ENSEMBLE
#define ENS	NS "::json::"
		ns_cmd = Tcl_CreateNamespace(interp, NS "::json", NULL, NULL);
		ens_cmd = Tcl_CreateEnsemble(interp, NS "::json", ns_cmd, 0);
#if 1
		{
			Tcl_Obj*		subcommands = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("parse",      -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("normalize",  -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("type",       -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("length",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("keys",       -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("exists",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("get",        -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("extract",    -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("set",        -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("unset",      -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("fmt",        -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("new",        -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("string",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("number",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("boolean",    -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("object",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("array",      -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("decode",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("isnull",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("template",   -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("_template",  -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("foreach",    -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("lmap",       -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("amap",       -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("omap",       -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("free_cache", -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("nop",        -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("pretty",     -1));
			Tcl_ListObjAppendElement(NULL, subcommands, Tcl_NewStringObj("debug",      -1));
			Tcl_SetEnsembleSubcommandList(interp, ens_cmd, subcommands);
		}
#endif
		TEST_OK(Tcl_Export(interp, ns_cmd, "*", 0));

		Tcl_CreateObjCommand(interp, ENS "parse",      jsonParse, l, NULL);		// Deprecated
		Tcl_CreateObjCommand(interp, ENS "normalize",  jsonNormalize, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "type",       jsonType, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "length",     jsonLength, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "keys",       jsonKeys, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "exists",     jsonExists, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "get",        jsonGet, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "extract",    jsonExtract, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "set",        jsonSet, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "unset",      jsonUnset, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "fmt",        jsonNew, l, NULL);		// Deprecated
		Tcl_CreateObjCommand(interp, ENS "new",        jsonNew, l, NULL);		// Deprecated
		Tcl_CreateObjCommand(interp, ENS "string",     jsonString, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "number",     jsonNumber, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "boolean",    jsonBoolean, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "object",     jsonObject, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "array",      jsonArray, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "decode",     jsonDecode, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "isnull",     jsonIsNull, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "template",   jsonTemplate, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "_template",  jsonTemplateNew, l, NULL);
		Tcl_NRCreateCommand(interp,  ENS "foreach",    jsonForeach, jsonNRForeach, l, NULL);
		Tcl_NRCreateCommand(interp,  ENS "lmap",       jsonLmap,    jsonNRLmap,    l, NULL);
		Tcl_NRCreateCommand(interp,  ENS "amap",       jsonAmap,    jsonNRAmap,    l, NULL);
		Tcl_NRCreateCommand(interp,  ENS "omap",       jsonOmap,    jsonNROmap,    l, NULL);
		Tcl_CreateObjCommand(interp, ENS "free_cache", jsonFreeCache, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "nop",        jsonNop, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "pretty",     jsonPretty, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "debug",      jsonDebug, l, NULL);
		Tcl_CreateObjCommand(interp, ENS "template_actions",      jsonTemplateActions, l, NULL);
		//Tcl_CreateObjCommand(interp, ENS "merge",      jsonMerge, l, NULL);
#else
		Tcl_NRCreateCommand(interp, "::rl_json::json", jsonObj, jsonNRObj, l, NULL);
#endif

		Tcl_CreateObjCommand(interp, NS "::checkmem", checkmem, l, NULL);
	}

	TEST_OK(Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, &rl_jsonStubs));

	return TCL_OK;
}

//}}}
_DLLEXPORT int Rl_json_SafeInit(Tcl_Interp* interp) //{{{
{
	// No unsafe features
	return Rl_json_Init(interp);
}

//}}}
_DLLEXPORT int Rl_json_Unload(Tcl_Interp* interp, int flags) //{{{
{
	Tcl_Namespace*		ns;

	switch (flags) {
		case TCL_UNLOAD_DETACH_FROM_INTERPRETER:
			//fprintf(stderr, "rl_json detach from interpreter\n");
			Tcl_DeleteAssocData(interp, "rl_json");
			ns = Tcl_FindNamespace(interp, "::rl_json", NULL, TCL_GLOBAL_ONLY);
			if (ns) {
				Tcl_DeleteNamespace(ns);
				ns = NULL;
			}
			break;
		case TCL_UNLOAD_DETACH_FROM_PROCESS:
			//fprintf(stderr, "rl_json detach from process\n");
			Tcl_DeleteAssocData(interp, "rl_json");
			ns = Tcl_FindNamespace(interp, "::rl_json", NULL, TCL_GLOBAL_ONLY);
			if (ns) {
				Tcl_DeleteNamespace(ns);
				ns = NULL;
			}
			break;
		default:
			THROW_ERROR("Unhandled flags");
	}

	return TCL_OK;
}

//}}}
_DLLEXPORT int Rl_json_SafeUnload(Tcl_Interp* interp, int flags) //{{{
{
	// No unsafe features
	return Rl_json_Unload(interp, flags);
}

//}}}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
