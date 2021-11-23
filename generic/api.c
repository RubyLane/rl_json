#include "rl_jsonInt.h"
#include "parser.h"

Tcl_Obj* JSON_NewJSONObj(Tcl_Interp* interp, Tcl_Obj* from) //{{{
{
	return as_json(interp, from);
}

//}}}
int JSON_NewJStringObj(Tcl_Interp* interp, Tcl_Obj* string, Tcl_Obj** new) //{{{
{
	replace_tclobj(new, JSON_NewJvalObj(JSON_STRING, string));

	return TCL_OK;
}

//}}}
int JSON_NewJNumberObj(Tcl_Interp* interp, Tcl_Obj* number, Tcl_Obj** new) //{{{
{
	Tcl_Obj*			forced = NULL;
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	TEST_OK(force_json_number(interp, l, number, &forced));
	replace_tclobj(new, JSON_NewJvalObj(JSON_NUMBER, forced));
	release_tclobj(&forced);

	return TCL_OK;
}

//}}}
int JSON_NewJBooleanObj(Tcl_Interp* interp, Tcl_Obj* boolean, Tcl_Obj** new) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	int					bool;

	TEST_OK(Tcl_GetBooleanFromObj(interp, boolean, &bool));
	replace_tclobj(new, bool ? l->json_true : l->json_false);

	return TCL_OK;
}

//}}}
int JSON_NewJNullObj(Tcl_Interp* interp, Tcl_Obj* *new) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	replace_tclobj(new, l->json_null);

    return TCL_OK;
}

//}}}
int JSON_NewJObjectObj(Tcl_Interp* interp, Tcl_Obj** new) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	replace_tclobj(new, JSON_NewJvalObj(JSON_OBJECT, l->tcl_empty_dict));

	return TCL_OK;
}

//}}}
int JSON_NewJArrayObj(Tcl_Interp* interp, int objc, Tcl_Obj* objv[], Tcl_Obj** new) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	if (objc == 0) {
		replace_tclobj(new, JSON_NewJvalObj(JSON_OBJECT, l->tcl_empty_list));
	} else {
		int		i;

		for (i=0; i<objc; i++) TEST_OK(JSON_ForceJSON(interp, objv[i]));

		replace_tclobj(new, JSON_NewJvalObj(JSON_OBJECT, Tcl_NewListObj(objc, objv)));
	}

	return TCL_OK;
}

//}}}
int JSON_NewTemplateObj(Tcl_Interp* interp, enum json_types type, Tcl_Obj* key, Tcl_Obj** new) //{{{
{
	if (!type_is_dynamic(type)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Requested type is not a valid template type: %s", type_names_int[type]));
		return TCL_ERROR;
	}

	replace_tclobj(new, JSON_NewJvalObj(type, key));

	return TCL_OK;	
}

//}}}
int JSON_ForceJSON(Tcl_Interp* interp, Tcl_Obj* obj) // Force a conversion to a JSON objtype, or throw an exception {{{
{
	Tcl_ObjIntRep*	ir;
	enum json_types	type;

	TEST_OK(JSON_GetIntrepFromObj(interp, obj, &type, &ir));

	return TCL_OK;
}

//}}}

enum json_types JSON_GetJSONType(Tcl_Obj* obj) //{{{
{
	Tcl_ObjIntRep*	ir = NULL;
	enum json_types	t;

	for (t=JSON_OBJECT; t<JSON_TYPE_MAX && ir==NULL; t++)
		ir = Tcl_FetchIntRep(obj, g_objtype_for_type[t]);

	return (ir == NULL) ? JSON_UNDEF : t-1;
}

//}}}
int JSON_GetObjFromJStringObj(Tcl_Interp* interp, Tcl_Obj* jstringObj, Tcl_Obj** stringObj) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, jstringObj, &type, &val));

	if (type_is_dynamic(type)) {
		replace_tclobj(stringObj, Tcl_ObjPrintf("%s%s", get_dyn_prefix(type), Tcl_GetString(val)));
		return TCL_OK;
	} else if (type == JSON_STRING) {
		replace_tclobj(stringObj, val);
		return TCL_OK;
	} else {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Asked for string from json string but supplied json %s", get_type_name(type)));
		return TCL_ERROR;
	}
}

//}}}
int JSON_GetObjFromJNumberObj(Tcl_Interp* interp, Tcl_Obj* jnumberObj, Tcl_Obj** numberObj) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, jnumberObj, &type, &val));

	if (type != JSON_NUMBER) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Asked for number from json number but supplied json %s", get_type_name(type)));
		return TCL_ERROR;
	}

	replace_tclobj(numberObj, val);

	return TCL_OK;
}

//}}}
int JSON_GetObjFromJBooleanObj(Tcl_Interp* interp, Tcl_Obj* jbooleanObj, Tcl_Obj** booleanObj) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, jbooleanObj, &type, &val));

	if (type != JSON_BOOL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Asked for boolean from json boolean but supplied json %s", get_type_name(type)));
		return TCL_ERROR;
	}

	replace_tclobj(booleanObj, val);

	return TCL_OK;
}

