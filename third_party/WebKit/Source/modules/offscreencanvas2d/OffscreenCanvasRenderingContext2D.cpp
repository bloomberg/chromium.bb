// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/Settings.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() {}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    ScriptState* script_state,
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(nullptr, canvas, attrs) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsDocument()) {
    if (ToDocument(execution_context)
            ->GetSettings()
            ->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }

  WorkerSettings* worker_settings =
      ToWorkerGlobalScope(execution_context)->GetWorkerSettings();
  if (worker_settings && worker_settings->DisableReadingFromCanvas())
    canvas->SetDisableReadingFromCanvasTrue();
}

DEFINE_TRACE(OffscreenCanvasRenderingContext2D) {
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
}

ScriptPromise OffscreenCanvasRenderingContext2D::commit(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  UseCounter::Feature feature = UseCounter::kOffscreenCanvasCommit2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  if (!offscreenCanvas()->HasPlaceholderCanvas()) {
    // If an OffscreenCanvas has no associated canvas Id, it indicates that
    // it is not an OffscreenCanvas created by transfering control from html
    // canvas.
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Commit() was called on a context whose "
        "OffscreenCanvas is not associated with a "
        "canvas element.");
    return exception_state.Reject(script_state);
  }

  bool is_web_gl_software_rendering = false;
  return offscreenCanvas()->Commit(TransferToStaticBitmapImage(),
                                   is_web_gl_software_rendering, script_state);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::OriginClean() const {
  return offscreenCanvas()->OriginClean();
}

