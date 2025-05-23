
/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN Tcl_Obj*		JSON_NewJSONObj(Tcl_Interp*interp, Tcl_Obj*from);
/* 1 */
EXTERN int		JSON_NewJStringObj(Tcl_Interp*interp, Tcl_Obj*string,
				Tcl_Obj**new);
/* 2 */
EXTERN int		JSON_NewJNumberObj(Tcl_Interp*interp, Tcl_Obj*number,
				Tcl_Obj**new);
/* 3 */
EXTERN int		JSON_NewJBooleanObj(Tcl_Interp*interp,
				Tcl_Obj*boolean, Tcl_Obj**new);
/* 4 */
EXTERN int		JSON_NewJNullObj(Tcl_Interp*interp, Tcl_Obj**new);
/* 5 */
EXTERN int		JSON_NewJObjectObj(Tcl_Interp*interp, Tcl_Obj**new);
/* 6 */
EXTERN int		JSON_NewJArrayObj(Tcl_Interp*interp, int objc,
				Tcl_Obj*objv[], Tcl_Obj**new);
/* 7 */
EXTERN int		JSON_NewTemplateObj(Tcl_Interp*interp,
				enum json_types type, Tcl_Obj*key,
				Tcl_Obj**new);
/* 8 */
EXTERN int		JSON_ForceJSON(Tcl_Interp*interp, Tcl_Obj*obj);
/* 9 */
EXTERN enum json_types	JSON_GetJSONType(Tcl_Obj*obj);
/* 10 */
EXTERN int		JSON_GetObjFromJStringObj(Tcl_Interp*interp,
				Tcl_Obj*jstringObj, Tcl_Obj**stringObj);
/* 11 */
EXTERN int		JSON_GetObjFromJNumberObj(Tcl_Interp*interp,
				Tcl_Obj*jnumberObj, Tcl_Obj**numberObj);
/* 12 */
EXTERN int		JSON_GetObjFromJBooleanObj(Tcl_Interp*interp,
				Tcl_Obj*jbooleanObj, Tcl_Obj**booleanObj);
/* 13 */
EXTERN int		JSON_JArrayObjAppendElement(Tcl_Interp*interp,
				Tcl_Obj*arrayObj, Tcl_Obj*elem);
/* 14 */
EXTERN int		JSON_JArrayObjAppendList(Tcl_Interp*interp,
				Tcl_Obj*arrayObj,
				Tcl_Obj* elems /* a JArrayObj or ListObj */);
/* 15 */
EXTERN int		JSON_SetJArrayObj(Tcl_Interp*interp, Tcl_Obj*obj,
				int objc, Tcl_Obj*objv[]);
/* 16 */
EXTERN int		JSON_JArrayObjGetElements(Tcl_Interp*interp,
				Tcl_Obj*arrayObj, int*objc, Tcl_Obj***objv);
/* 17 */
EXTERN int		JSON_JArrayObjIndex(Tcl_Interp*interp,
				Tcl_Obj*arrayObj, int index, Tcl_Obj**elem);
/* 18 */
EXTERN int		JSON_JArrayObjReplace(Tcl_Interp*interp,
				Tcl_Obj*arrayObj, int first, int count,
				int objc, Tcl_Obj*objv[]);
/* 19 */
EXTERN int		JSON_Get(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */,
				Tcl_Obj**res);
/* 20 */
EXTERN int		JSON_Extract(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */,
				Tcl_Obj**res);
/* 21 */
EXTERN int		JSON_Exists(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */, int*exists);
/* 22 */
EXTERN int		JSON_Set(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */,
				Tcl_Obj*replacement);
/* 23 */
EXTERN int		JSON_Unset(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */);
/* 24 */
EXTERN int		JSON_Normalize(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj**normalized);
/* 25 */
EXTERN int		JSON_Pretty(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj*indent, Tcl_Obj**prettyString);
/* 26 */
EXTERN int		JSON_Template(Tcl_Interp*interp, Tcl_Obj*template,
				Tcl_Obj*dict, Tcl_Obj**res);
/* 27 */
EXTERN int		JSON_IsNULL(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */, int*isnull);
/* 28 */
EXTERN int		JSON_Type(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */,
				enum json_types*type);