//}}}
int JSON_JArrayObjAppendElement(Tcl_Interp* interp, Tcl_Obj* arrayObj, Tcl_Obj* elem) //{{{
{
	int				code = TCL_OK;
	enum json_types	type;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;

	if (Tcl_IsShared(arrayObj)) {
		// Tcl_Panic?
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("JSON_JArrayObjAppendElement called with shared object"));
		return TCL_ERROR;
	}

	TEST_OK_LABEL(finally, code, JSON_GetIntrepFromObj(interp, arrayObj, &type, &ir));

	if (type != JSON_ARRAY) // Turn it into one by creating a new array with a single element containing the old value
		TEST_OK_LABEL(finally, code, JSON_SetIntRep(arrayObj, JSON_ARRAY, Tcl_NewListObj(1, &val)));

	replace_tclobj(&val, get_unshared_val(ir));

	TEST_OK_LABEL(finally, code, Tcl_ListObjAppendElement(interp, val, as_json(interp, elem)));

	release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
	Tcl_InvalidateStringRep(arrayObj);

finally:
	replace_tclobj(&val, NULL);
	return code;
}

//}}}
int JSON_JArrayObjAppendList(Tcl_Interp* interp, Tcl_Obj* arrayObj, Tcl_Obj* elems /* a JArrayObj or ListObj */ ) //{{{
{
	enum json_types	type, elems_type;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;
	Tcl_Obj*		elems_val = NULL;
	int				retval = TCL_OK;

	if (Tcl_IsShared(arrayObj)) {
		// Tcl_Panic?
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("JSON_JArrayObjAppendElement called with shared object"));
		return TCL_ERROR;
	}

	TEST_OK_LABEL(finally, retval, JSON_GetIntrepFromObj(interp, arrayObj, &type, &ir));

	if (type != JSON_ARRAY) // Turn it into one by creating a new array with a single element containing the old value
		TEST_OK_LABEL(finally, retval, JSON_SetIntRep(arrayObj, JSON_ARRAY, Tcl_NewListObj(1, &val)));

	val = get_unshared_val(ir);

	if (JSON_GetJvalFromObj(interp, elems, &elems_type, &elems_val) == TCL_OK) {
		switch (elems_type) {
			case JSON_ARRAY:	// Given a JSON array, append its elements
				TEST_OK_LABEL(finally, retval, Tcl_ListObjAppendList(interp, val, elems_val));
				break;

			case JSON_OBJECT:	// Given a JSON object, append its keys as strings and values as whatever they were
				{
					Tcl_DictSearch search;
					Tcl_Obj*		k = NULL;
					Tcl_Obj*		kjstring = NULL;
					Tcl_Obj*		v = NULL;
					int				done;

					TEST_OK_LABEL(finally, retval, Tcl_DictObjFirst(interp, elems_val, &search, &k, &v, &done));
					for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
						TEST_OK_BREAK(retval, JSON_NewJStringObj(interp, k, &kjstring));
						TEST_OK_BREAK(retval, Tcl_ListObjAppendElement(interp, val, kjstring));
						TEST_OK_BREAK(retval, Tcl_ListObjAppendElement(interp, val, v));
					}
					release_tclobj(&kjstring);
					Tcl_DictObjDone(&search);
				}
				break;

			default:			// elems is JSON, but not a sensible type for this call
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Could not append elements - not a JSON array, JSON object or list: %s", get_type_name(elems_type)));
				return TCL_ERROR;
		}
	} else {
		TEST_OK_LABEL(finally, retval, Tcl_ListObjAppendList(interp, val, elems));
	}

finally:
	return retval;
}

//}}}
int JSON_SetJArrayObj(Tcl_Interp* interp, Tcl_Obj* obj, const int objc, Tcl_Obj* objv[]) //{{{
{
	enum json_types	type;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;
	int				i, retval = TCL_OK;
	Tcl_Obj**		jov = NULL;
	Tcl_Obj*		newlist = NULL;

	if (Tcl_IsShared(obj)) {
		// Tcl_Panic?
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("JSON_SetJArrayObj called with shared object"));
		return TCL_ERROR;
	}

	jov = ckalloc(sizeof(Tcl_Obj*) * objc);
	for (i=0; i<objc; i++)
		Tcl_IncrRefCount(jov[i] = as_json(interp, objv[i]));

	// Possibly silly optimization: if obj is already a JSON array, call Tcl_SetListObj on its intrep list.
	// All this saves is freeing the old intrep list and creating a fresh one, at the cost of some other overhead
	if (JSON_IsJSON(obj, &type, &ir)) {
		val = get_unshared_val(ir);
		Tcl_SetListObj(val, objc, jov);
	} else {
		replace_tclobj(&newlist, Tcl_NewListObj(objc, jov));
		retval = JSON_SetIntRep(obj, JSON_ARRAY, newlist);
	}

	release_tclobj(&newlist);

	if (jov) {
		for (i=0; i<objc; i++) release_tclobj(&jov[i]);
		ckfree(jov);
		jov = NULL;
	}

	return retval;
}

