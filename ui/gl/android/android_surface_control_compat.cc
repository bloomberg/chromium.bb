// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/android_surface_control_compat.h"

#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

extern "C" {
// ASurface
using pASurfaceControl_createFromWindow =
    ASurfaceControl* (*)(ANativeWindow* parent, const char* name);
using pASurfaceControl_create = ASurfaceControl* (*)(ASurfaceControl* parent,
                                                     const char* name);
using pASurfaceControl_destroy = void (*)(ASurfaceControl*);

// ASurfaceTransaction enums
enum {
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT = 0,
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT = 1,
  ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE = 2,
};

enum {
  ASURFACE_TRANSACTION_VISIBILITY_HIDE = 0,
  ASURFACE_TRANSACTION_VISIBILITY_SHOW = 1,
};

// ASurfaceTransaction
using pASurfaceTransaction_create = ASurfaceTransaction* (*)(void);
using pASurfaceTransaction_delete = void (*)(ASurfaceTransaction*);
using pASurfaceTransaction_apply = int64_t (*)(ASurfaceTransaction*);
using pASurfaceTransaction_setOnComplete =
    void (*)(ASurfaceTransaction*, void* ctx, ASurfaceTransaction_OnComplete);
using pASurfaceTransaction_setVisibility = void (*)(ASurfaceTransaction*,
                                                    ASurfaceControl*,
                                                    int8_t visibility);
using pASurfaceTransaction_setZOrder =
    void (*)(ASurfaceTransaction* transaction, ASurfaceControl*, int32_t z);
using pASurfaceTransaction_setBuffer =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl*,
             AHardwareBuffer*,
             int32_t fence_fd);
using pASurfaceTransaction_setGeometry =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             const ARect& src,
             const ARect& dst,
             int32_t transform);
using pASurfaceTransaction_setBufferTransparency =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             int8_t transparency);
using pASurfaceTransaction_setDamageRegion =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             const ARect rects[],
             uint32_t count);
}

namespace gl {
namespace {

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;     \
    }                                                        \
  } while (0)

struct SurfaceControlMethods {
 public:
  static const SurfaceControlMethods& Get() {
    static const base::NoDestructor<SurfaceControlMethods> instance;
    return *instance;
  }

  SurfaceControlMethods() {
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load android so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_createFromWindow);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_destroy);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_delete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_apply);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setOnComplete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setVisibility);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setZOrder);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBuffer);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setGeometry);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferTransparency);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setDamageRegion);
  }

  ~SurfaceControlMethods() = default;

  bool supported = true;
  // Surface methods.
  pASurfaceControl_createFromWindow ASurfaceControl_createFromWindowFn;
  pASurfaceControl_create ASurfaceControl_createFn;
  pASurfaceControl_destroy ASurfaceControl_destroyFn;

  // Transaction methods.
  pASurfaceTransaction_create ASurfaceTransaction_createFn;
  pASurfaceTransaction_delete ASurfaceTransaction_deleteFn;
  pASurfaceTransaction_apply ASurfaceTransaction_applyFn;
  pASurfaceTransaction_setOnComplete ASurfaceTransaction_setOnCompleteFn;
  pASurfaceTransaction_setVisibility ASurfaceTransaction_setVisibilityFn;
  pASurfaceTransaction_setZOrder ASurfaceTransaction_setZOrderFn;
  pASurfaceTransaction_setBuffer ASurfaceTransaction_setBufferFn;
  pASurfaceTransaction_setGeometry ASurfaceTransaction_setGeometryFn;
  pASurfaceTransaction_setBufferTransparency
      ASurfaceTransaction_setBufferTransparencyFn;
  pASurfaceTransaction_setDamageRegion ASurfaceTransaction_setDamageRegionFn;
};

ARect RectToARect(const gfx::Rect& rect) {
  return ARect{rect.x(), rect.y(), rect.right(), rect.bottom()};
}

