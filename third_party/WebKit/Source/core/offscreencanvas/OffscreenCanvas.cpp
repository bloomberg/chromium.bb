// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/offscreencanvas/OffscreenCanvas.h"

#include <memory>
#include "core/css/CSSFontSelector.h"
#include "core/css/OffscreenFontSelector.h"
#include "core/css/StyleEngine.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasAsyncBlobCreator.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/image-encoders/ImageEncoderUtils.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/MathExtras.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size) : size_(size) {}

OffscreenCanvas* OffscreenCanvas::Create(unsigned width, unsigned height) {
  return new OffscreenCanvas(
      IntSize(clampTo<int>(width), clampTo<int>(height)));
}

OffscreenCanvas::~OffscreenCanvas() {}

void OffscreenCanvas::Dispose() {
  if (context_) {
    context_->DetachHost();
    context_ = nullptr;
  }
  if (commit_promise_resolver_) {
    // keepAliveWhilePending() guarantees the promise resolver is never
    // GC-ed before the OffscreenCanvas
    commit_promise_resolver_->Reject();
    commit_promise_resolver_.Clear();
  }
}

void OffscreenCanvas::setWidth(unsigned width) {
  IntSize new_size = size_;
  new_size.SetWidth(clampTo<int>(width));
  SetSize(new_size);
}

void OffscreenCanvas::setHeight(unsigned height) {
  IntSize new_size = size_;
  new_size.SetHeight(clampTo<int>(height));
  SetSize(new_size);
}

void OffscreenCanvas::SetSize(const IntSize& size) {
  if (context_) {
    if (context_->Is3d()) {
      if (size != size_)
        context_->Reshape(size.Width(), size.Height());
    } else if (context_->Is2d()) {
      context_->Reset();
      origin_clean_ = true;
    }
  }
  size_ = size;
  if (frame_dispatcher_) {
    frame_dispatcher_->Reshape(size_.Width(), size_.Height());
  }
  current_frame_damage_rect_ = SkIRect::MakeWH(size_.Width(), size_.Height());
}

void OffscreenCanvas::SetNeutered() {
  DCHECK(!context_);
  is_neutered_ = true;
  size_.SetWidth(0);
  size_.SetHeight(0);
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (is_neutered_) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
    return nullptr;
  }
  if (!context_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Cannot transfer an ImageBitmap from an "
                                      "OffscreenCanvas with no context");
    return nullptr;
  }
  ImageBitmap* image = context_->TransferToImageBitmap(script_state);
  if (!image) {
    // Undocumented exception (not in spec)
    exception_state.ThrowDOMException(kV8Error, "Out of memory");
  }
  return image;
}

RefPtr<Image> OffscreenCanvas::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint hint,
    SnapshotReason reason,
    const FloatSize& size) {
  if (!context_) {
    *status = kInvalidSourceImageStatus;
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
    return surface ? StaticBitmapImage::Create(surface->makeImageSnapshot())
                   : nullptr;
  }
  if (!size.Width() || !size.Height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  RefPtr<Image> image = context_->GetImage(hint, reason);
  if (!image) {
    *status = kInvalidSourceImageStatus;
  } else {
    *status = kNormalSourceImageStatus;
  }
  return image;
}

IntSize OffscreenCanvas::BitmapSourceSize() const {
  return size_;
}

ScriptPromise OffscreenCanvas::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget&,
    Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state,
      IsPaintable() ? ImageBitmap::Create(this, crop_rect, options) : nullptr);
}

bool OffscreenCanvas::IsOpaque() const {
  if (!context_)
    return false;
  return !context_->CreationAttributes().hasAlpha();
}

CanvasRenderingContext* OffscreenCanvas::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    const String& id,
    const CanvasContextCreationAttributes& attributes) {
  execution_context_ = execution_context;

  CanvasRenderingContext::ContextType context_type =
      CanvasRenderingContext::ContextTypeFromId(id);

  // Unknown type.
  if (context_type == CanvasRenderingContext::kContextTypeCount)
    return nullptr;

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(context_type);
  if (!factory)
    return nullptr;

  if (context_) {
    if (context_->GetContextType() != context_type) {
      factory->OnError(
          this, "OffscreenCanvas has an existing context of a different type");
      return nullptr;
    }
  } else {
    context_ = factory->Create(this, attributes);
  }

  return context_.Get();
}

