/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <Python.h>

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"


static PyTypeObject nacldesc_type;

struct PyDesc {
  PyObject_HEAD
  struct NaClDesc *nacldesc;
};


/* Create a Python wrapper for a NaClDesc. */
static PyObject *NaClToPyDesc(struct NaClDesc *nacldesc) {
  struct PyDesc *pydesc = PyObject_New(struct PyDesc, &nacldesc_type);
  if (pydesc == NULL)
    return NULL;
  pydesc->nacldesc = nacldesc;
  return (PyObject *) pydesc;
}

static PyObject *PyDescImcMakeBoundSock(PyObject *self, PyObject *args) {
  struct NaClDesc *pair[2];
  UNREFERENCED_PARAMETER(self);
  UNREFERENCED_PARAMETER(args);
  if (NaClCommonDescMakeBoundSock(pair) != 0) {
    PyErr_SetString(PyExc_Exception, "imc_makeboundsock() failed");
    return NULL;
  }
  return Py_BuildValue("OO", NaClToPyDesc(pair[0]),
                             NaClToPyDesc(pair[1]));
}

static void PyDescDealloc(struct PyDesc *self) {
  NaClDescUnref(self->nacldesc);
}

static struct NaClDescVtbl *GetVtbl(struct NaClDesc *nacldesc) {
  return (struct NaClDescVtbl *) nacldesc->base.vtbl;
}

static PyObject *PyDescImcConnect(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  int errval;
  struct NaClDesc *result_desc;
  UNREFERENCED_PARAMETER(args);
  Py_BEGIN_ALLOW_THREADS;
  errval = GetVtbl(self_desc)->ConnectAddr(self_desc, &result_desc);
  Py_END_ALLOW_THREADS;
  if (errval == 0) {
    return NaClToPyDesc(result_desc);
  } else {
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_connect() failed");
    return NULL;
  }
}

static PyObject *PyDescImcAccept(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  int errval;
  struct NaClDesc *result_desc;
  UNREFERENCED_PARAMETER(args);
  Py_BEGIN_ALLOW_THREADS;
  errval = GetVtbl(self_desc)->AcceptConn(self_desc, &result_desc);
  Py_END_ALLOW_THREADS;
  if (errval == 0) {
    return NaClToPyDesc(result_desc);
  } else {
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_accept() failed");
    return NULL;
  }
}

/* TODO(mseaborn): Add support for sending descriptors. */
static PyObject *PyDescImcSendmsg(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  char *data;
  int data_size;
  struct NaClImcMsgIoVec iov;
  struct NaClImcTypedMsgHdr message;
  ssize_t sent;

  if (!PyArg_ParseTuple(args, "s#", &data, &data_size))
    return NULL;
  iov.base = data;
  iov.length = data_size;
  message.iov = &iov;
  message.iov_length = 1;
  message.ndescv = NULL;
  message.ndesc_length = 0;
  message.flags = 0;
  Py_BEGIN_ALLOW_THREADS;
  sent = NaClImcSendTypedMessage(self_desc, &message, 0);
  Py_END_ALLOW_THREADS;
  if (sent >= 0) {
    return Py_BuildValue("i", sent);
  } else {
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_sendmsg() failed");
    return NULL;
  }
}

/* TODO(mseaborn): Add support for receiving descriptors. */
static PyObject *PyDescImcRecvmsg(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  int buffer_size;
  void *buffer;
  struct NaClImcMsgIoVec iov;
  struct NaClImcTypedMsgHdr message;
  ssize_t received;
  PyObject *result;

  if (!PyArg_ParseTuple(args, "i", &buffer_size))
    return NULL;
  buffer = malloc(buffer_size);
  if (buffer == NULL) {
    PyErr_SetString(PyExc_MemoryError,
                    "imc_recvmsg(): Failed to allocate buffer");
    return NULL;
  }
  iov.base = buffer;
  iov.length = buffer_size;
  message.iov = &iov;
  message.iov_length = 1;
  message.ndescv = NULL;
  message.ndesc_length = 0;
  message.flags = 0;
  Py_BEGIN_ALLOW_THREADS;
  received = NaClImcRecvTypedMessage(self_desc, &message, 0);
  Py_END_ALLOW_THREADS;
  if (received >= 0) {
    result = PyString_FromStringAndSize(buffer, received);
    free(buffer);
    return result;
  } else {
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_recvmsg() failed");
    return NULL;
  }
}

static PyMethodDef nacldesc_methods[] = {
  { "imc_connect", (PyCFunction) PyDescImcConnect, METH_NOARGS,
    "imc_connect()\n\n"
    "Make a connection to a NaCl IMC SocketAddress."
  },
  { "imc_accept", (PyCFunction) PyDescImcAccept, METH_NOARGS,
    "imc_accept()\n\n"
    "Accept a connection on a NaCl IMC BoundSocket."
  },
  { "imc_sendmsg", (PyCFunction) PyDescImcSendmsg, METH_VARARGS,
    "imc_sendmsg(data)\n\n"
    "Send a message on a NaCl IMC connected socket."
  },
  { "imc_recvmsg", (PyCFunction) PyDescImcRecvmsg, METH_VARARGS,
    "imc_recvmsg(buffer_size)\n\n"
    "Receive a message on a NaCl IMC connected socket."
  },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject nacldesc_type = {
  PyObject_HEAD_INIT(NULL)
  0,                         /* ob_size */
  "naclimc.NaClDesc",        /* tp_name */
  sizeof(struct PyDesc),     /* tp_basicsize */
  0,                         /* tp_itemsize */
  (destructor) PyDescDealloc, /* tp_dealloc */
  NULL,                      /* tp_print */
  NULL,                      /* tp_getattr */
  NULL,                      /* tp_setattr */
  NULL,                      /* tp_compare */
  NULL,                      /* tp_repr */
  NULL,                      /* tp_as_number */
  NULL,                      /* tp_as_sequence */
  NULL,                      /* tp_as_mapping */
  NULL,                      /* tp_hash  */
  NULL,                      /* tp_call */
  NULL,                      /* tp_str */
  NULL,                      /* tp_getattro */
  NULL,                      /* tp_setattro */
  NULL,                      /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,        /* tp_flags */
  "NaCl descriptor",         /* tp_doc */
  NULL,                      /* tp_traverse */
  NULL,                      /* tp_clear */
  NULL,                      /* tp_richcompare */
  0,                         /* tp_weaklistoffset */
  NULL,                      /* tp_iter */
  NULL,                      /* tp_iternext */
  nacldesc_methods,          /* tp_methods */
  NULL,                      /* tp_members */
  NULL,                      /* tp_getset */
  NULL,                      /* tp_base */
  NULL,                      /* tp_dict */
  NULL,                      /* tp_descr_get */
  NULL,                      /* tp_descr_set */
  0,                         /* tp_dictoffset */
  NULL,                      /* tp_init */
  NULL,                      /* tp_alloc */
  NULL,                      /* tp_new */
};

static PyMethodDef module_methods[] = {
  { "imc_makeboundsock", PyDescImcMakeBoundSock, METH_NOARGS,
    "imc_makeboundsock() -> (boundsock, sockaddr)\n\n"
    "Creates a new socket pair.  boundsock can be used with imc_accept(), "
    "while sockaddr can be used with imc_connect()."
  },
  { NULL, NULL, 0, NULL }
};

void initnaclimc(void) {
  if (PyType_Ready(&nacldesc_type) < 0)
    return;
  Py_InitModule3("naclimc", module_methods, "NaCl IMC");
  NaClNrdAllModulesInit();
}
