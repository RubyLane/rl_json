#include "rl_json.h"
#include "rl_jsonInt.h"

static void free_internal_rep(Tcl_Obj* obj);
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest);
static void update_string_rep(Tcl_Obj* obj);
static int set_from_any(Tcl_Interp* interp, Tcl_Obj* obj);

Tcl_ObjType json_type = {
	"JSON",
	free_internal_rep,
	dup_internal_rep,
	update_string_rep,
	set_from_any
};

int JSON_GetJvalFromObj(Tcl_Interp* interp, Tcl_Obj* obj, int* type, Tcl_Obj** val) //{{{
{
	if (obj->typePtr != &json_type)
		TEST_OK(set_from_any(interp, obj));

	*type = obj->internalRep.ptrAndLongRep.value;		// TODO: fix intrep access
	*val = obj->internalRep.ptrAndLongRep.ptr;

	return TCL_OK;
}

//}}}
int JSON_SetIntRep(Tcl_Interp* interp, Tcl_Obj* target, int type, Tcl_Obj* replacement) //{{{
{
	if (Tcl_IsShared(target))
		THROW_ERROR("Called JSON_SetIntRep on a shared object: ", Tcl_GetString(target));

	target->internalRep.ptrAndLongRep.value = type;		// TODO: fix intrep access

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
	res->internalRep.ptrAndLongRep.ptr = NULL;			// TODO: fix intrep access

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

static void free_internal_rep(Tcl_Obj* obj) //{{{
{
	Tcl_Obj*	jv = obj->internalRep.ptrAndLongRep.ptr;

	if (jv == NULL) return;

	Tcl_DecrRefCount(jv); jv = NULL;
}

//}}}
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest) //{{{
{
	Tcl_Obj* src_intrep_obj = (Tcl_Obj*)src->internalRep.ptrAndLongRep.ptr;

	dest->typePtr = src->typePtr;

	if (src == src_intrep_obj) {
		int			len;
		const char*	str = Tcl_GetStringFromObj(src_intrep_obj, &len);
		// Don't know how this happens yet, but it's bad news - we get into an endless recursion of duplicateobj calls until the stack blows up

		// Panic and go via the string rep
		Tcl_IncrRefCount((Tcl_Obj*)(dest->internalRep.ptrAndLongRep.ptr = Tcl_NewStringObj(str, len)));
	} else {
		Tcl_IncrRefCount((Tcl_Obj*)(dest->internalRep.ptrAndLongRep.ptr = Tcl_DuplicateObj(src_intrep_obj)));
		if (src_intrep_obj->typePtr && src_intrep_obj->internalRep.ptrAndLongRep.value == JSON_ARRAY) {
			// List intreps are themselves shared - this horrible hack is to ensure that the intrep is unshared
			//fprintf(stderr, "forcing dedup of list intrep\n");
			Tcl_ListObjReplace(NULL, (Tcl_Obj*)dest->internalRep.ptrAndLongRep.ptr, 0, 0, 0, NULL);
		}
	}
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
	scx.l = NULL;
	scx.allow_null = 1;

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

	// Skip BOM
	if (
		len >= 3 &&
		p[0] == 0xef &&
		p[1] == 0xbb &&
		p[2] == 0xbf
	) {
		p += 3;
	} /*else if (
		len >= 2 &&
		p[0] == 0xff &&
		p[1] == 0xfe
	) {
		fprintf(stderr, "UTF-16LE in UTF-8 detected\n");
		p += 2;
		// Somehow this got to us as a UTF-16LE, inside UTF-8
	} else if (
		len >= 2 &&
		p[0] == 0xfe &&
		p[1] == 0xff
	) {
		fprintf(stderr, "UTF-16BE in UTF-8 detected\n");
		p += 2;
		// Somehow this got to us as a UTF-16BE, inside UTF-8
	}
	*/
	

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

		if (unlikely(cx[0].last->closed)) {
			_parse_error(interp, "Trailing garbage after value", doc, (p-doc) - char_adj);
			goto err;
		}

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
	obj->internalRep.ptrAndLongRep.ptr   = cx[0].val->internalRep.ptrAndLongRep.ptr;

	// We're transferring the ref from cx[0].val to our intrep
	if (obj->internalRep.ptrAndLongRep.ptr != NULL) {
		// NULL signals a JSON null type
		Tcl_IncrRefCount((Tcl_Obj*)obj->internalRep.ptrAndLongRep.ptr);
	}

	Tcl_DecrRefCount(cx[0].val);
	cx[0].val = NULL;

	return TCL_OK;

whitespace_err:
	_parse_error(interp, errmsg, doc, (err_at - doc) - char_adj);

err:
	free_cx(cx);
	return TCL_ERROR;
}

//}}}

int init_types(Tcl_Interp* interp) //{{{
{
	Tcl_RegisterObjType(&json_type);

	return TCL_OK;
}

//}}}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
