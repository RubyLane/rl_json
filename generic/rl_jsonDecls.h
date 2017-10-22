
/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN Tcl_Obj *	JSON_NewJvalObj(int type, Tcl_Obj *val);
/* 1 */
EXTERN int		JSON_GetJvalFromObj(Tcl_Interp *interp, Tcl_Obj *obj,
				int *type, Tcl_Obj **val);

typedef struct Rl_jsonStubs {
    int magic;
    void *hooks;

    Tcl_Obj * (*jSON_NewJvalObj) (int type, Tcl_Obj *val); /* 0 */
    int (*jSON_GetJvalFromObj) (Tcl_Interp *interp, Tcl_Obj *obj, int *type, Tcl_Obj **val); /* 1 */
} Rl_jsonStubs;

extern const Rl_jsonStubs *rl_jsonStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_RL_JSON_STUBS)

/*
 * Inline function declarations:
 */

#define JSON_NewJvalObj \
	(rl_jsonStubsPtr->jSON_NewJvalObj) /* 0 */
#define JSON_GetJvalFromObj \
	(rl_jsonStubsPtr->jSON_GetJvalFromObj) /* 1 */

#endif /* defined(USE_RL_JSON_STUBS) */

/* !END!: Do not edit above this line. */
