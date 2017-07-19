#include "main.h"

static void free_internal_rep(Tcl_Obj* obj);
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest);
static void update_string_rep(Tcl_Obj* obj);
static int set_from_any(Tcl_Interp* interp, Tcl_Obj* obj);

#ifdef WIN32
#define _DLLEXPORT extern DLLEXPORT
#else
#define _DLLEXPORT
#endif

Tcl_ObjType json_type = {
	"JSON",
	free_internal_rep,
	dup_internal_rep,
	update_string_rep,
	set_from_any
};

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
// These are just for debugging
const char* type_names_dbg[] = {
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
/*
*/

enum serialize_modes {
	SERIALIZE_NORMAL,		// We're updating the string rep of a json value or template
	SERIALIZE_TEMPLATE		// We're interpolating values into a template
};

struct serialize_context {
	Tcl_DString*	ds;

	enum serialize_modes	serialize_mode;
	Tcl_Obj*				fromdict;	// NULL if no dict supplied
};

enum modifiers {
	MODIFIER_NONE,
	MODIFIER_LENGTH,	// for arrays and strings: return the length as an int
	MODIFIER_SIZE,		// for objects: return the number of keys as an int
	MODIFIER_TYPE,		// for all types: return the string name as Tcl_Obj
	MODIFIER_KEYS		// for objects: return the keys defined as Tcl_Obj
};

static int new_json_value_from_list(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], Tcl_Obj** res);
static int NRforeach_next_loop_bottom(ClientData cdata[], Tcl_Interp* interp, int retcode);

#ifdef WIN32
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
#else
#define FFSLL   ffsll
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
static void age_cache(struct interp_cx* l) //{{{
{
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	he = Tcl_FirstHashEntry(&l->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		//if (idx >= KC_ENTRIES) Tcl_Panic("age_cache: idx (%ld) is out of bounds, KC_ENTRIES: %d", idx, KC_ENTRIES);
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

	if (length == 0) {
		return l->tcl_empty;
	} else if (length < 0) {
		length = strlen(bytes);
	}

	if (length > STRING_DEDUP_MAX)
		return Tcl_NewStringObj(bytes, length);

	if (likely(bytes[length] == 0)) {
		keyname = bytes;
	} else {
		memcpy(buf, bytes, length);
		buf[length] = 0;
		keyname = buf;
	}
	Tcl_HashEntry* entry = Tcl_CreateHashEntry(&l->kc, keyname, &is_new);

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

static int JSON_GetJvalFromObj(Tcl_Interp* interp, Tcl_Obj* obj, int* type, Tcl_Obj** val) //{{{
{
	if (obj->typePtr != &json_type)
		TEST_OK(set_from_any(interp, obj));

	*type = obj->internalRep.ptrAndLongRep.value;
	*val = obj->internalRep.ptrAndLongRep.ptr;

	return TCL_OK;
}

//}}}
static int JSON_SetIntRep(Tcl_Interp* interp, Tcl_Obj* target, int type, Tcl_Obj* replacement) //{{{
{
	if (Tcl_IsShared(target))
		THROW_ERROR("Called JSON_SetIntRep on a shared object: ", Tcl_GetString(target));

	target->internalRep.ptrAndLongRep.value = type;

	if (target->internalRep.ptrAndLongRep.ptr != NULL)
		Tcl_DecrRefCount((Tcl_Obj*)target->internalRep.ptrAndLongRep.ptr);

	target->internalRep.ptrAndLongRep.ptr = replacement;
	if (target->internalRep.ptrAndLongRep.ptr != NULL)
		Tcl_IncrRefCount((Tcl_Obj*)target->internalRep.ptrAndLongRep.ptr);

	Tcl_InvalidateStringRep(target);

	return TCL_OK;
}

//}}}
Tcl_Obj* JSON_NewJvalObj(int type, Tcl_Obj* val) //{{{
{
	Tcl_Obj*	res = Tcl_NewObj();

	res->typePtr = &json_type;
	res->internalRep.ptrAndLongRep.ptr = NULL;

	switch (type) {
		case JSON_OBJECT:
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
			break;

		default:
			Tcl_Panic("JSON_NewJvalObj, unhandled type: %d", type);
	}

	if (JSON_SetIntRep(NULL, res, type, val) != TCL_OK)
		Tcl_Panic("Couldn't set JSON intrep");

	return res;
}

//}}}

static int force_json_number(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj** forced) //{{{
{
	Tcl_Obj*	cmd;
	int			res;

	cmd = Tcl_NewListObj(0, NULL);
	TEST_OK(Tcl_ListObjAppendElement(interp, cmd, Tcl_NewStringObj("::tcl::mathop::+", -1)));
	TEST_OK(Tcl_ListObjAppendElement(interp, cmd, Tcl_NewIntObj(0)));
	TEST_OK(Tcl_ListObjAppendElement(interp, cmd, obj));

	Tcl_IncrRefCount(cmd);
	res = Tcl_EvalObjEx(interp, cmd, TCL_EVAL_DIRECT);
	Tcl_DecrRefCount(cmd);

	if (res == TCL_OK)
		*forced = Tcl_GetObjResult(interp);

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
				int				v_type = 0;
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
								if (serialize_json_val(interp, scx, stype, Tcl_GetRange(k, 3, l-1)) != TCL_OK) {
									res = TCL_ERROR;
									break;
								}
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
				int			i, oc, first=1;
				Tcl_Obj**	ov;
				Tcl_Obj*	iv = NULL;
				int			v_type = 0;

				TEST_OK(Tcl_ListObjGetElements(interp, val, &oc, &ov));

				Tcl_DStringAppend(ds, "[", 1);
				for (i=0; i<oc; i++) {
					if (!first) {
						Tcl_DStringAppend(ds, ",", 1);
					} else {
						first = 0;
					}
					JSON_GetJvalFromObj(NULL, ov[i], &v_type, &iv);
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
				Tcl_Obj*	subst_val = NULL;
				int			subst_type;
				int			reset_mode = 0;

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
					// TODO: reject a null substitution if we're in an object key context?  Would need an extra flag on the function :(
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
					Tcl_Obj*	forced;

					if (force_json_number(interp, subst_val, &forced) != TCL_OK) {
						Tcl_ResetResult(interp);
						Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error substituting value from \"%s\" into template, not a number: \"%s\"", Tcl_GetString(val), Tcl_GetString(subst_val)));
						return TCL_ERROR;
					}

					if (subst_val != NULL)
						Tcl_DecrRefCount(subst_val);

					Tcl_IncrRefCount(subst_val = forced);
					Tcl_ResetResult(interp);
				}

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
	/*
	fprintf(stderr, "append_to_cx, storing %s: \"%s\"\n",
			type_names[val->internalRep.ptrAndLongRep.value],
			val->internalRep.ptrAndLongRep.ptr == NULL ? "NULL" :
			Tcl_GetString((Tcl_Obj*)val->internalRep.ptrAndLongRep.ptr));
	*/
	switch (cx->container) {
		case JSON_OBJECT:
			//fprintf(stderr, "append_to_cx, cx->hold_key->refCount: %d (%s)\n", cx->hold_key->refCount, Tcl_GetString(cx->hold_key));
			Tcl_DictObjPut(NULL, cx->val->internalRep.ptrAndLongRep.ptr, cx->hold_key, val);
			Tcl_InvalidateStringRep(cx->val);
			Tcl_DecrRefCount(cx->hold_key);
			cx->hold_key = NULL;
			break;

		case JSON_ARRAY:
			//fprintf(stderr, "append_to_cx, appending to list: (%s)\n", Tcl_GetString(val));
			Tcl_ListObjAppendElement(NULL, cx->val->internalRep.ptrAndLongRep.ptr, val);
			Tcl_InvalidateStringRep(cx->val);
			break;

		default:
			cx->val = val;
			Tcl_IncrRefCount(cx->val);
	}
}

