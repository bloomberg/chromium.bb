// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
#define UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_

#include <memory>

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include "base/files/scoped_file.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_export.h"

extern "C" {
typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;
typedef void (*ASurfaceTransaction_OnComplete)(void* context,
                                               int32_t present_fence);
}

namespace gl {

class GL_EXPORT SurfaceControl {
 public:
  static bool IsSupported();

  class GL_EXPORT Surface {
   public:
    Surface();
    Surface(const Surface& parent, const char* name);
    Surface(ANativeWindow* parent, const char* name);
    ~Surface();

    Surface(Surface&& other);
    Surface& operator=(Surface&& other);

    ASurfaceControl* surface() const { return surface_; }

   private:
    ASurfaceControl* surface_ = nullptr;
  };

  class GL_EXPORT Transaction {
   public:
    Transaction();
    ~Transaction();

    void SetVisibility(const Surface& surface, bool show);
    void SetZOrder(const Surface& surface, int32_t z);
    void SetBuffer(const Surface& surface,
                   AHardwareBuffer* buffer,
                   base::ScopedFD fence_fd);
    void SetGeometry(const Surface& surface,
                     const gfx::Rect& src,
                     const gfx::Rect& dst,
                     gfx::OverlayTransform transform);
    void SetOpaque(const Surface& surface, bool opaque);
    void SetDamageRect(const Surface& surface, const gfx::Rect& rect);
    void SetOnCompleteFunc(ASurfaceTransaction_OnComplete func, void* ctx);
    void Apply();

   private:
    ASurfaceTransaction* transaction_;
  };
};
};  // namespace gl

#endif  // UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
