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
#include "kern.h"
#include "region.h"
#include "thread.h"
#include "exception.h"
#include "task.h"


/*
 * Attach to the task
 *
 * Arguments: None
 * Returns:   None
 */
static PyObject *
kern_Task_attach (kern_TaskObj* self, PyObject *args, PyObject *kwds)
{
    kern_return_t kr;

    if (self->attached) {
        PyErr_SetNone(kern_AlreadyAttachedError);
        return NULL;
    }

    if ( (kr = task_for_pid(mach_task_self(),
                            (pid_t) self->pid,
                            &(self->port))) != KERN_SUCCESS)
        KERN_ERROR(kr);

    /* Hook-up special exception port */
    if ( (kr = kern_excserv_init(self->port,
                                 &(self->exc_port))) != KERN_SUCCESS)
        KERN_ERROR(kr);

    self->attached = 1;

    Py_RETURN_NONE;
}

/*
 * Find a memory region in the task's address space
 *
 * Arguments: address - the address at which to start looking for a region
 * Returns:   Region object, or None if no region is found
 */
static PyObject *
kern_Task_findRegion (kern_TaskObj *self, PyObject *args, PyObject *kwds)
{
    vm_address_t address;
    vm_size_t size;

    kern_return_t kr;
    mach_port_t object;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

    kern_RegionObj *region = NULL;

    static char *kwlist[] = {"address", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist,
                                      &address))
        return NULL;

    if (! self->attached) {
        PyErr_SetNone(kern_NotAttachedError);
        return NULL;
    }

    kr = vm_region_64(self->port, &address, &size, VM_REGION_BASIC_INFO,
                      (vm_region_info_t) &info, &info_count, &object);

    if (kr == KERN_INVALID_ADDRESS)
        Py_RETURN_NONE;

    region = PyObject_New(kern_RegionObj, &kern_RegionType);
    if (region == NULL)
        return NULL;

    region->address = address;
    region->size = size;

    region->protection = (int) info.protection;
    region->maxProtection = (int) info.max_protection;
    region->inheritance = (int) info.inheritance;
    region->shared = (char) info.shared;
    region->reserved = (char) info.reserved;
    region->behavior = (int) info.behavior;

    Py_INCREF(self);
    region->task = (PyObject *) self;

    return (PyObject *) region;
}

/*
 * Get the task's list of threads.
 *
 * Arguments: None
 * Returns: List of Thread objects
 */
static PyObject *
kern_Task_getThreads (kern_TaskObj *self)
{
    kern_return_t kr;
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    PyObject *threads = NULL;
    Py_ssize_t threads_size = 0;
    kern_ThreadObj *thread = NULL;

    int i;

    if (! self->attached) {
        PyErr_SetNone(kern_NotAttachedError);
        return NULL;
    }

    kr = task_threads(self->port, &thread_list, &thread_count);

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    threads = PyList_New((Py_ssize_t) 0);
    if (threads == NULL)
        return PyErr_NoMemory();

    for (i = 0; i < thread_count; ++i) {
        thread = PyObject_New(kern_ThreadObj, &kern_ThreadType);
        if (thread == NULL)
            goto error;

        thread->port = thread_list[i];
        thread->arch = _KERN_THREAD_ARCH_UNKNOWN;
        thread->paused = 0;

        Py_INCREF(self);
        thread->task = (PyObject *) self;
        if (PyList_Append(threads, (PyObject *) thread))
            return NULL;

        thread = NULL;
    }

    return threads;

 error:
    /* Decrement the reference count for previously allocated PyObjects */
    threads_size = PyList_Size(threads);
    for (i = 0; i < threads_size; ++i)
        Py_DECREF(PyList_GetItem(threads, (Py_ssize_t) i));

    Py_DECREF(threads);

    return PyErr_NoMemory();
}

/*
 * Get basic information about the task, such as the task's suspend count and
 * number of resident pages
 *
 * Arguments: None
 * Returns:   (suspend_count, virtual_size, resident_size, user_time,
 *             system_time)
 */
static PyObject *
kern_Task_basicInfo (kern_TaskObj *self)
{
    kern_return_t kr;
    task_basic_info_data_t info;
    mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;

    if (! self->attached) {
        PyErr_SetNone(kern_NotAttachedError);
        return NULL;
    }

    kr  = task_info(self->port, TASK_BASIC_INFO_64, (task_info_t) &info,
                    &info_count);

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    return Py_BuildValue("{s:K,s:K,s:K,s:(K,K),s:(K,K)}",
                         "suspend_count", info.suspend_count,
                         "virtual_size", info.virtual_size,
                         "resident_size", info.resident_size,
                         "user_time", info.user_time.seconds,
                         info.user_time.microseconds,
                         "system_time", info.system_time.seconds,
                         info.system_time.microseconds);
}

static PyObject *
kern_Task_poll (kern_TaskObj *self)
{
    kern_exc_event event;
    kern_ThreadObj *thread = NULL;

    if (kern_excserv_poll(self->exc_port, 100, &event) <= 0)
        Py_RETURN_NONE;

    thread = PyObject_New(kern_ThreadObj, &kern_ThreadType);
    if (thread == NULL)
        return PyErr_NoMemory();

    thread->port = event.thread;
    thread->arch = _KERN_THREAD_ARCH_UNKNOWN;
    thread->paused = 1;

    Py_INCREF(self);
    thread->task = (PyObject *) self;

    return Py_BuildValue("{s:O,s:s}", "thread", (PyObject *) thread,
                         "type", kern_exc_string(event.type));
}

static void
kern_Task_dealloc (kern_TaskObj* self)
{
    self->ob_type->tp_free( (PyObject*) self);
}

static PyObject *
kern_Task_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    kern_TaskObj *self = NULL;

    self = (kern_TaskObj *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->pid = 0;
        self->attached = 0;
    }

    return (PyObject *) self;
}

static int
kern_Task_init (kern_TaskObj *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"pid", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist,
                                      &self->pid))
        return -1;

    return 0;
}

static PyMemberDef kern_TaskMembers[] = {
    {"pid", T_INT, offsetof(kern_TaskObj, pid), 0,
     "Task pid"},
    {"attached", T_BOOL, offsetof(kern_TaskObj, attached), 0,
     "Attachment status"},
    {NULL} /* Sentinel */
};

static PyMethodDef kern_TaskMethods[] = {
    {"attach", (PyCFunction)kern_Task_attach, METH_NOARGS,
     "Attach to the task"},
    {"poll", (PyCFunction)kern_Task_poll, METH_NOARGS,
     "Poll the task for events"},
    {"findRegion", (PyCFunction)kern_Task_findRegion, METH_KEYWORDS,
     "Return memory region in the tasks address space"},
    {"getThreads", (PyCFunction)kern_Task_getThreads, METH_NOARGS,
     "Return the task's list of threads" },
    {"basicInfo", (PyCFunction)kern_Task_basicInfo, METH_NOARGS,
     "Return basic information about the task"},
    {NULL} /* Sentinel */
};

PyTypeObject kern_TaskType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "mdb.kern.Task",           /* tp_name */
    sizeof(kern_TaskObj),      /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)kern_Task_dealloc, /* tp_dealloc */
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
    "Task objects",            /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    kern_TaskMethods,          /* tp_methods */
    kern_TaskMembers,          /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)kern_Task_init,  /* tp_init */
    0,                         /* tp_alloc */
    kern_Task_new,             /* tp_new */
};
