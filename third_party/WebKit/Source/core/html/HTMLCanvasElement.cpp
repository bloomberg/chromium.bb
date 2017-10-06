/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLCanvasElement.h"

#include <math.h>

#include <memory>

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "build/build_config.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/fileapi/File.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasAsyncBlobCreator.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "core/html/forms/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html_names.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/input_type_names.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/ChromeClient.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintTiming.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/image-encoders/ImageEncoderUtils.h"
#include "platform/runtime_enabled_features.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "v8/include/v8.h"

namespace blink {

using namespace HTMLNames;

namespace {

// These values come from the WhatWG spec.
const int kDefaultWidth = 300;
const int kDefaultHeight = 150;

#if defined(OS_ANDROID)
// We estimate that the max limit for android phones is a quarter of that for
// desktops based on local experimental results on Android One.
const int kMaxGlobalAcceleratedImageBufferCount = 25;
#else
const int kMaxGlobalAcceleratedImageBufferCount = 100;
#endif

// We estimate the max limit of GPU allocated memory for canvases before Chrome
// becomes laggy by setting the total allocated memory for accelerated canvases
// to be equivalent to memory used by 100 accelerated canvases, each has a size
// of 1000*500 and 2d context.
// Each such canvas occupies 4000000 = 1000 * 500 * 2 * 4 bytes, where 2 is the
// gpuBufferCount in ImageBuffer::updateGPUMemoryUsage() and 4 means four bytes
// per pixel per buffer.
const int kMaxGlobalGPUMemoryUsage =
    4000000 * kMaxGlobalAcceleratedImageBufferCount;

// A default value of quality argument for toDataURL and toBlob
// It is in an invalid range (outside 0.0 - 1.0) so that it will not be
// misinterpreted as a user-input value
const int kUndefinedQualityValue = -1.0;

RefPtr<Image> CreateTransparentImage(const IntSize& size) {
  if (!ImageBuffer::CanCreateImageBuffer(size))
    return nullptr;
  sk_sp<SkSurface> surface =
      SkSurface::MakeRasterN32Premul(size.Width(), size.Height());
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

}  // namespace

inline HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(canvasTag, document),
      ContextLifecycleObserver(&document),
      PageVisibilityObserver(document.GetPage()),
      size_(kDefaultWidth, kDefaultHeight),
      ignore_reset_(false),
      externally_allocated_memory_(0),
      origin_clean_(true),
      did_fail_to_create_image_buffer_(false),
      image_buffer_is_clear_(false),
      surface_layer_bridge_(nullptr) {
  CanvasMetrics::CountCanvasContextUsage(CanvasMetrics::kCanvasCreated);
  UseCounter::Count(document, WebFeature::kHTMLCanvasElement);
}

DEFINE_NODE_FACTORY(HTMLCanvasElement)

HTMLCanvasElement::~HTMLCanvasElement() {
  if (surface_layer_bridge_ && surface_layer_bridge_->GetWebLayer()) {
    GraphicsLayer::UnregisterContentsLayer(
        surface_layer_bridge_->GetWebLayer());
  }
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -externally_allocated_memory_);
}

void HTMLCanvasElement::Dispose() {
  if (PlaceholderFrame())
    ReleasePlaceholderFrame();

  if (context_) {
    context_->DetachHost();
    context_ = nullptr;
  }

  if (image_buffer_) {
    image_buffer_->SetClient(nullptr);
    image_buffer_ = nullptr;
  }
}

void HTMLCanvasElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == widthAttr || params.name == heightAttr)
    Reset();
  HTMLElement::ParseAttribute(params);
}

LayoutObject* HTMLCanvasElement::CreateLayoutObject(
    const ComputedStyle& style) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (frame && GetDocument().CanExecuteScripts(kNotAboutToExecuteScript))
    return new LayoutHTMLCanvas(this);
  return HTMLElement::CreateLayoutObject(style);
}

Node::InsertionNotificationRequest HTMLCanvasElement::InsertedInto(
    ContainerNode* node) {
  SetIsInCanvasSubtree(true);
  return HTMLElement::InsertedInto(node);
}

void HTMLCanvasElement::setHeight(unsigned value,
                                  ExceptionState& exception_state) {
  if (IsPlaceholderRegistered()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot resize canvas after call to transferControlToOffscreen().");
    return;
  }
  SetUnsignedIntegralAttribute(heightAttr, value, kDefaultHeight);
}

void HTMLCanvasElement::setWidth(unsigned value,
                                 ExceptionState& exception_state) {
  if (IsPlaceholderRegistered()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot resize canvas after call to transferControlToOffscreen().");
    return;
  }
  SetUnsignedIntegralAttribute(widthAttr, value, kDefaultWidth);
}

void HTMLCanvasElement::SetSize(const IntSize& new_size) {
  if (new_size == Size())
    return;
  ignore_reset_ = true;
  SetIntegralAttribute(widthAttr, new_size.Width());
  SetIntegralAttribute(heightAttr, new_size.Height());
  ignore_reset_ = false;
  Reset();
}

HTMLCanvasElement::ContextFactoryVector&
HTMLCanvasElement::RenderingContextFactories() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ContextFactoryVector, context_factories,
                      (CanvasRenderingContext::kContextTypeCount));
  return context_factories;
}

CanvasRenderingContextFactory* HTMLCanvasElement::GetRenderingContextFactory(
    int type) {
  DCHECK(type < CanvasRenderingContext::kContextTypeCount);
  return RenderingContextFactories()[type].get();
}