int32_t OverlayTransformToWindowTransform(gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_INVALID:
      DCHECK(false) << "Invalid Transform";
      return ANATIVEWINDOW_TRANSFORM_IDENTITY;
    case gfx::OVERLAY_TRANSFORM_NONE:
      return ANATIVEWINDOW_TRANSFORM_IDENTITY;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_90;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_270;
  };
  NOTREACHED();
  return ANATIVEWINDOW_TRANSFORM_IDENTITY;
}
};

// static
bool SurfaceControl::IsSupported() {
#if 0
  // TODO(khushalsagar): Enable this code when the frame is correctly reporting
  // SDK version for P+.
  const int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk_int < 29) {
    LOG(ERROR) << "SurfaceControl not supported on sdk: " << sdk_int;
    return false;
  }
#endif
  return SurfaceControlMethods::Get().supported;
}

SurfaceControl::Surface::Surface() = default;

SurfaceControl::Surface::Surface(const Surface& parent, const char* name) {
  surface_ = SurfaceControlMethods::Get().ASurfaceControl_createFn(
      parent.surface(), name);
  DCHECK(surface_);
}

SurfaceControl::Surface::Surface(ANativeWindow* parent, const char* name) {
  surface_ = SurfaceControlMethods::Get().ASurfaceControl_createFromWindowFn(
      parent, name);
  DCHECK(surface_);
}

SurfaceControl::Surface::~Surface() {
  if (surface_)
    SurfaceControlMethods::Get().ASurfaceControl_destroyFn(surface_);
}

SurfaceControl::Surface::Surface(Surface&& other) {
  surface_ = other.surface_;
  other.surface_ = nullptr;
}

SurfaceControl::Surface& SurfaceControl::Surface::operator=(Surface&& other) {
  if (surface_)
    SurfaceControlMethods::Get().ASurfaceControl_destroyFn(surface_);

  surface_ = other.surface_;
  other.surface_ = nullptr;
  return *this;
}

SurfaceControl::Transaction::Transaction() {
  transaction_ = SurfaceControlMethods::Get().ASurfaceTransaction_createFn();
  DCHECK(transaction_);
}

SurfaceControl::Transaction::~Transaction() {
  SurfaceControlMethods::Get().ASurfaceTransaction_deleteFn(transaction_);
}

void SurfaceControl::Transaction::SetVisibility(const Surface& surface,
                                                bool show) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setVisibilityFn(
      transaction_, surface.surface(), show);
}

void SurfaceControl::Transaction::SetZOrder(const Surface& surface, int32_t z) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setZOrderFn(
      transaction_, surface.surface(), z);
}

void SurfaceControl::Transaction::SetBuffer(const Surface& surface,
                                            AHardwareBuffer* buffer,
                                            base::ScopedFD fence_fd) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferFn(
      transaction_, surface.surface(), buffer,
      fence_fd.is_valid() ? fence_fd.release() : -1);
}

void SurfaceControl::Transaction::SetGeometry(const Surface& surface,
                                              const gfx::Rect& src,
                                              const gfx::Rect& dst,
                                              gfx::OverlayTransform transform) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setGeometryFn(
      transaction_, surface.surface(), RectToARect(src), RectToARect(dst),
      OverlayTransformToWindowTransform(transform));
}

void SurfaceControl::Transaction::SetOpaque(const Surface& surface,
                                            bool opaque) {
  int8_t transparency = opaque ? ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE
                               : ASURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT;
  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferTransparencyFn(
      transaction_, surface.surface(), transparency);
}

void SurfaceControl::Transaction::SetDamageRect(const Surface& surface,
                                                const gfx::Rect& rect) {
  auto a_rect = RectToARect(rect);
  SurfaceControlMethods::Get().ASurfaceTransaction_setDamageRegionFn(
      transaction_, surface.surface(), &a_rect, 1u);
}

void SurfaceControl::Transaction::SetOnCompleteFunc(
    ASurfaceTransaction_OnComplete func,
    void* ctx) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setOnCompleteFn(transaction_,
                                                                   ctx, func);
}

void SurfaceControl::Transaction::Apply() {
  SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction_);
}

}  // namespace gl
