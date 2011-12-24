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
#include "task.h"
#include "thread.h"


static int
kern_thread_state (kern_ThreadObj *self, kern_multi_arch_tstate *multi_state)
{
    kern_return_t kr;

    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;

    switch (self->arch) {
    case (_KERN_THREAD_ARCH_UNKNOWN):
    case (_KERN_THREAD_ARCH_X86_64):
        {
            flavor = x86_THREAD_STATE64;
            count = x86_THREAD_STATE64_COUNT;
            kr = thread_get_state(self->port, flavor,
                                  (thread_state_t) &(multi_state->state64),
                                  &count);

            if (self->arch != _KERN_THREAD_ARCH_X86_64) {
                if (kr == KERN_SUCCESS) {
                    self->arch = _KERN_THREAD_ARCH_X86_64;
                    break;
                }
            } else { break; }
        }

    case (_KERN_THREAD_ARCH_X86):
        {
            flavor = x86_THREAD_STATE32;
            count = x86_THREAD_STATE32_COUNT;
            kr = thread_get_state(self->port, flavor,
                                  (thread_state_t) &(multi_state->state32),
                                  &count);

            if (kr == KERN_SUCCESS && self->arch != _KERN_THREAD_ARCH_X86)
                self->arch = _KERN_THREAD_ARCH_X86;
        }
    }

    return kr;
}

/*
 * Get execution state (e.g. machine registers) for the thread
 *
 * Arguments: None
 * Returns:   Dictionary of registers
 */
static PyObject *
kern_Thread_getState (kern_ThreadObj *self)
{
    kern_return_t kr;
    kern_multi_arch_tstate multi_state;

    kr = kern_thread_state(self, &multi_state);

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    /* self->arch is always set by this point */

    if (self->arch == _KERN_THREAD_ARCH_X86_64) {

#define KV(kv) #kv, multi_state.state64.__##kv
        return Py_BuildValue("{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,"
                             "s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
                             KV(rax), KV(rbx), KV(rcx), KV(rdx), KV(rdi),
                             KV(rsi), KV(rbp), KV(rsp), KV(r8), KV(r9), KV(r10),
                             KV(r11), KV(r12), KV(r13), KV(r14), KV(r15),
                             KV(rip), KV(rflags), KV(cs), KV(fs), KV(gs));
#undef KV

    } else {

#define KV(kv) #kv, multi_state.state32.__##kv
        return Py_BuildValue("{s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,s:I,"
                             "s:I,s:I,s:I,s:I}", KV(eax), KV(ebx), KV(ecx),
                             KV(edx), KV(edi), KV(esi), KV(ebp), KV(esp),
                             KV(ss), KV(eflags), KV(eip), KV(cs), KV(ds),
                             KV(es), KV(fs), KV(gs));
#undef KV

    }
}

/*
 * Set execution state (e.g. machine registers) for the thread. Should only be
 * called while the thread is paused
 *
 * Arguments: state - dictionary of register-value pairs
 *                    e.g. { "eip" : 0x41414141 }
 * Returns:   None
 */