void HTMLCanvasElement::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::ContextType type =
      rendering_context_factory->GetContextType();
  DCHECK(type < CanvasRenderingContext::kContextTypeCount);
  DCHECK(!RenderingContextFactories()[type]);
  RenderingContextFactories()[type] = std::move(rendering_context_factory);
}

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContext(
    const String& type,
    const CanvasContextCreationAttributes& attributes) {
  CanvasRenderingContext::ContextType context_type =
      CanvasRenderingContext::ContextTypeFromId(type);

  // Unknown type.
  if (context_type == CanvasRenderingContext::kContextTypeCount)
    return nullptr;

  // Log the aliased context type used.
  if (!context_) {
    DEFINE_STATIC_LOCAL(
        EnumerationHistogram, context_type_histogram,
        ("Canvas.ContextType", CanvasRenderingContext::kContextTypeCount));
    context_type_histogram.Count(context_type);
  }

  context_type =
      CanvasRenderingContext::ResolveContextTypeAliases(context_type);

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(context_type);
  if (!factory)
    return nullptr;

  // FIXME - The code depends on the context not going away once created, to
  // prevent JS from seeing a dangling pointer. So for now we will disallow the
  // context from being changed once it is created.
  if (context_) {
    if (context_->GetContextType() == context_type)
      return context_.Get();

    factory->OnError(this,
                     "Canvas has an existing context of a different type");
    return nullptr;
  }

  context_ = factory->Create(this, attributes);
  if (!context_)
    return nullptr;

  probe::didCreateCanvasContext(&GetDocument());

  if (Is3d()) {
    UpdateExternallyAllocatedMemory();
  }

  LayoutObject* layout_object = this->GetLayoutObject();
  if (layout_object && Is2d() && !context_->CreationAttributes().alpha()) {
    // In the alpha false case, canvas is initially opaque even though there is
    // no ImageBuffer, so we need to trigger an invalidation.
    DidDraw();
  }

  SetNeedsCompositingUpdate();

  return context_.Get();
}

bool HTMLCanvasElement::ShouldBeDirectComposited() const {
  return (context_ && context_->IsComposited()) ||
         (GetImageBuffer() && GetImageBuffer()->IsExpensiveToPaint()) ||
         (!!surface_layer_bridge_);
}

bool HTMLCanvasElement::IsPaintable() const {
  return (context_ && context_->IsPaintable()) ||
         ImageBuffer::CanCreateImageBuffer(Size());
}

bool HTMLCanvasElement::IsAccelerated() const {
  return context_ && context_->IsAccelerated();
}

bool HTMLCanvasElement::IsWebGL1Enabled() const {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (frame) {
    Settings* settings = frame->GetSettings();
    if (settings && settings->GetWebGL1Enabled())
      return true;
  }
  return false;
}

bool HTMLCanvasElement::IsWebGL2Enabled() const {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (frame) {
    Settings* settings = frame->GetSettings();
    if (settings && settings->GetWebGL2Enabled())
      return true;
  }
  return false;
}

bool HTMLCanvasElement::IsWebGLBlocked() const {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (frame && frame->Client()->ShouldBlockWebGL())
    return true;
  return false;
}

void HTMLCanvasElement::DidDraw(const FloatRect& rect) {
  if (rect.IsEmpty())
    return;
  image_buffer_is_clear_ = false;
  ClearCopiedImage();
  if (GetLayoutObject())
    GetLayoutObject()->SetMayNeedPaintInvalidation();
  if (Is2d() && context_->ShouldAntialias() && GetPage() &&
      GetPage()->DeviceScaleFactorDeprecated() > 1.0f) {
    FloatRect inflated_rect = rect;
    inflated_rect.Inflate(1);
    dirty_rect_.Unite(inflated_rect);
  } else {
    dirty_rect_.Unite(rect);
  }
  if (Is2d() && GetImageBuffer())
    GetImageBuffer()->DidDraw(rect);
}

void HTMLCanvasElement::DidDraw() {
  DidDraw(FloatRect(0, 0, Size().Width(), Size().Height()));
}

void HTMLCanvasElement::FinalizeFrame() {
  if (GetImageBuffer())
    image_buffer_->FinalizeFrame();

  // If the canvas is visible, notifying listeners is taken
  // care of in the in doDeferredPaintInvalidation, which allows
  // the frame to be grabbed prior to compositing, which is
  // critically important because compositing may clear the canvas's
  // image. (e.g. WebGL context with preserveDrawingBuffer=false).
  // If the canvas is not visible, doDeferredPaintInvalidation
  // will not get called, so we need to take care of business here.
  if (!did_notify_listeners_for_current_frame_)
    NotifyListenersCanvasChanged();
  did_notify_listeners_for_current_frame_ = false;
}

