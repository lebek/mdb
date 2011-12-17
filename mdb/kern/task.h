
#ifndef _KERN_TASK_H
#define _KERN_TASK_H

#include <mach/mach_types.h>

#include "structmember.h"

extern PyTypeObject mdb_TaskType;

typedef struct {
    PyObject_HEAD
    int pid;
    char attached;
    mach_port_t port;
} mdb_Task;

#endif