void OffscreenCanvasRenderingContext2D::SetOriginTainted() {
  return offscreenCanvas()->SetOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::WouldTaintOrigin(
    CanvasImageSource* source,
    ExecutionContext* execution_context) {
  if (execution_context->IsWorkerGlobalScope()) {
    // We only support passing in ImageBitmap and OffscreenCanvases as
    // source images in drawImage() or createPattern() in a
    // OffscreenCanvas2d in worker.
    DCHECK(source->IsImageBitmap() || source->IsOffscreenCanvas());
  }

  return CanvasRenderingContext::WouldTaintOrigin(
      source, execution_context->GetSecurityOrigin());
}

int OffscreenCanvasRenderingContext2D::Width() const {
  return offscreenCanvas()->width();
}

int OffscreenCanvasRenderingContext2D::Height() const {
  return offscreenCanvas()->height();
}

bool OffscreenCanvasRenderingContext2D::HasImageBuffer() const {
  return !!image_buffer_;
}

void OffscreenCanvasRenderingContext2D::Reset() {
  image_buffer_ = nullptr;
  BaseRenderingContext2D::Reset();
}

ColorBehavior OffscreenCanvasRenderingContext2D::DrawImageColorBehavior()
    const {
  return CanvasRenderingContext::ColorBehaviorForMediaDrawnToCanvas();
}

ImageBuffer* OffscreenCanvasRenderingContext2D::GetImageBuffer() const {
  if (!image_buffer_) {
    IntSize surface_size(Width(), Height());
    OpacityMode opacity_mode = HasAlpha() ? kNonOpaque : kOpaque;
    std::unique_ptr<ImageBufferSurface> surface;
    if (RuntimeEnabledFeatures::accelerated2dCanvasEnabled()) {
      surface.reset(
          new AcceleratedImageBufferSurface(surface_size, opacity_mode));
    }

    if (!surface || !surface->IsValid()) {
      surface.reset(new UnacceleratedImageBufferSurface(
          surface_size, opacity_mode, kInitializeImagePixels));
    }

    OffscreenCanvasRenderingContext2D* non_const_this =
        const_cast<OffscreenCanvasRenderingContext2D*>(this);
    non_const_this->image_buffer_ = ImageBuffer::Create(std::move(surface));

    if (needs_matrix_clip_restore_) {
      RestoreMatrixClipStack(image_buffer_->Canvas());
      non_const_this->needs_matrix_clip_restore_ = false;
    }
  }

  return image_buffer_.get();
}

RefPtr<StaticBitmapImage>
OffscreenCanvasRenderingContext2D::TransferToStaticBitmapImage() {
  if (!GetImageBuffer())
    return nullptr;
  sk_sp<SkImage> sk_image = image_buffer_->NewSkImageSnapshot(
      kPreferAcceleration, kSnapshotReasonTransferToImageBitmap);
  RefPtr<StaticBitmapImage> image =
      StaticBitmapImage::Create(std::move(sk_image));
  image->SetOriginClean(this->OriginClean());
  return image;
}

ImageBitmap* OffscreenCanvasRenderingContext2D::TransferToImageBitmap(
    ScriptState* script_state) {
  UseCounter::Feature feature =
      UseCounter::kOffscreenCanvasTransferToImageBitmap2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  RefPtr<StaticBitmapImage> image = TransferToStaticBitmapImage();
  if (!image)
    return nullptr;
  image_buffer_.reset();  // "Transfer" means no retained buffer
  needs_matrix_clip_restore_ = true;
  return ImageBitmap::Create(std::move(image));
}

PassRefPtr<Image> OffscreenCanvasRenderingContext2D::GetImage(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (!GetImageBuffer())
    return nullptr;
  sk_sp<SkImage> sk_image = image_buffer_->NewSkImageSnapshot(hint, reason);
  RefPtr<StaticBitmapImage> image =
      StaticBitmapImage::Create(std::move(sk_image));
  return image;
}

ImageData* OffscreenCanvasRenderingContext2D::ToImageData(
    SnapshotReason reason) {
  if (!GetImageBuffer())
    return nullptr;
  sk_sp<SkImage> snapshot =
      image_buffer_->NewSkImageSnapshot(kPreferNoAcceleration, reason);
  ImageData* image_data = nullptr;
  if (snapshot) {
    image_data = ImageData::Create(offscreenCanvas()->size());
    SkImageInfo image_info =
        SkImageInfo::Make(this->Width(), this->Height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    snapshot->readPixels(image_info, image_data->data()->Data(),
                         image_info.minRowBytes(), 0, 0);
  }
  return image_data;
}

void OffscreenCanvasRenderingContext2D::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.setOffscreenCanvasRenderingContext2D(this);
}

bool OffscreenCanvasRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  return ::blink::ParseColorOrCurrentColor(color, color_string, nullptr);
}

PaintCanvas* OffscreenCanvasRenderingContext2D::DrawingCanvas() const {
  ImageBuffer* buffer = GetImageBuffer();
  if (!buffer)
    return nullptr;
  return GetImageBuffer()->Canvas();
}

PaintCanvas* OffscreenCanvasRenderingContext2D::ExistingDrawingCanvas() const {
  if (!image_buffer_)
    return nullptr;
  return image_buffer_->Canvas();
}

void OffscreenCanvasRenderingContext2D::DisableDeferral(DisableDeferralReason) {
}

AffineTransform OffscreenCanvasRenderingContext2D::BaseTransform() const {
  if (!image_buffer_)
    return AffineTransform();  // identity
  return image_buffer_->BaseTransform();
}

void OffscreenCanvasRenderingContext2D::DidDraw(const SkIRect& dirty_rect) {}

bool OffscreenCanvasRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(offscreenCanvas()->size());
}

sk_sp<SkImageFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(offscreenCanvas()->size());
}

void OffscreenCanvasRenderingContext2D::ValidateStateStack() const {
#if DCHECK_IS_ON()
  if (PaintCanvas* sk_canvas = ExistingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(sk_canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const {
  return false;
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return this->GetImageBuffer();
}

bool OffscreenCanvasRenderingContext2D::IsAccelerated() const {
  return image_buffer_ && image_buffer_->IsAccelerated();
}
}