void HTMLCanvasElement::DidDisableAcceleration() {
  // We must force a paint invalidation on the canvas even if it's
  // content did not change because it layer was destroyed.
  DidDraw();
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::RestoreCanvasMatrixClipStack(
    PaintCanvas* canvas) const {
  if (context_)
    context_->RestoreCanvasMatrixClipStack(canvas);
}

void HTMLCanvasElement::SetNeedsCompositingUpdate() {
  Element::SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::DoDeferredPaintInvalidation() {
  DCHECK(!dirty_rect_.IsEmpty());
  if (Is2d()) {
    FloatRect src_rect(0, 0, Size().Width(), Size().Height());
    dirty_rect_.Intersect(src_rect);
    LayoutBox* lb = GetLayoutBox();
    FloatRect invalidation_rect;
    if (lb) {
      FloatRect mapped_dirty_rect =
          MapRect(dirty_rect_, src_rect, FloatRect(lb->ContentBoxRect()));
      if (context_->IsComposited()) {
        // Accelerated 2D canvases need the dirty rect to be expressed relative
        // to the content box, as opposed to the layout box.
        mapped_dirty_rect.Move(-lb->ContentBoxOffset());
      }
      invalidation_rect = mapped_dirty_rect;
    } else {
      invalidation_rect = dirty_rect_;
    }

    if (dirty_rect_.IsEmpty())
      return;

    if (GetImageBuffer()) {
      image_buffer_->DoPaintInvalidation(invalidation_rect);
    }
  }

  if (context_ &&
      context_->GetContextType() ==
          CanvasRenderingContext::kContextImageBitmap &&
      context_->PlatformLayer()) {
    context_->PlatformLayer()->Invalidate();
  }

  NotifyListenersCanvasChanged();
  did_notify_listeners_for_current_frame_ = true;

  // Propagate the m_dirtyRect accumulated so far to the compositor
  // before restarting with a blank dirty rect.
  FloatRect src_rect(0, 0, Size().Width(), Size().Height());

  LayoutBox* ro = GetLayoutBox();
  // Canvas content updates do not need to be propagated as
  // paint invalidations if the canvas is composited separately, since
  // the canvas contents are sent separately through a texture layer.
  if (ro && (!context_ || !context_->IsComposited())) {
    // If ro->contentBoxRect() is larger than srcRect the canvas's image is
    // being stretched, so we need to account for color bleeding caused by the
    // interpollation filter.
    if (ro->ContentBoxRect().Width() > src_rect.Width() ||
        ro->ContentBoxRect().Height() > src_rect.Height()) {
      dirty_rect_.Inflate(0.5);
    }

    dirty_rect_.Intersect(src_rect);
    LayoutRect mapped_dirty_rect(EnclosingIntRect(
        MapRect(dirty_rect_, src_rect, FloatRect(ro->ContentBoxRect()))));
    // For querying PaintLayer::compositingState()
    // FIXME: is this invalidation using the correct compositing state?
    DisableCompositingQueryAsserts disabler;
    ro->InvalidatePaintRectangle(mapped_dirty_rect);
  }
  dirty_rect_ = FloatRect();

  DCHECK(dirty_rect_.IsEmpty());
}

void HTMLCanvasElement::Reset() {
  if (ignore_reset_)
    return;

  dirty_rect_ = FloatRect();

  bool had_image_buffer = GetImageBuffer();

  unsigned w = 0;
  AtomicString value = getAttribute(widthAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, w) ||
      w > 0x7fffffffu)
    w = kDefaultWidth;

  unsigned h = 0;
  value = getAttribute(heightAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, h) ||
      h > 0x7fffffffu)
    h = kDefaultHeight;

  if (Is2d()) {
    context_->Reset();
    origin_clean_ = true;
  }

  IntSize old_size = Size();
  IntSize new_size(w, h);

  // If the size of an existing buffer matches, we can just clear it instead of
  // reallocating.  This optimization is only done for 2D canvases for now.
  if (had_image_buffer && old_size == new_size && Is2d() &&
      !GetOrCreateImageBuffer()->IsRecording()) {
    if (!image_buffer_is_clear_) {
      image_buffer_is_clear_ = true;
      context_->clearRect(0, 0, width(), height());
    }
    return;
  }

  SetSurfaceSize(new_size);

  if (Is3d() && old_size != Size())
    context_->Reshape(width(), height());

  if (LayoutObject* layout_object = this->GetLayoutObject()) {
    if (layout_object->IsCanvas()) {
      if (old_size != Size()) {
        ToLayoutHTMLCanvas(layout_object)->CanvasSizeChanged();
        if (GetLayoutBox() && GetLayoutBox()->HasAcceleratedCompositing())
          GetLayoutBox()->ContentChanged(kCanvasChanged);
      }
      if (had_image_buffer)
        layout_object->SetShouldDoFullPaintInvalidation();
    }
  }
}

bool HTMLCanvasElement::PaintsIntoCanvasBuffer() const {
  if (PlaceholderFrame())
    return false;
  DCHECK(context_);
  if (!context_->IsComposited())
    return true;
  if (GetLayoutBox() && GetLayoutBox()->HasAcceleratedCompositing())
    return false;

  return true;
}

CanvasColorParams HTMLCanvasElement::GetCanvasColorParams() const {
  if (context_)
    return context_->color_params();
  return CanvasColorParams();
}

void HTMLCanvasElement::NotifyListenersCanvasChanged() {
  if (listeners_.size() == 0)
    return;

  if (!OriginClean()) {
    listeners_.clear();
    return;
  }

  bool listener_needs_new_frame_capture = false;
  for (const CanvasDrawListener* listener : listeners_) {
    if (listener->NeedsNewFrame()) {
      listener_needs_new_frame_capture = true;
    }
  }

  if (listener_needs_new_frame_capture) {
    SourceImageStatus status;
    RefPtr<Image> source_image = GetSourceImageForCanvas(
        &status, kPreferNoAcceleration, kSnapshotReasonCanvasListenerCapture,
        FloatSize());
    if (status != kNormalSourceImageStatus)
      return;
    sk_sp<SkImage> image =
        source_image->PaintImageForCurrentFrame().GetSkImage();
    for (CanvasDrawListener* listener : listeners_) {
      if (listener->NeedsNewFrame()) {
        listener->SendNewFrame(image);
      }
    }
  }
}

void HTMLCanvasElement::Paint(GraphicsContext& context, const LayoutRect& r) {
  // FIXME: crbug.com/438240; there is a bug with the new CSS blending and
  // compositing feature.
  if (!context_ && !PlaceholderFrame())
    return;

  const ComputedStyle* style = EnsureComputedStyle();
  SkFilterQuality filter_quality =
      (style && style->ImageRendering() == EImageRendering::kPixelated)
          ? kNone_SkFilterQuality
          : kLow_SkFilterQuality;

  if (Is3d()) {
    context_->SetFilterQuality(filter_quality);
  } else if (GetImageBuffer()) {
    image_buffer_->SetFilterQuality(filter_quality);
  }

  if (GetImageBuffer() && !image_buffer_is_clear_)
    PaintTiming::From(GetDocument()).MarkFirstContentfulPaint();

  if (!PaintsIntoCanvasBuffer() && !GetDocument().Printing())
    return;

  if (PlaceholderFrame()) {
    DCHECK(GetDocument().Printing());
    context.DrawImage(PlaceholderFrame().get(), PixelSnappedIntRect(r));
    return;
  }

  // TODO(junov): Paint is currently only implemented by ImageBitmap contexts.
  // We could improve the abstraction by making all context types paint
  // themselves (implement paint()).
  if (context_->Paint(context, PixelSnappedIntRect(r)))
    return;

  context_->PaintRenderingResultsToCanvas(kFrontBuffer);
  if (GetImageBuffer()) {
    if (!context.ContextDisabled()) {
      SkBlendMode composite_operator =
          !context_ || context_->CreationAttributes().alpha()
              ? SkBlendMode::kSrcOver
              : SkBlendMode::kSrc;
      GetImageBuffer()->Draw(context, PixelSnappedIntRect(r), 0,
                             composite_operator);
    }
  } else {
    // When alpha is false, we should draw to opaque black.
    if (!context_->CreationAttributes().alpha())
      context.FillRect(FloatRect(r), Color(0, 0, 0));
  }

  if (Is3d() && PaintsIntoCanvasBuffer())
    context_->MarkLayerComposited();
}

