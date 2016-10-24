// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvas_h
#define OffscreenCanvas_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
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
    : public GarbageCollectedFinalized<OffscreenCanvas>,
      public ScriptWrappable,
      public CanvasImageSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OffscreenCanvas* create(unsigned width, unsigned height);

  // IDL attributes
  unsigned width() const { return m_size.width(); }
  unsigned height() const { return m_size.height(); }
  void setWidth(unsigned, ExceptionState&);
  void setHeight(unsigned, ExceptionState&);

  // API Methods
  ImageBitmap* transferToImageBitmap(ExceptionState&);
  ScriptPromise convertToBlob(ScriptState*,
                              const ImageEncodeOptions&,
                              ExceptionState&);

  IntSize size() const { return m_size; }
  void setAssociatedCanvasId(int canvasId) { m_canvasId = canvasId; }
  int getAssociatedCanvasId() const { return m_canvasId; }
  bool isNeutered() const { return m_isNeutered; }
  void setNeutered();
  CanvasRenderingContext* getCanvasRenderingContext(
      ScriptState*,
      const String&,
      const CanvasContextCreationAttributes&);
  CanvasRenderingContext* renderingContext() { return m_context; }

  static void registerRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);

  bool originClean() const;
  void setOriginTainted() { m_originClean = false; }
  // TODO(crbug.com/630356): apply the flag to WebGL context as well
  void setDisableReadingFromCanvasTrue() { m_disableReadingFromCanvas = true; }

  OffscreenCanvasFrameDispatcher* getOrCreateFrameDispatcher();

  void setSurfaceId(uint32_t clientId,
                    uint32_t sinkId,
                    uint32_t localId,
                    uint64_t nonce) {
    m_clientId = clientId;
    m_sinkId = sinkId;
    m_localId = localId;
    m_nonce = nonce;
  }
  uint32_t clientId() const { return m_clientId; }
  uint32_t sinkId() const { return m_sinkId; }
  uint32_t localId() const { return m_localId; }
  uint64_t nonce() const { return m_nonce; }

  // CanvasImageSource implementation
  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const final;
  bool wouldTaintOrigin(SecurityOrigin*) const final { return !m_originClean; }
  bool isOffscreenCanvas() const final { return true; }
  FloatSize elementSize(const FloatSize& defaultObjectSize) const final {
    return FloatSize(width(), height());
  }
  bool isOpaque() const final;
  bool isAccelerated() const final;
  int sourceWidth() final { return width(); }
  int sourceHeight() final { return height(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit OffscreenCanvas(const IntSize&);

  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& renderingContextFactories();
  static CanvasRenderingContextFactory* getRenderingContextFactory(int);

  Member<CanvasRenderingContext> m_context;
  int m_canvasId = -1;  // DOMNodeIds starts from 0, using -1 to indicate no
                        // associated canvas element.

  IntSize m_size;
  bool m_isNeutered = false;

  bool m_originClean;
  bool m_disableReadingFromCanvas = false;

  bool isPaintable() const;

  std::unique_ptr<OffscreenCanvasFrameDispatcher> m_frameDispatcher;
  // cc::SurfaceId is broken into three integer components as this can be used
  // in transfer of OffscreenCanvas across threads
  // If this object is not created via
  // HTMLCanvasElement.transferControlToOffscreen(),
  // then the following members would remain as initialized zero values.
  uint32_t m_clientId = 0;
  uint32_t m_sinkId = 0;
  uint32_t m_localId = 0;
  uint64_t m_nonce = 0;
};

}  // namespace blink

#endif  // OffscreenCanvas_h