//}}}

static int serialize(Tcl_Interp* interp, struct serialize_context* scx, Tcl_Obj* obj) //{{{
{
	int			type = 0, res;
	Tcl_Obj*	val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, obj, &type, &val));

	res = serialize_json_val(interp, scx, type, val);

	// The result of the serialization is left in scx->ds.  Once the caller
	// is done with this value it must be freed with Tcl_DStringFree()
	return res;
}

//}}}

static void free_internal_rep(Tcl_Obj* obj) //{{{
{
	Tcl_Obj*	jv = obj->internalRep.ptrAndLongRep.ptr;

	if (jv == NULL) return;

	Tcl_DecrRefCount(jv); jv = NULL;
}

//}}}
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest) //{{{
{
	dest->typePtr = src->typePtr;
	Tcl_IncrRefCount((Tcl_Obj*)(dest->internalRep.ptrAndLongRep.ptr = Tcl_DuplicateObj((Tcl_Obj*)src->internalRep.ptrAndLongRep.ptr)));
	dest->internalRep.ptrAndLongRep.value = src->internalRep.ptrAndLongRep.value;
}

//}}}
static void update_string_rep(Tcl_Obj* obj) //{{{
{
	struct serialize_context	scx;
	Tcl_DString					ds;

	Tcl_DStringInit(&ds);

	scx.ds = &ds;
	scx.serialize_mode = SERIALIZE_NORMAL;
	scx.fromdict = NULL;

	serialize(NULL, &scx, obj);

	obj->length = Tcl_DStringLength(&ds);
	obj->bytes = ckalloc(obj->length + 1);
	memcpy(obj->bytes, Tcl_DStringValue(&ds), obj->length);
	obj->bytes[obj->length] = 0;

	Tcl_DStringFree(&ds);	scx.ds = NULL;
}

