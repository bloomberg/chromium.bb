// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_

#include "base/bind.h"
#include "base/macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/canvas_image_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_canvas_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_canvas_gradient_or_canvas_pattern_or_css_color_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"

namespace blink {

class CanvasImageSource;
class Color;
class Image;
class Path2D;
class V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString;
class V8UnionCanvasFilterOrString;

class MODULES_EXPORT BaseRenderingContext2D : public GarbageCollectedMixin,
                                              public CanvasPath {
 public:
  ~BaseRenderingContext2D() override;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString* strokeStyle()
      const;
  void setStrokeStyle(
      const V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString* style);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void strokeStyle(StringOrCanvasGradientOrCanvasPatternOrCSSColorValue&) const;
  void setStrokeStyle(
      const StringOrCanvasGradientOrCanvasPatternOrCSSColorValue&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString* fillStyle()
      const;
  void setFillStyle(
      const V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString* style);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void fillStyle(StringOrCanvasGradientOrCanvasPatternOrCSSColorValue&) const;
  void setFillStyle(
      const StringOrCanvasGradientOrCanvasPatternOrCSSColorValue&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  double lineWidth() const;
  void setLineWidth(double);

  String lineCap() const;
  void setLineCap(const String&);

  String lineJoin() const;
  void setLineJoin(const String&);

  double miterLimit() const;
  void setMiterLimit(double);

  const Vector<double>& getLineDash() const;
  void setLineDash(const Vector<double>&);

  double lineDashOffset() const;
  void setLineDashOffset(double);

  virtual double shadowOffsetX() const;
  virtual void setShadowOffsetX(double);

  virtual double shadowOffsetY() const;
  virtual void setShadowOffsetY(double);

  virtual double shadowBlur() const;
  virtual void setShadowBlur(double);

  String shadowColor() const;
  void setShadowColor(const String&);

  double globalAlpha() const;
  void setGlobalAlpha(double);

  String globalCompositeOperation() const;
  void setGlobalCompositeOperation(const String&);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  const V8UnionCanvasFilterOrString* filter() const;
  void setFilter(const ExecutionContext* execution_context,
                 const V8UnionCanvasFilterOrString* input);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void filter(StringOrCanvasFilter&) const;
  void setFilter(const ExecutionContext*, StringOrCanvasFilter input);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  void save();
  void restore();
  void reset();

  void scale(double sx, double sy);
  void scale(double sx, double sy, double sz);
  void rotate(double angle_in_radians);
  void rotate3d(double rx, double ry, double rz);
  void rotateAxis(double axisX,
                  double axisY,
                  double axisZ,
                  double angle_in_radians);
  void translate(double tx, double ty);
  void translate(double tx, double ty, double tz);
  void perspective(double length);
  void transform(double m11,
                 double m12,
                 double m21,
                 double m22,
                 double dx,
                 double dy);
  void transform(double m11,
                 double m12,
                 double m13,
                 double m14,
                 double m21,
                 double m22,
                 double m23,
                 double m24,
                 double m31,
                 double m32,
                 double m33,
                 double m34,
                 double m41,
                 double m42,
                 double m43,
                 double m44);
  void setTransform(double m11,
                    double m12,
                    double m13,
                    double m14,
                    double m21,
                    double m22,
                    double m23,
                    double m24,
                    double m31,
                    double m32,
                    double m33,
                    double m34,
                    double m41,
                    double m42,
                    double m43,
                    double m44);
  void setTransform(double m11,
                    double m12,
                    double m21,
                    double m22,
                    double dx,
                    double dy);
  void setTransform(DOMMatrixInit*, ExceptionState&);
  virtual DOMMatrix* getTransform();
  virtual void resetTransform();

  void beginPath();

  void fill(const String& winding = "nonzero");
  void fill(Path2D*, const String& winding = "nonzero");
  void stroke();
  void stroke(Path2D*);
  void clip(const String& winding = "nonzero");
  void clip(Path2D*, const String& winding = "nonzero");

  bool isPointInPath(const double x,
                     const double y,
                     const String& winding = "nonzero");
  bool isPointInPath(Path2D*,
                     const double x,
                     const double y,
                     const String& winding = "nonzero");
  bool isPointInStroke(const double x, const double y);
  bool isPointInStroke(Path2D*, const double x, const double y);

  void clearRect(double x, double y, double width, double height);
  void fillRect(double x, double y, double width, double height);
  void strokeRect(double x, double y, double width, double height);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void drawImage(ScriptState* script_state,
                 const V8CanvasImageSource* image_source,
                 double x,
                 double y,
                 ExceptionState& exception_state);
  void drawImage(ScriptState* script_state,
                 const V8CanvasImageSource* image_source,
                 double x,
                 double y,
                 double width,
                 double height,
                 ExceptionState& exception_state);
  void drawImage(ScriptState* script_state,
                 const V8CanvasImageSource* image_source,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void drawImage(ScriptState*,
                 const CanvasImageSourceUnion&,
                 double x,
                 double y,
                 ExceptionState&);
  void drawImage(ScriptState*,
                 const CanvasImageSourceUnion&,
                 double x,
                 double y,
                 double width,
                 double height,
                 ExceptionState&);
  void drawImage(ScriptState*,
                 const CanvasImageSourceUnion&,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void drawImage(ScriptState*,
                 CanvasImageSource*,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState&);

  CanvasGradient* createLinearGradient(double x0,
                                       double y0,
                                       double x1,
                                       double y1);
  CanvasGradient* createRadialGradient(double x0,
                                       double y0,
                                       double r0,
                                       double x1,
                                       double y1,
                                       double r1,
                                       ExceptionState&);
  CanvasGradient* createConicGradient(double startAngle,
                                      double centerX,
                                      double centerY);
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CanvasPattern* createPattern(ScriptState* script_state,
                               const V8CanvasImageSource* image_source,
                               const String& repetition_type,
                               ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CanvasPattern* createPattern(ScriptState*,
                               const CanvasImageSourceUnion&,
                               const String& repetition_type,
                               ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CanvasPattern* createPattern(ScriptState*,
                               CanvasImageSource*,
                               const String& repetition_type,
                               ExceptionState&);

  ImageData* createImageData(ImageData*, ExceptionState&) const;
  ImageData* createImageData(int sw,
                             int sh,
                             ImageDataSettings*,
                             ExceptionState&) const;

  // For deferred canvases this will have the side effect of drawing recorded
  // commands in order to finalize the frame
  ImageData* getImageData(int sx,
                          int sy,
                          int sw,
                          int sh,
                          ImageDataSettings*,
                          ExceptionState&);
  virtual ImageData* getImageDataInternal(int sx,
                                          int sy,
                                          int sw,
                                          int sh,
                                          ImageDataSettings*,
                                          ExceptionState&);

  void putImageData(ImageData*, int dx, int dy, ExceptionState&);
  void putImageData(ImageData*,
                    int dx,
                    int dy,
                    int dirty_x,
                    int dirty_y,
                    int dirty_width,
                    int dirty_height,
                    ExceptionState&);

  bool imageSmoothingEnabled() const;
  void setImageSmoothingEnabled(bool);
  String imageSmoothingQuality() const;
  void setImageSmoothingQuality(const String&);

  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;
  virtual bool WouldTaintOrigin(CanvasImageSource*) = 0;

  virtual int Width() const = 0;
  virtual int Height() const = 0;

  virtual bool IsAccelerated() const {
    NOTREACHED();
    return false;
  }
  virtual bool CanCreateCanvas2dResourceProvider() const = 0;

  virtual RespectImageOrientationEnum RespectImageOrientation() const = 0;

  virtual bool ParseColorOrCurrentColor(Color&,
                                        const String& color_string) const = 0;

  virtual cc::PaintCanvas* GetOrCreatePaintCanvas() = 0;
  virtual cc::PaintCanvas* GetPaintCanvas() const = 0;

  virtual void DidDraw(const SkIRect& dirty_rect) = 0;

  virtual bool StateHasFilter() = 0;
  virtual sk_sp<PaintFilter> StateGetFilter() = 0;
  virtual void SnapshotStateForFilter() = 0;

  void ValidateStateStack() const {
    ValidateStateStackWithCanvas(GetPaintCanvas());
  }
  virtual void ValidateStateStackWithCanvas(const cc::PaintCanvas*) const = 0;

  virtual bool HasAlpha() const = 0;

  virtual bool IsDesynchronized() const {
    NOTREACHED();
    return false;
  }

  virtual bool isContextLost() const = 0;

  virtual void WillDrawImage(CanvasImageSource*) const {}

  void RestoreMatrixClipStack(cc::PaintCanvas*) const;

  String textAlign() const;
  void setTextAlign(const String&);

  String textBaseline() const;
  void setTextBaseline(const String&);

  double textLetterSpacing() const;
  double textWordSpacing() const;
  String textRendering() const;

  String fontKerning() const;
  String fontStretch() const;
  String fontVariantCaps() const;

  void Trace(Visitor*) const override;

  enum DrawCallType {
    kStrokePath = 0,
    kFillPath,
    kDrawVectorImage,
    kDrawBitmapImage,
    kFillText,
    kStrokeText,
    kFillRect,
    kStrokeRect,
    kDrawCallTypeCount  // used to specify the size of storage arrays
  };

  enum PathFillType {
    kColorFillType,
    kLinearGradientFillType,
    kRadialGradientFillType,
    kPatternFillType,
    kPathFillTypeCount  // used to specify the size of storage arrays
  };

  enum class GPUFallbackToCPUScenario {
    kLargePatternDrawnToGPU = 0,
    kGetImageData = 1,
    kMaxValue = kGetImageData
  };

  struct UsageCounters {
    int num_draw_calls[kDrawCallTypeCount];  // use DrawCallType enum as index
    float bounding_box_perimeter_draw_calls[kDrawCallTypeCount];
    float bounding_box_area_draw_calls[kDrawCallTypeCount];
    float bounding_box_area_fill_type[kPathFillTypeCount];
    int num_non_convex_fill_path_calls;
    float non_convex_fill_path_area;
    int num_radial_gradients;
    int num_linear_gradients;
    int num_patterns;
    int num_draw_with_complex_clips;
    int num_blurred_shadows;
    float bounding_box_area_times_shadow_blur_squared;
    float bounding_box_perimeter_times_shadow_blur_squared;
    int num_filters;
    int num_get_image_data_calls;
    float area_get_image_data_calls;
    int num_put_image_data_calls;
    float area_put_image_data_calls;
    int num_clear_rect_calls;
    int num_draw_focus_calls;
    int num_frames_since_reset;

    UsageCounters();
  };

  const UsageCounters& GetUsage();

 protected:
  BaseRenderingContext2D();

  ALWAYS_INLINE CanvasRenderingContext2DState& GetState() const {
    return *state_stack_.back();
  }

  bool ComputeDirtyRect(const FloatRect& local_bounds, SkIRect*);
  bool ComputeDirtyRect(const FloatRect& local_bounds,
                        const SkIRect& transformed_clip_bounds,
                        SkIRect*);

  template <typename DrawFunc, typename DrawCoversClipBoundsFunc>
  void Draw(const DrawFunc&,
            const DrawCoversClipBoundsFunc&,
            const SkRect& bounds,
            CanvasRenderingContext2DState::PaintType,
            CanvasRenderingContext2DState::ImageType);

  void InflateStrokeRect(FloatRect&) const;

  void UnwindStateStack();

  // The implementations of this will query the CanvasColorParams from the
  // CanvasRenderingContext.
  virtual CanvasColorParams GetCanvas2DColorParams() const = 0;

  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y) {
    NOTREACHED();
    return false;
  }
  virtual scoped_refptr<StaticBitmapImage> GetImage() {
    NOTREACHED();
    return nullptr;
  }

  enum DrawType {
    kClipFill,  // Fill that is already known to cover the current clip
    kUntransformedUnclippedFill
  };

  void CheckOverdraw(const SkRect&,
                     const PaintFlags*,
                     CanvasRenderingContext2DState::ImageType,
                     DrawType);

  HeapVector<Member<CanvasRenderingContext2DState>> state_stack_;
  AntiAliasingMode clip_antialiasing_;

  virtual void FinalizeFrame() {}

  float GetFontBaseline(const SimpleFontData&) const;

  static const char kDefaultFont[];
  static const char kInheritDirectionString[];
  static const char kRtlDirectionString[];
  static const char kLtrDirectionString[];
  static const char kAutoKerningString[];
  static const char kNormalKerningString[];
  static const char kNoneKerningString[];
  static const char kNormalVariantString[];
  static const char kUltraCondensedString[];
  static const char kExtraCondensedString[];
  static const char kCondensedString[];
  static const char kSemiCondensedString[];
  static const char kNormalStretchString[];
  static const char kSemiExpandedString[];
  static const char kExpandedString[];
  static const char kExtraExpandedString[];
  static const char kUltraExpandedString[];
  static const char kSmallCapsVariantString[];
  static const char kAllSmallCapsVariantString[];
  static const char kPetiteVariantString[];
  static const char kAllPetiteVariantString[];
  static const char kUnicaseVariantString[];
  static const char kTitlingCapsVariantString[];
  static const char kAutoRendering[];
  static const char kOptimizeSpeedRendering[];
  static const char kOptimizeLegibilityRendering[];
  static const char kGeometricPrecisionRendering[];
  // Canvas is device independent
  static const double kCDeviceScaleFactor;
  virtual void DisableAcceleration() {}

  virtual bool IsPaint2D() const { return false; }
  virtual void WillOverwriteCanvas() = 0;

  IdentifiabilityStudyHelper identifiability_study_helper_;

 private:
  bool ShouldDrawImageAntialiased(const FloatRect& dest_rect) const;

  // When the canvas is stroked or filled with a pattern, which is assumed to
  // have a transparent background, the shadow needs to be applied with
  // DropShadowPaintFilter for kNonOpaqueImageType
  // Used in Draw and CompositedDraw to avoid the shadow offset being modified
  // by the transformation matrix
  bool ShouldUseDropShadowPaintFilter(
      CanvasRenderingContext2DState::PaintType paint_type,
      CanvasRenderingContext2DState::ImageType image_type) const {
    return (paint_type == CanvasRenderingContext2DState::kFillPaintType ||
            paint_type == CanvasRenderingContext2DState::kStrokePaintType) &&
           image_type == CanvasRenderingContext2DState::kNonOpaqueImage;
  }

  template <typename DrawFunc, typename DrawCoversClipBoundsFunc>
  void DrawInternal(const DrawFunc&,
                    const DrawCoversClipBoundsFunc&,
                    const SkRect& bounds,
                    CanvasRenderingContext2DState::PaintType,
                    CanvasRenderingContext2DState::ImageType,
                    const SkIRect& clip_bounds);

  void DrawPathInternal(const Path&,
                        CanvasRenderingContext2DState::PaintType,
                        SkPathFillType = SkPathFillType::kWinding);
  void DrawImageInternal(cc::PaintCanvas*,
                         CanvasImageSource*,
                         Image*,
                         const FloatRect& src_rect,
                         const FloatRect& dst_rect,
                         const SkSamplingOptions&,
                         const PaintFlags*);
  void ClipInternal(const Path&, const String& winding_rule_string);

  bool IsPointInPathInternal(const Path&,
                             const double x,
                             const double y,
                             const String& winding_rule_string);
  bool IsPointInStrokeInternal(const Path&, const double x, const double y);

  static bool IsFullCanvasCompositeMode(SkBlendMode);

  template <typename DrawFunc>
  void CompositedDraw(const DrawFunc&,
                      cc::PaintCanvas*,
                      CanvasRenderingContext2DState::PaintType,
                      CanvasRenderingContext2DState::ImageType);

  template <typename T>
  bool ValidateRectForCanvas(T x, T y, T width, T height);

  template <typename T>
  void AdjustRectForCanvas(T& x, T& y, T& width, T& height);

  void ClearCanvas();
  bool RectContainsTransformedRect(const FloatRect&, const SkIRect&) const;
  // Sets the origin to be tainted by the content of the canvas, such
  // as a cross-origin image. This is as opposed to some other reason
  // such as tainting from a filter applied to the canvas.
  void SetOriginTaintedByContent();

  void PutByteArray(const SkPixmap& source,
                    const IntRect& source_rect,
                    const IntPoint& dest_point);
  virtual bool IsCanvas2DBufferValid() const {
    NOTREACHED();
    return false;
  }

  int getScaledElapsedTime(float width,
                           float height,
                           base::TimeTicks start_time);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void IdentifiabilityMaybeUpdateForStyleUnion(
      const V8UnionCSSColorValueOrCanvasGradientOrCanvasPatternOrString* style);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void IdentifiabilityMaybeUpdateForStyleUnion(
      const StringOrCanvasGradientOrCanvasPatternOrCSSColorValue& style);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  RespectImageOrientationEnum RespectImageOrientationInternal(
      CanvasImageSource*);

  bool origin_tainted_by_content_;

  DISALLOW_COPY_AND_ASSIGN(BaseRenderingContext2D);
};

template <typename DrawFunc, typename DrawCoversClipBoundsFunc>
void BaseRenderingContext2D::DrawInternal(
    const DrawFunc& draw_func,
    const DrawCoversClipBoundsFunc& draw_covers_clip_bounds,
    const SkRect& bounds,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type,
    const SkIRect& clip_bounds) {
  if (IsFullCanvasCompositeMode(GetState().GlobalComposite()) ||
      StateHasFilter() ||
      (GetState().ShouldDrawShadows() &&
       ShouldUseDropShadowPaintFilter(paint_type, image_type))) {
    CompositedDraw(draw_func, GetPaintCanvas(), paint_type, image_type);
    DidDraw(clip_bounds);
  } else if (GetState().GlobalComposite() == SkBlendMode::kSrc) {
    ClearCanvas();  // takes care of checkOverdraw()
    const PaintFlags* flags =
        GetState().GetFlags(paint_type, kDrawForegroundOnly, image_type);
    draw_func(GetPaintCanvas(), flags);
    DidDraw(clip_bounds);
  } else {
    SkIRect dirty_rect;
    if (ComputeDirtyRect(bounds, clip_bounds, &dirty_rect)) {
      const PaintFlags* flags =
          GetState().GetFlags(paint_type, kDrawShadowAndForeground, image_type);
      if (paint_type != CanvasRenderingContext2DState::kStrokePaintType &&
          draw_covers_clip_bounds(clip_bounds))
        CheckOverdraw(bounds, flags, image_type, kClipFill);
      draw_func(GetPaintCanvas(), flags);
      DidDraw(dirty_rect);
    }
  }
}

template <typename DrawFunc, typename DrawCoversClipBoundsFunc>
void BaseRenderingContext2D::Draw(
    const DrawFunc& draw_func,
    const DrawCoversClipBoundsFunc& draw_covers_clip_bounds,
    const SkRect& bounds,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type) {
  if (!GetState().IsTransformInvertible())
    return;

  SkIRect clip_bounds;
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas || !paint_canvas->getDeviceClipBounds(&clip_bounds))
    return;

  if (UNLIKELY(GetState().IsFilterUnresolved())) {
    // Resolving a filter requires allocating garbage-collected objects.
    PostDeferrableAction(WTF::Bind(
        &BaseRenderingContext2D::DrawInternal<DrawFunc,
                                              DrawCoversClipBoundsFunc>,
        WrapPersistent(this), draw_func, draw_covers_clip_bounds, bounds,
        paint_type, image_type, clip_bounds));
  } else {
    DrawInternal(draw_func, draw_covers_clip_bounds, bounds, paint_type,
                 image_type, clip_bounds);
  }
}

template <typename DrawFunc>
void BaseRenderingContext2D::CompositedDraw(
    const DrawFunc& draw_func,
    cc::PaintCanvas* c,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type) {
  sk_sp<PaintFilter> canvas_filter = StateGetFilter();
  DCHECK(IsFullCanvasCompositeMode(GetState().GlobalComposite()) ||
         canvas_filter ||
         (GetState().ShouldDrawShadows() &&
          ShouldUseDropShadowPaintFilter(paint_type, image_type)));
  SkM44 ctm = c->getLocalToDevice();
  c->setMatrix(SkM44());
  PaintFlags composite_flags;
  composite_flags.setBlendMode(GetState().GlobalComposite());
  if (GetState().ShouldDrawShadows()) {
    // unroll into two independently composited passes if drawing shadows
    PaintFlags shadow_flags =
        *GetState().GetFlags(paint_type, kDrawShadowOnly, image_type);
    int save_count = c->getSaveCount();
    c->save();
    if (canvas_filter ||
        ShouldUseDropShadowPaintFilter(paint_type, image_type)) {
      PaintFlags foreground_flags =
          *GetState().GetFlags(paint_type, kDrawForegroundOnly, image_type);
      shadow_flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
          sk_make_sp<ComposePaintFilter>(foreground_flags.getImageFilter(),
                                         shadow_flags.getImageFilter()),
          canvas_filter));
      // Saving the shadow layer before setting the matrix, so the shadow offset
      // does not get modified by the transformation matrix
      c->saveLayer(nullptr, &shadow_flags);
      c->setMatrix(ctm);
      draw_func(c, &foreground_flags);
    } else {
      DCHECK(IsFullCanvasCompositeMode(GetState().GlobalComposite()));
      c->saveLayer(nullptr, &composite_flags);
      shadow_flags.setBlendMode(SkBlendMode::kSrcOver);
      c->setMatrix(ctm);
      draw_func(c, &shadow_flags);
    }
    c->restoreToCount(save_count);
  }

  composite_flags.setImageFilter(std::move(canvas_filter));
  c->saveLayer(nullptr, &composite_flags);
  PaintFlags foreground_flags =
      *GetState().GetFlags(paint_type, kDrawForegroundOnly, image_type);
  foreground_flags.setBlendMode(SkBlendMode::kSrcOver);
  c->setMatrix(ctm);
  draw_func(c, &foreground_flags);
  c->restore();
  c->setMatrix(ctm);
}

template <typename T>
bool BaseRenderingContext2D::ValidateRectForCanvas(T x,
                                                   T y,
                                                   T width,
                                                   T height) {
  return (std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
          std::isfinite(height) && (width || height));
}

template <typename T>
void BaseRenderingContext2D::AdjustRectForCanvas(T& x,
                                                 T& y,
                                                 T& width,
                                                 T& height) {
  if (width < 0) {
    width = -width;
    x -= width;
  }

  if (height < 0) {
    height = -height;
    y -= height;
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
