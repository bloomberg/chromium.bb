// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvas_h
#define OffscreenCanvas_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMNodeIds.h"
#include "core/events/EventTarget.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/html/canvas/CanvasRenderingContextHost.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/offscreencanvas/ImageEncodeOptions.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "platform/heap/Handle.h"

namespace blink {

class CanvasContextCreationAttributes;
class ImageBitmap;
class
    OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext;
typedef OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext
    OffscreenRenderingContext;

class CORE_EXPORT OffscreenCanvas final
    : public EventTargetWithInlineData,
      public CanvasImageSource,
      public ImageBitmapSource,
      public CanvasRenderingContextHost,
      public OffscreenCanvasFrameDispatcherClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(OffscreenCanvas);
  USING_PRE_FINALIZER(OffscreenCanvas, Dispose);

 public:
  static OffscreenCanvas* Create(unsigned width, unsigned height);
  ~OffscreenCanvas() override;
  void Dispose();

  bool IsOffscreenCanvas() const override { return true; }
  // IDL attributes
  unsigned width() const { return size_.Width(); }
  unsigned height() const { return size_.Height(); }
  void setWidth(unsigned);
  void setHeight(unsigned);

  // API Methods
  ImageBitmap* transferToImageBitmap(ScriptState*, ExceptionState&);
  ScriptPromise convertToBlob(ScriptState*,
                              const ImageEncodeOptions&,
                              ExceptionState&);

  const IntSize& Size() const override { return size_; }
  void SetSize(const IntSize&);

  void SetPlaceholderCanvasId(DOMNodeId canvas_id) {
    placeholder_canvas_id_ = canvas_id;
  }
  DOMNodeId PlaceholderCanvasId() const { return placeholder_canvas_id_; }
  bool HasPlaceholderCanvas() {
    return placeholder_canvas_id_ != kInvalidDOMNodeId;
  }
  bool IsNeutered() const { return is_neutered_; }
  void SetNeutered();
  CanvasRenderingContext* GetCanvasRenderingContext(
      ExecutionContext*,
      const String&,
      const CanvasContextCreationAttributes&);
  CanvasRenderingContext* RenderingContext() { return context_; }

  static void RegisterRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);

  bool OriginClean() const;
  void SetOriginTainted() { origin_clean_ = false; }
  // TODO(crbug.com/630356): apply the flag to WebGL context as well
  void SetDisableReadingFromCanvasTrue() {
    disable_reading_from_canvas_ = true;
  }

  void DiscardImageBuffer() override;
  ImageBuffer* GetImageBuffer() const { return image_buffer_.get(); }
  ImageBuffer* GetOrCreateImageBuffer() override;

  void SetFrameSinkId(uint32_t client_id, uint32_t sink_id) {
    client_id_ = client_id;
    sink_id_ = sink_id;
  }
  uint32_t ClientId() const { return client_id_; }
  uint32_t SinkId() const { return sink_id_; }

  ScriptPromise Commit(RefPtr<StaticBitmapImage>,
                       const SkIRect& damage_rect,
                       bool is_web_gl_software_rendering,
                       ScriptState*,
                       ExceptionState&) override;
  void FinalizeFrame() override;

  void DetachContext() override { context_ = nullptr; }

  // OffscreenCanvasFrameDispatcherClient implementation
  void BeginFrame() final;

  // EventTarget implementation
  const AtomicString& InterfaceName() const final {
    return EventTargetNames::OffscreenCanvas;
  }
  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }

  const KURL& GetExecutionContextUrl() const override {
    return GetExecutionContext()->Url();
  }

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const final;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect>,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) final;

  // CanvasImageSource implementation
  RefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                        AccelerationHint,
                                        SnapshotReason,
                                        const FloatSize&) final;
  bool WouldTaintOrigin(SecurityOrigin*) const final { return !origin_clean_; }
  FloatSize ElementSize(const FloatSize& default_object_size) const final {
    return FloatSize(width(), height());
  }
  bool IsOpaque() const final;
  bool IsAccelerated() const final;
  int SourceWidth() final { return width(); }
  int SourceHeight() final { return height(); }

  DispatchEventResult HostDispatchEvent(Event* event) {
    return DispatchEvent(event);
  }

  bool IsWebGLAllowed() const override { return true; }

  FontSelector* GetFontSelector() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class OffscreenCanvasTest;
  explicit OffscreenCanvas(const IntSize&);
  OffscreenCanvasFrameDispatcher* GetOrCreateFrameDispatcher();
  void DoCommit();
  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& RenderingContextFactories();
  static CanvasRenderingContextFactory* GetRenderingContextFactory(int);

  Member<CanvasRenderingContext> context_;
  WeakMember<ExecutionContext> execution_context_;

  DOMNodeId placeholder_canvas_id_ = kInvalidDOMNodeId;

  IntSize size_;
  bool is_neutered_ = false;

  bool origin_clean_ = true;
  bool disable_reading_from_canvas_ = false;

  bool IsPaintable() const;

  std::unique_ptr<OffscreenCanvasFrameDispatcher> frame_dispatcher_;

  Member<ScriptPromiseResolver> commit_promise_resolver_;
  RefPtr<StaticBitmapImage> current_frame_;
  bool current_frame_is_web_gl_software_rendering_ = false;
  SkIRect current_frame_damage_rect_;

  std::unique_ptr<ImageBuffer> image_buffer_;
  bool needs_matrix_clip_restore_ = false;

  // cc::FrameSinkId is broken into two integer components as this can be used
  // in transfer of OffscreenCanvas across threads
  // If this object is not created via
  // HTMLCanvasElement.transferControlToOffscreen(),
  // then the following members would remain as initialized zero values.
  uint32_t client_id_ = 0;
  uint32_t sink_id_ = 0;
};

}  // namespace blink

#endif  // OffscreenCanvas_h