bool HTMLCanvasElement::Is3d() const {
  return context_ && context_->Is3d();
}

bool HTMLCanvasElement::Is2d() const {
  return context_ && context_->Is2d();
}

bool HTMLCanvasElement::IsAnimated2d() const {
  return Is2d() && GetImageBuffer() &&
         GetImageBuffer()->WasDrawnToAfterSnapshot();
}

void HTMLCanvasElement::SetSurfaceSize(const IntSize& size) {
  size_ = size;
  did_fail_to_create_image_buffer_ = false;
  DiscardImageBuffer();
  ClearCopiedImage();
  if (Is2d() && context_->isContextLost()) {
    context_->DidSetSurfaceSize();
  }
}

const AtomicString HTMLCanvasElement::ImageSourceURL() const {
  return AtomicString(
      ToDataURLInternal(ImageEncoderUtils::kDefaultMimeType, 0, kFrontBuffer));
}

void HTMLCanvasElement::PrepareSurfaceForPaintingIfNeeded() {
  DCHECK(Is2d());  // This function is called by the 2d context
  if (GetOrCreateImageBuffer())
    image_buffer_->PrepareSurfaceForPaintingIfNeeded();
}

ImageData* HTMLCanvasElement::ToImageData(SourceDrawingBuffer source_buffer,
                                          SnapshotReason reason) const {
  ImageData* image_data;
  if (Is3d()) {
    // Get non-premultiplied data because of inaccurate premultiplied alpha
    // conversion of buffer()->toDataURL().
    image_data = context_->PaintRenderingResultsToImageData(source_buffer);
    if (image_data)
      return image_data;

    context_->PaintRenderingResultsToCanvas(source_buffer);
    image_data = ImageData::Create(size_);
    if (image_data && GetImageBuffer()) {
      RefPtr<StaticBitmapImage> snapshot =
          GetImageBuffer()->NewImageSnapshot(kPreferNoAcceleration, reason);
      if (snapshot) {
        SkImageInfo image_info = SkImageInfo::Make(
            width(), height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        snapshot->PaintImageForCurrentFrame().GetSkImage()->readPixels(
            image_info, image_data->data()->Data(), image_info.minRowBytes(), 0,
            0);
      }
    }
    return image_data;
  }

  image_data = ImageData::Create(size_);

  if ((!context_ || !image_data) && !PlaceholderFrame())
    return image_data;

  DCHECK(Is2d() || PlaceholderFrame());
  RefPtr<StaticBitmapImage> snapshot;
  if (GetImageBuffer()) {
    snapshot =
        GetImageBuffer()->NewImageSnapshot(kPreferNoAcceleration, reason);
  } else if (PlaceholderFrame()) {
    DCHECK(PlaceholderFrame()->OriginClean());
    snapshot = PlaceholderFrame();
  }

  if (snapshot) {
    SkImageInfo image_info = SkImageInfo::Make(
        width(), height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    snapshot->PaintImageForCurrentFrame().GetSkImage()->readPixels(
        image_info, image_data->data()->Data(), image_info.minRowBytes(), 0, 0);
  }

  return image_data;
}

String HTMLCanvasElement::ToDataURLInternal(
    const String& mime_type,
    const double& quality,
    SourceDrawingBuffer source_buffer) const {
  if (!IsPaintable())
    return String("data:,");

  String encoding_mime_type = ImageEncoderUtils::ToEncodingMimeType(
      mime_type, ImageEncoderUtils::kEncodeReasonToDataURL);

  Optional<ScopedUsHistogramTimer> timer;
  if (encoding_mime_type == "image/png") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scoped_us_counter_png,
        ("Blink.Canvas.ToDataURL.PNG", 0, 10000000, 50));
    timer.emplace(scoped_us_counter_png);
  } else if (encoding_mime_type == "image/jpeg") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scoped_us_counter_jpeg,
        ("Blink.Canvas.ToDataURL.JPEG", 0, 10000000, 50));
    timer.emplace(scoped_us_counter_jpeg);
  } else if (encoding_mime_type == "image/webp") {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scoped_us_counter_webp,
        ("Blink.Canvas.ToDataURL.WEBP", 0, 10000000, 50));
    timer.emplace(scoped_us_counter_webp);
  } else {
    // Currently we only support three encoding types.
    NOTREACHED();
  }

  ImageData* image_data = ToImageData(source_buffer, kSnapshotReasonToDataURL);

  if (!image_data)  // allocation failure
    return String("data:,");

  return ImageDataBuffer(image_data->Size(), image_data->data()->Data())
      .ToDataURL(encoding_mime_type, quality);
}

String HTMLCanvasElement::toDataURL(const String& mime_type,
                                    const ScriptValue& quality_argument,
                                    ExceptionState& exception_state) const {
  if (!OriginClean()) {
    exception_state.ThrowSecurityError("Tainted canvases may not be exported.");
    return String();
  }

  double quality = kUndefinedQualityValue;
  if (!quality_argument.IsEmpty()) {
    v8::Local<v8::Value> v8_value = quality_argument.V8Value();
    if (v8_value->IsNumber()) {
      quality = v8_value.As<v8::Number>()->Value();
    }
  }
  return ToDataURLInternal(mime_type, quality, kBackBuffer);
}