//}}}
static int set_from_any(Tcl_Interp* interp, Tcl_Obj* obj) //{{{
{
	struct interp_cx*	l;
	const unsigned char*	err_at = NULL;
	const char*				errmsg = "Illegal character";
	size_t					char_adj = 0;		// Offset addjustment to account for multibyte UTF-8 sequences
	const unsigned char*	doc;
	enum json_types			type;
	Tcl_Obj*				val;
	const unsigned char*	p;
	const unsigned char*	e;
	const unsigned char*	val_start;
	int						len;
	struct parse_context	cx[CX_STACK_SIZE];

	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	cx[0].prev = NULL;
	cx[0].last = cx;
	cx[0].hold_key = NULL;
	cx[0].container = JSON_UNDEF;
	cx[0].val = NULL;
	cx[0].char_ofs = 0;
	cx[0].closed = 0;

	p = doc = (const unsigned char*)Tcl_GetStringFromObj(obj, &len);
	e = p + len;

	// Skip leading whitespace and comments
	if (skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0) goto whitespace_err;

	while (p < e) {
		if (cx[0].last->container == JSON_OBJECT) { // Read the key if in object mode {{{
			const unsigned char*	key_start = p;
			size_t					key_start_char_adj = char_adj;

			if (value_type(l, doc, p, e, &char_adj, &p, &type, &val) != TCL_OK) goto err;

			switch (type) {
				case JSON_DYN_STRING:
				case JSON_DYN_NUMBER:
				case JSON_DYN_BOOL:
				case JSON_DYN_JSON:
				case JSON_DYN_TEMPLATE:
				case JSON_DYN_LITERAL:
					/* Add back the template format prefix, since we can't store the type
					 * in the dict key.  The template generation code reparses it later.
					 */
					// Can do this because val's ref is on loan from new_stringobj_dedup
					val = Tcl_ObjPrintf("~%c:%s", key_start[2], Tcl_GetString(val));
					// Falls through
				case JSON_STRING:
					Tcl_IncrRefCount(cx[0].last->hold_key = val);
					break;

				default:
					_parse_error(interp, "Object key is not a string", doc, (key_start-doc) - key_start_char_adj);
					goto err;
			}

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

			if (unlikely(*p != ':')) {
				_parse_error(interp, "Expecting : after object key", doc, (p-doc) - char_adj);
				goto err;
			}
			p++;

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		}
		//}}}

		val_start = p;
		if (value_type(l, doc, p, e, &char_adj, &p, &type, &val) != TCL_OK) goto err;

		switch (type) {
			case JSON_OBJECT:
				push_parse_context(cx, JSON_OBJECT, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == '}') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_ARRAY:
				push_parse_context(cx, JSON_ARRAY, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == ']') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_DYN_STRING:
			case JSON_DYN_NUMBER:
			case JSON_DYN_BOOL:
			case JSON_DYN_JSON:
			case JSON_DYN_TEMPLATE:
			case JSON_DYN_LITERAL:
			case JSON_STRING:
			case JSON_BOOL:
			case JSON_NULL:
			case JSON_NUMBER:
				append_to_cx(cx->last, JSON_NewJvalObj(type, val));
				break;

			default:
				free_cx(cx);
				THROW_ERROR("Unexpected json value type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

after_value:	// Yeah, goto.  But the alternative abusing loops was worse
		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		if (p >= e) break;

		switch (cx[0].last->container) { // Handle eof and end-of-context or comma for array and object {{{
			case JSON_OBJECT:
				if (*p == '}') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					_parse_error(interp, "Expecting } or ,", doc, (p-doc) - char_adj);
					goto err;
				}

				p++;
				break;

			case JSON_ARRAY:
				if (*p == ']') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					_parse_error(interp, "Expecting ] or ,", doc, (p-doc) - char_adj);
					goto err;
				}

				p++;
				break;

			default:
				if (unlikely(p < e)) {
					_parse_error(interp, "Trailing garbage after value", doc, (p - doc) - char_adj);
					goto err;
				}
		}

		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		//}}}
	}

	if (unlikely(cx != cx[0].last || !cx[0].closed)) { // Unterminated object or array context {{{
		switch (cx[0].last->container) {
			case JSON_OBJECT:
				_parse_error(interp, "Unterminated object", doc, cx[0].last->char_ofs);
				goto err;

			case JSON_ARRAY:
				_parse_error(interp, "Unterminated array", doc, cx[0].last->char_ofs);
				goto err;
		}
	}
	//}}}

	if (unlikely(cx[0].val == NULL)) {
		err_at = doc;
		errmsg = "No JSON value found";
		goto whitespace_err;
	}

	if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL)
		obj->typePtr->freeIntRepProc(obj);

	obj->typePtr = &json_type;
	obj->internalRep.ptrAndLongRep.value = cx[0].val->internalRep.ptrAndLongRep.value;
	obj->internalRep.ptrAndLongRep.ptr = cx[0].val->internalRep.ptrAndLongRep.ptr;

	// We're transferring the ref from cx[0].val to our intrep
	//Tcl_IncrRefcount(obj->internalRep.ptrAndLongRep.ptr
	//Tcl_DecrRefCount(cx[0].val);
	cx[0].val = NULL;

	return TCL_OK;

whitespace_err:
	_parse_error(interp, errmsg, doc, (err_at - doc) - char_adj);

err:
	free_cx(cx);
	return TCL_ERROR;
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
static int set_path(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc, Tcl_Obj* replacement) //{{{
{
	int				type, i, newtype;
	Tcl_Obj*		val;
	Tcl_Obj*		step;
	Tcl_Obj*		src;
	Tcl_Obj*		target;
	Tcl_Obj*		newval;

	TEST_OK(JSON_GetJvalFromObj(interp, replacement, &newtype, &newval));

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
	fprintf(stderr, "set_path, srcvar: \"%s\", src: \"%s\"\n",
			Tcl_GetString(srcvar), Tcl_GetString(src));
			*/
	target = src;

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	if (val != NULL && Tcl_IsShared(val)) {
		Tcl_DecrRefCount(val);
		val = Tcl_DuplicateObj(val);
		Tcl_IncrRefCount((Tcl_Obj*)(target->internalRep.ptrAndLongRep.ptr = val));
	}

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_dbg[type]);

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

		TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
		//fprintf(stderr, "Followed path element %d: \"%s\", type %s\n", i, Tcl_GetString(step), type_names_dbg[type]);
		if (val != NULL && Tcl_IsShared(val)) {
			Tcl_DecrRefCount(val);
			val = Tcl_DuplicateObj(val);
			Tcl_IncrRefCount((Tcl_Obj*)(target->internalRep.ptrAndLongRep.ptr = val));
		}
		//fprintf(stderr, "Walked on to new type %s\n", type_names[type]);
	}

	goto set_val;

