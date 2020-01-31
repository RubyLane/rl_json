#include "rl_jsonInt.h"
#include "parser.h"

static void free_internal_rep(Tcl_Obj* obj, Tcl_ObjType* objtype);
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest, Tcl_ObjType* objtype);
static void update_string_rep(Tcl_Obj* obj, Tcl_ObjType* objtype);
static int set_from_any(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_ObjType** objtype, enum json_types* type);

extern Tcl_ObjType json_object;
extern Tcl_ObjType json_array;
extern Tcl_ObjType json_string;
extern Tcl_ObjType json_number;
extern Tcl_ObjType json_bool;
extern Tcl_ObjType json_null;
extern Tcl_ObjType json_dyn_string;
extern Tcl_ObjType json_dyn_number;
extern Tcl_ObjType json_dyn_bool;
extern Tcl_ObjType json_dyn_json;
extern Tcl_ObjType json_dyn_template;
extern Tcl_ObjType json_dyn_literal;

static void free_internal_rep_object(Tcl_Obj* obj)                { free_internal_rep(obj, &json_object);      }
static void dup_internal_rep_object(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_object); }
static void update_string_rep_object(Tcl_Obj* obj)                { update_string_rep(obj, &json_object);      }

static void free_internal_rep_array(Tcl_Obj* obj)                { free_internal_rep(obj, &json_array);      }
static void dup_internal_rep_array(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_array); }
static void update_string_rep_array(Tcl_Obj* obj)                { update_string_rep(obj, &json_array);      }

static void free_internal_rep_string(Tcl_Obj* obj)                { free_internal_rep(obj, &json_string);      }
static void dup_internal_rep_string(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_string); }
static void update_string_rep_string(Tcl_Obj* obj);

static void free_internal_rep_number(Tcl_Obj* obj)                { free_internal_rep(obj, &json_number);      }
static void dup_internal_rep_number(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_number); }
static void update_string_rep_number(Tcl_Obj* obj);

static void free_internal_rep_bool(Tcl_Obj* obj)                { free_internal_rep(obj, &json_bool);      }
static void dup_internal_rep_bool(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_bool); }
static void update_string_rep_bool(Tcl_Obj* obj);

static void free_internal_rep_null(Tcl_Obj* obj)                { free_internal_rep(obj, &json_null);      }
static void dup_internal_rep_null(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_null); }
static void update_string_rep_null(Tcl_Obj* obj);

static void free_internal_rep_dyn_string(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_string);      }
static void dup_internal_rep_dyn_string(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_string); }
static void update_string_rep_dyn_string(Tcl_Obj* obj)                { update_string_rep(obj, &json_dyn_string);      }

static void free_internal_rep_dyn_number(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_number);      }
static void dup_internal_rep_dyn_number(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_number); }
static void update_string_rep_dyn_number(Tcl_Obj* obj)                { update_string_rep(obj, &json_dyn_number);      }

static void free_internal_rep_dyn_bool(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_bool);      }
static void dup_internal_rep_dyn_bool(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_bool); }
static void update_string_rep_dyn_bool(Tcl_Obj* obj)                { update_string_rep(obj, &json_dyn_bool);      }

static void free_internal_rep_dyn_json(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_json);      }
static void dup_internal_rep_dyn_json(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_json); }
static void update_string_rep_dyn_json(Tcl_Obj* obj)                { update_string_rep(obj, &json_dyn_json);      }

static void free_internal_rep_dyn_template(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_template);      }
static void dup_internal_rep_dyn_template(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_template); }
static void update_string_rep_dyn_template(Tcl_Obj* obj)                { update_string_rep(obj, &json_dyn_template);      }

static void free_internal_rep_dyn_literal(Tcl_Obj* obj)                { free_internal_rep(obj, &json_dyn_literal);      }
static void dup_internal_rep_dyn_literal(Tcl_Obj* src, Tcl_Obj* dest)  { dup_internal_rep(src, dest, &json_dyn_literal); }
static void update_string_rep_dyn_literal(Tcl_Obj* obj);

