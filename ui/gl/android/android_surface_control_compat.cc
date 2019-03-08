// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/android_surface_control_compat.h"

#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "ui/gfx/color_space.h"

extern "C" {
typedef struct ASurfaceTransactionStats ASurfaceTransactionStats;
typedef void (*ASurfaceTransaction_OnComplete)(void* context,
                                               ASurfaceTransactionStats* stats);

// ASurface
using pASurfaceControl_createFromWindow =
    ASurfaceControl* (*)(ANativeWindow* parent, const char* name);
using pASurfaceControl_create = ASurfaceControl* (*)(ASurfaceControl* parent,
                                                     const char* name);
using pASurfaceControl_release = void (*)(ASurfaceControl*);

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

enum {
  ADATASPACE_UNKNOWN = 0,
  ADATASPACE_SCRGB_LINEAR = 406913024,
  ADATASPACE_SRGB = 142671872,
  ADATASPACE_DISPLAY_P3 = 143261696,
  ADATASPACE_BT2020_PQ = 163971072,
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
using pASurfaceTransaction_setBufferDataSpace =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             uint64_t data_space);

// ASurfaceTransactionStats
using pASurfaceTransactionStats_getPresentFenceFd =
    int (*)(ASurfaceTransactionStats* stats);
using pASurfaceTransactionStats_getASurfaceControls =
    void (*)(ASurfaceTransactionStats* stats,
             ASurfaceControl*** surface_controls,
             size_t* size);
using pASurfaceTransactionStats_releaseASurfaceControls =
    void (*)(ASurfaceControl** surface_controls);
using pASurfaceTransactionStats_getPreviousReleaseFenceFd =
    int (*)(ASurfaceTransactionStats* stats, ASurfaceControl* surface_control);
}

namespace gl {
namespace {

// Helper function to log errors from dlsym. Calling LOG(ERROR) inside a macro
// crashes clang code coverage. https://crbug.com/843356
void LogDlsymError(const char* func) {
  LOG(ERROR) << "Unable to load function " << func;
}

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LogDlsymError(#func);                                  \
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
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_release);

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
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferDataSpace);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getPresentFenceFd);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_releaseASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_getPreviousReleaseFenceFd);
  }

  ~SurfaceControlMethods() = default;

  bool supported = true;
  // Surface methods.
  pASurfaceControl_createFromWindow ASurfaceControl_createFromWindowFn;
  pASurfaceControl_create ASurfaceControl_createFn;
  pASurfaceControl_release ASurfaceControl_releaseFn;

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
  pASurfaceTransaction_setBufferDataSpace
      ASurfaceTransaction_setBufferDataSpaceFn;

  // TransactionStats methods.
  pASurfaceTransactionStats_getPresentFenceFd
      ASurfaceTransactionStats_getPresentFenceFdFn;
  pASurfaceTransactionStats_getASurfaceControls
      ASurfaceTransactionStats_getASurfaceControlsFn;
  pASurfaceTransactionStats_releaseASurfaceControls
      ASurfaceTransactionStats_releaseASurfaceControlsFn;
  pASurfaceTransactionStats_getPreviousReleaseFenceFd
      ASurfaceTransactionStats_getPreviousReleaseFenceFdFn;
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

uint64_t ColorSpaceToADataSpace(const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid() || color_space == gfx::ColorSpace::CreateSRGB())
    return ADATASPACE_SRGB;

  if (color_space == gfx::ColorSpace::CreateSCRGBLinear())
    return ADATASPACE_SCRGB_LINEAR;

  if (color_space == gfx::ColorSpace::CreateDisplayP3D65())
    return ADATASPACE_DISPLAY_P3;

  // TODO(khushalsagar): Check if we can support BT2020 using
  // ADATASPACE_BT2020_PQ.
  return ADATASPACE_UNKNOWN;
}