followed_path:
	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	//fprintf(stderr, "Followed path element %d: \"%s\", type %s\n", i, Tcl_GetString(step), type_names_dbg[type]);
	if (val != NULL && Tcl_IsShared(val)) {
		Tcl_DecrRefCount(val);
		val = Tcl_DuplicateObj(val);
		Tcl_IncrRefCount((Tcl_Obj*)(target->internalRep.ptrAndLongRep.ptr = val));
	}

	// target points at the (first) object to replace.  It and its internalRep
	// are unshared

	// If any path elements remain then they need to be created as object
	// keys
	//fprintf(stderr, "After walking path, %d elements remain to be created\n", pathc-i);
	for (; i<pathc; i++) {
		//fprintf(stderr, "create walk %d: %s, cx type: %s\n", i, Tcl_GetString(pathv[i]), type_names_dbg[type]);
		if (type != JSON_OBJECT) {
			//fprintf(stderr, "Type isn't JSON_OBJECT: %s, replacing with a JSON_OBJECT\n", type_names_dbg[type]);
			if (val != NULL)
				Tcl_DecrRefCount(val);
			val = Tcl_NewDictObj();
			TEST_OK(JSON_SetIntRep(interp, target, JSON_OBJECT, val));
		}

		target = JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj());
		//fprintf(stderr, "Adding key \"%s\"\n", Tcl_GetString(pathv[i]));
		TEST_OK(Tcl_DictObjPut(interp, val, pathv[i], target));
		TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
		//fprintf(stderr, "Newly added key \"%s\" is of type %s\n", Tcl_GetString(pathv[i]), type_names_dbg[type]);
		// This was just created - it can't be shared
	}

set_val:
	//fprintf(stderr, "Reached end of path, calling JSON_SetIntRep for replacement value %s (%s), target is %s\n",
	//		type_names_dbg[newtype], Tcl_GetString(replacement), type_names_dbg[type]);
	TEST_OK(JSON_SetIntRep(interp, target, newtype, newval));

	Tcl_InvalidateStringRep(src);

	if (interp)
		Tcl_SetObjResult(interp, src);

	return TCL_OK;
}

//}}}
static int unset_path(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc) //{{{
{
	int				type, i;
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
	fprintf(stderr, "set_path, srcvar: \"%s\", src: \"%s\"\n",
			Tcl_GetString(srcvar), Tcl_GetString(src));
			*/
	target = src;

	TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
	if (val != NULL && Tcl_IsShared(val)) {
		Tcl_DecrRefCount(val);
		val = Tcl_DuplicateObj(val);
		Tcl_IncrRefCount((Tcl_Obj*)(target->internalRep.ptrAndLongRep.ptr = val));
	}

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc-1; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_dbg[type]);

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

		TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
		//fprintf(stderr, "Followed path element %d: \"%s\", type %s\n", i, Tcl_GetString(step), type_names_dbg[type]);
		if (val != NULL && Tcl_IsShared(val)) {
			Tcl_DecrRefCount(val);
			val = Tcl_DuplicateObj(val);
			Tcl_IncrRefCount((Tcl_Obj*)(target->internalRep.ptrAndLongRep.ptr = val));
		}
		//fprintf(stderr, "Walked on to new type %s\n", type_names[type]);
	}

	//fprintf(stderr, "Reached end of path, calling JSON_SetIntRep for replacement value %s (%s), target is %s\n",
	//		type_names_dbg[newtype], Tcl_GetString(replacement), type_names_dbg[type]);

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
				const char* bad_path_str = Tcl_GetString(Tcl_NewListObj(i+1, pathv));
				Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", bad_path_str, NULL);
				THROW_ERROR("Attempt to index into atomic type ", type_names[type], " at path \"", bad_path_str, "\"");
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
		const char* bad_path_str = Tcl_GetString(Tcl_NewListObj(i+1, pathv));
		Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", bad_path_str, NULL);
		THROW_ERROR("Path element \"", bad_path_str, "\" doesn't exist");
	}
}

