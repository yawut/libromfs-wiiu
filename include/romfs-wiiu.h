#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
	romfsInit()
	Mounts app's romfs.

	Returns 0 if succesfull or a negative
	value if there was an error
*/
int32_t romfsInit(void);

/*
	romfsExit()
	Unmounts app's romfs.

	Returns 0 if succesfull or a negative
	value if there was an error
*/
int32_t romfsExit(void);


#ifdef __cplusplus
}
#endif
