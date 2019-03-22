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
using pASurfaceControl_CreateSurfaceForWindow =
    ASurface* (*)(ANativeWindow* parent, const char* name);
using pASurfaceControl_CreateSurface = ASurface* (*)(ASurface* parent,
                                                     const char* name);
using pASurfaceControl_DeleteSurface = void (*)(ASurface*);

// ASurfaceTransaction
using pASurfaceControl_CreateTransaction = ASurfaceTransaction* (*)(void);
using pASurfaceControl_DeleteTransaction = void (*)(ASurfaceTransaction*);
using pASurfaceControl_TransactionApply = int64_t (*)(ASurfaceTransaction*);
using pASurfaceControl_TransactionSetCompletedFunc =
    void (*)(ASurfaceTransaction*, void* ctx, TransactionCompletedFunc);
using pASurfaceControl_TransactionSetVisibility = void (*)(ASurfaceTransaction*,
                                                           ASurface*,
                                                           bool show);
using pASurfaceControl_TransactionSetZOrder =
    void (*)(ASurfaceTransaction* transaction, ASurface*, int32_t z);
using pASurfaceControl_TransactionSetBuffer =
    void (*)(ASurfaceTransaction* transaction,
             ASurface*,
             AHardwareBuffer*,
             int32_t fence_fd);
using pASurfaceControl_TransactionSetCropRect =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
             const ARect& rect);
using pASurfaceControl_TransactionSetDisplayFrame =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
             const ARect& rect);
using pASurfaceControl_TransactionSetBufferOpaque =
    void (*)(ASurfaceTransaction* transaction, ASurface* surface, bool opaque);
using pASurfaceControl_TransactionSetDamageRegion =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
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
    void* main_dl_handle = dlopen(nullptr, RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load android so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_CreateSurfaceForWindow);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_CreateSurface);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_DeleteSurface);

    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_CreateTransaction);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_DeleteTransaction);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionApply);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetCompletedFunc);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetVisibility);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetZOrder);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetBuffer);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetCropRect);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetDisplayFrame);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetBufferOpaque);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_TransactionSetDamageRegion);
  }

  ~SurfaceControlMethods() = default;

  bool supported = true;
  // Surface methods.
  pASurfaceControl_CreateSurfaceForWindow
      ASurfaceControl_CreateSurfaceForWindowFn;
  pASurfaceControl_CreateSurface ASurfaceControl_CreateSurfaceFn;
  pASurfaceControl_DeleteSurface ASurfaceControl_DeleteSurfaceFn;

  // Transaction methods.
  pASurfaceControl_CreateTransaction ASurfaceControl_CreateTransactionFn;
  pASurfaceControl_DeleteTransaction ASurfaceControl_DeleteTransactionFn;
  pASurfaceControl_TransactionApply ASurfaceControl_TransactionApplyFn;
  pASurfaceControl_TransactionSetCompletedFunc
      ASurfaceControl_TransactionSetCompletedFuncFn;
  pASurfaceControl_TransactionSetVisibility
      ASurfaceControl_TransactionSetVisibilityFn;
  pASurfaceControl_TransactionSetZOrder ASurfaceControl_TransactionSetZOrderFn;
  pASurfaceControl_TransactionSetBuffer ASurfaceControl_TransactionSetBufferFn;
  pASurfaceControl_TransactionSetCropRect
      ASurfaceControl_TransactionSetCropRectFn;
  pASurfaceControl_TransactionSetDisplayFrame
      ASurfaceControl_TransactionSetDisplayFrameFn;
  pASurfaceControl_TransactionSetBufferOpaque
      ASurfaceControl_TransactionSetBufferOpaqueFn;
  pASurfaceControl_TransactionSetDamageRegion
      ASurfaceControl_TransactionSetDamageRegionFn;
};

ARect RectToARect(const gfx::Rect& rect) {
  return ARect{rect.x(), rect.y(), rect.right(), rect.bottom()};
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
  surface_ = SurfaceControlMethods::Get().ASurfaceControl_CreateSurfaceFn(
      parent.surface(), name);
  DCHECK(surface_);
}

SurfaceControl::Surface::Surface(ANativeWindow* parent, const char* name) {
  surface_ =
      SurfaceControlMethods::Get().ASurfaceControl_CreateSurfaceForWindowFn(
          parent, name);
  DCHECK(surface_);
}

SurfaceControl::Surface::~Surface() {
  if (surface_)
    SurfaceControlMethods::Get().ASurfaceControl_DeleteSurfaceFn(surface_);
}

SurfaceControl::Surface::Surface(Surface&& other) {
  surface_ = other.surface_;
  other.surface_ = nullptr;
}

SurfaceControl::Surface& SurfaceControl::Surface::operator=(Surface&& other) {
  if (surface_)
    SurfaceControlMethods::Get().ASurfaceControl_DeleteSurfaceFn(surface_);

  surface_ = other.surface_;
  other.surface_ = nullptr;
  return *this;
}

SurfaceControl::Transaction::Transaction() {
  transaction_ =
      SurfaceControlMethods::Get().ASurfaceControl_CreateTransactionFn();
  DCHECK(transaction_);
}

SurfaceControl::Transaction::~Transaction() {
  SurfaceControlMethods::Get().ASurfaceControl_DeleteTransactionFn(
      transaction_);
}

void SurfaceControl::Transaction::SetVisibility(const Surface& surface,
                                                bool show) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetVisibilityFn(
      transaction_, surface.surface(), show);
}

void SurfaceControl::Transaction::SetZOrder(const Surface& surface, int32_t z) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetZOrderFn(
      transaction_, surface.surface(), z);
}

void SurfaceControl::Transaction::SetBuffer(const Surface& surface,
                                            AHardwareBuffer* buffer,
                                            base::ScopedFD fence_fd) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetBufferFn(
      transaction_, surface.surface(), buffer,
      fence_fd.is_valid() ? fence_fd.release() : -1);
}

void SurfaceControl::Transaction::SetCropRect(const Surface& surface,
                                              const gfx::Rect& rect) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetCropRectFn(
      transaction_, surface.surface(), RectToARect(rect));
}

void SurfaceControl::Transaction::SetDisplayFrame(const Surface& surface,
                                                  const gfx::Rect& rect) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetDisplayFrameFn(
      transaction_, surface.surface(), RectToARect(rect));
}

void SurfaceControl::Transaction::SetOpaque(const Surface& surface,
                                            bool opaque) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetBufferOpaqueFn(
      transaction_, surface.surface(), opaque);
}

void SurfaceControl::Transaction::SetDamageRect(const Surface& surface,
                                                const gfx::Rect& rect) {
  auto a_rect = RectToARect(rect);
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetDamageRegionFn(
      transaction_, surface.surface(), &a_rect, 1u);
}

void SurfaceControl::Transaction::SetCompletedFunc(
    TransactionCompletedFunc func,
    void* ctx) {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionSetCompletedFuncFn(
      transaction_, ctx, func);
}

void SurfaceControl::Transaction::Apply() {
  SurfaceControlMethods::Get().ASurfaceControl_TransactionApplyFn(transaction_);
}

}  // namespace gl