Tcl_ObjType json_object = {
	"JSON_object",
	free_internal_rep_object,
	dup_internal_rep_object,
	update_string_rep_object,
	NULL
};
Tcl_ObjType json_array = {
	"JSON_array",
	free_internal_rep_array,
	dup_internal_rep_array,
	update_string_rep_array,
	NULL
};
Tcl_ObjType json_string = {
	"JSON_string",
	free_internal_rep_string,
	dup_internal_rep_string,
	update_string_rep_string,
	NULL
};
Tcl_ObjType json_number = {
	"JSON_number",
	free_internal_rep_number,
	dup_internal_rep_number,
	update_string_rep_number,
	NULL
};
Tcl_ObjType json_bool = {
	"JSON_bool",
	free_internal_rep_bool,
	dup_internal_rep_bool,
	update_string_rep_bool,
	NULL
};
Tcl_ObjType json_null = {
	"JSON_null",
	free_internal_rep_null,
	dup_internal_rep_null,
	update_string_rep_null,
	NULL
};
Tcl_ObjType json_dyn_string = {
	"JSON_dyn_string",
	free_internal_rep_dyn_string,
	dup_internal_rep_dyn_string,
	update_string_rep_dyn_string,
	NULL
};
Tcl_ObjType json_dyn_number = {
	"JSON_dyn_number",
	free_internal_rep_dyn_number,
	dup_internal_rep_dyn_number,
	update_string_rep_dyn_number,
	NULL
};
Tcl_ObjType json_dyn_bool = {
	"JSON_dyn_bool",
	free_internal_rep_dyn_bool,
	dup_internal_rep_dyn_bool,
	update_string_rep_dyn_bool,
	NULL
};
Tcl_ObjType json_dyn_json = {
	"JSON_dyn_json",
	free_internal_rep_dyn_json,
	dup_internal_rep_dyn_json,
	update_string_rep_dyn_json,
	NULL
};
Tcl_ObjType json_dyn_template = {
	"JSON_dyn_template",
	free_internal_rep_dyn_template,
	dup_internal_rep_dyn_template,
	update_string_rep_dyn_template,
	NULL
};
Tcl_ObjType json_dyn_literal = {
	"JSON_dyn_literal",
	free_internal_rep_dyn_literal,
	dup_internal_rep_dyn_literal,
	update_string_rep_dyn_literal,
	NULL
};

Tcl_ObjType* g_objtype_for_type[JSON_TYPE_MAX];


int JSON_IsJSON(Tcl_Obj* obj, enum json_types* type, Tcl_ObjIntRep** ir) //{{{
{
	enum json_types		t;
	Tcl_ObjIntRep*		_ir = NULL;

	for (t=JSON_OBJECT; t<JSON_TYPE_MAX && _ir==NULL; t++)
		_ir = Tcl_FetchIntRep(obj, g_objtype_for_type[t]);
	t--;

	if (_ir == NULL)
		return 0;

	*ir = _ir;
	*type = t;
	return 1;
}

//}}}
int JSON_GetIntrepFromObj(Tcl_Interp* interp, Tcl_Obj* obj, enum json_types* type, Tcl_ObjIntRep** ir) //{{{
{
	enum json_types		t;
	Tcl_ObjIntRep*		_ir = NULL;
	Tcl_ObjType*		objtype = NULL;

	if (!JSON_IsJSON(obj, &t, &_ir)) {
		TEST_OK(set_from_any(interp, obj, &objtype, &t));
		_ir = Tcl_FetchIntRep(obj, objtype);
		if (_ir == NULL) Tcl_Panic("Could not retrieve the intrep we just created");
	}

	*type = t;
	*ir = _ir;

	return TCL_OK;
}

//}}}
int JSON_GetJvalFromObj(Tcl_Interp* interp, Tcl_Obj* obj, enum json_types* type, Tcl_Obj** val) //{{{
{
	Tcl_ObjIntRep*		ir = NULL;

	TEST_OK(JSON_GetIntrepFromObj(interp, obj, type, &ir));

	*val = ir->twoPtrValue.ptr1;

	return TCL_OK;
}

