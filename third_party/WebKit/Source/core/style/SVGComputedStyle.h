/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Computer, Inc.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGComputedStyle_h
#define SVGComputedStyle_h

#include "core/CoreExport.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/DataRef.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "platform/Length.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

class StyleDifference;

// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class SVGComputedStyle : public RefCounted<SVGComputedStyle> {
 public:
  static RefPtr<SVGComputedStyle> Create() {
    return AdoptRef(new SVGComputedStyle);
  }
  RefPtr<SVGComputedStyle> Copy() const {
    return AdoptRef(new SVGComputedStyle(*this));
  }
  CORE_EXPORT ~SVGComputedStyle();

  bool InheritedEqual(const SVGComputedStyle&) const;
  bool NonInheritedEqual(const SVGComputedStyle&) const;
  void InheritFrom(const SVGComputedStyle*);
  void CopyNonInheritedFromCached(const SVGComputedStyle*);

  CORE_EXPORT StyleDifference Diff(const SVGComputedStyle*) const;

  bool operator==(const SVGComputedStyle&) const;
  bool operator!=(const SVGComputedStyle& o) const { return !(*this == o); }

  // Initial values for all the properties
  static EAlignmentBaseline InitialAlignmentBaseline() { return AB_AUTO; }
  static EDominantBaseline InitialDominantBaseline() { return DB_AUTO; }
  static EBaselineShift InitialBaselineShift() { return BS_LENGTH; }
  static Length InitialBaselineShiftValue() { return Length(kFixed); }
  static EVectorEffect InitialVectorEffect() { return VE_NONE; }
  static EBufferedRendering InitialBufferedRendering() { return BR_AUTO; }
  static LineCap InitialCapStyle() { return kButtCap; }
  static WindRule InitialClipRule() { return RULE_NONZERO; }
  static EColorInterpolation InitialColorInterpolation() { return CI_SRGB; }
  static EColorInterpolation InitialColorInterpolationFilters() {
    return CI_LINEARRGB;
  }
  static EColorRendering InitialColorRendering() { return CR_AUTO; }
  static WindRule InitialFillRule() { return RULE_NONZERO; }
  static LineJoin InitialJoinStyle() { return kMiterJoin; }
  static EShapeRendering InitialShapeRendering() { return SR_AUTO; }
  static ETextAnchor InitialTextAnchor() { return TA_START; }
  static float InitialFillOpacity() { return 1; }
  static SVGPaintType InitialFillPaintType() { return SVG_PAINTTYPE_RGBCOLOR; }
  static Color InitialFillPaintColor() { return Color::kBlack; }
  static String InitialFillPaintUri() { return String(); }
  static float InitialStrokeOpacity() { return 1; }
  static SVGPaintType InitialStrokePaintType() { return SVG_PAINTTYPE_NONE; }
  static Color InitialStrokePaintColor() { return Color(); }
  static String InitialStrokePaintUri() { return String(); }
  static RefPtr<SVGDashArray> InitialStrokeDashArray();
  static Length InitialStrokeDashOffset() { return Length(kFixed); }
  static float InitialStrokeMiterLimit() { return 4; }
  static UnzoomedLength InitialStrokeWidth() {
    return UnzoomedLength(Length(1, kFixed));
  }
  static float InitialStopOpacity() { return 1; }
  static Color InitialStopColor() { return Color(0, 0, 0); }
  static float InitialFloodOpacity() { return 1; }
  static Color InitialFloodColor() { return Color(0, 0, 0); }
  static Color InitialLightingColor() { return Color(255, 255, 255); }
  static const AtomicString& InitialMaskerResource() { return g_null_atom; }
  static const AtomicString& InitialMarkerStartResource() {
    return g_null_atom;
  }
  static const AtomicString& InitialMarkerMidResource() { return g_null_atom; }
  static const AtomicString& InitialMarkerEndResource() { return g_null_atom; }
  static EMaskType InitialMaskType() { return MT_LUMINANCE; }
  static EPaintOrder InitialPaintOrder() { return kPaintOrderNormal; }
  static StylePath* InitialD() { return nullptr; }
  static Length InitialCx() { return Length(kFixed); }
  static Length InitialCy() { return Length(kFixed); }
  static Length InitialX() { return Length(kFixed); }
  static Length InitialY() { return Length(kFixed); }
  static Length InitialR() { return Length(kFixed); }
  static Length InitialRx() { return Length(kAuto); }
  static Length InitialRy() { return Length(kAuto); }

  // SVG CSS Property setters
  void SetAlignmentBaseline(EAlignmentBaseline val) {
    svg_noninherited_flags.f.alignment_baseline = val;
  }
  void SetDominantBaseline(EDominantBaseline val) {
    svg_inherited_flags.dominant_baseline = val;
  }
  void SetBaselineShift(EBaselineShift val) {
    svg_noninherited_flags.f.baseline_shift = val;
  }
  void SetVectorEffect(EVectorEffect val) {
    svg_noninherited_flags.f.vector_effect = val;
  }
  void SetBufferedRendering(EBufferedRendering val) {
    svg_noninherited_flags.f.buffered_rendering = val;
  }
  void SetCapStyle(LineCap val) { svg_inherited_flags.cap_style = val; }
  void SetClipRule(WindRule val) { svg_inherited_flags.clip_rule = val; }
  void SetColorInterpolation(EColorInterpolation val) {
    svg_inherited_flags.color_interpolation = val;
  }
  void SetColorInterpolationFilters(EColorInterpolation val) {
    svg_inherited_flags.color_interpolation_filters = val;
  }
  void SetColorRendering(EColorRendering val) {
    svg_inherited_flags.color_rendering = val;
  }
  void SetFillRule(WindRule val) { svg_inherited_flags.fill_rule = val; }
  void SetJoinStyle(LineJoin val) { svg_inherited_flags.join_style = val; }
  void SetShapeRendering(EShapeRendering val) {
    svg_inherited_flags.shape_rendering = val;
  }
  void SetTextAnchor(ETextAnchor val) { svg_inherited_flags.text_anchor = val; }
  void SetMaskType(EMaskType val) { svg_noninherited_flags.f.mask_type = val; }
  void SetPaintOrder(EPaintOrder val) {
    svg_inherited_flags.paint_order = (int)val;
  }
  void SetD(RefPtr<StylePath> d) {
    if (!(geometry->d == d))
      geometry.Access()->d = std::move(d);
  }
  void SetCx(const Length& obj) {
    if (!(geometry->cx == obj))
      geometry.Access()->cx = obj;
  }
  void SetCy(const Length& obj) {
    if (!(geometry->cy == obj))
      geometry.Access()->cy = obj;
  }
  void SetX(const Length& obj) {
    if (!(geometry->x == obj))
      geometry.Access()->x = obj;
  }
  void SetY(const Length& obj) {
    if (!(geometry->y == obj))
      geometry.Access()->y = obj;
  }
  void SetR(const Length& obj) {
    if (!(geometry->r == obj))
      geometry.Access()->r = obj;
  }
  void SetRx(const Length& obj) {
    if (!(geometry->rx == obj))
      geometry.Access()->rx = obj;
  }
  void SetRy(const Length& obj) {
    if (!(geometry->ry == obj))
      geometry.Access()->ry = obj;
  }
  void SetFillOpacity(float obj) {
    if (!(fill->opacity == obj))
      fill.Access()->opacity = obj;
  }

  void SetFillPaint(SVGPaintType type,
                    const Color& color,
                    const String& uri,
                    bool apply_to_regular_style = true,
                    bool apply_to_visited_link_style = false) {
    if (apply_to_regular_style) {
      if (!(fill->paint_type == type))
        fill.Access()->paint_type = type;
      if (!(fill->paint_color == color))
        fill.Access()->paint_color = color;
      if (!(fill->paint_uri == uri))
        fill.Access()->paint_uri = uri;
    }
    if (apply_to_visited_link_style) {
      if (!(fill->visited_link_paint_type == type))
        fill.Access()->visited_link_paint_type = type;
      if (!(fill->visited_link_paint_color == color))
        fill.Access()->visited_link_paint_color = color;
      if (!(fill->visited_link_paint_uri == uri))
        fill.Access()->visited_link_paint_uri = uri;
    }
  }

  void SetStrokeOpacity(float obj) {
    if (!(stroke->opacity == obj))
      stroke.Access()->opacity = obj;
  }

  void SetStrokePaint(SVGPaintType type,
                      const Color& color,
                      const String& uri,
                      bool apply_to_regular_style = true,
                      bool apply_to_visited_link_style = false) {
    if (apply_to_regular_style) {
      if (!(stroke->paint_type == type))
        stroke.Access()->paint_type = type;
      if (!(stroke->paint_color == color))
        stroke.Access()->paint_color = color;
      if (!(stroke->paint_uri == uri))
        stroke.Access()->paint_uri = uri;
    }
    if (apply_to_visited_link_style) {
      if (!(stroke->visited_link_paint_type == type))
        stroke.Access()->visited_link_paint_type = type;
      if (!(stroke->visited_link_paint_color == color))
        stroke.Access()->visited_link_paint_color = color;
      if (!(stroke->visited_link_paint_uri == uri))
        stroke.Access()->visited_link_paint_uri = uri;
    }
  }

  void SetStrokeDashArray(RefPtr<SVGDashArray> dash_array) {
    if (*stroke->dash_array != *dash_array)
      stroke.Access()->dash_array = std::move(dash_array);
  }

  void SetStrokeMiterLimit(float obj) {
    if (!(stroke->miter_limit == obj))
      stroke.Access()->miter_limit = obj;
  }

  void SetStrokeWidth(const UnzoomedLength& stroke_width) {
    if (!(stroke->width == stroke_width))
      stroke.Access()->width = stroke_width;
  }

  void SetStrokeDashOffset(const Length& dash_offset) {
    if (!(stroke->dash_offset == dash_offset))
      stroke.Access()->dash_offset = dash_offset;
  }

  void SetStopOpacity(float obj) {
    if (!(stops->opacity == obj))
      stops.Access()->opacity = obj;
  }

  void SetStopColor(const Color& obj) {
    if (!(stops->color == obj))
      stops.Access()->color = obj;
  }

  void SetFloodOpacity(float obj) {
    if (!(misc->flood_opacity == obj))
      misc.Access()->flood_opacity = obj;
  }

  void SetFloodColor(const Color& obj) {
    if (!(misc->flood_color == obj))
      misc.Access()->flood_color = obj;
  }

  void SetLightingColor(const Color& obj) {
    if (!(misc->lighting_color == obj))
      misc.Access()->lighting_color = obj;
  }

  void SetBaselineShiftValue(const Length& baseline_shift_value) {
    if (!(misc->baseline_shift_value == baseline_shift_value))
      misc.Access()->baseline_shift_value = baseline_shift_value;
  }

  // Setters for non-inherited resources
  void SetMaskerResource(const AtomicString& obj) {
    if (!(resources->masker == obj))
      resources.Access()->masker = obj;
  }

  // Setters for inherited resources
  void SetMarkerStartResource(const AtomicString& obj) {
    if (!(inherited_resources->marker_start == obj))
      inherited_resources.Access()->marker_start = obj;
  }

  void SetMarkerMidResource(const AtomicString& obj) {
    if (!(inherited_resources->marker_mid == obj))
      inherited_resources.Access()->marker_mid = obj;
  }

  void SetMarkerEndResource(const AtomicString& obj) {
    if (!(inherited_resources->marker_end == obj))
      inherited_resources.Access()->marker_end = obj;
  }

  // Read accessors for all the properties
  EAlignmentBaseline AlignmentBaseline() const {
    return (EAlignmentBaseline)svg_noninherited_flags.f.alignment_baseline;
  }
  EDominantBaseline DominantBaseline() const {
    return (EDominantBaseline)svg_inherited_flags.dominant_baseline;
  }
  EBaselineShift BaselineShift() const {
    return (EBaselineShift)svg_noninherited_flags.f.baseline_shift;
  }
  EVectorEffect VectorEffect() const {
    return (EVectorEffect)svg_noninherited_flags.f.vector_effect;
  }
  EBufferedRendering BufferedRendering() const {
    return (EBufferedRendering)svg_noninherited_flags.f.buffered_rendering;
  }
  LineCap CapStyle() const { return (LineCap)svg_inherited_flags.cap_style; }
  WindRule ClipRule() const { return (WindRule)svg_inherited_flags.clip_rule; }
  EColorInterpolation ColorInterpolation() const {
    return (EColorInterpolation)svg_inherited_flags.color_interpolation;
  }
  EColorInterpolation ColorInterpolationFilters() const {
    return (EColorInterpolation)svg_inherited_flags.color_interpolation_filters;
  }
  EColorRendering ColorRendering() const {
    return (EColorRendering)svg_inherited_flags.color_rendering;
  }
  WindRule FillRule() const { return (WindRule)svg_inherited_flags.fill_rule; }
  LineJoin JoinStyle() const {
    return (LineJoin)svg_inherited_flags.join_style;
  }
  EShapeRendering ShapeRendering() const {
    return (EShapeRendering)svg_inherited_flags.shape_rendering;
  }
  ETextAnchor TextAnchor() const {
    return (ETextAnchor)svg_inherited_flags.text_anchor;
  }
  float FillOpacity() const { return fill->opacity; }
  const SVGPaintType& FillPaintType() const { return fill->paint_type; }
  const Color& FillPaintColor() const { return fill->paint_color; }
  const String& FillPaintUri() const { return fill->paint_uri; }
  float StrokeOpacity() const { return stroke->opacity; }
  const SVGPaintType& StrokePaintType() const { return stroke->paint_type; }
  const Color& StrokePaintColor() const { return stroke->paint_color; }
  const String& StrokePaintUri() const { return stroke->paint_uri; }
  SVGDashArray* StrokeDashArray() const { return stroke->dash_array.Get(); }
  float StrokeMiterLimit() const { return stroke->miter_limit; }
  const UnzoomedLength& StrokeWidth() const { return stroke->width; }
  const Length& StrokeDashOffset() const { return stroke->dash_offset; }
  float StopOpacity() const { return stops->opacity; }
  const Color& StopColor() const { return stops->color; }
  float FloodOpacity() const { return misc->flood_opacity; }
  const Color& FloodColor() const { return misc->flood_color; }
  const Color& LightingColor() const { return misc->lighting_color; }
  const Length& BaselineShiftValue() const {
    return misc->baseline_shift_value;
  }
  StylePath* D() const { return geometry->d.Get(); }
  const Length& Cx() const { return geometry->cx; }
  const Length& Cy() const { return geometry->cy; }
  const Length& X() const { return geometry->x; }
  const Length& Y() const { return geometry->y; }
  const Length& R() const { return geometry->r; }
  const Length& Rx() const { return geometry->rx; }
  const Length& Ry() const { return geometry->ry; }
  const AtomicString& MaskerResource() const { return resources->masker; }
  const AtomicString& MarkerStartResource() const {
    return inherited_resources->marker_start;
  }
  const AtomicString& MarkerMidResource() const {
    return inherited_resources->marker_mid;
  }
  const AtomicString& MarkerEndResource() const {
    return inherited_resources->marker_end;
  }
  EMaskType MaskType() const {
    return (EMaskType)svg_noninherited_flags.f.mask_type;
  }
  EPaintOrder PaintOrder() const {
    return (EPaintOrder)svg_inherited_flags.paint_order;
  }
  EPaintOrderType PaintOrderType(unsigned index) const;

  const SVGPaintType& VisitedLinkFillPaintType() const {
    return fill->visited_link_paint_type;
  }
  const Color& VisitedLinkFillPaintColor() const {
    return fill->visited_link_paint_color;
  }
  const String& VisitedLinkFillPaintUri() const {
    return fill->visited_link_paint_uri;
  }
  const SVGPaintType& VisitedLinkStrokePaintType() const {
    return stroke->visited_link_paint_type;
  }
  const Color& VisitedLinkStrokePaintColor() const {
    return stroke->visited_link_paint_color;
  }
  const String& VisitedLinkStrokePaintUri() const {
    return stroke->visited_link_paint_uri;
  }

  bool IsFillColorCurrentColor() const {
    return FillPaintType() == SVG_PAINTTYPE_CURRENTCOLOR ||
           VisitedLinkFillPaintType() == SVG_PAINTTYPE_CURRENTCOLOR ||
           FillPaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR ||
           VisitedLinkFillPaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR;
  }

  bool IsStrokeColorCurrentColor() const {
    return StrokePaintType() == SVG_PAINTTYPE_CURRENTCOLOR ||
           VisitedLinkStrokePaintType() == SVG_PAINTTYPE_CURRENTCOLOR ||
           StrokePaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR ||
           VisitedLinkStrokePaintType() == SVG_PAINTTYPE_URI_CURRENTCOLOR;
  }

  // convenience
  bool HasMasker() const { return !MaskerResource().IsEmpty(); }
  bool HasMarkers() const {
    return !MarkerStartResource().IsEmpty() || !MarkerMidResource().IsEmpty() ||
           !MarkerEndResource().IsEmpty();
  }
  bool HasStroke() const { return StrokePaintType() != SVG_PAINTTYPE_NONE; }
  bool HasVisibleStroke() const {
    return HasStroke() && !StrokeWidth().IsZero();
  }
  bool HasSquareCapStyle() const { return CapStyle() == kSquareCap; }
  bool HasMiterJoinStyle() const { return JoinStyle() == kMiterJoin; }
  bool HasFill() const { return FillPaintType() != SVG_PAINTTYPE_NONE; }

 protected:
  // inherit
  struct InheritedFlags {
    bool operator==(const InheritedFlags& other) const {
      return (color_rendering == other.color_rendering) &&
             (shape_rendering == other.shape_rendering) &&
             (clip_rule == other.clip_rule) && (fill_rule == other.fill_rule) &&
             (cap_style == other.cap_style) &&
             (join_style == other.join_style) &&
             (text_anchor == other.text_anchor) &&
             (color_interpolation == other.color_interpolation) &&
             (color_interpolation_filters ==
              other.color_interpolation_filters) &&
             (paint_order == other.paint_order) &&
             (dominant_baseline == other.dominant_baseline);
    }

    bool operator!=(const InheritedFlags& other) const {
      return !(*this == other);
    }

    unsigned color_rendering : 2;              // EColorRendering
    unsigned shape_rendering : 2;              // EShapeRendering
    unsigned clip_rule : 1;                    // WindRule
    unsigned fill_rule : 1;                    // WindRule
    unsigned cap_style : 2;                    // LineCap
    unsigned join_style : 2;                   // LineJoin
    unsigned text_anchor : 2;                  // ETextAnchor
    unsigned color_interpolation : 2;          // EColorInterpolation
    unsigned color_interpolation_filters : 2;  // EColorInterpolation_
    unsigned paint_order : 3;                  // EPaintOrder
    unsigned dominant_baseline : 4;            // EDominantBaseline
  } svg_inherited_flags;

  // don't inherit
  struct NonInheritedFlags {
    // 32 bit non-inherited, don't add to the struct, or the operator will
    // break.
    bool operator==(const NonInheritedFlags& other) const {
      return niflags == other.niflags;
    }
    bool operator!=(const NonInheritedFlags& other) const {
      return niflags != other.niflags;
    }

    union {
      struct {
        unsigned alignment_baseline : 4;  // EAlignmentBaseline
        unsigned baseline_shift : 2;      // EBaselineShift
        unsigned vector_effect : 1;       // EVectorEffect
        unsigned buffered_rendering : 2;  // EBufferedRendering
        unsigned mask_type : 1;           // EMaskType
                                          // 18 bits unused
      } f;
      uint32_t niflags;
    };
  } svg_noninherited_flags;

  // inherited attributes
  DataRef<StyleFillData> fill;
  DataRef<StyleStrokeData> stroke;
  DataRef<StyleInheritedResourceData> inherited_resources;

  // non-inherited attributes
  DataRef<StyleStopData> stops;
  DataRef<StyleMiscData> misc;
  DataRef<StyleGeometryData> geometry;
  DataRef<StyleResourceData> resources;

 private:
  enum CreateInitialType { kCreateInitial };

  CORE_EXPORT SVGComputedStyle();
  SVGComputedStyle(const SVGComputedStyle&);
  SVGComputedStyle(
      CreateInitialType);  // Used to create the initial style singleton.

  bool DiffNeedsLayoutAndPaintInvalidation(const SVGComputedStyle* other) const;
  bool DiffNeedsPaintInvalidation(const SVGComputedStyle* other) const;

  void SetBitDefaults() {
    svg_inherited_flags.clip_rule = InitialClipRule();
    svg_inherited_flags.color_rendering = InitialColorRendering();
    svg_inherited_flags.fill_rule = InitialFillRule();
    svg_inherited_flags.shape_rendering = InitialShapeRendering();
    svg_inherited_flags.text_anchor = InitialTextAnchor();
    svg_inherited_flags.cap_style = InitialCapStyle();
    svg_inherited_flags.join_style = InitialJoinStyle();
    svg_inherited_flags.color_interpolation = InitialColorInterpolation();
    svg_inherited_flags.color_interpolation_filters =
        InitialColorInterpolationFilters();
    svg_inherited_flags.paint_order = InitialPaintOrder();
    svg_inherited_flags.dominant_baseline = InitialDominantBaseline();

    svg_noninherited_flags.niflags = 0;
    svg_noninherited_flags.f.alignment_baseline = InitialAlignmentBaseline();
    svg_noninherited_flags.f.baseline_shift = InitialBaselineShift();
    svg_noninherited_flags.f.vector_effect = InitialVectorEffect();
    svg_noninherited_flags.f.buffered_rendering = InitialBufferedRendering();
    svg_noninherited_flags.f.mask_type = InitialMaskType();
  }
};

}  // namespace blink

#endif  // SVGComputedStyle_h