/* 29 */
EXTERN int		JSON_Length(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */, int*length);
/* 30 */
EXTERN int		JSON_Keys(Tcl_Interp*interp, Tcl_Obj*obj,
				Tcl_Obj* path /* can be NULL */,
				Tcl_Obj**keyslist);
/* 31 */
EXTERN int		JSON_Decode(Tcl_Interp*interp, Tcl_Obj*bytes,
				Tcl_Obj*encoding, Tcl_Obj**decodedstring);
/* 32 */
EXTERN int		JSON_Foreach(Tcl_Interp*interp, Tcl_Obj*iterators,
				JSON_ForeachBody*body,
				enum collecting_mode collect, Tcl_Obj**res,
				ClientData cdata);
/* 33 */
EXTERN int		JSON_Valid(Tcl_Interp*interp, Tcl_Obj*json,
				int*valid, enum extensions extensions,
				struct parse_error*details);
/* 34 */
EXTERN Tcl_Obj*		JSON_NewJvalObj(enum json_types type, Tcl_Obj*val);
/* 35 */
EXTERN Tcl_Obj*		JSON_DbNewJvalObj(enum json_types type, Tcl_Obj*val,
				const char*file, int line);
/* Slot 36 is reserved */
/* Slot 37 is reserved */
/* Slot 38 is reserved */
/* Slot 39 is reserved */
/* 40 */
EXTERN int		CBOR_GetDataItemFromPath(Tcl_Interp*interp,
				Tcl_Obj*cborObj, Tcl_Obj*pathObj,
				const uint8_t**dataitemPtr,
				const uint8_t**ePtr, Tcl_DString*tagsPtr);
/* 41 */
EXTERN int		CBOR_Length(Tcl_Interp*interp, const uint8_t*p,
				const uint8_t*e, size_t*lenPtr);

