// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_SURFACE_OZONE_BASE_H_
#define UI_GFX_OZONE_SURFACE_OZONE_BASE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/ozone/surface_ozone.h"

namespace gfx {

// Base class for in-tree implementations of SurfaceOzone.
//
// This adds default implementations of all the methods in SurfaceOzone,
// for use by in-tree subclasses.
//
// If an out-of-tree implementation inherits this, it is possible there
// will be silent breakage instead of a compile time failure as a result
// of changes to the interface.
class GFX_EXPORT SurfaceOzoneBase : public SurfaceOzone {
 public:
  SurfaceOzoneBase();
  virtual ~SurfaceOzoneBase() OVERRIDE;

  // SurfaceOzone:
  virtual bool InitializeEGL() OVERRIDE;
  virtual intptr_t /* EGLNativeWindowType */ GetEGLNativeWindow() OVERRIDE;
  virtual bool InitializeCanvas() OVERRIDE;
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE;
  virtual bool ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool PresentCanvas() OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_SURFACE_OZONE_BASE_H_
