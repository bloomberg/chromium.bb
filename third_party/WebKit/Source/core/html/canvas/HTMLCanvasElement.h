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

#include "base/memory/weak_ptr.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/v8_blob_callback.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
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
#include "platform/graphics/CanvasResourceHost.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/SurfaceLayerBridge.h"
#include "platform/heap/Handle.h"

#define CanvasDefaultInterpolationQuality kInterpolationLow

namespace blink {

class Canvas2DLayerBridge;
class CanvasColorParams;
class CanvasContextCreationAttributesCore;
class CanvasRenderingContext;
class CanvasRenderingContextFactory;
class GraphicsContext;
class HitTestCanvasResult;
class HTMLCanvasElement;
class Image;
class ImageBitmapOptions;
class IntSize;

class
    CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContextOrXRPresentationContext;
typedef CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContextOrXRPresentationContext
    RenderingContext;

class CORE_EXPORT HTMLCanvasElement final
    : public HTMLElement,
      public ContextLifecycleObserver,
      public PageVisibilityObserver,
      public CanvasImageSource,
      public CanvasRenderingContextHost,
      public WebSurfaceLayerBridgeObserver,
      public ImageBitmapSource,
      public OffscreenCanvasPlaceholder,
      public CanvasResourceHost {
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
      const CanvasContextCreationAttributesCore&);

  String toDataURL(const String& mime_type,
                   const ScriptValue& quality_argument,
                   ExceptionState&) const;
  String toDataURL(const String& mime_type,
                   ExceptionState& exception_state) const {
    return toDataURL(mime_type, ScriptValue(), exception_state);
  }

  void toBlob(V8BlobCallback*,
              const String& mime_type,
              const ScriptValue& quality_argument,
              ExceptionState&);
  void toBlob(V8BlobCallback* callback,
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

  void DisableDeferral(DisableDeferralReason);

  CanvasRenderingContext* RenderingContext() const override {
    return context_.Get();
  }

  scoped_refptr<Image> CopiedImage(SourceDrawingBuffer, AccelerationHint);
  void ClearCopiedImage();

  bool OriginClean() const;
  void SetOriginTainted() { origin_clean_ = false; }

  bool Is3d() const;
  bool Is2d() const;
  bool IsAnimated2d() const;

  Canvas2DLayerBridge* GetCanvas2DLayerBridge() {
    return canvas2d_bridge_.get();
  }
  Canvas2DLayerBridge* GetOrCreateCanvas2DLayerBridge();
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderForWebGL();
  CanvasResourceProvider* ResourceProviderForWebGL() const {
    return webgl_resource_provider_.get();
  }
  void DiscardResourceProvider() override;

  FontSelector* GetFontSelector() override;

  bool ShouldBeDirectComposited() const;

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
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) override;
  bool WouldTaintOrigin(const SecurityOrigin*) const override;
  FloatSize ElementSize(const FloatSize&) const override;
  bool IsCanvasElement() const override { return true; }
  bool IsOpaque() const override;
  bool IsAccelerated() const override;

  // SurfaceLayerBridgeObserver implementation
  void OnWebLayerUpdated() override;
  void RegisterContentsLayer(WebLayer*) override;
  void UnregisterContentsLayer(WebLayer*) override;

  // CanvasResourceHost implementation
  void NotifySurfaceInvalid() override;
  void RestoreCanvasMatrixClipStack(PaintCanvas*) const override;
  void SetNeedsCompositingUpdate() override;
  void UpdateMemoryUsage() override;