typedef struct Rl_jsonStubs {
    int magic;
    void *hooks;

    Tcl_Obj* (*jSON_NewJSONObj) (Tcl_Interp*interp, Tcl_Obj*from); /* 0 */
    int (*jSON_NewJStringObj) (Tcl_Interp*interp, Tcl_Obj*string, Tcl_Obj**new); /* 1 */
    int (*jSON_NewJNumberObj) (Tcl_Interp*interp, Tcl_Obj*number, Tcl_Obj**new); /* 2 */
    int (*jSON_NewJBooleanObj) (Tcl_Interp*interp, Tcl_Obj*boolean, Tcl_Obj**new); /* 3 */
    int (*jSON_NewJNullObj) (Tcl_Interp*interp, Tcl_Obj**new); /* 4 */
    int (*jSON_NewJObjectObj) (Tcl_Interp*interp, Tcl_Obj**new); /* 5 */
    int (*jSON_NewJArrayObj) (Tcl_Interp*interp, int objc, Tcl_Obj*objv[], Tcl_Obj**new); /* 6 */
    int (*jSON_NewTemplateObj) (Tcl_Interp*interp, enum json_types type, Tcl_Obj*key, Tcl_Obj**new); /* 7 */
    int (*jSON_ForceJSON) (Tcl_Interp*interp, Tcl_Obj*obj); /* 8 */
    enum json_types (*jSON_GetJSONType) (Tcl_Obj*obj); /* 9 */
    int (*jSON_GetObjFromJStringObj) (Tcl_Interp*interp, Tcl_Obj*jstringObj, Tcl_Obj**stringObj); /* 10 */
    int (*jSON_GetObjFromJNumberObj) (Tcl_Interp*interp, Tcl_Obj*jnumberObj, Tcl_Obj**numberObj); /* 11 */
    int (*jSON_GetObjFromJBooleanObj) (Tcl_Interp*interp, Tcl_Obj*jbooleanObj, Tcl_Obj**booleanObj); /* 12 */
    int (*jSON_JArrayObjAppendElement) (Tcl_Interp*interp, Tcl_Obj*arrayObj, Tcl_Obj*elem); /* 13 */
    int (*jSON_JArrayObjAppendList) (Tcl_Interp*interp, Tcl_Obj*arrayObj, Tcl_Obj* elems /* a JArrayObj or ListObj */); /* 14 */
    int (*jSON_SetJArrayObj) (Tcl_Interp*interp, Tcl_Obj*obj, int objc, Tcl_Obj*objv[]); /* 15 */
    int (*jSON_JArrayObjGetElements) (Tcl_Interp*interp, Tcl_Obj*arrayObj, int*objc, Tcl_Obj***objv); /* 16 */
    int (*jSON_JArrayObjIndex) (Tcl_Interp*interp, Tcl_Obj*arrayObj, int index, Tcl_Obj**elem); /* 17 */
    int (*jSON_JArrayObjReplace) (Tcl_Interp*interp, Tcl_Obj*arrayObj, int first, int count, int objc, Tcl_Obj*objv[]); /* 18 */
    int (*jSON_Get) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, Tcl_Obj**res); /* 19 */
    int (*jSON_Extract) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, Tcl_Obj**res); /* 20 */
    int (*jSON_Exists) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, int*exists); /* 21 */
    int (*jSON_Set) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, Tcl_Obj*replacement); /* 22 */
    int (*jSON_Unset) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */); /* 23 */
    int (*jSON_Normalize) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj**normalized); /* 24 */
    int (*jSON_Pretty) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj*indent, Tcl_Obj**prettyString); /* 25 */
    int (*jSON_Template) (Tcl_Interp*interp, Tcl_Obj*template, Tcl_Obj*dict, Tcl_Obj**res); /* 26 */
    int (*jSON_IsNULL) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, int*isnull); /* 27 */
    int (*jSON_Type) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, enum json_types*type); /* 28 */
    int (*jSON_Length) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, int*length); /* 29 */
    int (*jSON_Keys) (Tcl_Interp*interp, Tcl_Obj*obj, Tcl_Obj* path /* can be NULL */, Tcl_Obj**keyslist); /* 30 */
    int (*jSON_Decode) (Tcl_Interp*interp, Tcl_Obj*bytes, Tcl_Obj*encoding, Tcl_Obj**decodedstring); /* 31 */
    int (*jSON_Foreach) (Tcl_Interp*interp, Tcl_Obj*iterators, JSON_ForeachBody*body, enum collecting_mode collect, Tcl_Obj**res, ClientData cdata); /* 32 */
    int (*jSON_Valid) (Tcl_Interp*interp, Tcl_Obj*json, int*valid, enum extensions extensions, struct parse_error*details); /* 33 */
    Tcl_Obj* (*jSON_NewJvalObj) (enum json_types type, Tcl_Obj*val); /* 34 */
    Tcl_Obj* (*jSON_DbNewJvalObj) (enum json_types type, Tcl_Obj*val, const char*file, int line); /* 35 */
    void (*reserved36)(void);
    void (*reserved37)(void);
    void (*reserved38)(void);
    void (*reserved39)(void);
    int (*cBOR_GetDataItemFromPath) (Tcl_Interp*interp, Tcl_Obj*cborObj, Tcl_Obj*pathObj, const uint8_t**dataitemPtr, const uint8_t**ePtr, Tcl_DString*tagsPtr); /* 40 */
    int (*cBOR_Length) (Tcl_Interp*interp, const uint8_t*p, const uint8_t*e, size_t*lenPtr); /* 41 */
} Rl_jsonStubs;

extern const Rl_jsonStubs *rl_jsonStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_RL_JSON_STUBS)

/*
 * Inline function declarations:
 */

#define JSON_NewJSONObj \
	(rl_jsonStubsPtr->jSON_NewJSONObj) /* 0 */
#define JSON_NewJStringObj \
	(rl_jsonStubsPtr->jSON_NewJStringObj) /* 1 */
#define JSON_NewJNumberObj \
	(rl_jsonStubsPtr->jSON_NewJNumberObj) /* 2 */
#define JSON_NewJBooleanObj \
	(rl_jsonStubsPtr->jSON_NewJBooleanObj) /* 3 */
#define JSON_NewJNullObj \
	(rl_jsonStubsPtr->jSON_NewJNullObj) /* 4 */
