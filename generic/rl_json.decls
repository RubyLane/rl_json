library rl_json
interface rl_json

declare 0 generic {
    Tcl_Obj* JSON_NewJSONObj(Tcl_Interp* interp, Tcl_Obj* from)
}
declare 1 generic {
	int JSON_NewJStringObj(Tcl_Interp* interp, Tcl_Obj* string, Tcl_Obj** new)
}
declare 2 generic {
	int JSON_NewJNumberObj(Tcl_Interp* interp, Tcl_Obj* number, Tcl_Obj** new)
}
declare 3 generic {
	int JSON_NewJBooleanObj(Tcl_Interp* interp, Tcl_Obj* boolean, Tcl_Obj** new)
}
declare 4 generic {
	int JSON_NewJNullObj(Tcl_Interp* interp, Tcl_Obj** new)
}
declare 5 generic {
	int JSON_NewJObjectObj(Tcl_Interp* interp, Tcl_Obj** new)
}
declare 6 generic {
	int JSON_NewJArrayObj(Tcl_Interp*, int objc, Tcl_Obj* objv[], Tcl_Obj** new)
}
# type is one of the DYN types, key is the variable name the template is replaced with
declare 7 generic {
	int JSON_NewTemplateObj(Tcl_Interp* interp, enum json_types type, Tcl_Obj* key, Tcl_Obj** new)
}

declare 8 generic {
	int JSON_ForceJSON(Tcl_Interp* interp, Tcl_Obj* obj)
}
declare 9 generic {
	enum json_types JSON_GetJSONType(Tcl_Obj* obj)
}
declare 10 generic {
	int JSON_GetObjFromJStringObj(Tcl_Interp* interp, Tcl_Obj* jstringObj, Tcl_Obj** stringObj)
}
# Return a native Tcl number type object
declare 11 generic {
	int JSON_GetObjFromJNumberObj(Tcl_Interp* interp, Tcl_Obj* jnumberObj, Tcl_Obj** numberObj)
}
# Return a native Tcl number type object
declare 12 generic {
	int JSON_GetObjFromJBooleanObj(Tcl_Interp* interp, Tcl_Obj* jbooleanObj, Tcl_Obj** booleanObj)
}

declare 13 generic {
	int JSON_JArrayObjAppendElement(Tcl_Interp* interp, Tcl_Obj* arrayObj, Tcl_Obj* elem)
}
declare 14 generic {
	int JSON_JArrayObjAppendList(Tcl_Interp* interp, Tcl_Obj* arrayObj, Tcl_Obj* elems /* a JArrayObj or ListObj */ )
}
declare 15 generic {
	int JSON_SetJArrayObj(Tcl_Interp* interp, Tcl_Obj* obj, int objc, Tcl_Obj* objv[])
}
declare 16 generic {
	int JSON_JArrayObjGetElements(Tcl_Interp* interp, Tcl_Obj* arrayObj, int* objc, Tcl_Obj*** objv)
}
declare 17 generic {
	int JSON_JArrayObjIndex(Tcl_Interp* interp, Tcl_Obj* arrayObj, int index, Tcl_Obj** elem)
}
declare 18 generic {
	int JSON_JArrayObjReplace(Tcl_Interp* interp, Tcl_Obj* arrayObj, int first, int count, int objc, Tcl_Obj* objv[])
}

# TODO: JObject interface, similar to DictObj
#

declare 19 generic {
	int JSON_Get(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** res)
}
declare 20 generic {
	int JSON_Extract(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** res)
}
declare 21 generic {
	int JSON_Exists(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* exists)
}
declare 22 generic {
	int JSON_Set(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj* replacement)
}
declare 23 generic {
	int JSON_Unset(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path)
}

declare 24 generic {
	int JSON_Normalize(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* normalized)
}
declare 25 generic {
	int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj** prettyString)
}
declare 26 generic {
	int JSON_Template(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* dict, Tcl_Obj** res)
}

declare 27 generic {
	int JSON_IsNULL(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* isnull)
}
declare 28 generic {
	int JSON_Type(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, enum json_types* type)
}
declare 29 generic {
	int JSON_Length(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, int* type)
}
declare 30 generic {
	int JSON_Keys(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** keyslist)
}
declare 31 generic {
	int JSON_Decode(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* path, Tcl_Obj** keys)
}

declare 32 generic {
	int JSON_Foreach(Tcl_Interp* interp, Tcl_Obj* iterators, int* body, enum collecting_mode collect, Tcl_Obj** res, ClientData cdata)
}
