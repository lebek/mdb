#include "mdb.h"

#include <Python.h>
#include "structmember.h"

/* forward declaration */
extern PyTypeObject mdb_TaskType;
extern PyTypeObject mdb_RegionType;

typedef struct
{
    PyObject_HEAD
    int pid;
    char attached;
    mach_port_t port;

} mdb_Task;

typedef struct
{
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
