#ifndef _TIP445_H
#define _TIP445_H

#if TIP445_SHIM
#include <string.h>
#include <assert.h>
#include <limits.h>

/* Just enough of TIP445 to build rl_json on Tcl 8.6 */

#ifndef Tcl_ObjInternalRep
typedef union Tcl_ObjInternalRep {
	struct {
		void*	ptr1;
		void*	ptr2;
	} twoPtrValue;
	struct {
		void*			ptr;
		unsigned long	value;
	} ptrAndLongRep;
} Tcl_ObjInternalRep;
#endif

#ifndef Tcl_FetchInternalRep
#ifdef Tcl_FetchIntRep
#define Tcl_FetchInternalRep	Tcl_FetchIntRep
#else
#define Tcl_FetchInternalRep(obj, type)		(Tcl_ObjInternalRep*)(((obj)->typePtr == (type)) ? &(obj)->internalRep : NULL)
#endif
#endif

#ifndef Tcl_FreeInternalRep
#ifdef Tcl_FreeIntRep
#define Tcl_FreeInternalRep	Tcl_FreeIntRep
#else
static inline void Tcl_FreeInternalRep(Tcl_Obj* obj)
{
	if (obj->typePtr && obj->typePtr->freeInternalRepProc)
		obj->typePtr->freeInternalRepProc(obj);
}
#endif
#endif

#ifndef Tcl_StoreInternalRep
#ifdef Tcl_StoreIntRep
#define Tcl_StoreInternalRep Tcl_StoreIntRep
#else
static inline void Tcl_StoreInternalRep(Tcl_Obj* objPtr, const Tcl_ObjType* typePtr, const Tcl_ObjInternalRep* irPtr)
{
	objPtr->typePtr = typePtr;
	memcpy(&objPtr->internalRep, irPtr, sizeof(Tcl_ObjInternalRep));
}
#endif
#endif

#ifndef Tcl_HasStringRep
#define Tcl_HasStringRep(obj)	((obj)->bytes != NULL)
#endif

#ifndef Tcl_InitStringRep
static char* Tcl_InitStringRep(Tcl_Obj* objPtr, const char* bytes, unsigned numBytes)
{
	assert(objPtr->bytes == NULL || bytes == NULL);

	if (numBytes > INT_MAX) {
		Tcl_Panic("max size of a Tcl value (%d bytes) exceeded", INT_MAX);
	}

	/* Allocate */
	if (objPtr->bytes == NULL) {
		/* Allocate only as empty - extend later if bytes copied */
		objPtr->length = 0;
		if (numBytes) {
			objPtr->bytes = (char*)attemptckalloc(numBytes + 1);
			if (objPtr->bytes == NULL) return NULL;
			if (bytes) {
				/* Copy */
				memcpy(objPtr->bytes, bytes, numBytes);
				objPtr->length = (int)numBytes;
			}
		} else {
			//TclInitStringRep(objPtr, NULL, 0);
			objPtr->bytes = "";
			objPtr->length = 0;
		}
	} else {
		objPtr->bytes = (char*)ckrealloc(objPtr->bytes, numBytes + 1);
		objPtr->length = (int)numBytes;
	}

	/* Terminate */
	objPtr->bytes[objPtr->length] = '\0';

	return objPtr->bytes;
}
#endif

#endif	// TIP445_SHIM

#endif	// _TI445_H