void HTMLCanvasElement::toBlob(BlobCallback* callback,
                               const String& mime_type,
                               const ScriptValue& quality_argument,
                               ExceptionState& exception_state) {
  if (!OriginClean()) {
    exception_state.ThrowSecurityError("Tainted canvases may not be exported.");
    return;
  }

  if (!IsPaintable()) {
    // If the canvas element's bitmap has no pixels
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, &GetDocument())
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&BlobCallback::handleEvent,
                             WrapPersistent(callback), nullptr));
    return;
  }

  double start_time = WTF::MonotonicallyIncreasingTime();
  double quality = kUndefinedQualityValue;
  if (!quality_argument.IsEmpty()) {
    v8::Local<v8::Value> v8_value = quality_argument.V8Value();
    if (v8_value->IsNumber()) {
      quality = v8_value.As<v8::Number>()->Value();
    }
  }

  String encoding_mime_type = ImageEncoderUtils::ToEncodingMimeType(
      mime_type, ImageEncoderUtils::kEncodeReasonToBlobCallback);

  ImageData* image_data = ToImageData(kBackBuffer, kSnapshotReasonToBlob);

  if (!image_data) {
    // ImageData allocation faillure
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, &GetDocument())
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&BlobCallback::handleEvent,
                             WrapPersistent(callback), nullptr));
    return;
  }

  CanvasAsyncBlobCreator* async_creator = CanvasAsyncBlobCreator::Create(
      image_data->data(), encoding_mime_type, image_data->Size(), callback,
      start_time, &GetDocument());

  async_creator->ScheduleAsyncBlobCreation(quality);
}

void HTMLCanvasElement::AddListener(CanvasDrawListener* listener) {
  listeners_.insert(listener);
}

void HTMLCanvasElement::RemoveListener(CanvasDrawListener* listener) {
  listeners_.erase(listener);
}

bool HTMLCanvasElement::OriginClean() const {
  if (GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetDisableReadingFromCanvas())
    return false;
  if (PlaceholderFrame())
    return PlaceholderFrame()->OriginClean();
  return origin_clean_;
}

bool HTMLCanvasElement::ShouldAccelerate(AccelerationCriteria criteria) const {
  if (context_ && !Is2d())
    return false;

  if (RuntimeEnabledFeatures::ForceDisplayList2dCanvasEnabled())
    return false;

  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled())
    return false;

  // The following is necessary for handling the special case of canvases in the
  // dev tools overlay, which run in a process that supports accelerated 2d
  // canvas but in a special compositing context that does not.
  if (GetLayoutBox() && !GetLayoutBox()->HasAcceleratedCompositing())
    return false;

  CheckedNumeric<int> checked_canvas_pixel_count = Size().Width();
  checked_canvas_pixel_count *= Size().Height();
  if (!checked_canvas_pixel_count.IsValid())
    return false;
  int canvas_pixel_count = checked_canvas_pixel_count.ValueOrDie();

  if (RuntimeEnabledFeatures::DisplayList2dCanvasEnabled()) {
#if 0
        // TODO(junov): re-enable this code once we solve the problem of recording
        // GPU-backed images to a PaintRecord for cross-context rendering crbug.com/490328

        // If the compositor provides GPU acceleration to display list canvases, we
        // prefer that over direct acceleration.
        if (document().viewportDescription().matchesHeuristicsForGpuRasterization())
            return false;
#endif
    // If the GPU resources would be very expensive, prefer a display list.
    if (canvas_pixel_count >
        CanvasHeuristicParameters::kPreferDisplayListOverGpuSizeThreshold)
      return false;
  }

  // Do not use acceleration for small canvas.
  if (criteria != kIgnoreResourceLimitCriteria) {
    Settings* settings = GetDocument().GetSettings();
    if (!settings ||
        canvas_pixel_count < settings->GetMinimumAccelerated2dCanvasSize())
      return false;

    // When GPU allocated memory runs low (due to having created too many
    // accelerated canvases), the compositor starves and browser becomes laggy.
    // Thus, we should stop allocating more GPU memory to new canvases created
    // when the current memory usage exceeds the threshold.
    if (ImageBuffer::GetGlobalGPUMemoryUsage() >= kMaxGlobalGPUMemoryUsage)
      return false;

    // Allocating too many GPU resources can makes us run into the driver's
    // resource limits. So we need to keep the number of texture resources
    // under tight control
    if (ImageBuffer::GetGlobalAcceleratedImageBufferCount() >=
        kMaxGlobalAcceleratedImageBufferCount)
      return false;
  }

  return true;
}

bool HTMLCanvasElement::ShouldUseDisplayList() {
  // Rasterization of web contents will blend in the output space. Only embed
  // the canvas as a display list if it intended to do output space blending as
  // well.
  if (GetCanvasColorParams().LinearPixelMath())
    return false;

  if (RuntimeEnabledFeatures::ForceDisplayList2dCanvasEnabled())
    return true;

  if (GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetForceDisplayList2dCanvasEnabled()) {
    return true;
  }

  if (MemoryCoordinator::IsLowEndDevice())
    return false;

  if (!RuntimeEnabledFeatures::DisplayList2dCanvasEnabled())
    return false;

  // TODO(crbug.com/721727): Due to a rendering regression with display-
  // list-2d-canvas, this feature is now disabled by default.  For now, the
  // feature will only be enabled with --force-display-list-2d-canvas.
  return false;
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::CreateWebGLImageBufferSurface(OpacityMode opacity_mode) {
  DCHECK(Is3d());
  // If 3d, but the use of the canvas will be for non-accelerated content
  // then make a non-accelerated ImageBuffer. This means copying the internal
  // Image will require a pixel readback, but that is unavoidable in this case.
  auto surface = WTF::MakeUnique<AcceleratedImageBufferSurface>(
      Size(), opacity_mode, GetCanvasColorParams());
  if (surface->IsValid())
    return std::move(surface);
  return nullptr;
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::CreateAcceleratedImageBufferSurface(OpacityMode opacity_mode,
                                                       int* msaa_sample_count) {
  if (GetDocument().GetSettings()) {
    *msaa_sample_count =
        GetDocument().GetSettings()->GetAccelerated2dCanvasMSAASampleCount();
  }

  // Avoid creating |contextProvider| until we're sure we want to try use it,
  // since it costs us GPU memory.
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper) {
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kAccelerated2DCanvasGPUContextLost);
    return nullptr;
  }

  if (context_provider_wrapper->ContextProvider()->IsSoftwareRendering())
    return nullptr;  // Don't use accelerated canvas with swiftshader.

  auto surface = WTF::MakeUnique<Canvas2DLayerBridge>(
      Size(), *msaa_sample_count, opacity_mode,
      Canvas2DLayerBridge::kEnableAcceleration, GetCanvasColorParams());
  if (!surface->IsValid()) {
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kGPUAccelerated2DCanvasImageBufferCreationFailed);
    return nullptr;
  }

  if (MemoryCoordinator::IsLowEndDevice())
    surface->DisableDeferral(kDisableDeferralReasonLowEndDevice);

  CanvasMetrics::CountCanvasContextUsage(
      CanvasMetrics::kGPUAccelerated2DCanvasImageBufferCreated);
  return std::move(surface);
}