//}}}
int JSON_JArrayObjGetElements(Tcl_Interp* interp, Tcl_Obj* arrayObj, int* objc, Tcl_Obj*** objv) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, arrayObj, &type, &val));
	if (type != JSON_ARRAY) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Expecting a JSON array, but got a JSON %s", get_type_name(type)));
		return TCL_ERROR;
	}
	TEST_OK(Tcl_ListObjGetElements(interp, val, objc, objv));

	return TCL_OK;
}

//}}}
int JSON_JArrayObjIndex(Tcl_Interp* interp, Tcl_Obj* arrayObj, int index, Tcl_Obj** elem) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;

	TEST_OK(JSON_GetJvalFromObj(interp, arrayObj, &type, &val));
	if (type != JSON_ARRAY) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Expecting a JSON array, but got a JSON %s", get_type_name(type)));
		return TCL_ERROR;
	}
	TEST_OK(Tcl_ListObjIndex(interp, val, index, elem));

	return TCL_OK;
}

//}}}
int JSON_JArrayObjReplace(Tcl_Interp* interp, Tcl_Obj* arrayObj, int first, int count, int objc, Tcl_Obj* objv[]) //{{{
{
	enum json_types	type;
	Tcl_Obj*		val = NULL;
	Tcl_Obj**		jov = NULL;
	int				i, retval=TCL_OK;

	TEST_OK(JSON_GetJvalFromObj(interp, arrayObj, &type, &val));
	if (type != JSON_ARRAY) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Expecting a JSON array, but got a JSON %s", get_type_name(type)));
		return TCL_ERROR;
	}

	jov = ckalloc(sizeof(Tcl_Obj*) * objc);
	for (i=0; i<objc; i++)
		Tcl_IncrRefCount(jov[i] = as_json(interp, objv[i]));

	retval = Tcl_ListObjReplace(interp, val, first, count, objc, jov);

	if (jov) {
		for (i=0; i<objc; i++) release_tclobj(&jov[i]);
		ckfree(jov);
		jov = NULL;
	}

	return retval;
}

//}}}
// TODO: JObject interface, similar to DictObj

int JSON_Get(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** res) //{{{
{
	int			retval = TCL_OK;
	Tcl_Obj*	jval = NULL;
	Tcl_Obj*	astcl = NULL;

	retval = JSON_Extract(interp, obj, path, &jval);

	if (retval == TCL_OK)
		retval = convert_to_tcl(interp, jval, &astcl);

	if (retval == TCL_OK)
		replace_tclobj(res, astcl);

	release_tclobj(&astcl);
	release_tclobj(&jval);

	return retval;
}

