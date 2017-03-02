// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_ANGLE_UTIL_WIN_H_
#define UI_GL_GL_ANGLE_UTIL_WIN_H_

#include <d3d11.h>
#include <d3d9.h>
#include <dcomp.h>

#include "base/win/scoped_comptr.h"
#include "ui/gl/gl_export.h"

namespace gl {

GL_EXPORT base::win::ScopedComPtr<ID3D11Device>
QueryD3D11DeviceObjectFromANGLE();
GL_EXPORT base::win::ScopedComPtr<IDirect3DDevice9>
QueryD3D9DeviceObjectFromANGLE();

// Query the DirectComposition device associated with a D3D11 device. May
// create a new one if none exists.
GL_EXPORT base::win::ScopedComPtr<IDCompositionDevice2>
QueryDirectCompositionDevice(
    base::win::ScopedComPtr<ID3D11Device> d3d11_device);

}  // namespace gl

#endif  // UI_GL_GL_ANGLE_UTIL_WIN_H_