std::unique_ptr<ImageBufferSurface>
HTMLCanvasElement::CreateUnacceleratedImageBufferSurface(
    OpacityMode opacity_mode) {
  if (ShouldUseDisplayList()) {
    auto surface = WTF::MakeUnique<RecordingImageBufferSurface>(
        Size(), RecordingImageBufferSurface::kAllowFallback, opacity_mode,
        GetCanvasColorParams());
    if (surface->IsValid()) {
      CanvasMetrics::CountCanvasContextUsage(
          CanvasMetrics::kDisplayList2DCanvasImageBufferCreated);
      return std::move(surface);
    }
    // We fallback to a non-display-list surface without recording a metric
    // here.
  }

  auto surface = WTF::MakeUnique<Canvas2DLayerBridge>(
      Size(), 0, opacity_mode, Canvas2DLayerBridge::kDisableAcceleration,
      GetCanvasColorParams());
  if (surface->IsValid()) {
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kUnaccelerated2DCanvasImageBufferCreated);
    return std::move(surface);
  }

  CanvasMetrics::CountCanvasContextUsage(
      CanvasMetrics::kUnaccelerated2DCanvasImageBufferCreationFailed);
  return nullptr;
}

void HTMLCanvasElement::CreateImageBuffer() {
  CreateImageBufferInternal(nullptr);
  if (did_fail_to_create_image_buffer_ && Is2d() && !Size().IsEmpty())
    context_->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
}

void HTMLCanvasElement::CreateImageBufferInternal(
    std::unique_ptr<ImageBufferSurface> external_surface) {
  DCHECK(!image_buffer_);

  did_fail_to_create_image_buffer_ = true;
  image_buffer_is_clear_ = true;

  if (!ImageBuffer::CanCreateImageBuffer(Size()))
    return;

  OpacityMode opacity_mode = !context_ || context_->CreationAttributes().alpha()
                                 ? kNonOpaque
                                 : kOpaque;
  int msaa_sample_count = 0;
  std::unique_ptr<ImageBufferSurface> surface;
  if (external_surface) {
    if (external_surface->IsValid())
      surface = std::move(external_surface);
  } else if (Is3d()) {
    surface = CreateWebGLImageBufferSurface(opacity_mode);
  } else {
    if (ShouldAccelerate(kNormalAccelerationCriteria)) {
      surface =
          CreateAcceleratedImageBufferSurface(opacity_mode, &msaa_sample_count);
    }
    if (!surface) {
      surface = CreateUnacceleratedImageBufferSurface(opacity_mode);
    }
  }
  if (!surface)
    return;
  DCHECK(surface->IsValid());
  image_buffer_ = ImageBuffer::Create(std::move(surface));
  DCHECK(image_buffer_);
  image_buffer_->SetClient(this);

  did_fail_to_create_image_buffer_ = false;

  UpdateExternallyAllocatedMemory();

  if (Is3d()) {
    // Early out for WebGL canvases
    return;
  }

  // Enabling MSAA overrides a request to disable antialiasing. This is true
  // regardless of whether the rendering mode is accelerated or not. For
  // consistency, we don't want to apply AA in accelerated canvases but not in
  // unaccelerated canvases.
  if (!msaa_sample_count && GetDocument().GetSettings() &&
      !GetDocument().GetSettings()->GetAntialiased2dCanvasEnabled())
    context_->SetShouldAntialias(false);

  if (context_)
    SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::NotifySurfaceInvalid() {
  if (Is2d())
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);
}

DEFINE_TRACE(HTMLCanvasElement) {
  visitor->Trace(listeners_);
  visitor->Trace(context_);
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  HTMLElement::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLCanvasElement) {
  visitor->TraceWrappers(context_);
  HTMLElement::TraceWrappers(visitor);
}

void HTMLCanvasElement::UpdateExternallyAllocatedMemory() const {
  int buffer_count = 0;
  if (image_buffer_) {
    buffer_count++;
    if (image_buffer_->IsAccelerated()) {
      // The number of internal GPU buffers vary between one (stable
      // non-displayed state) and three (triple-buffered animations).
      // Adding 2 is a pessimistic but relevant estimate.
      // Note: These buffers might be allocated in GPU memory.
      buffer_count += 2;
    }
  }
  if (copied_image_)
    buffer_count++;

  // Multiplying number of buffers by bytes per pixel
  CheckedNumeric<intptr_t> checked_externally_allocated_memory =
      buffer_count * GetCanvasColorParams().BytesPerPixel();
  if (Is3d()) {
    checked_externally_allocated_memory +=
        context_->ExternallyAllocatedBytesPerPixel();
  }

  checked_externally_allocated_memory *= width();
  checked_externally_allocated_memory *= height();
  intptr_t externally_allocated_memory =
      checked_externally_allocated_memory.ValueOrDefault(
          std::numeric_limits<intptr_t>::max());

  // Subtracting two intptr_t that are known to be positive will never
  // underflow.
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      externally_allocated_memory - externally_allocated_memory_);
  externally_allocated_memory_ = externally_allocated_memory;
}

PaintCanvas* HTMLCanvasElement::DrawingCanvas() {
  return GetOrCreateImageBuffer() ? GetImageBuffer()->Canvas() : nullptr;
}

void HTMLCanvasElement::DisableDeferral(DisableDeferralReason reason) {
  if (GetOrCreateImageBuffer())
    image_buffer_->DisableDeferral(reason);
}

PaintCanvas* HTMLCanvasElement::ExistingDrawingCanvas() const {
  if (!GetImageBuffer())
    return nullptr;

  return image_buffer_->Canvas();
}

ImageBuffer* HTMLCanvasElement::GetOrCreateImageBuffer() {
  DCHECK(context_);
  DCHECK(context_->GetContextType() !=
         CanvasRenderingContext::kContextImageBitmap);
  if (!image_buffer_ && !did_fail_to_create_image_buffer_)
    CreateImageBuffer();
  return image_buffer_.get();
}

