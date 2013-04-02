/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if NACL_WINDOWS
# include "io.h"
# include "fcntl.h"
/* The version of Python we pull in for Windows includes python26.lib
   but not python26_d.lib, which pyconfig.h insists on linking against
   when _DEBUG is defined.  We work around this by undefining _DEBUG. */
# undef _DEBUG
#endif

#include <Python.h>

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"


static PyTypeObject nacldesc_type;

struct PyDesc {
  PyObject_HEAD
  struct NaClDesc *nacldesc;
};


/* Internal helper functions */

/* Create a Python wrapper for a NaClDesc. */
static PyObject *NaClToPyDesc(struct NaClDesc *nacldesc) {
  struct PyDesc *pydesc = PyObject_New(struct PyDesc, &nacldesc_type);
  if (pydesc == NULL)
    return NULL;
  pydesc->nacldesc = nacldesc;
  return (PyObject *) pydesc;
}

/* Unwrap the Python wrapper to get a NaClDesc.
   This returns a *borrowed* reference. */
static struct NaClDesc *PyToNaClDesc(PyObject *obj) {
  if (PyObject_TypeCheck(obj, &nacldesc_type)) {
    struct PyDesc *pydesc = (struct PyDesc *) obj;
    return pydesc->nacldesc;
  } else {
    PyErr_SetString(PyExc_TypeError, "Argument is not a NaClDesc");
    return NULL;
  }
}


/* Module-level functions */

static PyObject *PyDescImcMakeBoundSock(PyObject *self, PyObject *args) {
  struct NaClDesc *pair[2];
  UNREFERENCED_PARAMETER(self);
  UNREFERENCED_PARAMETER(args);
  if (NaClCommonDescMakeBoundSock(pair) != 0) {
    PyErr_SetString(PyExc_Exception, "imc_makeboundsock() failed");
    return NULL;
  }
  return Py_BuildValue("NN", NaClToPyDesc(pair[0]),
                             NaClToPyDesc(pair[1]));
}

/* Python's libraries are not consistent in how or whether they wrap
   Windows handles.

   The modules _subprocess and win32api each have their own wrappers
   which automagically convert to integers, but the constructors are
   not exposed.  (_subprocess is a private, Windows-only module for
   use by subprocess.py.)

   msvcrt.get_osfhandle() just returns an integer.  This seems like
   the simplest approach, and it fits with how Python handles Unix
   file descriptors, so we just use integers below.  Windows handles
   are really just integer IDs, even if HANDLE is a typedef for
   void*. */

static PyObject *PyDescOsSocketpair(PyObject *self, PyObject *args) {
  NaClHandle pair[2];
  UNREFERENCED_PARAMETER(self);
  UNREFERENCED_PARAMETER(args);
  if (NaClSocketPair(pair) != 0) {
    PyErr_SetString(PyExc_Exception, "NaClSocketPair() failed");
    return NULL;
  }
  return Py_BuildValue("kk", (unsigned long) pair[0],
                             (unsigned long) pair[1]);
}

static PyObject *PyDescFromOsSocket(PyObject *self, PyObject *args) {
  unsigned long socket;
  struct NaClDescImcDesc *nacldesc;
  UNREFERENCED_PARAMETER(self);

  if (!PyArg_ParseTuple(args, "k", &socket))
    return NULL;
  nacldesc = malloc(sizeof(*nacldesc));
  if (nacldesc == NULL) {
    PyErr_SetString(PyExc_MemoryError, "Failed to allocate NaClDesc");
    return NULL;
  }
  if (!NaClDescImcDescCtor(nacldesc, (NaClHandle) socket)) {
    free(nacldesc);
    PyErr_SetString(PyExc_MemoryError, "Failed to allocate NaClDesc");
    return NULL;
  }
  return NaClToPyDesc(&nacldesc->base.base);
}