OffscreenCanvas::ContextFactoryVector&
OffscreenCanvas::RenderingContextFactories() {
  DEFINE_STATIC_LOCAL(ContextFactoryVector, context_factories,
                      (CanvasRenderingContext::kContextTypeCount));
  return context_factories;
}

CanvasRenderingContextFactory* OffscreenCanvas::GetRenderingContextFactory(
    int type) {
  DCHECK_LT(type, CanvasRenderingContext::kContextTypeCount);
  return RenderingContextFactories()[type].get();
}

void OffscreenCanvas::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::ContextType type =
      rendering_context_factory->GetContextType();
  DCHECK_LT(type, CanvasRenderingContext::kContextTypeCount);
  DCHECK(!RenderingContextFactories()[type]);
  RenderingContextFactories()[type] = std::move(rendering_context_factory);
}

bool OffscreenCanvas::OriginClean() const {
  return origin_clean_ && !disable_reading_from_canvas_;
}

bool OffscreenCanvas::IsPaintable() const {
  if (!context_)
    return ImageBuffer::CanCreateImageBuffer(size_);
  return context_->IsPaintable() && size_.Width() && size_.Height();
}

bool OffscreenCanvas::IsAccelerated() const {
  return context_ && context_->IsAccelerated();
}

OffscreenCanvasFrameDispatcher* OffscreenCanvas::GetOrCreateFrameDispatcher() {
  if (!frame_dispatcher_) {
    // The frame dispatcher connects the current thread of OffscreenCanvas
    // (either main or worker) to the browser process and remains unchanged
    // throughout the lifetime of this OffscreenCanvas.
    frame_dispatcher_ = WTF::WrapUnique(new OffscreenCanvasFrameDispatcherImpl(
        this, client_id_, sink_id_, placeholder_canvas_id_, size_.Width(),
        size_.Height()));
  }
  return frame_dispatcher_.get();
}

void OffscreenCanvas::DiscardImageBuffer() {
  image_buffer_.reset();
  needs_matrix_clip_restore_ = true;
}

ImageBuffer* OffscreenCanvas::GetOrCreateImageBuffer() {
  if (!image_buffer_) {
    IntSize surface_size(width(), height());
    OpacityMode opacity_mode =
        context_->CreationAttributes().hasAlpha() ? kNonOpaque : kOpaque;
    std::unique_ptr<ImageBufferSurface> surface;
    if (RuntimeEnabledFeatures::Accelerated2dCanvasEnabled()) {
      surface.reset(
          new AcceleratedImageBufferSurface(surface_size, opacity_mode));
    }

    if (!surface || !surface->IsValid()) {
      surface.reset(new UnacceleratedImageBufferSurface(
          surface_size, opacity_mode, kInitializeImagePixels));
    }

    image_buffer_ = ImageBuffer::Create(std::move(surface));

    if (image_buffer_ && needs_matrix_clip_restore_) {
      needs_matrix_clip_restore_ = false;
      context_->RestoreCanvasMatrixClipStack(image_buffer_->Canvas());
    }
  }

  return image_buffer_.get();
}

ScriptPromise OffscreenCanvas::Commit(RefPtr<StaticBitmapImage> image,
                                      const SkIRect& damage_rect,
                                      bool is_web_gl_software_rendering,
                                      ScriptState* script_state,
                                      ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "OffscreenCanvas::Commit");

  if (!HasPlaceholderCanvas()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Commit() was called on a context whose "
        "OffscreenCanvas is not associated with a "
        "canvas element.");
    return exception_state.Reject(script_state);
  }

  GetOrCreateFrameDispatcher()->SetNeedsBeginFrame(true);

  if (!commit_promise_resolver_) {
    commit_promise_resolver_ = ScriptPromiseResolver::Create(script_state);
    commit_promise_resolver_->KeepAliveWhilePending();

    if (image) {
      // We defer the submission of commit frames at the end of JS task
      current_frame_ = std::move(image);
      // union of rects is necessary in case some frames are skipped.
      current_frame_damage_rect_.join(damage_rect);
      current_frame_is_web_gl_software_rendering_ =
          is_web_gl_software_rendering;
      context_->NeedsFinalizeFrame();
    }
  } else if (image) {
    // Two possible scenarios:
    // 1. An override of m_currentFrame can happen when there are multiple
    // frames committed before JS task finishes. (m_currentFrame!=nullptr)
    // 2. The current frame has been dispatched but the promise is not
    // resolved yet. (m_currentFrame==nullptr)
    current_frame_ = std::move(image);
    current_frame_damage_rect_.join(damage_rect);
    current_frame_is_web_gl_software_rendering_ = is_web_gl_software_rendering;
  }

  return commit_promise_resolver_->Promise();
}