//}}}
int JSON_Extract(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** res) //{{{
{
	int			code = TCL_OK;
	Tcl_Obj*	target = NULL;
	Tcl_Obj**	pathv = NULL;
	int			pathc;

	TEST_OK_LABEL(finally, code, Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	if (pathc > 0) {
		TEST_OK_LABEL(finally, code, resolve_path(interp, obj, pathv, pathc, &target, 0, 0));
	} else {
		TEST_OK_LABEL(finally, code, JSON_ForceJSON(interp, obj));
		replace_tclobj(&target, obj);
	}

	if (code == TCL_OK)
		replace_tclobj(res, target);

finally:
	release_tclobj(&target);
	return code;
}

//}}}
int JSON_Exists(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* exists) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	Tcl_Obj*			target = NULL;
	Tcl_Obj**			pathv = NULL;
	int					pathc;

	TEST_OK(Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	if (pathc > 0) {
		TEST_OK(resolve_path(interp, obj, pathv, pathc, &target, 1, 0));
		release_tclobj(&target);
		// resolve_path sets the interp result in exists mode
		*exists = (Tcl_GetObjResult(interp) == l->json_true);
		Tcl_ResetResult(interp);
	} else {
		enum json_types	type = JSON_GetJSONType(obj);
		*exists = (type != JSON_UNDEF && type != JSON_NULL);
	}

	return TCL_OK;
}

//}}}
int JSON_Set(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj *path, Tcl_Obj* replacement) //{{{
{
	int				code = TCL_OK;
	int				i, pathc;
	enum json_types	type, newtype;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;
	Tcl_Obj*		step;
	Tcl_Obj*		src;
	Tcl_Obj*		target;
	Tcl_Obj*		newval;
	Tcl_Obj*		rep = NULL;
	Tcl_Obj**		pathv = NULL;

	if (Tcl_IsShared(obj))
		THROW_ERROR_LABEL(finally, code, "JSON_Set called with shared object");

	/*
	fprintf(stderr, "JSON_Set, obj: \"%s\", src: \"%s\"\n",
			Tcl_GetString(obj), Tcl_GetString(src));
			*/
	target = src = obj;

	TEST_OK_LABEL(finally, code, JSON_GetIntrepFromObj(interp, target, &type, &ir));
	val = get_unshared_val(ir);

	TEST_OK_LABEL(finally, code, Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_int[type]);

		switch (type) {
			case JSON_UNDEF: //{{{
				THROW_ERROR_LABEL(finally, code, "Found JSON_UNDEF type jval following path");
				//}}}
			case JSON_OBJECT: //{{{
				TEST_OK_LABEL(finally, code, Tcl_DictObjGet(interp, val, step, &target));
				if (target == NULL) {
					//fprintf(stderr, "Path element %d: \"%s\" doesn't exist creating a new key for it and storing a null\n",
					//		i, Tcl_GetString(step));
					target = JSON_NewJvalObj(JSON_NULL, NULL);
					TEST_OK_LABEL(finally, code, Tcl_DictObjPut(interp, val, step, target));
					i++;
					goto followed_path;
				}
				if (Tcl_IsShared(target)) {
					//fprintf(stderr, "Path element %d: \"%s\" exists but the TclObj is shared (%d), replacing it with an unshared duplicate\n",
					//		i, Tcl_GetString(step), target->refCount);
					target = Tcl_DuplicateObj(target);
					TEST_OK_LABEL(finally, code, Tcl_DictObjPut(interp, val, step, target));
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

					TEST_OK_LABEL(finally, code, Tcl_ListObjGetElements(interp, val, &ac, &av));
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
							THROW_ERROR_LABEL(finally, code, "Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

						//fprintf(stderr, "Resolved index of %ld from \"%s\"\n", index, index_str);
					} else {
						//fprintf(stderr, "Explicit index: %ld\n", index);
					}

					if (index < 0) {
						// Prepend element to the array
						target = JSON_NewJvalObj(JSON_NULL, NULL);
						TEST_OK_LABEL(finally, code, Tcl_ListObjReplace(interp, val, -1, 0, 1, &target));

						i++;
						goto followed_path;
					} else if (index >= ac) {
						int			new_i;
						for (new_i=ac; new_i<index; new_i++) {
							TEST_OK_LABEL(finally, code, Tcl_ListObjAppendElement(interp, val,
										JSON_NewJvalObj(JSON_NULL, NULL)));
						}
						target = JSON_NewJvalObj(JSON_NULL, NULL);
						TEST_OK_LABEL(finally, code, Tcl_ListObjAppendElement(interp, val, target));

						i++;
						goto followed_path;
					} else {
						target = av[index];
						if (Tcl_IsShared(target)) {
							target = Tcl_DuplicateObj(target);
							TEST_OK_LABEL(finally, code, Tcl_ListObjReplace(interp, val, index, 1, 1, &target));
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
				THROW_ERROR_LABEL(finally, code, "Attempt to index into atomic type ", get_type_name(type), " at path key \"", Tcl_GetString(step), "\"");
				/*
				i++;
				goto followed_path;
				*/
			default:
				THROW_ERROR_LABEL(finally, code, "Unhandled type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

		TEST_OK_LABEL(finally, code, JSON_GetIntrepFromObj(interp, target, &type, &ir));
		val = get_unshared_val(ir);
	}

	goto set_val;

followed_path:
	TEST_OK_LABEL(finally, code, JSON_GetIntrepFromObj(interp, target, &type, &ir));
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
			TEST_OK_LABEL(finally, code, JSON_SetIntRep(target, JSON_OBJECT, val));
		}

		target = JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj());
		//fprintf(stderr, "Adding key \"%s\"\n", Tcl_GetString(pathv[i]));
		TEST_OK_LABEL(finally, code, Tcl_DictObjPut(interp, val, pathv[i], target));
		TEST_OK_LABEL(finally, code, JSON_GetJvalFromObj(interp, target, &type, &val));
		//fprintf(stderr, "Newly added key \"%s\" is of type %s\n", Tcl_GetString(pathv[i]), type_names_int[type]);
		// This was just created - it can't be shared
	}

set_val:
	//fprintf(stderr, "Reached end of path, calling JSON_SetIntRep for replacement value %s (%s), target is %s\n",
	//		type_names_int[newtype], Tcl_GetString(replacement), type_names_int[type]);
	replace_tclobj(&rep, as_json(interp, replacement));

	TEST_OK_LABEL(finally, code, JSON_GetJvalFromObj(interp, rep, &newtype, &newval));
	TEST_OK_LABEL(finally, code, JSON_SetIntRep(target, newtype, newval));
	release_tclobj(&rep);

	Tcl_InvalidateStringRep(src);

	if (interp)
		Tcl_SetObjResult(interp, src);

finally:
	return code;
}

//}}}
int JSON_Unset(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj *path) //{{{
{
	enum json_types	type;
	int				i;
	Tcl_Obj*		val = NULL;
	Tcl_Obj*		step = NULL;
	Tcl_Obj*		src = NULL;
	Tcl_Obj*		target = NULL;
	int				pathc;
	Tcl_Obj**		pathv = NULL;
	int				retval = TCL_OK;

	TEST_OK(Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	target = src = obj;

	if (pathc == 0) {
		Tcl_SetObjResult(interp, src);
		return TCL_OK;	// Do Nothing Gracefully
	}

	if (Tcl_IsShared(src))
		THROW_ERROR("JSON_Set called with shared Tcl_Obj");

	/*
	fprintf(stderr, "JSON_Set, obj: \"%s\", src: \"%s\"\n",
			Tcl_GetString(obj), Tcl_GetString(src));
			*/

	{
		Tcl_ObjIntRep*	ir = NULL;
		TEST_OK_LABEL(finally, retval, JSON_GetIntrepFromObj(interp, target, &type, &ir));
		val = get_unshared_val(ir);
	}

	// Walk the path as far as it exists in src
	//fprintf(stderr, "set, initial type %s\n", type_names[type]);
	for (i=0; i<pathc-1; i++) {
		step = pathv[i];
		//fprintf(stderr, "looking at step %s, cx type: %s\n", Tcl_GetString(step), type_names_int[type]);

		switch (type) {
			case JSON_UNDEF: //{{{
				THROW_ERROR_LABEL(finally, retval, "Found JSON_UNDEF type jval following path");
				//}}}
			case JSON_OBJECT: //{{{
				TEST_OK_LABEL(finally, retval, Tcl_DictObjGet(interp, val, step, &target));
				if (target == NULL) {
					goto bad_path;
				}
				if (Tcl_IsShared(target)) {
					//fprintf(stderr, "Path element %d: \"%s\" exists but the TclObj is shared (%d), replacing it with an unshared duplicate\n",
					//		i, Tcl_GetString(step), target->refCount);
					target = Tcl_DuplicateObj(target);
					TEST_OK_LABEL(finally, retval, Tcl_DictObjPut(interp, val, step, target));
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

					TEST_OK_LABEL(finally, retval, Tcl_ListObjGetElements(interp, val, &ac, &av));
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
							THROW_ERROR_LABEL(finally, retval, "Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

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
							TEST_OK_LABEL(finally, retval, Tcl_ListObjReplace(interp, val, index, 1, 1, &target));
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
				THROW_ERROR_LABEL(finally, retval, "Attempt to index into atomic type ", get_type_name(type), " at path key \"", Tcl_GetString(step), "\"");
				/*
				i++;
				goto bad_path;
				*/
			default:
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unhandled type: %d", type));
				retval = TCL_ERROR;
				goto finally;
		}

		{
			Tcl_ObjIntRep*	ir = NULL;
			TEST_OK_LABEL(finally, retval, JSON_GetIntrepFromObj(interp, target, &type, &ir));
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
			THROW_ERROR_LABEL(finally, retval, "Found JSON_UNDEF type jval following path");
			//}}}
		case JSON_OBJECT: //{{{
			TEST_OK_LABEL(finally, retval, Tcl_DictObjRemove(interp, val, step));
			break;
			//}}}
		case JSON_ARRAY: //{{{
			{
				int			ac, index_str_len, ok=1;
				long		index;
				const char*	index_str;
				char*		end;
				Tcl_Obj**	av;

				TEST_OK_LABEL(finally, retval, Tcl_ListObjGetElements(interp, val, &ac, &av));
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
						THROW_ERROR_LABEL(finally, retval, "Expected an integer index or end(+/-integer)?, got ", Tcl_GetString(step));

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
					TEST_OK_LABEL(finally, retval, Tcl_ListObjReplace(interp, val, index, 1, 0, NULL));
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

				replace_tclobj(&bad_path, Tcl_NewListObj(i+1, pathv));
				Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", Tcl_GetString(bad_path), NULL);
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Attempt to index into atomic type %s at path \"%s\"", get_type_name(type), Tcl_GetString(bad_path)));
				release_tclobj(&bad_path);
				retval = TCL_ERROR;
				goto finally;
			}
		default:
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unhandled type: %d", type));
			retval = TCL_ERROR;
			goto finally;
	}

	Tcl_InvalidateStringRep(src);

	if (interp && retval == TCL_OK)
		Tcl_SetObjResult(interp, src);

finally:
	return retval;

bad_path:
	{
		Tcl_Obj* bad_path = NULL;

		Tcl_IncrRefCount(bad_path = Tcl_NewListObj(i+1, pathv));
		Tcl_SetErrorCode(interp, "RL", "JSON", "BAD_PATH", Tcl_GetString(bad_path), NULL);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Path element \"%s\" doesn't exist", Tcl_GetString(bad_path)));
		Tcl_DecrRefCount(bad_path); bad_path = NULL;
		retval = TCL_ERROR;
		goto finally;
	}
}

//}}}
int JSON_Normalize(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj** normalized) //{{{
{
	int				retval = TCL_OK;
	Tcl_Obj*		json = NULL;
	enum json_types	type;

	type = JSON_GetJSONType(obj);

	if (type != JSON_UNDEF && !Tcl_HasStringRep(obj))
		return TCL_OK;		// Nothing to do - already parsed as json and have no string rep

	if (Tcl_IsShared(obj)) {
		replace_tclobj(&json, Tcl_DuplicateObj(obj));
	} else {
		replace_tclobj(&json, obj);
	}

	retval = JSON_ForceJSON(interp, json);
	Tcl_InvalidateStringRep(json);			// Defer string rep generation to our caller

	if (retval == TCL_OK)
		replace_tclobj(normalized, json);

	release_tclobj(&json);

	return retval;
}

//}}}
int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* indent, Tcl_Obj** prettyString) //{{{
{
	int					retval = TCL_OK;
	Tcl_DString			ds;
	Tcl_Obj*			pad = NULL;
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);

	if (indent == NULL)
		replace_tclobj(&indent, get_string(l, "    ", 4));

	replace_tclobj(&pad, l->tcl_empty);
	Tcl_DStringInit(&ds);
	retval = json_pretty(interp, obj, indent, pad, &ds);

	if (retval == TCL_OK)
		replace_tclobj(prettyString, Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)));

	Tcl_DStringFree(&ds);
	release_tclobj(&pad);
	release_tclobj(&indent);

	return retval;
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
		//DBG("Building template actions and storing in intrep ptr2 for %s\n", name(template));
		TEST_OK_LABEL(finally, retcode, build_template_actions(interp, template, &actions));
		replace_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2, actions);
	}

	//DBG("template %s refcount before: %d\n", name(template), template->refCount);
	retcode = apply_template_actions(interp, template, actions, dict, res);
	//DBG("template %s refcount after: %d\n", name(template), template->refCount);
	release_tclobj(&actions);

