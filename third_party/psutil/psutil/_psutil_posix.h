/*
 * $Id: _psutil_posix.h 1142 2011-10-05 18:45:49Z g.rodola $
 *
 * Copyright (c) 2009, Jay Loden, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * POSIX specific module methods for _psutil_posix
 */

#include <Python.h>

static PyObject* posix_getpriority(PyObject* self, PyObject* args);
static PyObject* posix_setpriority(PyObject* self, PyObject* args);

