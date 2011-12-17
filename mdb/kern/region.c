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
#include "region.h"

static void
Region_dealloc(mdb_Region *self)
{
    Py_XDECREF(self->task);
    self->ob_type->tp_free( (PyObject*) self);
}

static PyObject *
Region_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    mdb_Region *self;

    self = (mdb_Region *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->address = 0;
        self->size = 0;

        self->protection = 0;
        self->maxProtection = 0;
        self->inheritance = 0;
        self->shared = 0;
        self->reserved = 0;
        self->behavior = 0;

        self->task = Py_None;
        if (self->task == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *) self;
}

/* Necessary? */
static int
Region_init (mdb_Region *self, PyObject *args, PyObject *kwds)
{
    PyObject *task = NULL, *tmp;

    static char *kwlist[] = {"task", "address", "size", NULL};
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "OKK", kwlist, &task,
                                      &self->address, &self->size))
        return -1;

    /* XXX TODO enforce type of `task` */

    if (task) {
        tmp = self->task;
        Py_INCREF(task);
        self->task = task;
        Py_XDECREF(tmp);
    }

    return 0;
}

static PyMemberDef Region_members[] = {
    {"task", T_OBJECT_EX, offsetof(mdb_Region, task), 0,
     "Task containing this region"},
    {"address", T_ULONGLONG, offsetof(mdb_Region, address), 0,
     "Region start address"},
    {"size", T_ULONGLONG, offsetof(mdb_Region, size), 0,
     "Region size"},
    {"protection", T_INT, offsetof(mdb_Region, protection), 0,
     "The current protection for this region"},
    {"maxProtection", T_INT, offsetof(mdb_Region, maxProtection), 0,
     "The maximum protection allowed for this region"},
    {"inheritance", T_INT, offsetof(mdb_Region, inheritance), 0,
     "The inheritance attribute for this region"},
    {"shared", T_BOOL, offsetof(mdb_Region, shared), 0,
     "If true, this region is shared by another task"},
    {"reserved", T_BOOL, offsetof(mdb_Region, reserved), 0,
      "If true this region is protected from random allocation"},
    {"behavior", T_INT, offsetof(mdb_Region, behavior), 0,
     "Expected reference pattern for the memory"},
    {NULL} /* Sentinel */
};

/*
 * `offset`
 *  the byte offset from region start at which to start the read
 *  default = 0
 *
 * `size`
 *  the number of bytes to read
 *  default = self->size
 */
static PyObject *
Region_read (mdb_Region *self, PyObject *args, PyObject *kwds)
{
    kern_return_t kr;
    unsigned char *buf;
    uint64_t offset = 0;
    uint64_t buf_size = self->size;

    static char *kwlist[] = {"offset", "size", NULL};
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|KK", kwlist,
                                      &offset, &buf_size))
        return NULL;

    if (offset + buf_size > self->size)
        buf_size = self->size - offset;

    /* XXX TODO check `task` not none */

    buf = (unsigned char *) malloc((size_t) buf_size);

    kr = vm_read_overwrite( ( (mdb_Task *)self->task)->port,
                            (vm_address_t) (self->address + offset),
                            (vm_size_t) buf_size,
                            (vm_address_t) buf, (vm_size_t *) &buf_size);

    return PyString_FromStringAndSize((const char *) buf,
                                      (Py_ssize_t) buf_size);
}

static PyMethodDef Region_methods[] = {
    {"read", (PyCFunction)Region_read, METH_KEYWORDS,
     "Read bytes in this region"},
    {NULL} /* Sentinel */
};

PyTypeObject mdb_RegionType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "mdb.kern.Region",         /* tp_name */
    sizeof(mdb_Region),        /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Region_dealloc, /* tp_dealloc */
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
    "Region objects",          /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Region_methods,            /* tp_methods */
    Region_members,            /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Region_init,     /* tp_init */
    0,                         /* tp_alloc */
    Region_new,                /* tp_new */
};
