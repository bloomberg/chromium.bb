/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef CanvasRenderingContext_h
#define CanvasRenderingContext_h

#include "core/CoreExport.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/StringHash.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

class CanvasImageSource;
class HTMLCanvasElement;
class ImageData;
class ImageBitmap;
class WebLayer;

constexpr const char* kLegacyCanvasColorSpaceName = "legacy-srgb";
constexpr const char* kSRGBCanvasColorSpaceName = "srgb";
constexpr const char* kRec2020CanvasColorSpaceName = "rec2020";
constexpr const char* kP3CanvasColorSpaceName = "p3";

constexpr const char* kRGBA8CanvasPixelFormatName = "8-8-8-8";
constexpr const char* kRGB10A2CanvasPixelFormatName = "10-10-10-2";
constexpr const char* kRGBA12CanvasPixelFormatName = "12-12-12-12";
constexpr const char* kF16CanvasPixelFormatName = "float16";

class CORE_EXPORT CanvasRenderingContext
    : public GarbageCollectedFinalized<CanvasRenderingContext>,
      public ScriptWrappable,
      public WebThread::TaskObserver {
  WTF_MAKE_NONCOPYABLE(CanvasRenderingContext);
  USING_PRE_FINALIZER(CanvasRenderingContext, Dispose);

 public:
  virtual ~CanvasRenderingContext() {}

  // A Canvas can either be "2D" or "webgl" but never both. If you request a 2D
  // canvas and the existing context is already 2D, just return that. If the
  // existing context is WebGL, then destroy it before creating a new 2D
  // context. Vice versa when requesting a WebGL canvas. Requesting a context
  // with any other type string will destroy any existing context.
  enum ContextType {
    // Do not change assigned numbers of existing items: add new features to the
    // end of the list.
    kContext2d = 0,
    kContextExperimentalWebgl = 2,
    kContextWebgl = 3,
    kContextWebgl2 = 4,
    kContextImageBitmap = 5,
    kContextTypeCount,
  };

  static ContextType ContextTypeFromId(const String& id);
  static ContextType ResolveContextTypeAliases(ContextType);

  CanvasRenderingContextHost* host() const { return host_; }

  WTF::String ColorSpaceAsString() const;
  WTF::String PixelFormatAsString() const;

  const CanvasColorParams& color_params() const { return color_params_; }

  virtual RefPtr<StaticBitmapImage> GetImage(AccelerationHint,
                                             SnapshotReason) const = 0;
  virtual ImageData* ToImageData(SnapshotReason reason) { return nullptr; }
  virtual ContextType GetContextType() const = 0;
  virtual bool IsComposited() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool ShouldAntialias() const { return false; }
  virtual void SetIsHidden(bool) = 0;
  virtual bool isContextLost() const { return true; }
  // TODO(fserb): remove SetCanvasGetContextResult.
  virtual void SetCanvasGetContextResult(RenderingContext&) { NOTREACHED(); };
  virtual void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) {
    NOTREACHED();
  }
  virtual bool IsPaintable() const = 0;
  virtual void DidDraw(const SkIRect& dirty_rect);
  virtual void DidDraw();

  // Return true if the content is updated.
  virtual bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) {
    return false;
  }

  virtual WebLayer* PlatformLayer() const { return nullptr; }

  enum LostContextMode {
    kNotLostContext,

    // Lost context occurred at the graphics system level.
    kRealLostContext,

    // Lost context provoked by WEBGL_lose_context.
    kWebGLLoseContextLostContext,

    // Lost context occurred due to internal implementation reasons.
    kSyntheticLostContext,
  };
  virtual void LoseContext(LostContextMode) {}

  // This method gets called at the end of script tasks that modified
  // the contents of the canvas (called didDraw). It marks the completion
  // of a presentable frame.
  virtual void FinalizeFrame() {}

  void NeedsFinalizeFrame();

  // WebThread::TaskObserver implementation
  void DidProcessTask() override;
  void WillProcessTask() final {}

  // Canvas2D-specific interface
  virtual bool Is2d() const { return false; }
  virtual void RestoreCanvasMatrixClipStack(PaintCanvas*) const {}
  virtual void Reset() {}
  virtual void clearRect(double x, double y, double width, double height) {}
  virtual void DidSetSurfaceSize() {}
  virtual void SetShouldAntialias(bool) {}
  virtual unsigned HitRegionsCount() const { return 0; }
  virtual void setFont(const String&) {}
  virtual void StyleDidChange(const ComputedStyle* old_style,
                              const ComputedStyle& new_style) {}
  virtual HitTestCanvasResult* GetControlAndIdIfHitRegionExists(
      const LayoutPoint& location) {
    NOTREACHED();
    return HitTestCanvasResult::Create(String(), nullptr);
  }
  virtual String GetIdFromControl(const Element* element) { return String(); }
  virtual void ResetUsageTracking(){};

  // WebGL-specific interface
  virtual bool Is3d() const { return false; }
  virtual void SetFilterQuality(SkFilterQuality) { NOTREACHED(); }
  virtual void Reshape(int width, int height) { NOTREACHED(); }
  virtual void MarkLayerComposited() { NOTREACHED(); }
  virtual ImageData* PaintRenderingResultsToImageData(SourceDrawingBuffer) {
    NOTREACHED();
    return nullptr;
  }
  virtual int ExternallyAllocatedBytesPerPixel() {
    NOTREACHED();
    return 0;
  }

  // ImageBitmap-specific interface
  virtual bool Paint(GraphicsContext&, const IntRect&) { return false; }

  // OffscreenCanvas-specific methods
  virtual ImageBitmap* TransferToImageBitmap(ScriptState*) { return nullptr; }

  bool WouldTaintOrigin(CanvasImageSource*, SecurityOrigin*);
  void DidMoveToNewDocument(Document*);

  void DetachHost() { host_ = nullptr; }

  const CanvasContextCreationAttributes& CreationAttributes() const {
    return creation_attributes_;
  }

 protected:
  CanvasRenderingContext(CanvasRenderingContextHost*,
                         const CanvasContextCreationAttributes&);
  DECLARE_VIRTUAL_TRACE();
  virtual void Stop() = 0;

 private:
  void Dispose();

  Member<CanvasRenderingContextHost> host_;
  HashSet<String> clean_urls_;
  HashSet<String> dirty_urls_;
  CanvasColorParams color_params_;
  CanvasContextCreationAttributes creation_attributes_;
  bool finalize_frame_scheduled_ = false;
};

}  // namespace blink

#endif
