#ifndef _NAMES_H
#define _NAMES_H
#if DEBUG

const char* name(const void *const thing);	// name given thing
void* thing(const char *const name);		// thing given name
void names_shutdown();

#endif
#endif
