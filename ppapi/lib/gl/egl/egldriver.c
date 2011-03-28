// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "third_party/mesa/MesaLib/src/egl/main/egldriver.h"

#include "third_party/mesa/MesaLib/src/egl/main/eglconfig.h"
#include "third_party/mesa/MesaLib/src/egl/main/eglcontext.h"
#include "third_party/mesa/MesaLib/src/egl/main/egldisplay.h"
#include "third_party/mesa/MesaLib/src/egl/main/eglmisc.h"
#include "third_party/mesa/MesaLib/src/egl/main/eglsurface.h"

/**
 * Functions for choosing and opening/loading device drivers.
 */

static _EGLDriver *_eglDriver;

static EGLBoolean _eglLoadDriver() {
  assert(!_eglDriver);

  _eglDriver = _eglMain(NULL);
  return _eglDriver ? EGL_TRUE : EGL_FALSE;
}

/**
 * Match a display to a driver.  The display is initialized unless use_probe is
 * true.
 */

_EGLDriver * _eglMatchDriver(_EGLDisplay *dpy, EGLBoolean use_probe) {
  _EGLDriver * drv = NULL;
  EGLint major = 0, minor = 0;

  if (!_eglDriver && !_eglLoadDriver())
    return NULL;

  if (use_probe) {
    if (_eglDriver->Probe)
      _eglDriver->Probe(_eglDriver, dpy);
    drv = _eglDriver;
  } else {
    if (_eglDriver->API.Initialize(_eglDriver, dpy, &major, &minor)) {
      drv = _eglDriver;
      dpy->Driver = drv;
      dpy->Initialized = EGL_TRUE;
      dpy->APImajor = major;
      dpy->APIminor = minor;
    }
  }

  return drv;
}

__eglMustCastToProperFunctionPointerType _eglGetDriverProc(
    const char *procname) {
  _EGLProc proc = NULL;

  return proc;
}

/**
 * Unload all drivers.
 */
void _eglUnloadDrivers(void) {
  if (_eglDriver && _eglDriver->Unload)
    _eglDriver->Unload(_eglDriver);

  _eglDriver = NULL;
}

/**
 * Plug all the available fallback routines into the given driver's
 * dispatch table.
 */
void _eglInitDriverFallbacks(_EGLDriver *drv) {
  /* If a pointer is set to NULL, then the device driver _really_ has
   * to implement it.
   */
  drv->API.Initialize = NULL;
  drv->API.Terminate = NULL;

  drv->API.GetConfigs = _eglGetConfigs;
  drv->API.ChooseConfig = _eglChooseConfig;
  drv->API.GetConfigAttrib = _eglGetConfigAttrib;

  drv->API.CreateContext = _eglCreateContext;
  drv->API.DestroyContext = _eglDestroyContext;
  drv->API.MakeCurrent = _eglMakeCurrent;
  drv->API.QueryContext = _eglQueryContext;

  drv->API.CreateWindowSurface = _eglCreateWindowSurface;
  drv->API.CreatePixmapSurface = _eglCreatePixmapSurface;
  drv->API.CreatePbufferSurface = _eglCreatePbufferSurface;
  drv->API.DestroySurface = _eglDestroySurface;
  drv->API.QuerySurface = _eglQuerySurface;
  drv->API.SurfaceAttrib = _eglSurfaceAttrib;
  drv->API.BindTexImage = _eglBindTexImage;
  drv->API.ReleaseTexImage = _eglReleaseTexImage;
  drv->API.SwapInterval = _eglSwapInterval;
  drv->API.SwapBuffers = _eglSwapBuffers;
  drv->API.CopyBuffers = _eglCopyBuffers;

  drv->API.QueryString = _eglQueryString;
  drv->API.WaitClient = _eglWaitClient;
  drv->API.WaitNative = _eglWaitNative;
}

