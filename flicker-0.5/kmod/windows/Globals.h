//
// Globals.h contains information about global variables in the FlickerDrv
// driver. It also contains macros used throughout the driver code.
//

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "flicker.h"

#define FLICKER_POOL_ALLOCATION_TAG       'Flic'

// The structure FLICKER_GLOBALS contains all the global variables used by
// this driver in one place, so it's clear what's global and what isn't.
typedef struct _flicker_globals {
    PDEVICE_OBJECT               flickerDeviceObject;
    BOOLEAN                      createdSymbolicLinkToDevice;
    KSPIN_LOCK                   mainMutex;

	// These get assigned to point into mle_region
	void *reload_state; 
	void *input_params;
	void *output_params;
	// This helps Windows kernel cope with being suspended
	KIRQL savedIrql; 
	
	cpu_t kcpu_region;
} FLICKER_GLOBALS;


// EXTERNAL VARIABLES, visible throughout all source files
extern FLICKER_GLOBALS globals;

#endif /* __GLOBALS_H__ */