//}}}
static int resolve_path(Tcl_Interp* interp, Tcl_Obj* src, Tcl_Obj *const pathv[], int pathc, Tcl_Obj** target, int exists) //{{{
{
	int				type, i, modstrlen;
	const char*		modstr;
	enum modifiers	modifier;
	Tcl_Obj*		val;
	Tcl_Obj*		step;

#define EXISTS(bool) \
	if (exists) { \
		Tcl_SetObjResult(interp, Tcl_NewBooleanObj(bool)); return TCL_OK; \
	}

	*target = src;

	if (unlikely(JSON_GetJvalFromObj(interp, *target, &type, &val) != TCL_OK)) {
		if (exists) {
			Tcl_ResetResult(interp);
			// [dict exists] considers any test to be false when applied to an invalid value, so we do the same
			EXISTS(0);
			return TCL_OK;
		}
		return TCL_ERROR;
	}

	//fprintf(stderr, "resolve_path, initial type %s\n", type_names[type]);
	for (i=0; i<pathc; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s\n", Tcl_GetString(step));

		if (i == pathc-1) {
			modstr = Tcl_GetStringFromObj(step, &modstrlen);
			if (modstrlen >= 1 && modstr[0] == '?') {
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
							*target = Tcl_NewStringObj(type_names[type], -1);
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
								Tcl_Obj*		res = Tcl_NewListObj(0, NULL);

								TEST_OK(Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
								if (exists) {
									Tcl_DictObjDone(&search);
									EXISTS(1);
								}

								for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
									if (Tcl_ListObjAppendElement(interp, res, k) != TCL_OK) {
										Tcl_DictObjDone(&search);
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
					THROW_ERROR(
							"Path element ",
							Tcl_GetString(Tcl_NewIntObj(pathc+1)),
							": \"", Tcl_GetString(step), "\" not found");
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
				EXISTS(0);
				THROW_ERROR("Cannot descend into atomic type \"",
						type_names[type],
						"\" with path element ",
						Tcl_GetString(Tcl_NewIntObj(pathc)),
						": \"", Tcl_GetString(step), "\"");
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
	int			type, res = TCL_OK;
	Tcl_Obj*	val = NULL;

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
	int			i, ac;
	Tcl_Obj**	av;
	Tcl_Obj*	k;
	Tcl_Obj*	v;
	Tcl_Obj*	new_val;
	Tcl_Obj*	val;

	if (objc % 2 != 0)
		THROW_ERROR("json fmt object needs an even number of arguments");

	*res = JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj());
	val = ((Tcl_Obj*)*res)->internalRep.ptrAndLongRep.ptr;

	for (i=0; i<objc; i+=2) {
		k = objv[i];
		v = objv[i+1];
		TEST_OK(Tcl_ListObjGetElements(interp, v, &ac, &av));
		TEST_OK(new_json_value_from_list(interp, ac, av, &new_val));
		TEST_OK(Tcl_DictObjPut(interp, val, k, new_val));
	}

	return TCL_OK;
}

//}}}
static int new_json_value_from_list(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], Tcl_Obj** res) //{{{
{
	int		new_type;
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
		case NEW_STRING: //{{{
			{
				int			l, type;
				const char*	s;
				CHECK_ARGS(1, "string val");
				s = Tcl_GetStringFromObj(objv[1], &l);
				if (
						l >= 3 &&
						s[0] == '~' &&
						s[2] == ':'
				) {
					switch (s[1]) {
						case 'S': type = JSON_DYN_STRING; break;
						case 'N': type = JSON_DYN_NUMBER; break;
						case 'B': type = JSON_DYN_BOOL; break;
						case 'J': type = JSON_DYN_JSON; break;
						case 'T': type = JSON_DYN_TEMPLATE; break;
						case 'L': type = JSON_DYN_LITERAL; break;
						default:  type = JSON_UNDEF; break;
					}

					if (type != JSON_UNDEF) {
						*res = JSON_NewJvalObj(type, Tcl_NewStringObj((const char*)s+3, l-3));
						break;
					}
				}
				*res = JSON_NewJvalObj(JSON_STRING, Tcl_NewStringObj(s, l));
			}
			break;
			//}}}
		case NEW_OBJECT: //{{{
			{
				int			oc;
				Tcl_Obj**	ov;

				if (objc == 2) {
					TEST_OK(Tcl_ListObjGetElements(interp, objv[1], &oc, &ov));
					TEST_OK(_new_object(interp, oc, ov, res));
				} else {
					TEST_OK(_new_object(interp, objc-1, objv+1, res));
				}
			}
			break;
			//}}}
		case NEW_ARRAY: //{{{
			{
				int			i, ac;
				Tcl_Obj**	av;
				Tcl_Obj*	elem;
				Tcl_Obj*	val;

				*res = JSON_NewJvalObj(JSON_ARRAY, Tcl_NewListObj(0, NULL));
				val = ((Tcl_Obj*)*res)->internalRep.ptrAndLongRep.ptr;
				for (i=1; i<objc; i++) {
					TEST_OK(Tcl_ListObjGetElements(interp, objv[i], &ac, &av));
					TEST_OK(new_json_value_from_list(interp, ac, av, &elem));
					TEST_OK(Tcl_ListObjAppendElement(interp, val, elem));
				}
			}
			break;
			//}}}
		case NEW_NUMBER: //{{{
			{
				Tcl_Obj*	forced;

				CHECK_ARGS(1, "number val");
				TEST_OK(force_json_number(interp, objv[1], &forced));
				*res = JSON_NewJvalObj(JSON_NUMBER, forced);
			}
			break;
			//}}}
		case NEW_TRUE: //{{{
			{
				CHECK_ARGS(0, "true");
				*res = JSON_NewJvalObj(JSON_BOOL, Tcl_NewBooleanObj(1));
			}
			break;
			//}}}
		case NEW_FALSE: //{{{
			{
				CHECK_ARGS(0, "false");
				*res = JSON_NewJvalObj(JSON_BOOL, Tcl_NewBooleanObj(0));
			}
			break;
			//}}}
		case NEW_NULL: //{{{
			CHECK_ARGS(0, "null");
			*res = JSON_NewJvalObj(JSON_NULL, NULL);
			break;
			//}}}
		case NEW_BOOL: //{{{
			{
				int b;

				CHECK_ARGS(1, "boolean val");
				TEST_OK(Tcl_GetBooleanFromObj(interp, objv[1], &b));
				*res = JSON_NewJvalObj(JSON_BOOL, Tcl_NewBooleanObj(b));
			}
			break;
			//}}}
		case NEW_JSON: //{{{
			{
				int _type;
				Tcl_Obj *_val;

				CHECK_ARGS(1, "json val");
				TEST_OK(JSON_GetJvalFromObj(interp, objv[1], &_type, &_val));
				*res = objv[1];
				Tcl_IncrRefCount(objv[1]);
			}
			break;
			//}}}
		default:
			THROW_ERROR("Invalid new_type: ", Tcl_GetString(Tcl_NewIntObj(new_type)));
	}

	return TCL_OK;
}

//}}}
static void foreach_state_free(struct foreach_state* state) //{{{
{
	unsigned i;

	Tcl_DecrRefCount(state->script);
	state->script = NULL;

	// Close any pending searches
	for (i=0; i<state->iterators; i++)
		if (state->it[i].search.dictionaryPtr != NULL)
			Tcl_DictObjDone(&state->it[i].search);

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
	unsigned j;
	int k;

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
					struct interp_cx* l = Tcl_GetAssocData(interp, "rl_json", NULL);
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

	switch (retcode) {
		case TCL_OK:
			if (state->res != NULL) // collecting
				TEST_OK_LABEL(done, retcode, Tcl_ListObjAppendElement(interp, state->res, Tcl_GetObjResult(interp)));
			break;

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
	if (retcode == TCL_OK && state->res != NULL /* collecting */)
		Tcl_SetObjResult(interp, state->res);

	foreach_state_free(state);
	Tcl_Free((char*)state);
	state = NULL;

	return retcode;
}

//}}}
static int foreach(Tcl_Interp* interp, int objc, Tcl_Obj *const objv[], int collecting) //{{{
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

	Tcl_IncrRefCount(state->script = objv[objc-1]);

	if (collecting) {
		Tcl_IncrRefCount(state->res = Tcl_NewListObj(0, NULL));
	} else {
		state->res = NULL;
	}

	for (i=0; i<state->iterators; i++) {
		state->it[i].search.dictionaryPtr = NULL;
		state->it[i].data_v = NULL;
		state->it[i].is_array = 0;
		state->it[i].var_v = NULL;
	}

	for (i=0; i<state->iterators; i++) {
		int			loops, type;
		Tcl_Obj*	val;
		Tcl_Obj*	varlist = objv[i*2];

		if (Tcl_IsShared(varlist))
			varlist = Tcl_DuplicateObj(varlist);

		TEST_OK_LABEL(done, retcode, Tcl_ListObjGetElements(interp, varlist, &state->it[i].var_c, &state->it[i].var_v));

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
	if (retcode == TCL_OK && collecting)
		Tcl_SetObjResult(interp, state->res);

	foreach_state_free(state);
	Tcl_Free((char*)state);
	state = NULL;

	return retcode;
}

//}}}
static int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent, Tcl_Obj* pad, Tcl_DString* ds) //{{{
{
	int							type, indent_len, pad_len, next_pad_len, count;
	const char*					pad_str;
	const char*					next_pad_str;
	Tcl_Obj*					next_pad;
	Tcl_Obj*					val;
	struct serialize_context	scx;

	scx.ds = ds;
	scx.serialize_mode = SERIALIZE_NORMAL;
	scx.fromdict = NULL;

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
static int jsonNRObjCmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	int method, retcode=TCL_OK;
	static const char *methods[] = {
		"parse",
		"normalize",
		"extract",
		"type",
		"exists",
		"get",
		"get_typed",
		"set",
		"unset",
		"new",
		"fmt",
		"isnull",
		"template",
		"foreach",
		"lmap",
		"pretty",
//		"merge",

		// Debugging
		"nop",
		(char*)NULL
	};
	enum {
		M_PARSE,
		M_NORMALIZE,
		M_EXTRACT,
		M_TYPE,
		M_EXISTS,
		M_GET,
		M_GET_TYPED,
		M_SET,
		M_UNSET,
		M_NEW,
		M_FMT,
		M_ISNULL,
		M_TEMPLATE,
		M_FOREACH,
		M_LMAP,
		M_PRETTY,
//		M_MERGE,

		// Debugging
		M_NOP
	};

	if (objc < 2)
		CHECK_ARGS(1, "method ?arg ...?");

	TEST_OK(Tcl_GetIndexFromObj(interp, objv[1], methods, "method", TCL_EXACT, &method));

	switch (method) {
		case M_PARSE: //{{{
			CHECK_ARGS(2, "parse json_val");
			{
				Tcl_Obj*	res = NULL;
				TEST_OK(convert_to_tcl(interp, objv[2], &res));
				Tcl_SetObjResult(interp, res);
			}
			break;
			//}}}
		case M_NORMALIZE: //{{{
			CHECK_ARGS(2, "normalize json_val");
			{
				int			type;
				Tcl_Obj*	json = objv[2];
				Tcl_Obj*	val;

				if (Tcl_IsShared(json))
					json = Tcl_DuplicateObj(json);

				TEST_OK(JSON_GetJvalFromObj(interp, json, &type, &val));
				Tcl_InvalidateStringRep(json);

				// Defer string rep generation to our caller
				Tcl_SetObjResult(interp, json);
			}
			break;
			//}}}
		case M_TYPE: //{{{
			{
				int			type;
				Tcl_Obj*	val;
				CHECK_ARGS(2, "type json_val");
				TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
				Tcl_SetObjResult(interp, Tcl_NewStringObj(type_names[type], -1));
			}
			break;
			//}}}
		case M_EXISTS: //{{{
			{
				Tcl_Obj*		target = NULL;

				if (objc < 3) CHECK_ARGS(2, "exists json_val ?path ...?");

				if (objc >= 4) {
					TEST_OK(resolve_path(interp, objv[2], objv+3, objc-3, &target, 1));
					// resolve_path sets the interp result in exists mode
				} else {
					int			type;
					Tcl_Obj*	val;
					TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
					Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
				}
			}
			break;
			//}}}
		case M_GET: //{{{
			{
				Tcl_Obj*	target = NULL;
				Tcl_Obj*	res = NULL;

				if (objc < 3) CHECK_ARGS(2, "get json_val ?path ...?");

				if (objc >= 4) {
					TEST_OK(resolve_path(interp, objv[2], objv+3, objc-3, &target, 0));
				} else {
					int			type;
					Tcl_Obj*	val;
					TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
					target = objv[2];
				}

				// Might be the result of a modifier
				if (target->typePtr == &json_type) {
					TEST_OK(convert_to_tcl(interp, target, &res));
					target = res;
				}

				Tcl_SetObjResult(interp, target);
			}
			break;
			//}}}
		case M_GET_TYPED: //{{{
			{
				Tcl_Obj*		target = NULL;
				Tcl_Obj*		res[2];
				int				rescount;

				if (objc < 3) CHECK_ARGS(2, "get_typed json_val ?path ...?");

				if (objc >= 4) {
					TEST_OK(resolve_path(interp, objv[2], objv+3, objc-3, &target, 0));
				} else {
					int			type;
					Tcl_Obj*	val;
					TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
					target = objv[2];
				}

				// Might be the result of a modifier
				if (target->typePtr == &json_type) {
					int				type;
					Tcl_Obj*		val;

					TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));
					TEST_OK(convert_to_tcl(interp, target, &target));
					res[0] = target;
					res[1] = Tcl_NewStringObj(type_names[type], -1);
					rescount = 2;
				} else {
					res[0] = target;
					rescount = 1;
				}

				Tcl_SetObjResult(interp, Tcl_NewListObj(rescount, res));
			}
			break;
			//}}}
		case M_EXTRACT: //{{{
			{
				Tcl_Obj*		target = NULL;

				if (objc < 3) CHECK_ARGS(2, "extract json_val ?path ...?");

				if (objc >= 4) {
					TEST_OK(resolve_path(interp, objv[2], objv+3, objc-3, &target, 0));
				} else {
					int			type;
					Tcl_Obj*	val;
					TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
					target = objv[2];
				}

				Tcl_SetObjResult(interp, target);
			}
			break;
			//}}}
		case M_SET: //{{{
			if (objc < 4) CHECK_ARGS(5, "set varname ?path ...? json_val");
			TEST_OK(set_path(interp, objv[2], objv+3, objc-4, objv[objc-1]));
			break;
			//}}}
		case M_UNSET: //{{{
			if (objc < 3) CHECK_ARGS(4, "unset varname ?path ...?");
			TEST_OK(unset_path(interp, objv[2], objv+3, objc-3));
			break;
			//}}}
		case M_FMT:
		case M_NEW: //{{{
			{
				Tcl_Obj*	res = NULL;

				if (objc < 3) CHECK_ARGS(2, "new type ?val?");

				TEST_OK(new_json_value_from_list(interp, objc-2, objv+2, &res));

				Tcl_SetObjResult(interp, res);
			}
			break;
			//}}}
		case M_ISNULL: //{{{
			{
				Tcl_Obj*	target = NULL;
				Tcl_Obj*	val;
				int			type;

				if (objc < 3) CHECK_ARGS(2, "isnull json_val ?path ...?");

				if (objc >= 4) {
					TEST_OK(resolve_path(interp, objv[2], objv+3, objc-3, &target, 0));
				} else {
					int			type;
					Tcl_Obj*	val;
					TEST_OK(JSON_GetJvalFromObj(interp, objv[2], &type, &val));
					target = objv[2];
				}

				TEST_OK(JSON_GetJvalFromObj(interp, target, &type, &val));

				Tcl_SetObjResult(interp, Tcl_NewBooleanObj(type == JSON_NULL));
			}
			break;
			//}}}
		case M_TEMPLATE: //{{{
			{
				int		res;
				struct serialize_context	scx;
				Tcl_DString					ds;

				if (objc < 3 || objc > 4)
					CHECK_ARGS(2, "template json_template ?source_dict?");

				Tcl_DStringInit(&ds);

				scx.ds = &ds;
				scx.serialize_mode = SERIALIZE_TEMPLATE;
				scx.fromdict = NULL;

				if (objc == 4)
					Tcl_IncrRefCount(scx.fromdict = objv[3]);

				res = serialize(interp, &scx, objv[2]);

				if (scx.fromdict != NULL) {
					Tcl_DecrRefCount(scx.fromdict); scx.fromdict = NULL;
				}

				if (res == TCL_OK)
					Tcl_DStringResult(interp, scx.ds);

				Tcl_DStringFree(scx.ds); scx.ds = NULL;

				return res == TCL_OK ? TCL_OK : TCL_ERROR;
			}

			break;
			//}}}
		case M_FOREACH: //{{{
			if (objc < 5 || (objc-3) % 2 != 0)
				CHECK_ARGS(5, "foreach varlist datalist ?varlist datalist ...? script");

			retcode = foreach(interp, objc-2, objv+2, 0);
			break;
			//}}}
		case M_LMAP: //{{{
			if (objc < 5 || (objc-3) % 2 != 0)
				CHECK_ARGS(5, "lmap varlist datalist ?varlist datalist ...? script");

			retcode = foreach(interp, objc-2, objv+2, 1);
			break;
			//}}}
		case M_NOP: //{{{
			break;
			//}}}
		case M_PRETTY: //{{{
			{
				Tcl_DString	ds;
				Tcl_Obj*	indent;
				Tcl_Obj*	pad = Tcl_NewStringObj("", 0);

				if (objc < 3 || objc > 4)
					CHECK_ARGS(2, "pretty json_val ?indent?");

				if (objc > 3) {
					indent = objv[3];
				} else {
					indent = Tcl_NewStringObj("    ", 4);
				}

				Tcl_DStringInit(&ds);
				if (json_pretty(interp, objv[2], indent, pad, &ds) != TCL_OK) {
					Tcl_DStringFree(&ds);
					return TCL_ERROR;
				}
				Tcl_DStringResult(interp, &ds);
				Tcl_DStringFree(&ds);
			}
			break;
			//}}}
			/*
		case M_MERGE: //{{{
			THROW_ERROR("merge method is not functional yet, sorry");
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

				if (objc < 2) CHECK_ARGS(1, "merge ?flag ...? ?json_val ...?");

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
			break;
			//}}}
			*/

		default:
			// Should be impossible to reach
			THROW_ERROR("Invalid method");
	}

	return retcode;
}

