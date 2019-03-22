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
#include "ui/gl/gl_export.h"

extern "C" {
typedef struct ASurface ASurface;
typedef struct ASurfaceTransaction ASurfaceTransaction;
typedef void (*TransactionCompletedFunc)(void* context,
                                         int64_t present_time_ns);
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

    ASurface* surface() const { return surface_; }

   private:
    ASurface* surface_ = nullptr;
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
    void SetCropRect(const Surface& surface, const gfx::Rect& rect);
    void SetDisplayFrame(const Surface& surface, const gfx::Rect& rect);
    void SetOpaque(const Surface& surface, bool opaque);
    void SetDamageRect(const Surface& surface, const gfx::Rect& rect);
    void SetCompletedFunc(TransactionCompletedFunc func, void* ctx);
    void Apply();

   private:
    ASurfaceTransaction* transaction_;
  };
};
};  // namespace gl

#endif  // UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