SurfaceControl::TransactionStats ToTransactionStats(
    ASurfaceTransactionStats* stats) {
  SurfaceControl::TransactionStats transaction_stats;
  transaction_stats.present_fence = base::ScopedFD(
      SurfaceControlMethods::Get().ASurfaceTransactionStats_getPresentFenceFdFn(
          stats));

  ASurfaceControl** surface_controls = nullptr;
  size_t size = 0u;
  SurfaceControlMethods::Get().ASurfaceTransactionStats_getASurfaceControlsFn(
      stats, &surface_controls, &size);
  transaction_stats.surface_stats.resize(size);
  for (size_t i = 0u; i < size; ++i) {
    transaction_stats.surface_stats[i].surface = surface_controls[i];
    int fence_fd = SurfaceControlMethods::Get()
                       .ASurfaceTransactionStats_getPreviousReleaseFenceFdFn(
                           stats, surface_controls[i]);
    if (fence_fd != -1) {
      transaction_stats.surface_stats[i].fence = base::ScopedFD(fence_fd);
    }
  }
  SurfaceControlMethods::Get()
      .ASurfaceTransactionStats_releaseASurfaceControlsFn(surface_controls);

  return transaction_stats;
}

struct TransactionAckCtx {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  SurfaceControl::Transaction::OnCompleteCb callback;
};

// Note that the framework API states that this callback can be dispatched on
// any thread (in practice it should be the binder thread).
void OnTransactionCompletedOnAnyThread(void* context,
                                       ASurfaceTransactionStats* stats) {
  auto* ack_ctx = static_cast<TransactionAckCtx*>(context);
  auto transaction_stats = ToTransactionStats(stats);

  if (ack_ctx->task_runner) {
    ack_ctx->task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(ack_ctx->callback),
                                  std::move(transaction_stats)));
  } else {
    std::move(ack_ctx->callback).Run(std::move(transaction_stats));
  }

  delete ack_ctx;
}
}  // namespace

// static
bool SurfaceControl::IsSupported() {
  if (!base::android::BuildInfo::GetInstance()->is_at_least_q()) {
    LOG(ERROR) << "SurfaceControl requires at least Q";
    return false;
  }
  return SurfaceControlMethods::Get().supported;
}

bool SurfaceControl::SupportsColorSpace(const gfx::ColorSpace& color_space) {
  return ColorSpaceToADataSpace(color_space) != ADATASPACE_UNKNOWN;
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
    SurfaceControlMethods::Get().ASurfaceControl_releaseFn(surface_);
}

SurfaceControl::SurfaceStats::SurfaceStats() = default;
SurfaceControl::SurfaceStats::~SurfaceStats() = default;

SurfaceControl::SurfaceStats::SurfaceStats(SurfaceStats&& other) = default;
SurfaceControl::SurfaceStats& SurfaceControl::SurfaceStats::operator=(
    SurfaceStats&& other) = default;

SurfaceControl::TransactionStats::TransactionStats() = default;
SurfaceControl::TransactionStats::~TransactionStats() = default;

SurfaceControl::TransactionStats::TransactionStats(TransactionStats&& other) =
    default;
SurfaceControl::TransactionStats& SurfaceControl::TransactionStats::operator=(
    TransactionStats&& other) = default;

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

void SurfaceControl::Transaction::SetColorSpace(
    const Surface& surface,
    const gfx::ColorSpace& color_space) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferDataSpaceFn(
      transaction_, surface.surface(), ColorSpaceToADataSpace(color_space));
}

void SurfaceControl::Transaction::SetOnCompleteCb(
    OnCompleteCb cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TransactionAckCtx* ack_ctx = new TransactionAckCtx;
  ack_ctx->callback = std::move(cb);
  ack_ctx->task_runner = std::move(task_runner);

  SurfaceControlMethods::Get().ASurfaceTransaction_setOnCompleteFn(
      transaction_, ack_ctx, &OnTransactionCompletedOnAnyThread);
}

void SurfaceControl::Transaction::Apply() {
  SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction_);
}

}  // namespace gl