//}}}
int JSON_SetIntRep(Tcl_Obj* target, enum json_types type, Tcl_Obj* replacement) //{{{
{
	Tcl_ObjIntRep		intrep;
	Tcl_ObjType*		objtype = NULL;

	if (Tcl_IsShared(target))
		Tcl_Panic("Called JSON_SetIntRep on a shared object");

	objtype = g_objtype_for_type[type];

	Tcl_FreeIntRep(target);

	intrep.twoPtrValue.ptr1 = replacement;		// ptr1 is the Tcl_Obj holding the Tcl structure for this value
	if (replacement) Tcl_IncrRefCount((Tcl_Obj*)intrep.twoPtrValue.ptr1);

	intrep.twoPtrValue.ptr2 = NULL;				// ptr2 holds the template actions, if any have been generated for this value

	Tcl_StoreIntRep(target, objtype, &intrep);

	Tcl_InvalidateStringRep(target);

	return TCL_OK;
}

//}}}
#ifdef TCL_MEM_DEBUG
Tcl_Obj* JSON_DbNewJvalObj(enum json_types type, Tcl_Obj* val, const char* file, int line)
#else
Tcl_Obj* JSON_NewJvalObj(enum json_types type, Tcl_Obj* val)
#endif
{ //{{{
#ifdef TCL_MEM_DEBUG
	Tcl_Obj*	res = Tcl_DbNewObj(file, line);
#else
	Tcl_Obj*	res = Tcl_NewObj();
#endif

	/*
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
	*/

	if (JSON_SetIntRep(res, type, val) != TCL_OK)
		Tcl_Panic("Couldn't set JSON intrep");

	return res;
}

//}}}

static void free_internal_rep(Tcl_Obj* obj, Tcl_ObjType* objtype) //{{{
{
	Tcl_ObjIntRep*		ir = NULL;

	ir = Tcl_FetchIntRep(obj, objtype);
	if (ir != NULL) {
		release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr1);
		release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);

#if 0
		//Tcl_Obj* ir_obj = ir->twoPtrValue.ptr1;
		Tcl_Obj* actions = ir->twoPtrValue.ptr2;
		if (ir->twoPtrValue.ptr1) {
			/*
			fprintf(stderr, "%s Releasing ptr1 %p, refcount %d, which is %s\n",
					objtype->name, ir->twoPtrValue.ptr1, ir_obj == NULL ? -42 : ir_obj->refCount,
					ir_obj->typePtr ? ir_obj->typePtr->name : "pure string"
				   );
				   */
			Tcl_DecrRefCount((Tcl_Obj*)ir->twoPtrValue.ptr1); ir->twoPtrValue.ptr1 = NULL;}
		if (ir->twoPtrValue.ptr2 && actions->refCount > 0) {
			/*
			fprintf(stderr, "%s Releasing ptr2 %p, refcount %d, which is %s\n",
					objtype->name, ir->twoPtrValue.ptr2, actions == NULL ? -42 : actions->refCount,
					actions->typePtr ? actions->typePtr->name : "pure string"
				   );
				   */
			Tcl_DecrRefCount((Tcl_Obj*)ir->twoPtrValue.ptr2); ir->twoPtrValue.ptr2 = NULL;}
#endif
	}
}

//}}}
static void dup_internal_rep(Tcl_Obj* src, Tcl_Obj* dest, Tcl_ObjType* objtype) //{{{
{
	Tcl_ObjIntRep*		srcir = NULL;
	Tcl_ObjIntRep		destir;

	srcir = Tcl_FetchIntRep(src, objtype);
	if (srcir == NULL)
		Tcl_Panic("dup_internal_rep asked to duplicate for type, but that type wasn't available on the src object");

	if (src == srcir->twoPtrValue.ptr1) {
		int			len;
		const char*	str = Tcl_GetStringFromObj((Tcl_Obj*)srcir->twoPtrValue.ptr1, &len);
		// Don't know how this happens yet, but it's bad news - we get into an endless recursion of duplicateobj calls until the stack blows up

		// Panic and go via the string rep
		destir.twoPtrValue.ptr1 = Tcl_NewStringObj(str, len);
		Tcl_IncrRefCount((Tcl_Obj*)(destir.twoPtrValue.ptr1 = Tcl_NewStringObj(str, len)));
	} else {
		destir.twoPtrValue.ptr1 = srcir->twoPtrValue.ptr1;
	}

	destir.twoPtrValue.ptr2 = srcir->twoPtrValue.ptr2;
	if (destir.twoPtrValue.ptr1) Tcl_IncrRefCount((Tcl_Obj*)destir.twoPtrValue.ptr1);
	if (destir.twoPtrValue.ptr2) Tcl_IncrRefCount((Tcl_Obj*)destir.twoPtrValue.ptr2);

	Tcl_StoreIntRep(dest, objtype, &destir);
}

