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
#include "region.h"
#include "task.h"

static void
Task_dealloc (mdb_Task* self)
{
    self->ob_type->tp_free( (PyObject*) self);
}

static PyObject *
Task_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    mdb_Task *self;

    self = (mdb_Task *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->pid = 0;
        self->attached = 0;
    }

    return (PyObject *) self;
}

static int
Task_init (mdb_Task *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"pid", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist,
                                      &self->pid))
        return -1;

    return 0;
}

static PyMemberDef Task_members[] = {
    {"pid", T_INT, offsetof(mdb_Task, pid), 0,
     "Task pid"},
    {"attached", T_BOOL, offsetof(mdb_Task, attached), 0,
     "Attachment status"},
    {NULL} /* Sentinel */
};

static PyObject *
Task_attach (mdb_Task* self)
{
    kern_return_t kr = task_for_pid(mach_task_self(), (pid_t) self->pid,
                                    &(self->port));

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    self->attached = 1;

    return Py_None;
}

/*
 * `address` - the address at which to start looking for a region
 */
static PyObject *
Task_findRegion (mdb_Task *self, PyObject *args, PyObject *kwds)
{
    vm_address_t address;
    vm_size_t size;

    kern_return_t kr;
    mach_port_t object;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

    mdb_Region *region;

    static char *kwlist[] = {"address", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist,
                                      &address))
        return NULL;

    if (! self->attached) {
        PyErr_SetString(PyExc_RuntimeError, "not attached");
        return NULL;
    }

    kr = vm_region_64(self->port, &address, &size, VM_REGION_BASIC_INFO,
                      (vm_region_info_t) &info, &info_count, &object);

    if (kr != KERN_SUCCESS)
        return Py_None;

    region = PyObject_New(mdb_Region, &mdb_RegionType);
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

static PyObject *
Task_basicInfo (mdb_Task *self)
{
    kern_return_t kr;
    task_basic_info_data_t info;
    mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;

    if (! self->attached) {
        PyErr_SetString(PyExc_RuntimeError, "not attached");
        return NULL;
    }

    kr  = task_info(self->port, TASK_BASIC_INFO_64, (task_info_t) &info, &info_count);

    /* NOTE
     *
     * struct task_basic_info
     * {
     *  integer_t      suspend_count;
     *  vm_size_t       virtual_size;
     *  vm_size_t      resident_size;
     *  time_value_t       user_time;
     *  time_value_t     system_time;
     *  policy_t              policy;
     * };
     *
     * struct time_value {
     *  integer_t seconds;
     *  integer_t microseconds;
     * };
     */

    return Py_BuildValue("(KKK(KK)(KK))",
                         (uint64_t) info.suspend_count,
                         (uint64_t) info.virtual_size,
                         (uint64_t) info.resident_size,
                         (uint64_t) info.user_time.seconds,
                         (uint64_t) info.user_time.microseconds,
                         (uint64_t) info.system_time.seconds,
                         (uint64_t) info.system_time.microseconds);
}

static PyMethodDef Task_methods[] = {
    {"attach", (PyCFunction)Task_attach, METH_NOARGS,
     "Attach to this task"},
    {"findRegion", (PyCFunction)Task_findRegion, METH_KEYWORDS,
     "Return memory region in this tasks address space"},
    {"basicInfo", (PyCFunction)Task_basicInfo, METH_NOARGS,
     "Return basic information about the task"},
    {NULL} /* Sentinel */
};

PyTypeObject mdb_TaskType= {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "mdb.kern.Task",           /* tp_name */
    sizeof(mdb_Task),          /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Task_dealloc,  /* tp_dealloc */
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
    Task_methods,              /* tp_methods */
    Task_members,              /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Task_init,       /* tp_init */
    0,                         /* tp_alloc */
    Task_new,                  /* tp_new */
};