static PyObject *PyDescFromOsFileDescriptor(PyObject *self, PyObject *args) {
  unsigned long fd;
  struct NaClDescIoDesc *nacldesc;
  UNREFERENCED_PARAMETER(self);

  if (!PyArg_ParseTuple(args, "k", &fd))
    return NULL;
#if NACL_WINDOWS
  /* Wrap the Windows handle to get a crt-emulated file descriptor.
     The FD is specific to the instance of crt that we are linked to,
     which is the same instance used by the NaClDesc library. */
  fd = _open_osfhandle(fd, _O_RDWR | _O_BINARY);
#endif
  /* The "mode" argument is only used for a sanity check and is not
     stored, so passing 0 works. */
  nacldesc = NaClDescIoDescMake(NaClHostDescPosixMake(fd,
                                                      /* flags= */
                                                      NACL_ABI_O_RDWR));
  if (nacldesc == NULL) {
    PyErr_SetString(PyExc_MemoryError, "Failed to create NaClDesc wrapper");
    return NULL;
  }
  return NaClToPyDesc(&nacldesc->base);
}


/* Methods */

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

static PyObject *PyDescImcSendmsg(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  char *data;
  int data_size;
  Py_ssize_t desc_array_len;
  PyObject *pydesc_array;
  struct NaClDesc *nacldesc_array[NACL_ABI_IMC_DESC_MAX];
  struct NaClImcMsgIoVec iov;
  struct NaClImcTypedMsgHdr message;
  ssize_t sent;
  int i;

  if (!PyArg_ParseTuple(args, "s#O", &data, &data_size, &pydesc_array) ||
      !PyTuple_Check(pydesc_array))
    return NULL;
  desc_array_len = PyTuple_GET_SIZE(pydesc_array);
  if (desc_array_len > NACL_ABI_IMC_DESC_MAX) {
    /* Checking the size early saves the hassle of a dynamic allocation.
       TODO(mseaborn): Record errno value in exception as NACL_ABI_EINVAL. */
    PyErr_SetString(PyExc_Exception,
                    "imc_sendmsg() failed: Too many descriptor arguments");
    return NULL;
  }
  for (i = 0; i < desc_array_len; i++) {
    PyObject *obj = PyTuple_GET_ITEM(pydesc_array, i);
    /* PyToNaClDesc() returns a borrowed reference.  Because
       pydesc_array is a tuple, and therefore immutable, and each of
       its PyDesc members is also immutable, this is safe.  This saves
       us having to increment and decrement the refcounts across the
       SendMsg() call below. */
    struct NaClDesc *nacldesc = PyToNaClDesc(obj);
    if (nacldesc == NULL)
      return NULL;
    nacldesc_array[i] = nacldesc;
  }
  iov.base = data;
  iov.length = data_size;
  message.iov = &iov;
  message.iov_length = 1;
  message.ndescv = nacldesc_array;
  message.ndesc_length = (nacl_abi_size_t) desc_array_len;
  message.flags = 0;
  Py_BEGIN_ALLOW_THREADS;
  sent = NACL_VTBL(NaClDesc, self_desc)->SendMsg(self_desc, &message, 0);
  Py_END_ALLOW_THREADS;
  if (sent >= 0) {
    return Py_BuildValue("i", sent);
  } else {
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_sendmsg() failed");
    return NULL;
  }
}