//}}}
static int jsonObjCmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	return Tcl_NRCallObjProc(interp, jsonNRObjCmd, cdata, objc, objv);
}

//}}}
void free_interp_cx(ClientData cdata) //{{{
{
	struct interp_cx* l = cdata;
	Tcl_HashEntry*		he;
	Tcl_HashSearch		search;
	struct kc_entry*	e;

	l->interp = NULL;

	Tcl_DecrRefCount(l->tcl_true);  l->tcl_true = NULL;
	Tcl_DecrRefCount(l->tcl_false); l->tcl_false = NULL;

	Tcl_DecrRefCount(l->tcl_empty);
	Tcl_DecrRefCount(l->tcl_empty); l->tcl_empty = NULL;

	he = Tcl_FirstHashEntry(&l->kc, &search);
	while (he) {
		ptrdiff_t	idx = (ptrdiff_t)Tcl_GetHashValue(he);

		e = &l->kc_entries[idx];

		Tcl_DeleteHashEntry(he);
		Tcl_DecrRefCount(e->val);
		Tcl_DecrRefCount(e->val);	// Two references - one for the cache table and one on loan to callers' interim processing
		mark_free(l->freemap, idx);
		e->val = NULL;

		he = Tcl_NextHashEntry(&search);
	}
	l->kc_count = 0;

	Tcl_DeleteHashTable(&l->kc);
	free(l); l = NULL;
}

