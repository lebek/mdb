#include <Python.h>

#include "task.h"
#include "region.h"

static PyMethodDef kern_methods[] = {
	{ NULL } /* Sentinel */
};


#ifndef PyMODINIT_FUNC /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initkern(void)
{
    PyObject* m;

    if (PyType_Ready(&mdb_TaskType) < 0)
        return;

    if (PyType_Ready(&mdb_RegionType) < 0)
        return;

    m = Py_InitModule3("mdb.kern", kern_methods,
                       "Kernel module.");
    Py_INCREF(&mdb_TaskType);
    Py_INCREF(&mdb_RegionType);

    PyModule_AddObject(m, "Task", (PyObject *)&mdb_TaskType);
    PyModule_AddObject(m, "Region", (PyObject *)&mdb_RegionType);
}
