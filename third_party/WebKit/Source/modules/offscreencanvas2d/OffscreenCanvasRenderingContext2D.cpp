// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/OffscreenRenderingContext.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() {}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(canvas, attrs) {
  ExecutionContext* execution_context = canvas->GetTopExecutionContext();
  if (execution_context->IsDocument()) {
    if (ToDocument(execution_context)
            ->GetSettings()
            ->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }
  dirty_rect_for_commit_.setEmpty();
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
  WebFeature feature = WebFeature::kOffscreenCanvasCommit2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  bool is_web_gl_software_rendering = false;
  SkIRect damage_rect(dirty_rect_for_commit_);
  dirty_rect_for_commit_.setEmpty();
  return host()->Commit(TransferToStaticBitmapImage(), damage_rect,
                        is_web_gl_software_rendering, script_state,
                        exception_state);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::OriginClean() const {
  return host()->OriginClean();
}

void OffscreenCanvasRenderingContext2D::SetOriginTainted() {
  return host()->SetOriginTainted();
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
  return host()->Size().Width();
}

int OffscreenCanvasRenderingContext2D::Height() const {
  return host()->Size().Height();
}

bool OffscreenCanvasRenderingContext2D::HasImageBuffer() const {
  return host()->GetImageBuffer();
}

void OffscreenCanvasRenderingContext2D::Reset() {
  host()->DiscardImageBuffer();
  BaseRenderingContext2D::Reset();
}

ImageBuffer* OffscreenCanvasRenderingContext2D::GetImageBuffer() const {
  return const_cast<CanvasRenderingContextHost*>(host())
      ->GetOrCreateImageBuffer();
}

RefPtr<StaticBitmapImage>
OffscreenCanvasRenderingContext2D::TransferToStaticBitmapImage() {
  if (!GetImageBuffer())
    return nullptr;
  RefPtr<StaticBitmapImage> image = GetImageBuffer()->NewImageSnapshot(
      kPreferAcceleration, kSnapshotReasonTransferToImageBitmap);

  image->SetOriginClean(this->OriginClean());
  return image;
}

ImageBitmap* OffscreenCanvasRenderingContext2D::TransferToImageBitmap(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmap2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  RefPtr<StaticBitmapImage> image = TransferToStaticBitmapImage();
  if (!image)
    return nullptr;
  if (image->IsTextureBacked()) {
    // Before discarding the ImageBuffer, we need to flush pending render ops
    // to fully resolve the snapshot.
    image->PaintImageForCurrentFrame().GetSkImage()->getTextureHandle(
        true);  // Flush pending ops.
  }
  host()->DiscardImageBuffer();  // "Transfer" means no retained buffer.
  return ImageBitmap::Create(std::move(image));
}

RefPtr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (!GetImageBuffer())
    return nullptr;
  RefPtr<StaticBitmapImage> image =
      GetImageBuffer()->NewImageSnapshot(hint, reason);
  return image;
}

ImageData* OffscreenCanvasRenderingContext2D::ToImageData(
    SnapshotReason reason) {
  if (!GetImageBuffer())
    return nullptr;
  RefPtr<StaticBitmapImage> snapshot =
      GetImageBuffer()->NewImageSnapshot(kPreferNoAcceleration, reason);
  ImageData* image_data = nullptr;
  if (snapshot) {
    image_data = ImageData::Create(host()->Size());
    SkImageInfo image_info =
        SkImageInfo::Make(this->Width(), this->Height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    snapshot->PaintImageForCurrentFrame().GetSkImage()->readPixels(
        image_info, image_data->data()->Data(), image_info.minRowBytes(), 0, 0);
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
  if (!HasImageBuffer())
    return nullptr;
  return GetImageBuffer()->Canvas();
}

void OffscreenCanvasRenderingContext2D::DisableDeferral(DisableDeferralReason) {
}

AffineTransform OffscreenCanvasRenderingContext2D::BaseTransform() const {
  if (!HasImageBuffer())
    return AffineTransform();  // identity
  return GetImageBuffer()->BaseTransform();
}

void OffscreenCanvasRenderingContext2D::DidDraw(const SkIRect& dirty_rect) {
  dirty_rect_for_commit_.join(dirty_rect);
}

bool OffscreenCanvasRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(host()->Size());
}

sk_sp<SkImageFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(host()->Size());
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
  return GetImageBuffer();
}

CanvasColorSpace OffscreenCanvasRenderingContext2D::ColorSpace() const {
  return color_params().color_space();
}

String OffscreenCanvasRenderingContext2D::ColorSpaceAsString() const {
  return CanvasRenderingContext::ColorSpaceAsString();
}

CanvasPixelFormat OffscreenCanvasRenderingContext2D::PixelFormat() const {
  return color_params().pixel_format();
}

bool OffscreenCanvasRenderingContext2D::IsAccelerated() const {
  return HasImageBuffer() && GetImageBuffer()->IsAccelerated();
}
}
