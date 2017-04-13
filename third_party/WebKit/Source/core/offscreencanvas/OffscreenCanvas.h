// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvas_h
#define OffscreenCanvas_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/offscreencanvas/ImageEncodeOptions.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "platform/heap/Handle.h"
#include <memory>

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
      public OffscreenCanvasFrameDispatcherClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(OffscreenCanvas, Dispose);

 public:
  static OffscreenCanvas* Create(unsigned width, unsigned height);
  ~OffscreenCanvas() override;
  void Dispose();

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

  IntSize Size() const { return size_; }
  void SetSize(const IntSize&);

  void SetPlaceholderCanvasId(int canvas_id) {
    placeholder_canvas_id_ = canvas_id;
  }
  int PlaceholderCanvasId() const { return placeholder_canvas_id_; }
  bool HasPlaceholderCanvas() {
    return placeholder_canvas_id_ != kNoPlaceholderCanvas;
  }
  bool IsNeutered() const { return is_neutered_; }
  void SetNeutered();
  CanvasRenderingContext* GetCanvasRenderingContext(
      ScriptState*,
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

  void SetFrameSinkId(uint32_t client_id, uint32_t sink_id) {
    client_id_ = client_id;
    sink_id_ = sink_id;
  }
  uint32_t ClientId() const { return client_id_; }
  uint32_t SinkId() const { return sink_id_; }

  void SetExecutionContext(ExecutionContext* context) {
    execution_context_ = context;
  }

  ScriptPromise Commit(RefPtr<StaticBitmapImage>,
                       bool is_web_gl_software_rendering,
                       ScriptState*);
  void FinalizeFrame();

  void DetachContext() { context_ = nullptr; }

  // OffscreenCanvasFrameDispatcherClient implementation
  void BeginFrame() final;

  // EventTarget implementation
  const AtomicString& InterfaceName() const final {
    return EventTargetNames::OffscreenCanvas;
  }
  ExecutionContext* GetExecutionContext() const {
    return execution_context_.Get();
  }

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const final;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect>,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) final;

  // CanvasImageSource implementation
  PassRefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const final;
  bool WouldTaintOrigin(SecurityOrigin*) const final { return !origin_clean_; }
  bool IsOffscreenCanvas() const final { return true; }
  FloatSize ElementSize(const FloatSize& default_object_size) const final {
    return FloatSize(width(), height());
  }
  bool IsOpaque() const final;
  bool IsAccelerated() const final;
  int SourceWidth() final { return width(); }
  int SourceHeight() final { return height(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit OffscreenCanvas(const IntSize&);
  OffscreenCanvasFrameDispatcher* GetOrCreateFrameDispatcher();
  void DoCommit(RefPtr<StaticBitmapImage>, bool is_web_gl_software_rendering);
  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& RenderingContextFactories();
  static CanvasRenderingContextFactory* GetRenderingContextFactory(int);

  Member<CanvasRenderingContext> context_;
  WeakMember<ExecutionContext> execution_context_;

  enum {
    kNoPlaceholderCanvas = -1,  // DOMNodeIds starts from 0, using -1 to
                                // indicate no associated canvas element.
  };
  int placeholder_canvas_id_ = kNoPlaceholderCanvas;

  IntSize size_;
  bool is_neutered_ = false;

  bool origin_clean_ = true;
  bool disable_reading_from_canvas_ = false;

  bool IsPaintable() const;

  std::unique_ptr<OffscreenCanvasFrameDispatcher> frame_dispatcher_;

  Member<ScriptPromiseResolver> commit_promise_resolver_;
  RefPtr<StaticBitmapImage> current_frame_;
  bool current_frame_is_web_gl_software_rendering_ = false;

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
