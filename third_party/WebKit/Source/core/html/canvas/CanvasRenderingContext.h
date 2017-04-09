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
#include "platform/graphics/ColorBehavior.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/StringHash.h"

namespace blink {

class CanvasImageSource;
class HTMLCanvasElement;
class ImageData;
class ImageBitmap;
class WebLayer;

enum CanvasColorSpace {
  kLegacyCanvasColorSpace,
  kSRGBCanvasColorSpace,
  kRec2020CanvasColorSpace,
  kP3CanvasColorSpace,
};

enum CanvasPixelFormat {
  kRGBA8CanvasPixelFormat,
  kRGB10A2CanvasPixelFormat,
  kRGBA12CanvasPixelFormat,
  kF16CanvasPixelFormat,
};

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

  HTMLCanvasElement* canvas() const { return canvas_; }

  CanvasColorSpace ColorSpace() const { return color_space_; };
  WTF::String ColorSpaceAsString() const;
  CanvasPixelFormat PixelFormat() const { return pixel_format_; };
  WTF::String PixelFormatAsString() const;
  bool LinearPixelMath() const { return linear_pixel_math_; };

  // The color space in which the the content should be interpreted by the
  // compositor. This is always defined.
  gfx::ColorSpace GfxColorSpace() const;
  // The color space that should be used for SkSurface creation. This may
  // be nullptr.
  sk_sp<SkColorSpace> SkSurfaceColorSpace() const;
  SkColorType ColorType() const;
  ColorBehavior ColorBehaviorForMediaDrawnToCanvas() const;
  bool SkSurfacesUseColorSpace() const;

  virtual PassRefPtr<Image> GetImage(AccelerationHint,
                                     SnapshotReason) const = 0;
  virtual ImageData* ToImageData(SnapshotReason reason) { return nullptr; }
  virtual ContextType GetContextType() const = 0;
  virtual bool IsComposited() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool ShouldAntialias() const { return false; }
  virtual void SetIsHidden(bool) = 0;
  virtual bool isContextLost() const { return true; }
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
  virtual bool IsAccelerationOptimalForCanvasContent() const { return true; }
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
  OffscreenCanvas* offscreenCanvas() const { return offscreen_canvas_; }
  virtual ImageBitmap* TransferToImageBitmap(ScriptState*) { return nullptr; }

  bool WouldTaintOrigin(CanvasImageSource*, SecurityOrigin* = nullptr);
  void DidMoveToNewDocument(Document*);

  void DetachCanvas() { canvas_ = nullptr; }
  void DetachOffscreenCanvas() { offscreen_canvas_ = nullptr; }

  const CanvasContextCreationAttributes& CreationAttributes() const {
    return creation_attributes_;
  }

 protected:
  CanvasRenderingContext(HTMLCanvasElement*,
                         OffscreenCanvas*,
                         const CanvasContextCreationAttributes&);
  DECLARE_VIRTUAL_TRACE();
  virtual void Stop() = 0;

 private:
  void Dispose();

  Member<HTMLCanvasElement> canvas_;
  Member<OffscreenCanvas> offscreen_canvas_;
  HashSet<String> clean_ur_ls_;
  HashSet<String> dirty_ur_ls_;
  CanvasColorSpace color_space_;
  CanvasPixelFormat pixel_format_;
  bool linear_pixel_math_ = false;
  CanvasContextCreationAttributes creation_attributes_;
  bool finalize_frame_scheduled_ = false;
};

}  // namespace blink

#endif