//}}}
static void update_string_rep(Tcl_Obj* obj, Tcl_ObjType* objtype) //{{{
{
	Tcl_ObjIntRep*				ir = Tcl_FetchIntRep(obj, objtype);
	struct serialize_context	scx;
	Tcl_DString					ds;

	if (ir == NULL)
		Tcl_Panic("dup_internal_rep asked to duplicate for type, but that type wasn't available on the src object");

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
static void update_string_rep_string(Tcl_Obj* obj) //{{{
{
	update_string_rep(obj, &json_string);
	/*
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, &json_string);
	const char*		str;
	int				len;

	str = Tcl_GetStringFromObj((Tcl_Obj*)ir->twoPtrValue.ptr1, &len);
	obj->bytes = ckalloc(len+3);
	obj->bytes[0] = '"';
	memcpy(obj->bytes+1, str, len);
	obj->bytes[len+1] = '"';
	obj->bytes[len+2] = 0;
	obj->length = len+2;
	*/
}

//}}}
static void update_string_rep_number(Tcl_Obj* obj) //{{{
{
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, &json_number);
	const char*		str;
	int				len;

	if (ir->twoPtrValue.ptr1 == obj)
		Tcl_Panic("Turtles all the way down!");

	str = Tcl_GetStringFromObj((Tcl_Obj*)ir->twoPtrValue.ptr1, &len);
	obj->bytes = ckalloc(len+1);
	memcpy(obj->bytes, str, len+1);
	obj->length = len;
}

//}}}
static void update_string_rep_bool(Tcl_Obj* obj) //{{{
{
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, &json_bool);
	int				boolval;

	if (Tcl_GetBooleanFromObj(NULL, (Tcl_Obj*)ir->twoPtrValue.ptr1, &boolval) != TCL_OK)
		Tcl_Panic("json_bool's intrep tclobj is not a boolean");

	if (boolval) {
		obj->bytes = ckalloc(5);
		memcpy(obj->bytes, "true", 5);
		obj->length = 4;
	} else {
		obj->bytes = ckalloc(6);
		memcpy(obj->bytes, "false", 6);
		obj->length = 5;
	}
}

//}}}
static void update_string_rep_null(Tcl_Obj* obj) //{{{
{
	obj->bytes = ckalloc(5);
	memcpy(obj->bytes, "null", 5);
	obj->length = 4;
}

//}}}
static void update_string_rep_dyn_literal(Tcl_Obj* obj) //{{{
{
	update_string_rep(obj, &json_dyn_literal);
	/*
	Tcl_ObjIntRep*	ir = Tcl_FetchIntRep(obj, &json_dyn_literal);
	const char*		str;
	int				len;

	str = Tcl_GetStringFromObj((Tcl_Obj*)ir->twoPtrValue.ptr1, &len);
	obj->bytes = ckalloc(len+6);
	obj->bytes[0] = '"';
	obj->bytes[1] = '~';
	obj->bytes[2] = 'L';
	obj->bytes[3] = ':';
	memcpy(obj->bytes+4, str, len);
	obj->bytes[len+4] = '"';
	obj->bytes[len+5] = 0;
	obj->length = len+5;
	*/
}