void HTMLCanvasElement::CreateImageBufferUsingSurfaceForTesting(
    std::unique_ptr<ImageBufferSurface> surface) {
  DiscardImageBuffer();
  SetIntegralAttribute(widthAttr, surface->size().Width());
  SetIntegralAttribute(heightAttr, surface->size().Height());
  CreateImageBufferInternal(std::move(surface));
}

void HTMLCanvasElement::EnsureUnacceleratedImageBuffer() {
  DCHECK(context_);
  if ((GetImageBuffer() && !GetImageBuffer()->IsAccelerated()) ||
      did_fail_to_create_image_buffer_)
    return;
  DiscardImageBuffer();
  OpacityMode opacity_mode =
      context_->CreationAttributes().alpha() ? kNonOpaque : kOpaque;
  image_buffer_ = ImageBuffer::Create(Size(), opacity_mode);
  did_fail_to_create_image_buffer_ = !image_buffer_;
}

RefPtr<Image> HTMLCanvasElement::CopiedImage(SourceDrawingBuffer source_buffer,
                                             AccelerationHint hint,
                                             SnapshotReason snapshot_reason) {
  if (!IsPaintable())
    return nullptr;
  if (!context_)
    return CreateTransparentImage(Size());

  if (context_->GetContextType() ==
      CanvasRenderingContext::kContextImageBitmap) {
    RefPtr<Image> image = context_->GetImage(hint, snapshot_reason);
    // TODO(fserb): return image?
    if (image)
      return context_->GetImage(hint, snapshot_reason);
    // Special case: transferFromImageBitmap is not yet called.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(width(), height());
    return StaticBitmapImage::Create(surface->makeImageSnapshot());
  }

  bool need_to_update = !copied_image_;
  // The concept of SourceDrawingBuffer is valid on only WebGL.
  if (context_->Is3d())
    need_to_update |= context_->PaintRenderingResultsToCanvas(source_buffer);
  if (need_to_update && GetOrCreateImageBuffer()) {
    copied_image_ = GetImageBuffer()->NewImageSnapshot(hint, snapshot_reason);
    UpdateExternallyAllocatedMemory();
  }
  return copied_image_;
}

void HTMLCanvasElement::DiscardImageBuffer() {
  image_buffer_.reset();
  dirty_rect_ = FloatRect();
  UpdateExternallyAllocatedMemory();
}

void HTMLCanvasElement::ClearCopiedImage() {
  if (copied_image_) {
    copied_image_ = nullptr;
    UpdateExternallyAllocatedMemory();
  }
}

AffineTransform HTMLCanvasElement::BaseTransform() const {
  DCHECK(GetImageBuffer() && !did_fail_to_create_image_buffer_);
  return image_buffer_->BaseTransform();
}

void HTMLCanvasElement::PageVisibilityChanged() {
  bool hidden = !GetPage()->IsPageVisible();
  SetSuspendOffscreenCanvasAnimation(hidden);

  if (!context_)
    return;

  context_->SetIsHidden(hidden);
  if (hidden) {
    ClearCopiedImage();
    if (Is3d()) {
      DiscardImageBuffer();
    }
  }
}

void HTMLCanvasElement::ContextDestroyed(ExecutionContext*) {
  if (context_)
    context_->Stop();
}

void HTMLCanvasElement::StyleDidChange(const ComputedStyle* old_style,
                                       const ComputedStyle& new_style) {
  if (context_)
    context_->StyleDidChange(old_style, new_style);
}

void HTMLCanvasElement::DidMoveToNewDocument(Document& old_document) {
  ContextLifecycleObserver::SetContext(&GetDocument());
  PageVisibilityObserver::SetContext(GetDocument().GetPage());
  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLCanvasElement::WillDrawImageTo2DContext(CanvasImageSource* source) {
  if (CanvasHeuristicParameters::kEnableAccelerationToAvoidReadbacks &&
      SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() &&
      source->IsAccelerated() && GetOrCreateImageBuffer() &&
      !GetImageBuffer()->IsAccelerated() &&
      ShouldAccelerate(kIgnoreResourceLimitCriteria)) {
    OpacityMode opacity_mode =
        context_->CreationAttributes().alpha() ? kNonOpaque : kOpaque;
    int msaa_sample_count = 0;
    std::unique_ptr<ImageBufferSurface> surface =
        CreateAcceleratedImageBufferSurface(opacity_mode, &msaa_sample_count);
    if (surface) {
      GetOrCreateImageBuffer()->SetSurface(std::move(surface));
      SetNeedsCompositingUpdate();
    }
  }
}

RefPtr<Image> HTMLCanvasElement::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint hint,
    SnapshotReason reason,
    const FloatSize&) {
  if (!width() || !height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }

  if (!IsPaintable()) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  if (PlaceholderFrame()) {
    *status = kNormalSourceImageStatus;
    return PlaceholderFrame();
  }

  if (!context_) {
    RefPtr<Image> result = CreateTransparentImage(Size());
    *status = result ? kNormalSourceImageStatus : kInvalidSourceImageStatus;
    return result;
  }

  if (context_->GetContextType() ==
      CanvasRenderingContext::kContextImageBitmap) {
    *status = kNormalSourceImageStatus;
    RefPtr<Image> result = context_->GetImage(hint, reason);
    if (!result)
      result = CreateTransparentImage(Size());
    *status = result ? kNormalSourceImageStatus : kInvalidSourceImageStatus;
    return result;
  }

  RefPtr<Image> image;
  // TODO(ccameron): Canvas should produce sRGB images.
  // https://crbug.com/672299
  if (Is3d()) {
    // Because WebGL sources always require making a copy of the back buffer, we
    // use paintRenderingResultsToCanvas instead of getImage in order to keep a
    // cached copy of the backing in the canvas's ImageBuffer.
    RenderingContext()->PaintRenderingResultsToCanvas(kBackBuffer);
    if (GetImageBuffer()) {
      image = GetImageBuffer()->NewImageSnapshot(hint, reason);
    } else {
      image = CreateTransparentImage(Size());
    }
  } else {
    if (CanvasHeuristicParameters::kDisableAccelerationToAvoidReadbacks &&
        !RuntimeEnabledFeatures::Canvas2dFixedRenderingModeEnabled() &&
        hint == kPreferNoAcceleration && GetImageBuffer() &&
        GetImageBuffer()->IsAccelerated()) {
      GetImageBuffer()->DisableAcceleration();
    }
    image = RenderingContext()->GetImage(hint, reason);
    if (!image) {
      image = CreateTransparentImage(Size());
    }
  }

  if (image) {
    *status = kNormalSourceImageStatus;
  } else {
    *status = kInvalidSourceImageStatus;
  }
  return image;
}