static PyObject *
kern_Thread_setState (kern_ThreadObj *self, PyObject *args, PyObject *kwds)
{
    kern_return_t kr;
    kern_multi_arch_tstate multi_state;
    PyObject *stateDict = NULL, *key, *value;
    char *reg;
    uint64_t val64;
    unsigned int val32;
    Py_ssize_t pos = 0;

    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;

    static char *kwlist[] = {"state", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist,
                                      &PyDict_Type, &stateDict))
        return NULL;

    kr = kern_thread_state(self, &multi_state);

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    /* self->arch is always set by this point */

    while (PyDict_Next(stateDict, &pos, &key, &value)) {
        reg = PyString_AsString(key);
        if (self->arch == _KERN_THREAD_ARCH_X86_64) {
            val64 = (uint64_t) PyLong_AsSsize_t(value);
            if (val64 == -1)
                return NULL;

            if (!strcmp(reg, "rax")) multi_state.state64.__rax = val64;
            else if (!strcmp(reg, "rax")) multi_state.state64.__rax = val64;
            else if (!strcmp(reg, "rbx")) multi_state.state64.__rbx = val64;
            else if (!strcmp(reg, "rcx")) multi_state.state64.__rcx = val64;
            else if (!strcmp(reg, "rdx")) multi_state.state64.__rdx = val64;
            else if (!strcmp(reg, "rdi")) multi_state.state64.__rdi = val64;
            else if (!strcmp(reg, "rsi")) multi_state.state64.__rsi = val64;
            else if (!strcmp(reg, "rbp")) multi_state.state64.__rbp = val64;
            else if (!strcmp(reg, "rsp")) multi_state.state64.__rsp = val64;
            else if (!strcmp(reg, "r8")) multi_state.state64.__r8 = val64;
            else if (!strcmp(reg, "r9")) multi_state.state64.__r9 = val64;
            else if (!strcmp(reg, "r10")) multi_state.state64.__r10 = val64;
            else if (!strcmp(reg, "r11")) multi_state.state64.__r11 = val64;
            else if (!strcmp(reg, "r12")) multi_state.state64.__r12 = val64;
            else if (!strcmp(reg, "r13")) multi_state.state64.__r13 = val64;
            else if (!strcmp(reg, "r14")) multi_state.state64.__r14 = val64;
            else if (!strcmp(reg, "r15")) multi_state.state64.__r15 = val64;
            else if (!strcmp(reg, "rip")) multi_state.state64.__rip = val64;
            else if (!strcmp(reg, "rflags")) multi_state.state64.__rflags = val64;
            else if (!strcmp(reg, "cs")) multi_state.state64.__cs = val64;
            else if (!strcmp(reg, "fs")) multi_state.state64.__fs = val64;
            else if (!strcmp(reg, "gs")) multi_state.state64.__gs = val64;
        } else {
            val32 = (unsigned int) PyInt_AsSsize_t(value);

            if (!strcmp(reg, "eax")) multi_state.state32.__eax = val32;
            else if (!strcmp(reg, "ebx")) multi_state.state32.__ebx = val32;
            else if (!strcmp(reg, "ecx")) multi_state.state32.__ecx = val32;
            else if (!strcmp(reg, "edx")) multi_state.state32.__edx = val32;
            else if (!strcmp(reg, "edi")) multi_state.state32.__edi = val32;
            else if (!strcmp(reg, "esi")) multi_state.state32.__esi = val32;
            else if (!strcmp(reg, "ebp")) multi_state.state32.__ebp = val32;
            else if (!strcmp(reg, "esp")) multi_state.state32.__esp = val32;
            else if (!strcmp(reg, "ss")) multi_state.state32.__ss = val32;
            else if (!strcmp(reg, "eflags")) multi_state.state32.__eflags = val32;
            else if (!strcmp(reg, "eip")) multi_state.state32.__eip = val32;
            else if (!strcmp(reg, "cs")) multi_state.state32.__cs = val32;
            else if (!strcmp(reg, "ds")) multi_state.state32.__ds = val32;
            else if (!strcmp(reg, "es")) multi_state.state32.__es = val32;
            else if (!strcmp(reg, "fs")) multi_state.state32.__fs = val32;
            else if (!strcmp(reg, "gs")) multi_state.state32.__gs = val32;
        }
    }

    if (self->arch == _KERN_THREAD_ARCH_X86_64) {
        flavor = x86_THREAD_STATE64;
        count = x86_THREAD_STATE64_COUNT;
        kr = thread_set_state(self->port, flavor,
                              (thread_state_t) &(multi_state.state64), count);
    } else {
        flavor = x86_THREAD_STATE32;
        count = x86_THREAD_STATE32_COUNT;
        kr = thread_set_state(self->port, flavor,
                              (thread_state_t) &(multi_state.state32), count);
    }

    if (kr != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    Py_RETURN_NONE;
}

/*
 * Pause the thread, making it safe for state modification. For a complete
 * explanation see the thread_abort_safely man page. May not be called while
 * already paused
 *
 * Arguments: None
 * Returns:   None
 */
static PyObject *
kern_Thread_pause (kern_ThreadObj *self)
{
    kern_return_t kr;

    if (self->paused) {
        PyErr_SetNone(kern_AlreadyPausedError);
        return NULL;
    }

    if ( (kr = thread_suspend(self->port)) != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    if ( (kr = thread_abort_safely(self->port)) != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    self->paused = 1;

    Py_RETURN_NONE;
}

/*
 * Resume the thread. May only be called while paused
 *
 * Arguments: None
 * Returns:   None
 */
static PyObject *
kern_Thread_resume (kern_ThreadObj *self)
{
    kern_return_t kr;

    if (! self->paused) {
        PyErr_SetNone(kern_NotPausedError);
        return NULL;
    }

    if ( (kr = thread_resume(self->port)) != KERN_SUCCESS) {
        handle_kern_rtn(kr);
        return NULL;
    }

    self->paused = 0;

    Py_RETURN_NONE;
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
        self->arch = _KERN_THREAD_ARCH_UNKNOWN;
        self->paused = 0;
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
    {"paused", T_BOOL, offsetof(kern_ThreadObj, paused), 0,
     "Pause status"},
    {NULL} /* Sentinel */
};

static PyMethodDef kern_ThreadMethods[] = {
    {"getState", (PyCFunction)kern_Thread_getState, METH_NOARGS,
     "Return execution state for the thread"},
    {"setState", (PyCFunction)kern_Thread_setState, METH_KEYWORDS,
     "Set the execution state for the thread"},
    {"pause", (PyCFunction)kern_Thread_pause, METH_NOARGS,
     "Pause the thread"},
    {"resume", (PyCFunction)kern_Thread_resume, METH_NOARGS,
     "Resume the thread"},
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
