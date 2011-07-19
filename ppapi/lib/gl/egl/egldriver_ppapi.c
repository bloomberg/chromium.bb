// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include "third_party/mesa/MesaLib/src/egl/main/egldriver.h"

/**
 * This is an EGL driver that wraps PPAPI.
 */

/** Subclass of _EGLDriver */
typedef struct _egl_driver_ppapi {
  _EGLDriver Base;  /* base class */
} _EGLDriverPPAPI;

static EGLBoolean _eglInitializePPAPI(_EGLDriver *drv,
                                      _EGLDisplay *dpy,
                                      EGLint *major,
                                      EGLint *minor) {
  return EGL_FALSE;
}

static EGLBoolean _eglTerminatePPAPI(_EGLDriver *drv,
                                     _EGLDisplay *dpy) {
  return EGL_FALSE;
}

static _EGLContext *_eglCreateContextPPAPI(_EGLDriver *drv,
                                           _EGLDisplay *dpy,
                                           _EGLConfig *config,
                                           _EGLContext *share,
                                           const EGLint *attrib_list) {
  return NULL;
}

static EGLBoolean _eglDestroyContextPPAPI(_EGLDriver *drv,
                                          _EGLDisplay *dpy,
                                          _EGLContext *ctx) {
  return EGL_FALSE;
}

static _EGLSurface *_eglCreateWindowSurfacePPAPI(_EGLDriver *drv,
                                                 _EGLDisplay *dpy,
                                                 _EGLConfig *config,
                                                 EGLNativeWindowType window,
                                                 const EGLint *attrib_list) {
  return NULL;
}

static _EGLSurface *_eglCreatePbufferSurfacePPAPI(_EGLDriver *drv,
                                                  _EGLDisplay *dpy,
                                                  _EGLConfig *config,
                                                  const EGLint *attrib_list) {
  return NULL;
}

static EGLBoolean _eglDestroySurfacePPAPI(_EGLDriver *drv,
                                          _EGLDisplay *dpy,
                                          _EGLSurface *surface) {
  return EGL_FALSE;
}

static EGLBoolean _eglSwapBuffersPPAPI(_EGLDriver *drv,
                                       _EGLDisplay *dpy,
                                       _EGLSurface *draw) {
  return EGL_FALSE;
}

static void _unloadPPAPI(_EGLDriver *drv) {
  free(drv);
}

/**
 * This is the main entrypoint into the driver, called by egl dispatch API.
 * Create a new _EGLDriver object and init its dispatch table.
 */
_EGLDriver* _eglMain(const char *args)
{
   _EGLDriverPPAPI *drv = (_EGLDriverPPAPI *)
       calloc(1, sizeof(_EGLDriverPPAPI));

   if (!drv)
      return NULL;

   _eglInitDriverFallbacks(&drv->Base);
   drv->Base.API.Initialize = _eglInitializePPAPI;
   drv->Base.API.Terminate = _eglTerminatePPAPI;
   drv->Base.API.CreateContext = _eglCreateContextPPAPI;
   drv->Base.API.DestroyContext = _eglDestroyContextPPAPI;
   drv->Base.API.CreateWindowSurface = _eglCreateWindowSurfacePPAPI;
   drv->Base.API.CreatePbufferSurface = _eglCreatePbufferSurfacePPAPI;
   drv->Base.API.DestroySurface = _eglDestroySurfacePPAPI;
   drv->Base.API.SwapBuffers = _eglSwapBuffersPPAPI;

   drv->Base.Name = "PPAPI";
   drv->Base.Unload = _unloadPPAPI;

   return &drv->Base;
}

