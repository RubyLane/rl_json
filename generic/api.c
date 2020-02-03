#include "rl_jsonInt.h"

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
	enum json_types	type;
	Tcl_ObjIntRep*	ir = NULL;
	Tcl_Obj*		val = NULL;

	if (Tcl_IsShared(arrayObj)) {
		// Tcl_Panic?
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("JSON_JArrayObjAppendElement called with shared object"));
		return TCL_ERROR;
	}

	TEST_OK(JSON_GetIntrepFromObj(interp, arrayObj, &type, &ir));

	if (type != JSON_ARRAY) // Turn it into one by creating a new array with a single element containing the old value
		TEST_OK(JSON_SetIntRep(arrayObj, JSON_ARRAY, Tcl_NewListObj(1, &val)));

	val = get_unshared_val(ir);

	TEST_OK(Tcl_ListObjAppendElement(interp, val, as_json(interp, elem)));

	release_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2);
	Tcl_InvalidateStringRep(arrayObj);

	return TCL_OK;
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

	TEST_OK(JSON_GetIntrepFromObj(interp, arrayObj, &type, &ir));

	if (type != JSON_ARRAY) // Turn it into one by creating a new array with a single element containing the old value
		TEST_OK(JSON_SetIntRep(arrayObj, JSON_ARRAY, Tcl_NewListObj(1, &val)));

	val = get_unshared_val(ir);

	if (JSON_GetJvalFromObj(interp, elems, &elems_type, &elems_val) == TCL_OK) {
		switch (elems_type) {
			case JSON_ARRAY:	// Given a JSON array, append its elements
				TEST_OK(Tcl_ListObjAppendList(interp, val, elems_val));
				break;

			case JSON_OBJECT:	// Given a JSON object, append its keys as strings and values as whatever they were
				{
					Tcl_DictSearch search;
					Tcl_Obj*		k = NULL;
					Tcl_Obj*		kjstring = NULL;
					Tcl_Obj*		v = NULL;
					int				done;

					TEST_OK(Tcl_DictObjFirst(interp, elems_val, &search, &k, &v, &done));
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
		TEST_OK(Tcl_ListObjAppendList(interp, val, elems));
	}

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
	Tcl_Obj*	target = NULL;
	int			retval=TCL_OK;
	Tcl_Obj**	pathv = NULL;
	int			pathc;

	TEST_OK(Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	if (pathc > 0) {
		TEST_OK(resolve_path(interp, obj, pathv, pathc, &target, 0, 0));
	} else {
		TEST_OK(JSON_ForceJSON(interp, obj));
		replace_tclobj(&target, obj);
	}

	if (retval == TCL_OK)
		replace_tclobj(res, target);

	release_tclobj(&target);

	return retval;
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

	src = Tcl_ObjGetVar2(interp, obj, NULL, 0);
	if (src == NULL) {
		src = Tcl_ObjSetVar2(interp, obj, NULL, JSON_NewJvalObj(JSON_OBJECT, Tcl_NewDictObj()), TCL_LEAVE_ERR_MSG);
	}

	if (Tcl_IsShared(src)) {
		src = Tcl_ObjSetVar2(interp, obj, NULL, Tcl_DuplicateObj(src), TCL_LEAVE_ERR_MSG);
		if (src == NULL)
			return TCL_ERROR;
	}

	/*
	fprintf(stderr, "JSON_Set, obj: \"%s\", src: \"%s\"\n",
			Tcl_GetString(obj), Tcl_GetString(src));
			*/
	target = src;

	TEST_OK(JSON_GetIntrepFromObj(interp, target, &type, &ir));
	val = get_unshared_val(ir);

	TEST_OK(Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

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
				THROW_ERROR("Attempt to index into atomic type ", get_type_name(type), " at path key \"", Tcl_GetString(step), "\"");
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
int JSON_Unset(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj *path) //{{{
{
	enum json_types	type;
	int				i;
	Tcl_Obj*		val;
	Tcl_Obj*		step;
	Tcl_Obj*		src;
	Tcl_Obj*		target;
	int				pathc;
	Tcl_Obj**		pathv = NULL;

	src = Tcl_ObjGetVar2(interp, obj, NULL, TCL_LEAVE_ERR_MSG);
	if (src == NULL)
		return TCL_ERROR;

	TEST_OK(Tcl_ListObjGetElements(interp, path, &pathc, &pathv));

	if (pathc == 0) {
		Tcl_SetObjResult(interp, src);
		return TCL_OK;	// Do Nothing Gracefully
	}

	if (Tcl_IsShared(src)) {
		src = Tcl_ObjSetVar2(interp, obj, NULL, Tcl_DuplicateObj(src), TCL_LEAVE_ERR_MSG);
		if (src == NULL)
			return TCL_ERROR;
	}

	/*
	fprintf(stderr, "JSON_Set, obj: \"%s\", src: \"%s\"\n",
			Tcl_GetString(obj), Tcl_GetString(src));
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
				THROW_ERROR("Attempt to index into atomic type ", get_type_name(type), " at path key \"", Tcl_GetString(step), "\"");
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
				Tcl_SetObjResult(interp, Tcl_ObjPrintf("Attempt to index into atomic type %s at path \"%s\"", get_type_name(type), Tcl_GetString(bad_path)));
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
		TEST_OK(build_template_actions(interp, template, &actions));
		replace_tclobj((Tcl_Obj**)&ir->twoPtrValue.ptr2, actions);
	}

	retcode = apply_template_actions(interp, template, actions, dict, res);
	release_tclobj(&actions);

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
int JSON_Foreach(Tcl_Interp* interp, Tcl_Obj* iterators, int* body, enum collecting_mode collect, Tcl_Obj** res, ClientData cdata)
{
	THROW_ERROR("Not implemented yet");
}

/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* End: */
// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