bool HTMLCanvasElement::WouldTaintOrigin(SecurityOrigin*) const {
  return !OriginClean();
}

FloatSize HTMLCanvasElement::ElementSize(const FloatSize&) const {
  if (context_ && context_->GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap) {
    RefPtr<Image> image =
        context_->GetImage(kPreferNoAcceleration, kSnapshotReasonDrawImage);
    if (image)
      return FloatSize(image->width(), image->height());
    return FloatSize(0, 0);
  }
  if (PlaceholderFrame())
    return FloatSize(PlaceholderFrame()->Size());
  return FloatSize(width(), height());
}

IntSize HTMLCanvasElement::BitmapSourceSize() const {
  return IntSize(width(), height());
}

ScriptPromise HTMLCanvasElement::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  DCHECK(event_target.ToLocalDOMWindow());

  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(this, crop_rect, options));
}

void HTMLCanvasElement::SetPlaceholderFrame(
    RefPtr<StaticBitmapImage> image,
    WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
    RefPtr<WebTaskRunner> task_runner,
    unsigned resource_id) {
  OffscreenCanvasPlaceholder::SetPlaceholderFrame(
      std::move(image), std::move(dispatcher), std::move(task_runner),
      resource_id);
  IntSize new_size(PlaceholderFrame()->width(), PlaceholderFrame()->height());
  SetSize(new_size);
  NotifyListenersCanvasChanged();
}

bool HTMLCanvasElement::IsOpaque() const {
  return context_ && !context_->CreationAttributes().alpha();
}

bool HTMLCanvasElement::IsSupportedInteractiveCanvasFallback(
    const Element& element) {
  if (!element.IsDescendantOf(this))
    return false;

  // An element is a supported interactive canvas fallback element if it is one
  // of the following:
  // https://html.spec.whatwg.org/multipage/scripting.html#supported-interactive-canvas-fallback-element

  // An a element that represents a hyperlink and that does not have any img
  // descendants.
  if (IsHTMLAnchorElement(element))
    return !Traversal<HTMLImageElement>::FirstWithin(element);

  // A button element
  if (IsHTMLButtonElement(element))
    return true;

  // An input element whose type attribute is in one of the Checkbox or Radio
  // Button states.  An input element that is a button but its type attribute is
  // not in the Image Button state.
  if (auto* input_element = ToHTMLInputElementOrNull(element)) {
    if (input_element->type() == InputTypeNames::checkbox ||
        input_element->type() == InputTypeNames::radio ||
        input_element->IsTextButton())
      return true;
  }

  // A select element with a "multiple" attribute or with a display size greater
  // than 1.
  if (auto* select_element = ToHTMLSelectElementOrNull(element)) {
    if (select_element->IsMultiple() || select_element->size() > 1)
      return true;
  }

  // An option element that is in a list of options of a select element with a
  // "multiple" attribute or with a display size greater than 1.
  if (IsHTMLOptionElement(element) && element.parentNode() &&
      IsHTMLSelectElement(*element.parentNode())) {
    const HTMLSelectElement& select_element =
        ToHTMLSelectElement(*element.parentNode());
    if (select_element.IsMultiple() || select_element.size() > 1)
      return true;
  }

  // An element that would not be interactive content except for having the
  // tabindex attribute specified.
  if (element.FastHasAttribute(HTMLNames::tabindexAttr))
    return true;

  // A non-interactive table, caption, thead, tbody, tfoot, tr, td, or th
  // element.
  if (IsHTMLTableElement(element) ||
      element.HasTagName(HTMLNames::captionTag) ||
      element.HasTagName(HTMLNames::theadTag) ||
      element.HasTagName(HTMLNames::tbodyTag) ||
      element.HasTagName(HTMLNames::tfootTag) ||
      element.HasTagName(HTMLNames::trTag) ||
      element.HasTagName(HTMLNames::tdTag) ||
      element.HasTagName(HTMLNames::thTag))
    return true;

  return false;
}

HitTestCanvasResult* HTMLCanvasElement::GetControlAndIdIfHitRegionExists(
    const LayoutPoint& location) {
  if (Is2d())
    return context_->GetControlAndIdIfHitRegionExists(location);
  return HitTestCanvasResult::Create(String(), nullptr);
}

String HTMLCanvasElement::GetIdFromControl(const Element* element) {
  if (context_)
    return context_->GetIdFromControl(element);
  return String();
}

void HTMLCanvasElement::CreateLayer() {
  DCHECK(!surface_layer_bridge_);
  LocalFrame* frame = GetDocument().GetFrame();
  WebLayerTreeView* layer_tree_view = nullptr;
  // TODO(xlai): Ensure OffscreenCanvas commit() is still functional when a
  // frame-less HTML canvas's document is reparenting under another frame.
  // See crbug.com/683172.
  if (frame) {
    layer_tree_view =
        frame->GetPage()->GetChromeClient().GetWebLayerTreeView(frame);
    surface_layer_bridge_ =
        WTF::MakeUnique<::blink::SurfaceLayerBridge>(layer_tree_view, this);
    // Creates a placeholder layer first before Surface is created.
    surface_layer_bridge_->CreateSolidColorLayer();
  }
}

void HTMLCanvasElement::OnWebLayerReplaced() {
  GraphicsLayer::RegisterContentsLayer(surface_layer_bridge_->GetWebLayer());
  SetNeedsCompositingUpdate();
}

FontSelector* HTMLCanvasElement::GetFontSelector() {
  return GetDocument().GetStyleEngine().GetFontSelector();
}

}  // namespace blink
