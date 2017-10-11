/*
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include <memory>
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/fileapi/BlobCallback.h"
#include "core/html/HTMLElement.h"
#include "core/html/canvas/CanvasDrawListener.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/html/canvas/CanvasRenderingContextHost.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/page/PageVisibilityObserver.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/SurfaceLayerBridge.h"
#include "platform/heap/Handle.h"

#define CanvasDefaultInterpolationQuality kInterpolationLow

namespace blink {

class AffineTransform;
class CanvasColorParams;
class CanvasContextCreationAttributes;
class CanvasRenderingContext;
class CanvasRenderingContextFactory;
class GraphicsContext;
class HitTestCanvasResult;
class HTMLCanvasElement;
class Image;
class ImageBitmapOptions;
class ImageBuffer;
class ImageBufferSurface;
class ImageData;
class IntSize;

class
    CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContext;
typedef CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContext
    RenderingContext;

class CORE_EXPORT HTMLCanvasElement final
    : public HTMLElement,
      public ContextLifecycleObserver,
      public PageVisibilityObserver,
      public CanvasImageSource,
      public CanvasRenderingContextHost,
      public WebSurfaceLayerBridgeObserver,
      public ImageBufferClient,
      public ImageBitmapSource,
      public OffscreenCanvasPlaceholder {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLCanvasElement);
  USING_PRE_FINALIZER(HTMLCanvasElement, Dispose);

 public:
  using Node::GetExecutionContext;

  DECLARE_NODE_FACTORY(HTMLCanvasElement);
  ~HTMLCanvasElement() override;

  // Attributes and functions exposed to script
  unsigned width() const { return Size().Width(); }
  unsigned height() const { return Size().Height(); }

  const IntSize& Size() const override { return size_; }

  void setWidth(unsigned, ExceptionState&);
  void setHeight(unsigned, ExceptionState&);

  void SetSize(const IntSize& new_size);

  // Called by Document::getCSSCanvasContext as well as above getContext().
  CanvasRenderingContext* GetCanvasRenderingContext(
      const String&,
      const CanvasContextCreationAttributes&);

  bool IsPaintable() const;

  String toDataURL(const String& mime_type,
                   const ScriptValue& quality_argument,
                   ExceptionState&) const;
  String toDataURL(const String& mime_type,
                   ExceptionState& exception_state) const {
    return toDataURL(mime_type, ScriptValue(), exception_state);
  }

  void toBlob(BlobCallback*,
              const String& mime_type,
              const ScriptValue& quality_argument,
              ExceptionState&);
  void toBlob(BlobCallback* callback,
              const String& mime_type,
              ExceptionState& exception_state) {
    return toBlob(callback, mime_type, ScriptValue(), exception_state);
  }

  // Used for canvas capture.
  void AddListener(CanvasDrawListener*);
  void RemoveListener(CanvasDrawListener*);

  // Used for rendering
  void DidDraw(const FloatRect&) override;
  void DidDraw() override;

  void Paint(GraphicsContext&, const LayoutRect&);

  PaintCanvas* DrawingCanvas();
  void DisableDeferral(DisableDeferralReason);
  PaintCanvas* ExistingDrawingCanvas() const;

  CanvasRenderingContext* RenderingContext() const { return context_.Get(); }

  void EnsureUnacceleratedImageBuffer();
  RefPtr<Image> CopiedImage(SourceDrawingBuffer,
                            AccelerationHint,
                            SnapshotReason);
  void ClearCopiedImage();

  bool OriginClean() const;
  void SetOriginTainted() { origin_clean_ = false; }

  AffineTransform BaseTransform() const;

  bool Is3d() const;
  bool Is2d() const;
  bool IsAnimated2d() const;

  void DiscardImageBuffer() override;
  ImageBuffer* GetImageBuffer() const override { return image_buffer_.get(); }
  ImageBuffer* GetOrCreateImageBuffer() override;

  FontSelector* GetFontSelector() override;

  bool ShouldBeDirectComposited() const;

  void PrepareSurfaceForPaintingIfNeeded();

  const AtomicString ImageSourceURL() const override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;

  bool IsDirty() { return !dirty_rect_.IsEmpty(); }

  void DoDeferredPaintInvalidation();

  void FinalizeFrame() override;

  // ContextLifecycleObserver and PageVisibilityObserver implementation
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver implementation
  void PageVisibilityChanged() override;

  // CanvasImageSource implementation
  RefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                        AccelerationHint,
                                        SnapshotReason,
                                        const FloatSize&) override;
  bool WouldTaintOrigin(SecurityOrigin*) const override;
  FloatSize ElementSize(const FloatSize&) const override;
  bool IsCanvasElement() const override { return true; }
  bool IsOpaque() const override;
  bool IsAccelerated() const override;
  int SourceWidth() override { return size_.Width(); }
  int SourceHeight() override { return size_.Height(); }

  // SurfaceLayerBridgeObserver implementation
  void OnWebLayerReplaced() override;

  // ImageBufferClient implementation
  void NotifySurfaceInvalid() override;
  void DidDisableAcceleration() override;
  void RestoreCanvasMatrixClipStack(PaintCanvas*) const override;
  void SetNeedsCompositingUpdate() override;

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions&) override;

  // OffscreenCanvasPlaceholder implementation.
  void SetPlaceholderFrame(RefPtr<StaticBitmapImage>,
                           WeakPtr<OffscreenCanvasFrameDispatcher>,
                           RefPtr<WebTaskRunner>,
                           unsigned resource_id) override;
  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  void CreateImageBufferUsingSurfaceForTesting(
      std::unique_ptr<ImageBufferSurface>);

  static void RegisterRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);
  void UpdateExternallyAllocatedMemory() const;

  void StyleDidChange(const ComputedStyle* old_style,
                      const ComputedStyle& new_style);

  void NotifyListenersCanvasChanged();

  // For Canvas HitRegions
  bool IsSupportedInteractiveCanvasFallback(const Element&);
  HitTestCanvasResult* GetControlAndIdIfHitRegionExists(const LayoutPoint&);
  String GetIdFromControl(const Element*);

  // For OffscreenCanvas that controls this html canvas element
  ::blink::SurfaceLayerBridge* SurfaceLayerBridge() const {
    return surface_layer_bridge_.get();
  }
  void CreateLayer();

  void DetachContext() override { context_ = nullptr; }

  void WillDrawImageTo2DContext(CanvasImageSource*);

  ExecutionContext* GetTopExecutionContext() const override {
    return GetDocument().GetExecutionContext();
  }

  const KURL& GetExecutionContextUrl() const override {
    return GetDocument().TopDocument().Url();
  }

  DispatchEventResult HostDispatchEvent(Event* event) override {
    return DispatchEvent(event);
  }

  bool IsWebGL1Enabled() const override;
  bool IsWebGL2Enabled() const override;
  bool IsWebGLBlocked() const override;

 protected:
  void DidMoveToNewDocument(Document& old_document) override;

 private:
  explicit HTMLCanvasElement(Document&);
  void Dispose();

  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& RenderingContextFactories();
  static CanvasRenderingContextFactory* GetRenderingContextFactory(int);

  enum AccelerationCriteria {
    kNormalAccelerationCriteria,
    kIgnoreResourceLimitCriteria,
  };
  bool ShouldAccelerate(AccelerationCriteria) const;

  void ParseAttribute(const AttributeModificationParams&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  bool AreAuthorShadowsAllowed() const override { return false; }

  void Reset();

  std::unique_ptr<ImageBufferSurface> CreateWebGLImageBufferSurface();
  std::unique_ptr<ImageBufferSurface> CreateAcceleratedImageBufferSurface(
      int* msaa_sample_count);
  std::unique_ptr<ImageBufferSurface> CreateUnacceleratedImageBufferSurface();
  void CreateImageBuffer();
  void CreateImageBufferInternal(
      std::unique_ptr<ImageBufferSurface> external_surface);
  bool ShouldUseDisplayList();

  void SetSurfaceSize(const IntSize&);

  bool PaintsIntoCanvasBuffer() const;
  CanvasColorParams ColorParams() const;

  ImageData* ToImageData(SourceDrawingBuffer, SnapshotReason) const;

  String ToDataURLInternal(const String& mime_type,
                           const double& quality,
                           SourceDrawingBuffer) const;

  HeapHashSet<WeakMember<CanvasDrawListener>> listeners_;

  IntSize size_;

  TraceWrapperMember<CanvasRenderingContext> context_;

  bool ignore_reset_;
  FloatRect dirty_rect_;

  mutable intptr_t externally_allocated_memory_;

  bool origin_clean_;

  // It prevents HTMLCanvasElement::buffer() from continuously re-attempting to
  // allocate an imageBuffer after the first attempt failed.
  mutable bool did_fail_to_create_image_buffer_;
  bool image_buffer_is_clear_;
  std::unique_ptr<ImageBuffer> image_buffer_;

  // FIXME: This is temporary for platforms that have to copy the image buffer
  // to render (and for CSSCanvasValue).
  mutable RefPtr<Image> copied_image_;

  // Used for OffscreenCanvas that controls this HTML canvas element
  std::unique_ptr<::blink::SurfaceLayerBridge> surface_layer_bridge_;

  bool did_notify_listeners_for_current_frame_ = false;
};

}  // namespace blink

#endif  // HTMLCanvasElement_h