static PyObject *PyDescImcRecvmsg(PyObject *self, PyObject *args) {
  struct NaClDesc *self_desc = ((struct PyDesc *) self)->nacldesc;
  int buffer_size;
  void *buffer;
  struct NaClDesc *nacldesc_array[NACL_ABI_IMC_DESC_MAX];
  struct NaClImcMsgIoVec iov;
  struct NaClImcTypedMsgHdr message;
  ssize_t received;
  unsigned int i;
  PyObject *descs_tuple;
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
  message.ndescv = nacldesc_array;
  message.ndesc_length = NACL_ABI_IMC_DESC_MAX;
  message.flags = 0;
  Py_BEGIN_ALLOW_THREADS;
  /* No quota management from these bindings. */
  received = NACL_VTBL(NaClDesc, self_desc)->RecvMsg(self_desc, &message, 0,
                                                     NULL);
  Py_END_ALLOW_THREADS;

  if (received < 0) {
    free(buffer);
    /* TODO(mseaborn): Record errno value in exception. */
    PyErr_SetString(PyExc_Exception, "imc_recvmsg() failed");
    return NULL;
  }

  descs_tuple = PyTuple_New(message.ndesc_length);
  if (descs_tuple == NULL) {
    free(buffer);
    return NULL;
  }
  for (i = 0; i < message.ndesc_length; i++) {
    PyObject *wrapper = NaClToPyDesc(message.ndescv[i]);
    if (wrapper == NULL) {
      /* On error, free the remaining received descriptors.  The
         already-processed descriptors are freed when unreffing
         descs_tuple.  Unreffing a partially-initialised tuple is safe
         because the fields are NULL-initialised. */
      unsigned int j;
      for (j = i; j < message.ndesc_length; j++) {
        NaClDescUnref(message.ndescv[j]);
      }
      free(buffer);
      Py_DECREF(descs_tuple);
      return NULL;
    }
    PyTuple_SET_ITEM(descs_tuple, i, wrapper);
  }

  result = Py_BuildValue("s#N", buffer, received, descs_tuple);
  free(buffer);
  return result;
}

static PyMethodDef nacldesc_methods[] = {
  { "imc_connect", (PyCFunction) PyDescImcConnect, METH_NOARGS,
    "imc_connect() -> socket\n\n"
    "Make a connection to a NaCl IMC SocketAddress."
  },
  { "imc_accept", (PyCFunction) PyDescImcAccept, METH_NOARGS,
    "imc_accept() -> socket\n\n"
    "Accept a connection on a NaCl IMC BoundSocket."
  },
  { "imc_sendmsg", (PyCFunction) PyDescImcSendmsg, METH_VARARGS,
    "imc_sendmsg(data, desc_tuple) -> byte_count\n\n"
    "Send a message on a NaCl IMC connected socket.  "
    "Returns the number of bytes sent."
  },
  { "imc_recvmsg", (PyCFunction) PyDescImcRecvmsg, METH_VARARGS,
    "imc_recvmsg(buffer_size) -> (data, desc_tuple)\n\n"
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
  { "os_socketpair", PyDescOsSocketpair, METH_NOARGS,
    "os_socketpair() -> (fd1, fd2)\n\n"
    "Returns a pair of host-OS sockets.  "
    "On Unix, these are file descriptors.  On Windows, these are handles.  "
    "This is for setting up a connection to a newly-spawned subprocess."
  },
  { "from_os_socket", PyDescFromOsSocket, METH_VARARGS,
    "from_os_socket(fd) -> socket\n\n"
    "Takes a host-OS socket and returns a connected socket NaClDesc.  "
    "This takes ownership of the host-OS socket.  "
    "This is for setting up a connection inside or to a "
    "newly-spawned subprocess."
  },
  { "from_os_file_descriptor", PyDescFromOsFileDescriptor, METH_VARARGS,
    "from_os_file_descriptor(fd) -> nacldesc\n\n"
    "Takes a host-OS file handle and returns a NaClDesc wrapper for it.  "
    "This takes ownership of the host-OS file handle.\n\n"
    "On Unix, this takes a file descriptor.\n\n"
    "On Windows, this takes a Windows handle, not a file descriptor.  "
    "This is because Windows file descriptors are emulated by crt (the "
    "C runtime library), and python.exe can be linked to a different crt "
    "instance from the Python extension DLL, and so the two can be using "
    "separate file descriptor tables."
  },
  { NULL, NULL, 0, NULL }
};

/* Note that although this does not return an error code, Python's
   importdl.c checks PyErr_Occurred(), so our early returns should
   raise exceptions correctly. */
void initnaclimc(void) {
  PyObject *module;

  if (PyType_Ready(&nacldesc_type) < 0)
    return;
  module = Py_InitModule3("naclimc", module_methods, "NaCl IMC");
  if (module == NULL)
    return;
  if (PyModule_AddIntConstant(module, "DESC_MAX", NACL_ABI_IMC_DESC_MAX) != 0)
    return;

  NaClNrdAllModulesInit();
}
