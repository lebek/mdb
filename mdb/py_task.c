#include "mdb.h"
#include "py_mdb.h"

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
	int ret;

	if ( (ret = mdb_get_port(self->pid, &(self->port))) != KERN_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, "failed to attach");
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
	uint64_t address;
	uint64_t size;
	vm_region_basic_info_data_64_t info;

	mdb_Region *region;

	int ret;

	static char *kwlist[] = {"address", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist,
									  &address))
		return NULL;

	if (self->attached) {

		ret = mdb_get_region(self->port, &address, &size, &info);

		/* XXX TODO should kernel return codes and structs even
		 * be visible at this level? */
		if (ret != KERN_SUCCESS) {
			return Py_None;
		}

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

	} else {
		PyErr_SetString(PyExc_RuntimeError, "not attached");
		return NULL;
	}
}

static PyObject *
Task_basicInfo (mdb_Task *self)
{
	int ret;
	task_basic_info_data_t info;

	if (self->attached) {
		ret = mdb_get_info(self->port, &info);
		printf("%d\n", ret);

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
	} else {
		PyErr_SetString(PyExc_RuntimeError, "not attached");
		return NULL;
	}
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
	0,						   /*ob_size*/
	"mdb.Task",				   /*tp_name*/
	sizeof(mdb_Task),		   /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Task_dealloc,  /*tp_dealloc*/
	0,						   /*tp_print*/
	0,						   /*tp_getattr*/
	0,						   /*tp_setattr*/
	0,						   /*tp_compare*/
	0,						   /*tp_repr*/
	0,						   /*tp_as_number*/
	0,						   /*tp_as_sequence*/
	0,						   /*tp_as_mapping*/
	0,						   /*tp_hash*/
	0,						   /*tp_call*/
	0,						   /*tp_str*/
	0,						   /*tp_getattro*/
	0,						   /*tp_setattro*/
	0,						   /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	"Task objects",			   /*tp_doc*/
	0,				           /* tp_traverse */
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