#define JSON_NewJObjectObj \
	(rl_jsonStubsPtr->jSON_NewJObjectObj) /* 5 */
#define JSON_NewJArrayObj \
	(rl_jsonStubsPtr->jSON_NewJArrayObj) /* 6 */
#define JSON_NewTemplateObj \
	(rl_jsonStubsPtr->jSON_NewTemplateObj) /* 7 */
#define JSON_ForceJSON \
	(rl_jsonStubsPtr->jSON_ForceJSON) /* 8 */
#define JSON_GetJSONType \
	(rl_jsonStubsPtr->jSON_GetJSONType) /* 9 */
#define JSON_GetObjFromJStringObj \
	(rl_jsonStubsPtr->jSON_GetObjFromJStringObj) /* 10 */
#define JSON_GetObjFromJNumberObj \
	(rl_jsonStubsPtr->jSON_GetObjFromJNumberObj) /* 11 */
#define JSON_GetObjFromJBooleanObj \
	(rl_jsonStubsPtr->jSON_GetObjFromJBooleanObj) /* 12 */
#define JSON_JArrayObjAppendElement \
	(rl_jsonStubsPtr->jSON_JArrayObjAppendElement) /* 13 */
#define JSON_JArrayObjAppendList \
	(rl_jsonStubsPtr->jSON_JArrayObjAppendList) /* 14 */
#define JSON_SetJArrayObj \
	(rl_jsonStubsPtr->jSON_SetJArrayObj) /* 15 */
#define JSON_JArrayObjGetElements \
	(rl_jsonStubsPtr->jSON_JArrayObjGetElements) /* 16 */
#define JSON_JArrayObjIndex \
	(rl_jsonStubsPtr->jSON_JArrayObjIndex) /* 17 */
#define JSON_JArrayObjReplace \
	(rl_jsonStubsPtr->jSON_JArrayObjReplace) /* 18 */
#define JSON_Get \
	(rl_jsonStubsPtr->jSON_Get) /* 19 */
#define JSON_Extract \
	(rl_jsonStubsPtr->jSON_Extract) /* 20 */
#define JSON_Exists \
	(rl_jsonStubsPtr->jSON_Exists) /* 21 */
#define JSON_Set \
	(rl_jsonStubsPtr->jSON_Set) /* 22 */
#define JSON_Unset \
	(rl_jsonStubsPtr->jSON_Unset) /* 23 */
#define JSON_Normalize \
	(rl_jsonStubsPtr->jSON_Normalize) /* 24 */
#define JSON_Pretty \
	(rl_jsonStubsPtr->jSON_Pretty) /* 25 */
#define JSON_Template \
	(rl_jsonStubsPtr->jSON_Template) /* 26 */
#define JSON_IsNULL \
	(rl_jsonStubsPtr->jSON_IsNULL) /* 27 */
#define JSON_Type \
	(rl_jsonStubsPtr->jSON_Type) /* 28 */
#define JSON_Length \
	(rl_jsonStubsPtr->jSON_Length) /* 29 */
#define JSON_Keys \
	(rl_jsonStubsPtr->jSON_Keys) /* 30 */
#define JSON_Decode \
	(rl_jsonStubsPtr->jSON_Decode) /* 31 */
#define JSON_Foreach \
	(rl_jsonStubsPtr->jSON_Foreach) /* 32 */
#define JSON_Valid \
	(rl_jsonStubsPtr->jSON_Valid) /* 33 */
#define JSON_NewJvalObj \
	(rl_jsonStubsPtr->jSON_NewJvalObj) /* 34 */
#define JSON_DbNewJvalObj \
	(rl_jsonStubsPtr->jSON_DbNewJvalObj) /* 35 */
/* Slot 36 is reserved */
/* Slot 37 is reserved */
/* Slot 38 is reserved */
/* Slot 39 is reserved */
#define CBOR_GetDataItemFromPath \
	(rl_jsonStubsPtr->cBOR_GetDataItemFromPath) /* 40 */
#define CBOR_Length \
	(rl_jsonStubsPtr->cBOR_Length) /* 41 */

#endif /* defined(USE_RL_JSON_STUBS) */

/* !END!: Do not edit above this line. */
