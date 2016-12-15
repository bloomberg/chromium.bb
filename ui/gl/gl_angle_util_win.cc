// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_angle_util_win.h"

#include "base/trace_event/trace_event.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

void* QueryDeviceObjectFromANGLE(int object_type) {
  EGLDisplay egl_display = nullptr;
  intptr_t egl_device = 0;
  intptr_t device = 0;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. GetHardwareDisplay");
    egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  }

  if (!gl::GLSurfaceEGL::HasEGLExtension("EGL_EXT_device_query"))
    return nullptr;

  PFNEGLQUERYDISPLAYATTRIBEXTPROC QueryDisplayAttribEXT = nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. eglGetProcAddress");

    QueryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDisplayAttribEXT"));

    if (!QueryDisplayAttribEXT)
      return nullptr;
  }

  PFNEGLQUERYDEVICEATTRIBEXTPROC QueryDeviceAttribEXT = nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. eglGetProcAddress");

    QueryDeviceAttribEXT = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDeviceAttribEXT"));
    if (!QueryDeviceAttribEXT)
      return nullptr;
  }

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. QueryDisplayAttribEXT");

    if (!QueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &egl_device))
      return nullptr;
  }
  if (!egl_device)
    return nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. QueryDisplayAttribEXT");

    if (!QueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(egl_device),
                              object_type, &device)) {
      return nullptr;
    }
  }

  return reinterpret_cast<void*>(device);
}

base::win::ScopedComPtr<ID3D11Device> QueryD3D11DeviceObjectFromANGLE() {
  base::win::ScopedComPtr<ID3D11Device> d3d11_device;
  d3d11_device = reinterpret_cast<ID3D11Device*>(
      QueryDeviceObjectFromANGLE(EGL_D3D11_DEVICE_ANGLE));
  return d3d11_device;
}

base::win::ScopedComPtr<IDirect3DDevice9> QueryD3D9DeviceObjectFromANGLE() {
  base::win::ScopedComPtr<IDirect3DDevice9> d3d9_device;
  d3d9_device = reinterpret_cast<IDirect3DDevice9*>(
      QueryDeviceObjectFromANGLE(EGL_D3D9_DEVICE_ANGLE));
  return d3d9_device;
}

}  // namespace gl
