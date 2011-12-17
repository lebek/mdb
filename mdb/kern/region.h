
#ifndef _KERN_REGION_H
#define _KERN_REGION_H

#include <mach/mach_types.h>

#include "structmember.h"

extern PyTypeObject mdb_RegionType;

typedef struct {
    PyObject_HEAD
    uint64_t address;
    uint64_t size;
    PyObject *task;

	int protection;
	int maxProtection;
	int inheritance;
	char shared;
	char reserved;
	int behavior;
} mdb_Region;

#endif
