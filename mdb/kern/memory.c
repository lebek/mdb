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

/* mach_vm_* routines appear to do the right thing in both 32 and 64-bit mode */
#include <mach/mach_vm.h>

#include "util.h"
#include "task.h"
#include "memory.h"


/*
 * Read the specified range of memory. The range is trimmed if it exceeds the
 * size of the memory
 *
 * Arguments: offset - byte offset from memory start at which to start the
 *                     read, default = 0
 *            size - number of bytes to read, default = self->size
 * Returns:   Byte string
 */
static PyObject *
kern_Memory_read (kern_MemoryObj *self, PyObject *args, PyObject *kwds)
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

    buf = (unsigned char *) malloc((size_t) buf_size);
    if (buf == NULL)
        return NULL;

    kr = mach_vm_read_overwrite( ( (kern_TaskObj *)self->task)->port,
                                 (vm_address_t) (self->address + offset),
                                 (vm_size_t) buf_size,
                                 (vm_address_t) buf,
                                 (mach_vm_size_t *) &buf_size);
    CHECK_KR(kr);

    return PyString_FromStringAndSize((const char *) buf,
                                      (Py_ssize_t) buf_size);
}

/*
 * Write data to memory at the specified offset. The data is trimmed if it
 * exceeds the size of the memory
 *
 * Arguments: data - byte string to write
 *            offset - byte offset from memory start at which to start the
 *            write, default = 0
 * Returns:   None
 */
static PyObject *
kern_Memory_write (kern_MemoryObj *self, PyObject *args, PyObject *kwds)
{
    kern_return_t kr;
    PyObject *data = NULL;
    uint64_t data_size = 0;
    vm_address_t offset = 0;
    static char *kwlist[] = {"data", "offset", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O!|K", kwlist,
                                      &PyString_Type, &data, &offset))
        return NULL;

    data_size = PyString_Size(data);

    if (offset + data_size > self->size)
        data_size = self->size - offset;

    kr = mach_vm_write( ( (kern_TaskObj *)self->task)->port,
                        self->address + offset,
                        (pointer_t) PyString_AsString(data),
                        (mach_msg_type_number_t) data_size);
    CHECK_KR(kr);

    Py_RETURN_NONE;
}

static void
kern_Memory_dealloc(kern_MemoryObj *self)
{
    Py_XDECREF(self->task);
    self->ob_type->tp_free( (PyObject*) self);
}

static PyObject *
kern_Memory_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    kern_MemoryObj *self = NULL;

    self = (kern_MemoryObj *) type->tp_alloc(type, 0);

    if (self != NULL) {
        self->address = 0;
        self->size = 0;

        Py_INCREF(Py_None);
        self->task = Py_None;
    }

    return (PyObject *) self;
}

static int
kern_Memory_init (kern_MemoryObj *self, PyObject *args, PyObject *kwds)
{
    PyObject *task = NULL, *tmp;

    static char *kwlist[] = {"task", "address", "size", NULL};
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O!KK", kwlist,
                                      &kern_TaskType, &task, &self->address,
                                      &self->size))
        return -1;

    if (task) {
        tmp = self->task;
        Py_INCREF(task);
        self->task = task;
        Py_XDECREF(tmp);
    }

    return 0;
}

static PyMemberDef kern_MemoryMembers[] = {
    {"task", T_OBJECT_EX, offsetof(kern_MemoryObj, task), 0,
     "Task containing this memory"},
    {"address", T_ULONGLONG, offsetof(kern_MemoryObj, address), 0,
     "Memory start address"},
    {"size", T_ULONGLONG, offsetof(kern_MemoryObj, size), 0,
     "Memory size"},
    {NULL} /* Sentinel */
};

static PyMethodDef kern_MemoryMethods[] = {
    {"read", (PyCFunction)kern_Memory_read, METH_KEYWORDS,
     "Read bytes of memory"},
    {"write", (PyCFunction)kern_Memory_write, METH_KEYWORDS,
     "Write bytes to memory"},
    {NULL} /* Sentinel */
};

PyTypeObject kern_MemoryType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "mdb.kern.Memory",         /* tp_name */
    sizeof(kern_MemoryObj),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)kern_Memory_dealloc, /* tp_dealloc */
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
    "Memory objects",          /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    kern_MemoryMethods,        /* tp_methods */
    kern_MemoryMembers,        /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)kern_Memory_init, /* tp_init */
    0,                         /* tp_alloc */
    kern_Memory_new,           /* tp_new */
};