finally:
	return retcode;
}

//}}}
int JSON_IsNULL(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* isnull) //{{{
{
	int			retval = TCL_OK;
	Tcl_Obj*	jval = NULL;

	retval = JSON_Extract(interp, obj, path, &jval);

	if (retval == TCL_OK)
		*isnull = (JSON_NULL == JSON_GetJSONType(jval));

	release_tclobj(&jval);

	return retval;
}

//}}}
int JSON_Type(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, enum json_types* type) //{{{
{
	int			retval = TCL_OK;
	Tcl_Obj*	jval = NULL;

	retval = JSON_Extract(interp, obj, path, &jval);

	if (retval == TCL_OK)
		*type = JSON_GetJSONType(jval);

	release_tclobj(&jval);

	return retval;
}

//}}}
int JSON_Length(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* length) //{{{
{
	enum json_types	type;
	int				retval = TCL_OK;
	Tcl_Obj*		val = NULL;
	Tcl_Obj*		target = NULL;

	TEST_OK_LABEL(finally, retval, JSON_Extract(interp, obj, path, &target));

	TEST_OK_LABEL(finally, retval, JSON_GetJvalFromObj(interp, target, &type, &val));

	switch (type) {
		case JSON_ARRAY:  retval = Tcl_ListObjLength(interp, val, length); break;
		case JSON_OBJECT: retval = Tcl_DictObjSize(interp, val, length);   break;

		case JSON_DYN_STRING:
		case JSON_DYN_NUMBER:
		case JSON_DYN_BOOL:
		case JSON_DYN_JSON:
		case JSON_DYN_TEMPLATE:
		case JSON_DYN_LITERAL:   *length = Tcl_GetCharLength(val) + 3; break;	// dynamic types have a 3 character prefix
		case JSON_STRING:        *length = Tcl_GetCharLength(val);     break;

		default:
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("Named JSON value type isn't supported: %s", get_type_name(type)));
			retval = TCL_ERROR;
	}