  void DisableAcceleration(std::unique_ptr<Canvas2DLayerBridge>
                               unaccelerated_bridge_used_for_testing = nullptr);

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions&) override;

  // OffscreenCanvasPlaceholder implementation.
  void SetPlaceholderFrame(scoped_refptr<StaticBitmapImage>,
                           base::WeakPtr<OffscreenCanvasFrameDispatcher>,
                           scoped_refptr<base::SingleThreadTaskRunner>,
                           unsigned resource_id) override;
  virtual void Trace(blink::Visitor*);

  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  void CreateCanvas2DLayerBridgeForTesting(std::unique_ptr<Canvas2DLayerBridge>,
                                           const IntSize&);

  static void RegisterRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);

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

  // Memory Management
  static intptr_t GetGlobalGPUMemoryUsage() { return global_gpu_memory_usage_; }
  static unsigned GetGlobalAcceleratedContextCount() {
    return global_accelerated_context_count_;
  }
  intptr_t GetGPUMemoryUsage() { return gpu_memory_usage_; }
  void DidInvokeGPUReadbackInCurrentFrame() {
    gpu_readback_invoked_in_current_frame_ = true;
  }

  bool NeedsUnbufferedInputEvents() const { return needs_unbuffered_input_; }

  void SetNeedsUnbufferedInputEvents(bool value) {
    needs_unbuffered_input_ = value;
  }

  bool LowLatencyEnabled() const { return !!frame_dispatcher_; }

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

  std::unique_ptr<Canvas2DLayerBridge> CreateAccelerated2dBuffer(
      int* msaa_sample_count);
  std::unique_ptr<Canvas2DLayerBridge> CreateUnaccelerated2dBuffer();
  void CreateCanvas2DLayerBridgeInternal(std::unique_ptr<Canvas2DLayerBridge>);

  void SetSurfaceSize(const IntSize&);

  bool PaintsIntoCanvasBuffer() const;
  CanvasColorParams ColorParams() const;

  scoped_refptr<StaticBitmapImage> ToStaticBitmapImage(SourceDrawingBuffer,
                                                       AccelerationHint) const;

  String ToDataURLInternal(const String& mime_type,
                           const double& quality,
                           SourceDrawingBuffer) const;

  // Returns true if the canvas' context type is inherited from
  // ImageBitmapRenderingContextBase.
  bool HasImageBitmapContext() const;

  HeapHashSet<WeakMember<CanvasDrawListener>> listeners_;

  IntSize size_;

  TraceWrapperMember<CanvasRenderingContext> context_;

  bool ignore_reset_;
  FloatRect dirty_rect_;

  bool origin_clean_;
  bool needs_unbuffered_input_ = false;

  // It prevents repeated attempts in allocating resources after the first
  // attempt failed.
  mutable bool did_fail_to_create_resource_provider_;

  bool HasResourceProvider() {
    return canvas2d_bridge_ || webgl_resource_provider_;
  }

  bool resource_provider_is_clear_;

  // This is only used by canvas with webgl rendering context.
  std::unique_ptr<CanvasResourceProvider> webgl_resource_provider_;

  // Canvas2DLayerBridge is used when canvas has 2d rendering context
  std::unique_ptr<Canvas2DLayerBridge> canvas2d_bridge_;
  void ReplaceExisting2dLayerBridge(std::unique_ptr<Canvas2DLayerBridge>);

  // FIXME: This is temporary for platforms that have to copy the image buffer
  // to render (and for CSSCanvasValue).
  mutable scoped_refptr<Image> copied_image_;

  // Used for OffscreenCanvas that controls this HTML canvas element
  // and for low latency mode.
  std::unique_ptr<::blink::SurfaceLayerBridge> surface_layer_bridge_;

  // Used for low latency mode.
  // TODO: rename to CanvasFrameDispatcher.
  std::unique_ptr<OffscreenCanvasFrameDispatcher> frame_dispatcher_;

  bool did_notify_listeners_for_current_frame_ = false;

  // GPU Memory Management
  static intptr_t global_gpu_memory_usage_;
  static unsigned global_accelerated_context_count_;
  mutable intptr_t gpu_memory_usage_;
  mutable intptr_t externally_allocated_memory_;

  mutable bool gpu_readback_invoked_in_current_frame_;
  int gpu_readback_successive_frames_;
};

}  // namespace blink

#endif  // HTMLCanvasElement_h
