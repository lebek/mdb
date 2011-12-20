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

#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/mach_types.h>

#include "util.h"
#include "task.h"
#include "thread.h"


/*
 * Get execution state (e.g. machine registers) for the thread
 *
 * Arguments: None
 * Returns:   Dictionary
 */
static PyObject *
kern_Thread_getState (kern_ThreadObj *self, PyObject *args, PyObject *kwds)
{
    kern_return_t kr;

    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;
    x86_thread_state64_t state64;
    x86_thread_state32_t state32;

    PyObject *state_dict = NULL, *intval = NULL;

    /* Brute-force the arch.. */

    /* 64-bit? */
    flavor = x86_THREAD_STATE64;
    count = x86_THREAD_STATE64_COUNT;
    kr = thread_get_state(self->port, flavor, (thread_state_t) &state64,
                          &count);
    if (kr == KERN_SUCCESS) {
        state_dict = PyDict_New();
        if (state_dict == NULL)
            return PyErr_NoMemory();

#define INSERT_KV(kv) do { intval = PyInt_FromLong((long) state64.__##kv); \
        if (intval == NULL) \
            return PyErr_NoMemory(); \
        PyDict_SetItemString(state_dict, #kv, intval); \
        intval = NULL; } while (0)

        INSERT_KV(rax);
        INSERT_KV(rbx);
        INSERT_KV(rcx);
        INSERT_KV(rdx);
        INSERT_KV(rdi);
        INSERT_KV(rsi);
        INSERT_KV(rbp);
        INSERT_KV(rsp);
        INSERT_KV(r8);
        INSERT_KV(r9);
        INSERT_KV(r10);
        INSERT_KV(r11);
        INSERT_KV(r12);
        INSERT_KV(r13);
        INSERT_KV(r14);
        INSERT_KV(r15);
        INSERT_KV(rip);
        INSERT_KV(rflags);
        INSERT_KV(cs);
        INSERT_KV(fs);
        INSERT_KV(gs);
#undef INSERT_KV

        return state_dict;
    }

    /* 32-bit? */
    flavor = x86_THREAD_STATE32;
    count = x86_THREAD_STATE32_COUNT;
    kr = thread_get_state(self->port, flavor, (thread_state_t) &state32,
                          &count);

    if (kr == KERN_SUCCESS) {
        state_dict = PyDict_New();
        if (state_dict == NULL)
            return PyErr_NoMemory();

#define INSERT_KV(kv) do { intval = PyInt_FromLong((long) state32.__##kv); \
        if (intval == NULL) \
            return PyErr_NoMemory(); \
        PyDict_SetItemString(state_dict, #kv, intval); \
        intval = NULL; } while (0)

        INSERT_KV(eax);
        INSERT_KV(ebx);
        INSERT_KV(ecx);
        INSERT_KV(edx);
        INSERT_KV(edi);
        INSERT_KV(esi);
        INSERT_KV(ebp);
        INSERT_KV(esp);
        INSERT_KV(ss);
        INSERT_KV(eflags);
        INSERT_KV(eip);
        INSERT_KV(cs);
        INSERT_KV(ds);
        INSERT_KV(es);
        INSERT_KV(fs);
        INSERT_KV(gs);
#undef INSERT_KV

        return state_dict;
    }

    /* Nothing worked, fail */
    handle_kern_rtn(kr);
    return NULL;
}

static void
kern_Thread_dealloc(kern_ThreadObj *self)
{
    Py_XDECREF(self->task);
    self->ob_type->tp_free( (PyObject*) self);
}

static PyObject *
kern_Thread_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    kern_ThreadObj *self = NULL;

    self = (kern_ThreadObj *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->task = Py_None;
        if (self->task == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *) self;
}

static int
kern_Thread_init (kern_ThreadObj *self, PyObject *args, PyObject *kwds)
{
    PyObject *task = NULL, *tmp;

    static char *kwlist[] = {"task", NULL};
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &kern_TaskType,
                                      &task))
        return -1;

    if (task) {
        tmp = self->task;
        Py_INCREF(task);
        self->task = task;
        Py_XDECREF(tmp);
    }

    return 0;
}

static PyMemberDef kern_ThreadMembers[] = {
    {"task", T_OBJECT_EX, offsetof(kern_ThreadObj, task), 0,
     "Task containing this thread"},
    {NULL} /* Sentinel */
};

static PyMethodDef kern_ThreadMethods[] = {
    {"getState", (PyCFunction)kern_Thread_getState, METH_NOARGS,
     "Return execution state for the thread"},
    {NULL} /* Sentinel */
};

PyTypeObject kern_ThreadType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "mdb.kern.Thread",         /* tp_name */
    sizeof(kern_ThreadObj),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)kern_Thread_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Thread objects",          /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    kern_ThreadMethods,        /* tp_methods */
    kern_ThreadMembers,        /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)kern_Thread_init, /* tp_init */
    0,                         /* tp_alloc */
    kern_Thread_new,           /* tp_new */
};