finally:
	release_tclobj(&target);

	return retval;
}

//}}}
int JSON_Keys(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** keyslist) //{{{
{
	enum json_types		type;
	int					retval = TCL_OK;
	Tcl_Obj*			val = NULL;
	Tcl_Obj*			target = NULL;

	TEST_OK_LABEL(finally, retval, JSON_Extract(interp, obj, path, &target));
	TEST_OK_LABEL(finally, retval, JSON_GetJvalFromObj(interp, target, &type, &val));

	if (type != JSON_OBJECT) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Named JSON value type isn't supported: %s", get_type_name(type)));
		retval = TCL_ERROR;
	} else {
		Tcl_Obj*		res = NULL;
		Tcl_Obj*		k = NULL;
		Tcl_Obj*		v = NULL;
		Tcl_DictSearch	search;
		int				done;

		replace_tclobj(&res, Tcl_NewListObj(0, NULL));

		TEST_OK_LABEL(finally, retval, Tcl_DictObjFirst(interp, val, &search, &k, &v, &done));
		for (; !done; Tcl_DictObjNext(&search, &k, &v, &done))
			TEST_OK_BREAK(retval, Tcl_ListObjAppendElement(interp, res, k));
		Tcl_DictObjDone(&search);

		if (retval == TCL_OK)
			replace_tclobj(keyslist, res);

		release_tclobj(&res);
	}

finally:
	release_tclobj(&target);

	return retval;
}

