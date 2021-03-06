/*-
 * Copyright (c) 2011 Peter Le Bek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <Python.h>

#include "task.h"
#include "memory.h"
#include "thread.h"
#include "kern.h"


PyObject *kern_Error,
         *kern_AlreadyAttachedError,
         *kern_NotAttachedError,
         *kern_KernelError,
         *kern_AlreadyPausedError,
         *kern_NotPausedError;

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

    if (PyType_Ready(&kern_TaskType) < 0)
        return;

    if (PyType_Ready(&kern_MemoryType) < 0)
        return;

    if (PyType_Ready(&kern_ThreadType) < 0)
        return;

    m = Py_InitModule3("mdb.kern", kern_methods,
                       "Kernel module.");
    Py_INCREF(&kern_TaskType);
    Py_INCREF(&kern_MemoryType);
    Py_INCREF(&kern_ThreadType);

    PyModule_AddObject(m, "Task", (PyObject *)&kern_TaskType);
    PyModule_AddObject(m, "Memory", (PyObject *)&kern_MemoryType);
    PyModule_AddObject(m, "Thread", (PyObject *)&kern_ThreadType);

    /*
     * ADD_EXCEPTION(dict,name,base) expands to a correct Exception declaration,
     * inserting mdb.kern.name into dict, derviving the exception from base.
     *
     * Adapted from PyOpenSSL.
     */
#define ADD_EXCEPTION(_name, _base)                                     \
    do {                                                                \
        kern_##_name = PyErr_NewException("mdb.kern."#_name, _base, NULL); \
        if (kern_##_name == NULL)                                       \
            return;                                                     \
        Py_INCREF(kern_##_name);                                        \
        if (PyModule_AddObject(m, #_name, kern_##_name) != 0)           \
            return;                                                     \
    } while (0)

    kern_Error = PyErr_NewException("mdb.kern.Error", NULL, NULL);
    if (kern_Error == NULL) {
        return;
    }

    Py_INCREF(kern_Error);
    if (PyModule_AddObject(m, "Error", kern_Error) != 0)
        return;

    ADD_EXCEPTION(AlreadyAttachedError, kern_Error);
    ADD_EXCEPTION(NotAttachedError, kern_Error);
    ADD_EXCEPTION(KernelError, kern_Error);
    ADD_EXCEPTION(AlreadyPausedError, kern_Error);
    ADD_EXCEPTION(NotPausedError, kern_Error);
#undef ADD_EXCEPTION

}