//}}}
static int set_from_any(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_ObjType** objtype, enum json_types* out_type) //{{{
{
	struct interp_cx*		l = NULL;
	const unsigned char*	err_at = NULL;
	const char*				errmsg = "Illegal character";
	size_t					char_adj = 0;		// Offset addjustment to account for multibyte UTF-8 sequences
	const unsigned char*	doc;
	enum json_types			type;
	Tcl_Obj*				val = NULL;
	const unsigned char*	p;
	const unsigned char*	e;
	const unsigned char*	val_start;
	int						len;
	struct parse_context	cx[CX_STACK_SIZE];

	if (interp)
		l = Tcl_GetAssocData(interp, "rl_json", NULL);

#if 1
	// Snoop on the intrep for clues on optimized conversions {{{
	{
		if (
			l && (
				(l->typeInt    && Tcl_FetchIntRep(obj, l->typeInt)    != NULL) ||
				(l->typeDouble && Tcl_FetchIntRep(obj, l->typeDouble) != NULL) ||
				(l->typeBignum && Tcl_FetchIntRep(obj, l->typeBignum) != NULL)
			)
		) {
			Tcl_ObjIntRep			ir = {.twoPtrValue = {}};

			// Must dup because obj will soon be us, creating a circular ref
			replace_tclobj((Tcl_Obj**)&ir.twoPtrValue.ptr1, Tcl_DuplicateObj(obj));
			release_tclobj((Tcl_Obj**)&ir.twoPtrValue.ptr2);

			*out_type = JSON_NUMBER;
			*objtype = g_objtype_for_type[JSON_NUMBER];

			Tcl_StoreIntRep(obj, *objtype, &ir);
			return TCL_OK;
		}
	}
	// Snoop on the intrep for clues on optimized conversions }}}
#endif

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
	}	

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
					{
						Tcl_Obj*	new = Tcl_ObjPrintf("~%c:%s", key_start[2], Tcl_GetString(val));
						REPLACE(val, new);
						// Can do this because val's ref is on loan from new_stringobj_dedup
						//val = Tcl_ObjPrintf("~%c:%s", key_start[2], Tcl_GetString(val));
					}
					// Falls through
				case JSON_STRING:
					REPLACE(cx[0].last->hold_key, val);
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
				if (unlikely(cx->last->container != JSON_OBJECT && cx->last->container != JSON_ARRAY))
					cx->last->container = type;	// Record our type (at the document top-level)
				break;

			default:
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unexpected json value type: %d", type));
				goto err;
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

			default:	// Suppress compiler warning
				break;
		}
	}
	//}}}

	if (unlikely(cx[0].val == NULL)) {
		err_at = doc;
		errmsg = "No JSON value found";
		goto whitespace_err;
	}

	//Tcl_FreeIntRep(obj);

	{
		Tcl_ObjType*	top_objtype = g_objtype_for_type[cx[0].container];
		Tcl_ObjIntRep*	top_ir = Tcl_FetchIntRep(cx[0].val, top_objtype);
		Tcl_ObjIntRep	ir = {.twoPtrValue = {}};

		if (unlikely(top_ir == NULL))
			Tcl_Panic("Can't get intrep for the top container");

		// We're transferring the ref from cx[0].val to our intrep
		replace_tclobj((Tcl_Obj**)&ir.twoPtrValue.ptr1, top_ir->twoPtrValue.ptr1);
		release_tclobj((Tcl_Obj**)&ir.twoPtrValue.ptr2);
		release_tclobj(&cx[0].val);

		Tcl_StoreIntRep(obj, top_objtype, &ir);
		*objtype = top_objtype;
		*out_type = cx[0].container;
	}

	RELEASE(val);
	return TCL_OK;

whitespace_err:
	_parse_error(interp, errmsg, doc, (err_at - doc) - char_adj);

err:
	RELEASE(val);
	free_cx(cx);
	return TCL_ERROR;
}

//}}}
int type_is_dynamic(const enum json_types type) //{{{
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
Tcl_Obj* get_unshared_val(Tcl_ObjIntRep* ir) //{{{
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

int init_types(Tcl_Interp* interp) //{{{
{
	// We don't define set_from_any callbacks for our types, so they must not be Tcl_RegisterObjType'ed

	g_objtype_for_type[JSON_UNDEF]			= NULL;
	g_objtype_for_type[JSON_OBJECT]			= &json_object;
	g_objtype_for_type[JSON_ARRAY]			= &json_array;
	g_objtype_for_type[JSON_STRING]			= &json_string;
	g_objtype_for_type[JSON_NUMBER]			= &json_number;
	g_objtype_for_type[JSON_BOOL]			= &json_bool;
	g_objtype_for_type[JSON_NULL]			= &json_null;
	g_objtype_for_type[JSON_DYN_STRING]		= &json_dyn_string;
	g_objtype_for_type[JSON_DYN_NUMBER]		= &json_dyn_number;
	g_objtype_for_type[JSON_DYN_BOOL]		= &json_dyn_bool;
	g_objtype_for_type[JSON_DYN_JSON]		= &json_dyn_json;
	g_objtype_for_type[JSON_DYN_TEMPLATE]	= &json_dyn_template;
	g_objtype_for_type[JSON_DYN_LITERAL]	= &json_dyn_literal;

	return TCL_OK;
}

//}}}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