//}}}
int JSON_Decode(Tcl_Interp* interp, Tcl_Obj* bytes, Tcl_Obj* encoding, Tcl_Obj** decodedstring) //{{{
{
	struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
	Tcl_Obj*			ov[4];
	int					i, retval;

	ov[0] = l->apply;
	ov[1] = l->decode_bytes;
	ov[2] = bytes;
	ov[3] = encoding;

	for (i=0; i<4 && ov[i]; i++) if (ov[i]) Tcl_IncrRefCount(ov[i]);
	retval = Tcl_EvalObjv(interp, i, ov, TCL_EVAL_GLOBAL);
	for (i=0; i<4 && ov[i]; i++) release_tclobj(&ov[i]);

	if (retval == TCL_OK) {
		replace_tclobj(decodedstring, Tcl_GetObjResult(interp));
		Tcl_ResetResult(interp);
	}

	return retval;
}

//}}}
int JSON_Foreach(Tcl_Interp* interp, Tcl_Obj* iterators, JSON_ForeachBody* body, enum collecting_mode collect, Tcl_Obj** res, ClientData cdata) //{{{
{
#if 0
	unsigned int			i;
	int						retcode=TCL_OK;
	struct foreach_state*	state = NULL;
	int						objc;
	Tcl_Obj**				objv = NULL;
	Tcl_Obj*				it_res = NULL;

	TEST_OK(Tcl_ListObjGetElements(interp, iterators, &objc, &objv));

	if (objc % 2 == 1) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("iterators must be a list with an even number of elements"));
		return TCL_ERROR;
	}

	state = (struct foreach_state*)Tcl_Alloc(sizeof(*state));
	state->iterators = objc/2;
	state->it = (struct foreach_iterator*)Tcl_Alloc(sizeof(struct foreach_iterator) * state->iterators);
	state->max_loops = 0;
	state->loop_num = 0;
	state->collecting = collect;
	state->script = NULL;

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
		Tcl_Obj*		val = NULL;
		Tcl_Obj*		varlist = objv[i*2];

		if (Tcl_IsShared(varlist))
			varlist = Tcl_DuplicateObj(varlist);	// TODO: why do we care if this is shared?

		replace_tclobj(&state->it[i].varlist, varlist);

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

	while (state->loop_num < state->max_loops) {
		struct interp_cx*	l = Tcl_GetAssocData(interp, "rl_json", NULL);
		unsigned int		j, k;
		Tcl_Obj*			loopvars = NULL;

		replace_tclobj(&loopvars, Tcl_NewDictObj());

		//fprintf(stderr, "Starting iteration %d/%d\n", i, max_loops);
		// Set the iterator variables
		for (j=0; j < state->iterators; j++) {
			struct foreach_iterator* this_it = &state->it[j];

			if (this_it->is_array) { // Iterating over a JSON array
				//fprintf(stderr, "Array iteration, data_i: %d, length %d\n", this_it->data_i, this_it->data_c);
				for (k=0; k<this_it->var_c; k++) {
					Tcl_Obj* it_val = NULL;

					if (this_it->data_i < this_it->data_c) {
						//fprintf(stderr, "Pulling next element %d off the data list (length %d)\n", this_it->data_i, this_it->data_c);
						replace_tclobj(&it_val, this_it->data_v[this_it->data_i++]);
					} else {
						//fprintf(stderr, "Ran out of elements in this data list, setting null\n");
						replace_tclobj(&it_val, l->json_null);
					}
					//fprintf(stderr, "pre  Iteration %d, this_it: %p, setting var %p, varname ref: %d\n",
					//		i, this_it, this_it->var_v[k]/*, Tcl_GetString(this_it->var_v[k])*/, this_it->var_v[k]->refCount);
					//fprintf(stderr, "varname: \"%s\"\n", Tcl_GetString(it[j].var_v[k]));
					//Tcl_ObjSetVar2(interp, this_it->var_v[k], NULL, it_val, 0);
					retcode = Tcl_DictObjPut(interp, loopvars, this_it->var_v[k], it_val);
					release_tclobj(&it_val);
					if (retcode != TCL_OK) goto done;
					//Tcl_ObjSetVar2(interp, Tcl_NewStringObj("elem", 4), NULL, it_val, 0);
					//fprintf(stderr, "post Iteration %d, this_it: %p, setting var %p, varname ref: %d\n",
					//		i, this_it, this_it->var_v[k]/*, Tcl_GetString(this_it->var_v[k])*/, this_it->var_v[k]->refCount);
				}
			} else { // Iterating over a JSON object
				//fprintf(stderr, "Object iteration\n");
				if (!this_it->done) {
					// We check that this_it->var_c == 2 in the setup
					TEST_OK_LABEL(done, retcode, Tcl_DictObjPut(interp, loopvars, this_it->var_v[0], this->k));
					TEST_OK_LABEL(done, retcode, Tcl_DictObjPut(interp, loopvars, this_it->var_v[1], this->v));
					Tcl_DictObjNext(&this_it->search, &this_it->k, &this_it->v, &this_it->done);
				}
			}
		}

		{
			int						bodycode;

			bodycode = body(cdata, interp, loopvars);

			switch (bodycode) {
				case TCL_OK:
					switch (state->collecting) {
						case COLLECT_NONE: break;
						case COLLECT_LIST:
							replace_tclobj(&it_res, Tcl_GetObjResult(interp));
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
		}
	}

done:
	//fprintf(stderr, "done\n");
	release_tclobj(&it_res);
	if (retcode == TCL_OK && state->collecting != COLLECT_NONE)
		replace_tclobj(res, state->res);

	foreach_state_free(state);
	Tcl_Free((char*)state);
	state = NULL;

	return retcode;
#else
	return TCL_OK;
#endif
}

//}}}
int JSON_Valid(Tcl_Interp* interp, Tcl_Obj* json, int* valid, enum extensions extensions, struct parse_error* details) //{{{
{
	struct interp_cx*		l = NULL;
	const unsigned char*	err_at = NULL;
	const char*				errmsg = "Illegal character";
	size_t					char_adj = 0;		// Offset addjustment to account for multibyte UTF-8 sequences
	const unsigned char*	doc;
	enum json_types			type;
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
				(l->typeInt    && Tcl_FetchIntRep(json, l->typeInt)    != NULL) ||
				(l->typeDouble && Tcl_FetchIntRep(json, l->typeDouble) != NULL) ||
				(l->typeBignum && Tcl_FetchIntRep(json, l->typeBignum) != NULL)
			)
		) {
			*valid = 1;
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
	cx[0].l = l;
	cx[0].mode = VALIDATE;

	p = doc = (const unsigned char*)Tcl_GetStringFromObj(json, &len);
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
	if (skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0) goto whitespace_err;

	if (unlikely(p >= e)) {
		err_at = p;
		errmsg = "No JSON value found";
		goto whitespace_err;
	}

	while (p < e) {
		if (cx[0].last->container == JSON_OBJECT) { // Read the key if in object mode {{{
			const unsigned char*	key_start = p;
			size_t					key_start_char_adj = char_adj;

			if (value_type(l, doc, p, e, &char_adj, &p, &type, NULL, details) != TCL_OK) goto invalid;

			switch (type) {
				case JSON_DYN_STRING:
				case JSON_DYN_NUMBER:
				case JSON_DYN_BOOL:
				case JSON_DYN_JSON:
				case JSON_DYN_TEMPLATE:
				case JSON_DYN_LITERAL:
				case JSON_STRING:
					break;

				default:
					parse_error(details, "Object key is not a string", doc, (key_start-doc) - key_start_char_adj);
					goto invalid;
			}

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;

			if (unlikely(*p != ':')) {
				parse_error(details, "Expecting : after object key", doc, (p-doc) - char_adj);
				goto invalid;
			}
			p++;

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;
		}
		//}}}

		val_start = p;
		if (value_type(l, doc, p, e, &char_adj, &p, &type, NULL, details) != TCL_OK) goto invalid;

		switch (type) {
			case JSON_OBJECT:
				push_parse_context(cx, JSON_OBJECT, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;

				if (*p == '}') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_ARRAY:
				push_parse_context(cx, JSON_ARRAY, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;

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
				if (unlikely(cx->last->container != JSON_OBJECT && cx->last->container != JSON_ARRAY))
					cx->last->container = type;	// Record our type (at the document top-level)
				break;

			default:
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unexpected json value type: %d", type));
				goto err;
		}

after_value:	// Yeah, goto.  But the alternative abusing loops was worse
		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;
		if (p >= e) break;

		if (unlikely(cx[0].last->closed)) {
			parse_error(details, "Trailing garbage after value", doc, (p-doc) - char_adj);
			goto invalid;
		}

		switch (cx[0].last->container) { // Handle eof and end-of-context or comma for array and object {{{
			case JSON_OBJECT:
				if (*p == '}') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					parse_error(details, "Expecting } or ,", doc, (p-doc) - char_adj);
					goto invalid;
				}

				p++;
				break;

			case JSON_ARRAY:
				if (*p == ']') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					parse_error(details, "Expecting ] or ,", doc, (p-doc) - char_adj);
					goto invalid;
				}

				p++;
				break;

			default:
				if (unlikely(p < e)) {
					parse_error(details, "Trailing garbage after value", doc, (p - doc) - char_adj);
					goto invalid;
				}
		}

		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj, extensions) != 0)) goto whitespace_err;
		//}}}
	}

	if (unlikely(cx != cx[0].last || !cx[0].closed)) { // Unterminated object or array context {{{
		switch (cx[0].last->container) {
			case JSON_OBJECT:
				parse_error(details, "Unterminated object", doc, cx[0].last->char_ofs);
				goto invalid;

			case JSON_ARRAY:
				parse_error(details, "Unterminated array", doc, cx[0].last->char_ofs);
				goto invalid;

			default:	// Suppress compiler warning
				break;
		}
	}
	//}}}

	*valid = 1;
	return TCL_OK;

whitespace_err:
	parse_error(details, errmsg, doc, (err_at - doc) - char_adj);

invalid:
	free_cx(cx);

	// This was a parse error, which is a successful outcome for us
	*valid = 0;
	return TCL_OK;

err:
	free_cx(cx);
	return TCL_ERROR;
}

//}}}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