//}}}
_DLLEXPORT
int Rl_json_Init(Tcl_Interp* interp) //{{{
{
	struct interp_cx*	l = NULL;

#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.5", 0) == NULL)
		return TCL_ERROR;
#endif // USE_TCL_STUBS

	Tcl_RegisterObjType(&json_type);

	l = (struct interp_cx*)malloc(sizeof *l);
	l->interp = interp;
	Tcl_IncrRefCount(l->tcl_true  = Tcl_NewStringObj("1", 1));
	Tcl_IncrRefCount(l->tcl_false = Tcl_NewStringObj("0", 1));
	Tcl_IncrRefCount(l->tcl_empty = Tcl_NewStringObj("", 0));
	Tcl_IncrRefCount(l->json_null = JSON_NewJvalObj(JSON_NULL, NULL));

	// Ensure the empty string rep is considered "shared"
	Tcl_IncrRefCount(l->tcl_empty);

	Tcl_InitHashTable(&l->kc, TCL_STRING_KEYS);
	l->kc_count = 0;
	memset(&l->freemap, 0xFF, sizeof(l->freemap));

	Tcl_SetAssocData(interp, "rl_json", NULL, l);

	Tcl_NRCreateCommand(interp, "::rl_json::json", jsonObjCmd, jsonNRObjCmd, NULL, free_interp_cx);
	TEST_OK(Tcl_EvalEx(interp, "namespace eval ::rl_json {namespace export *}", -1, TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL));

	TEST_OK(Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION));

	return TCL_OK;
}

//}}}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