void OffscreenCanvas::FinalizeFrame() {
  if (current_frame_) {
    // TODO(eseckler): OffscreenCanvas shouldn't dispatch CompositorFrames
    // without a prior BeginFrame.
    DoCommit();
  }
}

void OffscreenCanvas::DoCommit() {
  TRACE_EVENT0("blink", "OffscreenCanvas::DoCommit");
  double commit_start_time = WTF::MonotonicallyIncreasingTime();
  DCHECK(current_frame_);
  GetOrCreateFrameDispatcher()->DispatchFrame(
      std::move(current_frame_), commit_start_time, current_frame_damage_rect_,
      current_frame_is_web_gl_software_rendering_);
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
}

void OffscreenCanvas::BeginFrame() {
  TRACE_EVENT0("blink", "OffscreenCanvas::BeginFrame");
  if (current_frame_) {
    // TODO(eseckler): beginFrame() shouldn't be used as confirmation of
    // CompositorFrame activation.
    // If we have an overdraw backlog, push the frame from the backlog
    // first and save the promise resolution for later.
    // Then we need to wait for one more frame time to resolve the existing
    // promise.
    DoCommit();
  } else if (commit_promise_resolver_) {
    commit_promise_resolver_->Resolve();
    commit_promise_resolver_.Clear();

    // We need to tell parent frame to stop sending signals on begin frame to
    // avoid overhead once we resolve the promise.
    GetOrCreateFrameDispatcher()->SetNeedsBeginFrame(false);
  }
}

ScriptPromise OffscreenCanvas::convertToBlob(ScriptState* script_state,
                                             const ImageEncodeOptions& options,
                                             ExceptionState& exception_state) {
  if (this->IsNeutered()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return exception_state.Reject(script_state);
  }

  if (!this->OriginClean()) {
    exception_state.ThrowSecurityError(
        "Tainted OffscreenCanvas may not be exported.");
    return exception_state.Reject(script_state);
  }

  if (!this->IsPaintable()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The size of the OffscreenCanvas is zero.");
    return exception_state.Reject(script_state);
  }

  double start_time = WTF::MonotonicallyIncreasingTime();
  String encoding_mime_type = ImageEncoderUtils::ToEncodingMimeType(
      options.type(), ImageEncoderUtils::kEncodeReasonConvertToBlobPromise);

  ImageData* image_data = nullptr;
  if (this->RenderingContext()) {
    image_data = this->RenderingContext()->ToImageData(kSnapshotReasonUnknown);
  }
  if (!image_data) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "OffscreenCanvas object has no rendering contexts");
    return exception_state.Reject(script_state);
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  Document* document =
      ExecutionContext::From(script_state)->IsDocument()
          ? static_cast<Document*>(ExecutionContext::From(script_state))
          : nullptr;

  CanvasAsyncBlobCreator* async_creator = CanvasAsyncBlobCreator::Create(
      image_data->data(), encoding_mime_type, image_data->Size(), start_time,
      document, resolver);

  async_creator->ScheduleAsyncBlobCreation(options.quality());

  return resolver->Promise();
}

FontSelector* OffscreenCanvas::GetFontSelector() {
  if (GetExecutionContext()->IsDocument()) {
    return ToDocument(execution_context_)->GetStyleEngine().GetFontSelector();
  }
  return ToWorkerGlobalScope(execution_context_)->GetFontSelector();
}

DEFINE_TRACE(OffscreenCanvas) {
  visitor->Trace(context_);
  visitor->Trace(execution_context_);
  visitor->Trace(commit_promise_resolver_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
