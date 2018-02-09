/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef CanvasRenderingContext2D_h
#define CanvasRenderingContext2D_h

#include "core/html/canvas/CanvasContextCreationAttributesCore.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "core/style/FilterOperations.h"
#include "core/svg/SVGResourceClient.h"
#include "modules/ModulesExport.h"
#include "modules/canvas/canvas2d/BaseRenderingContext2D.h"
#include "modules/canvas/canvas2d/CanvasRenderingContext2DSettings.h"
#include "modules/canvas/canvas2d/CanvasRenderingContext2DState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebThread.h"

namespace blink {
class WebLayer;
}

namespace blink {

class CanvasImageSource;
class Element;
class ExceptionState;
class Font;
class HitRegion;
class HitRegionOptions;
class HitRegionManager;
class HitTestCanvasResult;
class Path2D;
class TextMetrics;

typedef CSSImageValueOrHTMLImageElementOrSVGImageElementOrHTMLVideoElementOrHTMLCanvasElementOrImageBitmapOrOffscreenCanvas
    CanvasImageSourceUnion;

class MODULES_EXPORT CanvasRenderingContext2D final
    : public CanvasRenderingContext,
      public BaseRenderingContext2D,
      public SVGResourceClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(CanvasRenderingContext2D);

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost* host,
        const CanvasContextCreationAttributesCore& attrs) override {
      DCHECK(!host->IsOffscreenCanvas());
      return new CanvasRenderingContext2D(static_cast<HTMLCanvasElement*>(host),
                                          attrs);
    }
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContext2d;
    }
  };

  ~CanvasRenderingContext2D() override;

  HTMLCanvasElement* canvas() const {
    DCHECK(!Host() || !Host()->IsOffscreenCanvas());
    return static_cast<HTMLCanvasElement*>(Host());
  }
  void SetCanvasGetContextResult(RenderingContext&) final;

  bool isContextLost() const override;

  bool ShouldAntialias() const override;
  void SetShouldAntialias(bool) override;

  void scrollPathIntoView();
  void scrollPathIntoView(Path2D*);

  void clearRect(double x, double y, double width, double height);
  void ClearRect(double x, double y, double width, double height) override {
    clearRect(x, y, width, height);
  }

  void Reset() override;

  String font() const;
  void setFont(const String&) override;

  String direction() const;
  void setDirection(const String&);

  void fillText(const String& text, double x, double y);
  void fillText(const String& text, double x, double y, double max_width);
  void strokeText(const String& text, double x, double y);
  void strokeText(const String& text, double x, double y, double max_width);
  TextMetrics* measureText(const String& text);

  void getContextAttributes(CanvasRenderingContext2DSettings&) const;

  void drawFocusIfNeeded(Element*);
  void drawFocusIfNeeded(Path2D*, Element*);

  void addHitRegion(const HitRegionOptions&, ExceptionState&);
  void removeHitRegion(const String& id);
  void clearHitRegions();
  HitRegion* HitRegionAtPoint(const FloatPoint&);
  unsigned HitRegionsCount() const override;

  void LoseContext(LostContextMode) override;
  void DidSetSurfaceSize() override;

  void RestoreCanvasMatrixClipStack(PaintCanvas*) const override;

  // TaskObserver implementation
  void DidProcessTask() final;

  void StyleDidChange(const ComputedStyle* old_style,
                      const ComputedStyle& new_style) override;
  HitTestCanvasResult* GetControlAndIdIfHitRegionExists(
      const LayoutPoint& location) override;
  String GetIdFromControl(const Element*) override;

  // SVGResourceClient implementation
  TreeScope* GetTreeScope() override;
  void ResourceContentChanged() override;
  void ResourceElementChanged() override;

  void UpdateFilterReferences(const FilterOperations&);
  void ClearFilterReferences();

  // BaseRenderingContext2D implementation
  bool OriginClean() const final;
  void SetOriginTainted() final;
  bool WouldTaintOrigin(CanvasImageSource* source,
                        ExecutionContext* execution_context) final {
    return CanvasRenderingContext::WouldTaintOrigin(
        source, execution_context->GetSecurityOrigin());
  }
  void DisableAcceleration() override;
  void DidInvokeGPUReadbackInCurrentFrame() override;

  int Width() const final;
  int Height() const final;

  bool HasCanvas2DBuffer() const final;
  bool CanCreateCanvas2DBuffer() const final;

  bool ParseColorOrCurrentColor(Color&, const String& color_string) const final;

  PaintCanvas* DrawingCanvas() const final;
  PaintCanvas* ExistingDrawingCanvas() const final;
  void DisableDeferral(DisableDeferralReason) final;

  void DidDraw(const SkIRect& dirty_rect) final;
  scoped_refptr<StaticBitmapImage> GetImage(AccelerationHint) const final;

  bool StateHasFilter() final;
  sk_sp<PaintFilter> StateGetFilter() final;
  void SnapshotStateForFilter() final;

  void ValidateStateStack() const final;

  void FinalizeFrame() override { usage_counters_.num_frames_since_reset++; }

  bool IsPaintable() const final { return HasCanvas2DBuffer(); }

  void WillDrawImage(CanvasImageSource*) const final;

  virtual void Trace(blink::Visitor*);

 protected:
  virtual void NeedsFinalizeFrame() {
    CanvasRenderingContext::NeedsFinalizeFrame();
  }

  CanvasColorParams ColorParams() const override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;
  void WillOverwriteCanvas() override;

 private:
  friend class CanvasRenderingContext2DAutoRestoreSkCanvas;

  CanvasRenderingContext2D(HTMLCanvasElement*,
                           const CanvasContextCreationAttributesCore&);
  void DispatchContextLostEvent(TimerBase*);
  void DispatchContextRestoredEvent(TimerBase*);
  void TryRestoreContextEvent(TimerBase*);

  void PruneLocalFontCache(size_t target_size);

  void ScrollPathIntoViewInternal(const Path&);

  void DrawTextInternal(const String&,
                        double x,
                        double y,
                        CanvasRenderingContext2DState::PaintType,
                        double* max_width = nullptr);

  const Font& AccessFont();

  void DrawFocusIfNeededInternal(const Path&, Element*);
  bool FocusRingCallIsValid(const Path&, Element*);
  void DrawFocusRing(const Path&);
  void UpdateElementAccessibility(const Path&, Element*);

  CanvasRenderingContext::ContextType GetContextType() const override {
    return CanvasRenderingContext::kContext2d;
  }

  String ColorSpaceAsString() const override;
  CanvasPixelFormat PixelFormat() const override;

  bool Is2d() const override { return true; }
  bool IsComposited() const override;
  bool IsAccelerated() const override;
  bool HasAlpha() const override { return CreationAttributes().alpha; }
  void SetIsHidden(bool) override;
  void Stop() final;

  virtual bool IsTransformInvertible() const;

  WebLayer* PlatformLayer() const override;
  bool IsCanvas2DBufferValid() const override;

  Member<HitRegionManager> hit_region_manager_;
  LostContextMode context_lost_mode_;
  bool context_restorable_;
  unsigned try_restore_context_attempt_count_;
  TaskRunnerTimer<CanvasRenderingContext2D> dispatch_context_lost_event_timer_;
  TaskRunnerTimer<CanvasRenderingContext2D>
      dispatch_context_restored_event_timer_;
  TaskRunnerTimer<CanvasRenderingContext2D> try_restore_context_event_timer_;

  FilterOperations filter_operations_;
  HashMap<String, Font> fonts_resolved_using_current_style_;
  bool should_prune_local_font_cache_;
  ListHashSet<String> font_lru_list_;
};

DEFINE_TYPE_CASTS(CanvasRenderingContext2D,
                  CanvasRenderingContext,
                  context,
                  context->Is2d() && context->Host(),
                  context.Is2d() && context.Host());

}  // namespace blink

#endif  // CanvasRenderingContext2D_h
