/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ComputedStyle_h
#define ComputedStyle_h

#include <memory>
#include "core/CSSPropertyNames.h"
#include "core/ComputedStyleBase.h"
#include "core/CoreExport.h"
#include "core/css/StyleAutoColor.h"
#include "core/css/StyleColor.h"
#include "core/style/BorderValue.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/CounterDirectives.h"
#include "core/style/CursorList.h"
#include "core/style/DataRef.h"
#include "core/style/LineClampValue.h"
#include "core/style/NinePieceImage.h"
#include "core/style/QuotesData.h"
#include "core/style/SVGComputedStyle.h"
#include "core/style/StyleContentAlignmentData.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleReflection.h"
#include "core/style/StyleSelfAlignmentData.h"
#include "core/style/TransformOrigin.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"
#include "platform/LengthPoint.h"
#include "platform/LengthSize.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontDescription.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/TouchAction.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextDirection.h"
#include "platform/text/WritingModeUtils.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/LeakAnnotations.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

using std::max;

class FilterOperations;

class AppliedTextDecoration;
struct BorderEdge;
class CSSAnimationData;
class CSSTransitionData;
class CSSVariableData;
class Font;
class Hyphenation;
class ShadowList;
class ShapeValue;
class StyleDifference;
class StyleImage;
class StylePath;
class StyleResolver;
class TransformationMatrix;

class ContentData;

typedef Vector<RefPtr<ComputedStyle>, 4> PseudoStyleCache;

// ComputedStyle stores the computed value [1] for every CSS property on an
// element and provides the interface between the style engine and the rest of
// Blink. It acts as a container where the computed value of every CSS property
// can be stored and retrieved:
//
//   auto style = ComputedStyle::Create();
//   style->SetDisplay(EDisplay::kNone); //'display' keyword property
//   style->Display();
//
// In addition to storing the computed value of every CSS property,
// ComputedStyle also contains various internal style information. Examples
// include cached_pseudo_styles_ (for storing pseudo element styles), unique_
// (for style sharing) and has_simple_underline_ (cached indicator flag of
// text-decoration). These are stored on ComputedStyle for two reasons:
//
//  1) They share the same lifetime as ComputedStyle, so it is convenient to
//  store them in the same object rather than a separate object that have to be
//  passed around as well.
//
//  2) Many of these data members can be packed as bit fields, so we use less
//  memory by packing them in this object with other bit fields.
//
// STORAGE:
//
// ComputedStyle is optimized for memory and performance. The data is not
// actually stored directly in ComputedStyle, but rather in a generated parent
// class ComputedStyleBase. This separation of concerns allows us to optimise
// the memory layout without affecting users of ComputedStyle. ComputedStyle
// inherits from ComputedStyleBase. For more about the memory layout, there is
// documentation in ComputedStyleBase and make_computed_style_base.py.
//
// INTERFACE:
//
// For most CSS properties, ComputedStyle provides a consistent interface which
// includes a getter, setter, initial method (the computed value when the
// property is to 'initial'), and resetter (that resets the computed value to
// its initial value). Exceptions include vertical-align, which has a separate
// set of accessors for its length and its keyword components. Apart from
// accessors, ComputedStyle also has a wealth of helper functions.
//
// Because ComputedStyleBase defines simple accessors to every CSS property,
// ComputedStyle inherits these and so they are not redeclared in this file.
// This means that the interface to ComputedStyle is split between this file and
// ComputedStyleBase.h.
//
// [1] https://developer.mozilla.org/en-US/docs/Web/CSS/computed_value
//
// NOTE:
//
// Currently, some properties are stored in ComputedStyle and some in
// ComputedStyleBase. Eventually, the storage of all properties (except SVG
// ones) will be in ComputedStyleBase.
//
// Since this class is huge, do not mark all of it CORE_EXPORT.  Instead,
// export only the methods you need below.
class ComputedStyle : public ComputedStyleBase,
                      public RefCounted<ComputedStyle> {
  // Needed to allow access to private/protected getters of fields to allow diff
  // generation
  friend class ComputedStyleBase;
  // Used by Web Animations CSS. Sets the color styles.
  friend class AnimatedStyleBuilder;
  // Used by Web Animations CSS. Gets visited and unvisited colors separately.
  friend class CSSAnimatableValueFactory;
  // Used by CSS animations. We can't allow them to animate based off visited
  // colors.
  friend class CSSPropertyEquality;
  // Editing has to only reveal unvisited info.
  friend class ApplyStyleCommand;
  // Editing has to only reveal unvisited info.
  friend class EditingStyle;
  // Needs to be able to see visited and unvisited colors for devtools.
  friend class ComputedStyleCSSValueMapping;
  // Sets color styles
  friend class StyleBuilderFunctions;
  // Saves Border/Background information for later comparison.
  friend class CachedUAStyle;
  // Accesses visited and unvisited colors.
  friend class ColorPropertyFunctions;

  // FIXME: When we stop resolving currentColor at style time, these can be
  // removed.
  friend class CSSToStyleMap;
  friend class FilterOperationResolver;
  friend class StyleBuilderConverter;
  friend class StyleResolverState;
  friend class StyleResolver;

 protected:
  // list of associated pseudo styles
  std::unique_ptr<PseudoStyleCache> cached_pseudo_styles_;

  DataRef<SVGComputedStyle> svg_style_;

 private:
  // TODO(sashab): Move these private members to the bottom of ComputedStyle.
  ALWAYS_INLINE ComputedStyle();
  ALWAYS_INLINE ComputedStyle(const ComputedStyle&);

  static RefPtr<ComputedStyle> CreateInitialStyle();
  // TODO(shend): Remove this. Initial style should not be mutable.
  CORE_EXPORT static ComputedStyle& MutableInitialStyle();

 public:
  CORE_EXPORT static RefPtr<ComputedStyle> Create();
  static RefPtr<ComputedStyle> CreateAnonymousStyleWithDisplay(
      const ComputedStyle& parent_style,
      EDisplay);
  CORE_EXPORT static RefPtr<ComputedStyle> Clone(const ComputedStyle&);
  static const ComputedStyle& InitialStyle() { return MutableInitialStyle(); }
  static void InvalidateInitialStyle();

  // Computes how the style change should be propagated down the tree.
  static StyleRecalcChange StylePropagationDiff(const ComputedStyle* old_style,
                                                const ComputedStyle* new_style);

  // Copies the values of any independent inherited properties from the parent
  // that are not explicitly set in this style.
  void PropagateIndependentInheritedProperties(
      const ComputedStyle& parent_style);

  ContentPosition ResolvedJustifyContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedJustifyContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentPosition ResolvedAlignContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedAlignContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  StyleSelfAlignmentData ResolvedAlignItems(
      ItemPosition normal_value_behaviour) const;
  StyleSelfAlignmentData ResolvedAlignSelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;
  StyleSelfAlignmentData ResolvedJustifyItems(
      ItemPosition normal_value_behaviour) const;
  StyleSelfAlignmentData ResolvedJustifySelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;

  StyleDifference VisualInvalidationDiff(const ComputedStyle&) const;

  void InheritFrom(const ComputedStyle& inherit_parent,
                   IsAtShadowBoundary = kNotAtShadowBoundary);
  void CopyNonInheritedFromCached(const ComputedStyle&);

  PseudoId StyleType() const { return StyleTypeInternal(); }
  void SetStyleType(PseudoId style_type) { SetStyleTypeInternal(style_type); }

  ComputedStyle* GetCachedPseudoStyle(PseudoId) const;
  ComputedStyle* AddCachedPseudoStyle(RefPtr<ComputedStyle>);
  void RemoveCachedPseudoStyle(PseudoId);

  const PseudoStyleCache* CachedPseudoStyles() const {
    return cached_pseudo_styles_.get();
  }

  /**
     * ComputedStyle properties
     *
     * Each property stored in ComputedStyle is made up of fields. Fields have
     * initial value functions, getters and setters. A field is preferably a
     * basic data type or enum, but can be any type. A set of fields should be
     * preceded by the property the field is stored for.
     *
     * Field method naming should be done like so:
     *   // name-of-property
     *   static int initialNameOfProperty();
     *   int nameOfProperty() const;
     *   void setNameOfProperty(int);
     * If the property has multiple fields, add the field name to the end of the
     * method name.
     *
     * Avoid nested types by splitting up fields where possible, e.g.:
     *  int getBorderTopWidth();
     *  int getBorderBottomWidth();
     *  int getBorderLeftWidth();
     *  int getBorderRightWidth();
     * is preferable to:
     *  BorderWidths getBorderWidths();
     *
     * Utility functions should go in a separate section at the end of the
     * class, and be kept to a minimum.
     */

  // Non-Inherited properties.

  static StyleSelfAlignmentData InitialSelfAlignment() {
    return StyleSelfAlignmentData(kItemPositionAuto, kOverflowAlignmentDefault);
  }

  static StyleContentAlignmentData InitialContentAlignment() {
    return StyleContentAlignmentData(kContentPositionNormal,
                                     kContentDistributionDefault,
                                     kOverflowAlignmentDefault);
  }

  static StyleSelfAlignmentData InitialDefaultAlignment() {
    return StyleSelfAlignmentData(RuntimeEnabledFeatures::CSSGridLayoutEnabled()
                                      ? kItemPositionNormal
                                      : kItemPositionStretch,
                                  kOverflowAlignmentDefault);
  }

  // Filter properties.

  // backdrop-filter
  static const FilterOperations& InitialBackdropFilter();
  const FilterOperations& BackdropFilter() const {
    DCHECK(BackdropFilterInternal().Get());
    return BackdropFilterInternal()->operations_;
  }
  FilterOperations& MutableBackdropFilter() {
    DCHECK(BackdropFilterInternal().Get());
    return MutableBackdropFilterInternal()->operations_;
  }
  bool HasBackdropFilter() const {
    DCHECK(BackdropFilterInternal().Get());
    return !BackdropFilterInternal()->operations_.Operations().IsEmpty();
  }
  void SetBackdropFilter(const FilterOperations& ops) {
    DCHECK(BackdropFilterInternal().Get());
    if (BackdropFilterInternal()->operations_ != ops)
      MutableBackdropFilterInternal()->operations_ = ops;
  }
  bool BackdropFilterDataEquivalent(const ComputedStyle& o) const {
    return DataEquivalent(BackdropFilterInternal(), o.BackdropFilterInternal());
  }

  // filter (aka -webkit-filter)
  static const FilterOperations& InitialFilter();
  FilterOperations& MutableFilter() {
    DCHECK(FilterInternal().Get());
    return MutableFilterInternal()->operations_;
  }
  const FilterOperations& Filter() const {
    DCHECK(FilterInternal().Get());
    return FilterInternal()->operations_;
  }
  bool HasFilter() const {
    DCHECK(FilterInternal().Get());
    return !FilterInternal()->operations_.Operations().IsEmpty();
  }
  void SetFilter(const FilterOperations& v) {
    DCHECK(FilterInternal().Get());
    if (FilterInternal()->operations_ != v)
      MutableFilterInternal()->operations_ = v;
  }
  bool FilterDataEquivalent(const ComputedStyle& o) const {
    return DataEquivalent(FilterInternal(), o.FilterInternal());
  }

  // Background properties.
  // background-color
  static Color InitialBackgroundColor() { return Color::kTransparent; }


  // background-image
  bool HasBackgroundImage() const { return BackgroundInternal().HasImage(); }
  bool HasFixedBackgroundImage() const {
    return BackgroundInternal().HasFixedImage();
  }
  bool HasEntirelyFixedBackground() const;

  // background-clip
  EFillBox BackgroundClip() const {
    return static_cast<EFillBox>(BackgroundInternal().Clip());
  }

  // Border properties.
  // border-image-slice
  const LengthBox& BorderImageSlices() const {
    return BorderImage().ImageSlices();
  }
  void SetBorderImageSlices(const LengthBox&);

  // border-image-source
  static StyleImage* InitialBorderImageSource() { return 0; }
  StyleImage* BorderImageSource() const { return BorderImage().GetImage(); }
  void SetBorderImageSource(StyleImage*);

  // border-image-width
  const BorderImageLengthBox& BorderImageWidth() const {
    return BorderImage().BorderSlices();
  }
  void SetBorderImageWidth(const BorderImageLengthBox&);

  // border-image-outset
  const BorderImageLengthBox& BorderImageOutset() const {
    return BorderImage().Outset();
  }
  void SetBorderImageOutset(const BorderImageLengthBox&);

  // Border width properties.
  static float InitialBorderWidth() { return 3; }

  // TODO(nainar): Move all fixed point logic to a separate class.
  // border-top-width
  float BorderTopWidth() const {
    if (BorderTopStyle() == EBorderStyle::kNone ||
        BorderTopStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderTopWidthInternal().ToFloat();
  }
  void SetBorderTopWidth(float v) { SetBorderTopWidthInternal(LayoutUnit(v)); }
  bool BorderTopNonZero() const {
    return BorderTopWidth() && (BorderTopStyle() != EBorderStyle::kNone);
  }

  // border-bottom-width
  float BorderBottomWidth() const {
    if (BorderBottomStyle() == EBorderStyle::kNone ||
        BorderBottomStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderBottomWidthInternal().ToFloat();
  }
  void SetBorderBottomWidth(float v) {
    SetBorderBottomWidthInternal(LayoutUnit(v));
  }
  bool BorderBottomNonZero() const {
    return BorderBottomWidth() && (BorderBottomStyle() != EBorderStyle::kNone);
  }

  // border-left-width
  float BorderLeftWidth() const {
    if (BorderLeftStyle() == EBorderStyle::kNone ||
        BorderLeftStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderLeftWidthInternal().ToFloat();
  }
  void SetBorderLeftWidth(float v) {
    SetBorderLeftWidthInternal(LayoutUnit(v));
  }
  bool BorderLeftNonZero() const {
    return BorderLeftWidth() && (BorderLeftStyle() != EBorderStyle::kNone);
  }

  // border-right-width
  float BorderRightWidth() const {
    if (BorderRightStyle() == EBorderStyle::kNone ||
        BorderRightStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderRightWidthInternal().ToFloat();
  }
  void SetBorderRightWidth(float v) {
    SetBorderRightWidthInternal(LayoutUnit(v));
  }
  bool BorderRightNonZero() const {
    return BorderRightWidth() && (BorderRightStyle() != EBorderStyle::kNone);
  }

  // Border color properties.
  // border-left-color
  void SetBorderLeftColor(const StyleColor& color) {
    if (BorderLeftColor() != color) {
      SetBorderLeftColorInternal(color.Resolve(Color()));
      SetBorderLeftColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-right-color
  void SetBorderRightColor(const StyleColor& color) {
    if (BorderRightColor() != color) {
      SetBorderRightColorInternal(color.Resolve(Color()));
      SetBorderRightColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-top-color
  void SetBorderTopColor(const StyleColor& color) {
    if (BorderTopColor() != color) {
      SetBorderTopColorInternal(color.Resolve(Color()));
      SetBorderTopColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-bottom-color
  void SetBorderBottomColor(const StyleColor& color) {
    if (BorderBottomColor() != color) {
      SetBorderBottomColorInternal(color.Resolve(Color()));
      SetBorderBottomColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // box-shadow (aka -webkit-box-shadow)
  bool BoxShadowDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxShadow(), other.BoxShadow());
  }

  // clip
  static LengthBox InitialClip() { return LengthBox(); }
  void SetClip(const LengthBox& box) {
    SetHasAutoClipInternal(false);
    SetClipInternal(box);
  }
  bool HasAutoClip() const { return HasAutoClipInternal(); }
  void SetHasAutoClip() {
    SetHasAutoClipInternal(true);
    SetClipInternal(ComputedStyle::InitialClip());
  }

  // Column properties.
  // column-count (aka -webkit-column-count)
  static unsigned short InitialColumnCount() { return 1; }
  void SetColumnCount(unsigned short c) {
    SetColumnAutoCountInternal(false);
    SetColumnCountInternal(c);
  }
  bool HasAutoColumnCount() const { return ColumnAutoCountInternal(); }
  void SetHasAutoColumnCount() {
    SetColumnAutoCountInternal(true);
    SetColumnCountInternal(InitialColumnCount());
  }

  // column-gap (aka -webkit-column-gap)
  void SetColumnGap(float f) {
    SetColumnNormalGapInternal(false);
    SetColumnGapInternal(f);
  }
  bool HasNormalColumnGap() const { return ColumnNormalGapInternal(); }
  void SetHasNormalColumnGap() {
    SetColumnNormalGapInternal(true);
    SetColumnGapInternal(0);
  }

  // column-rule-color (aka -webkit-column-rule-color)
  void SetColumnRuleColor(const StyleColor& c) {
    if (ColumnRuleColor() != c) {
      SetColumnRuleColorInternal(c.Resolve(Color()));
      SetColumnRuleColorIsCurrentColor(c.IsCurrentColor());
    }
  }

  // column-rule-width (aka -webkit-column-rule-width)
  static unsigned short InitialColumnRuleWidth() { return 3; }
  unsigned short ColumnRuleWidth() const {
    if (ColumnRuleStyle() == EBorderStyle::kNone ||
        ColumnRuleStyle() == EBorderStyle::kHidden)
      return 0;
    return ColumnRuleWidthInternal().ToFloat();
  }
  void SetColumnRuleWidth(unsigned short w) {
    SetColumnRuleWidthInternal(LayoutUnit(w));
  }

  // column-width (aka -webkit-column-width)
  void SetColumnWidth(float f) {
    SetColumnAutoWidthInternal(false);
    SetColumnWidthInternal(f);
  }
  bool HasAutoColumnWidth() const { return ColumnAutoWidthInternal(); }
  void SetHasAutoColumnWidth() {
    SetColumnAutoWidthInternal(true);
    SetColumnWidthInternal(0);
  }

  // contain
  static Containment InitialContain() { return kContainsNone; }

  // content
  ContentData* GetContentData() const { return ContentInternal().Get(); }
  void SetContent(ContentData*);

  // -webkit-box-ordinal-group
  static unsigned InitialBoxOrdinalGroup() { return 1; }
  void SetBoxOrdinalGroup(unsigned og) {
    SetBoxOrdinalGroupInternal(
        std::min(std::numeric_limits<unsigned>::max() - 1, og));
  }

  // Grid properties.
  static size_t InitialGridAutoRepeatInsertionPoint() { return 0; }
  static AutoRepeatType InitialGridAutoRepeatType() {
    return AutoRepeatType::kNoAutoRepeat;
  }

  // grid-auto-flow
  static GridAutoFlow InitialGridAutoFlow() { return kAutoFlowRow; }

  // opacity (aka -webkit-opacity)
  static float InitialOpacity() { return 1.0f; }
  void SetOpacity(float f) {
    float v = clampTo<float>(f, 0, 1);
    SetOpacityInternal(v);
  }

  bool OpacityChangedStackingContext(const ComputedStyle& other) const {
    // We only need do layout for opacity changes if adding or losing opacity
    // could trigger a change
    // in us being a stacking context.
    if (IsStackingContext() == other.IsStackingContext() ||
        HasOpacity() == other.HasOpacity()) {
      // FIXME: We would like to use SimplifiedLayout here, but we can't quite
      // do that yet.  We need to make sure SimplifiedLayout can operate
      // correctly on LayoutInlines (we will need to add a
      // selfNeedsSimplifiedLayout bit in order to not get confused and taint
      // every line).  In addition we need to solve the floating object issue
      // when layers come and go. Right now a full layout is necessary to keep
      // floating object lists sane.
      return true;
    }
    return false;
  }

  // order (aka -webkit-order)
  static int InitialOrder() { return 0; }
  // We restrict the smallest value to int min + 2 because we use int min and
  // int min + 1 as special values in a hash set.
  void SetOrder(int o) {
    SetOrderInternal(max(std::numeric_limits<int>::min() + 2, o));
  }

  // Outline properties.

  bool OutlineVisuallyEqual(const ComputedStyle& other) const {
    if (OutlineStyle() == EBorderStyle::kNone &&
        other.OutlineStyle() == EBorderStyle::kNone)
      return true;
    return OutlineWidthInternal() == other.OutlineWidthInternal() &&
           OutlineColorIsCurrentColor() == other.OutlineColorIsCurrentColor() &&
           OutlineColor() == other.OutlineColor() &&
           OutlineStyle() == other.OutlineStyle() &&
           OutlineOffsetInternal() == other.OutlineOffsetInternal() &&
           OutlineStyleIsAuto() == other.OutlineStyleIsAuto();
  }

  // outline-color
  void SetOutlineColor(const StyleColor& v) {
    if (OutlineColor() != v) {
      SetOutlineColorInternal(v.Resolve(Color()));
      SetOutlineColorIsCurrentColor(v.IsCurrentColor());
    }
  }

  // outline-width
  static unsigned short InitialOutlineWidth() { return 3; }
  unsigned short OutlineWidth() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    // FIXME: Why is this stored as a float but converted to short?
    return OutlineWidthInternal().ToFloat();
  }
  void SetOutlineWidth(unsigned short v) {
    SetOutlineWidthInternal(LayoutUnit(v));
  }

  // outline-offset
  static int InitialOutlineOffset() { return 0; }
  int OutlineOffset() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    return OutlineOffsetInternal();
  }

  // -webkit-perspective-origin-x
  static Length InitialPerspectiveOriginX() { return Length(50.0, kPercent); }
  const Length& PerspectiveOriginX() const { return PerspectiveOrigin().X(); }
  void SetPerspectiveOriginX(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(v, PerspectiveOriginY()));
  }

  // -webkit-perspective-origin-y
  static Length InitialPerspectiveOriginY() { return Length(50.0, kPercent); }
  const Length& PerspectiveOriginY() const { return PerspectiveOrigin().Y(); }
  void SetPerspectiveOriginY(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(PerspectiveOriginX(), v));
  }

  // Transform properties.
  // transform (aka -webkit-transform)
  static EmptyTransformOperations InitialTransform() {
    return EmptyTransformOperations();
  }

  // -webkit-transform-origin-x
  static Length InitialTransformOriginX() { return Length(50.0, kPercent); }
  const Length& TransformOriginX() const { return GetTransformOrigin().X(); }
  void SetTransformOriginX(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(v, TransformOriginY(), TransformOriginZ()));
  }

  // -webkit-transform-origin-y
  static Length InitialTransformOriginY() { return Length(50.0, kPercent); }
  const Length& TransformOriginY() const { return GetTransformOrigin().Y(); }
  void SetTransformOriginY(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(TransformOriginX(), v, TransformOriginZ()));
  }

  // -webkit-transform-origin-z
  static float InitialTransformOriginZ() { return 0; }
  float TransformOriginZ() const { return GetTransformOrigin().Z(); }
  void SetTransformOriginZ(float f) {
    SetTransformOrigin(
        TransformOrigin(TransformOriginX(), TransformOriginY(), f));
  }

  // Scroll properties.
  // scroll-behavior
  static ScrollBehavior InitialScrollBehavior() { return kScrollBehaviorAuto; }

  // scroll-padding-block-start
  const Length& ScrollPaddingBlockStart() const {
    return IsHorizontalWritingMode() ? ScrollPaddingTop() : ScrollPaddingLeft();
  }
  void SetScrollPaddingBlockStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingTop(v);
    else
      SetScrollPaddingLeft(v);
  }

  // scroll-padding-block-end
  const Length& ScrollPaddingBlockEnd() const {
    return IsHorizontalWritingMode() ? ScrollPaddingBottom()
                                     : ScrollPaddingRight();
  }
  void SetScrollPaddingBlockEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingBottom(v);
    else
      SetScrollPaddingRight(v);
  }

  // scroll-padding-inline-start
  const Length& ScrollPaddingInlineStart() const {
    return IsHorizontalWritingMode() ? ScrollPaddingLeft() : ScrollPaddingTop();
  }
  void SetScrollPaddingInlineStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingLeft(v);
    else
      SetScrollPaddingTop(v);
  }

  // scroll-padding-inline-end
  const Length& ScrollPaddingInlineEnd() const {
    return IsHorizontalWritingMode() ? ScrollPaddingRight()
                                     : ScrollPaddingBottom();
  }
  void SetScrollPaddingInlineEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingRight(v);
    else
      SetScrollPaddingBottom(v);
  }

  // scroll-snap-margin-block-start
  const Length& ScrollSnapMarginBlockStart() const {
    return IsHorizontalWritingMode() ? ScrollSnapMarginTop()
                                     : ScrollSnapMarginLeft();
  }
  void SetScrollSnapMarginBlockStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollSnapMarginTop(v);
    else
      SetScrollSnapMarginLeft(v);
  }

  // scroll-snap-margin-block-end
  const Length& ScrollSnapMarginBlockEnd() const {
    return IsHorizontalWritingMode() ? ScrollSnapMarginBottom()
                                     : ScrollSnapMarginRight();
  }
  void SetScrollSnapMarginBlockEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollSnapMarginBottom(v);
    else
      SetScrollSnapMarginRight(v);
  }

  // scroll-snap-margin-inline-start
  const Length& ScrollSnapMarginInlineStart() const {
    return IsHorizontalWritingMode() ? ScrollSnapMarginLeft()
                                     : ScrollSnapMarginTop();
  }
  void SetScrollSnapMarginInlineStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollSnapMarginLeft(v);
    else
      SetScrollSnapMarginTop(v);
  }

  // scroll-snap-margin-inline-end
  const Length& ScrollSnapMarginInlineEnd() const {
    return IsHorizontalWritingMode() ? ScrollSnapMarginRight()
                                     : ScrollSnapMarginBottom();
  }
  void SetScrollSnapMarginInlineEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollSnapMarginRight(v);
    else
      SetScrollSnapMarginBottom(v);
  }

  // shape-image-threshold (aka -webkit-shape-image-threshold)
  static float InitialShapeImageThreshold() { return 0; }
  void SetShapeImageThreshold(float shape_image_threshold) {
    float clamped_shape_image_threshold =
        clampTo<float>(shape_image_threshold, 0, 1);
    SetShapeImageThresholdInternal(clamped_shape_image_threshold);
  }

  // shape-outside (aka -webkit-shape-outside)
  static ShapeValue* InitialShapeOutside() { return 0; }
  ShapeValue* ShapeOutside() const { return ShapeOutsideInternal().Get(); }
  bool ShapeOutsideDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ShapeOutside(), other.ShapeOutside());
  }

  // Text decoration properties.
  // text-decoration-skip
  static TextDecorationSkip InitialTextDecorationSkip() {
    return TextDecorationSkip::kObjects;
  }

  // touch-action
  static TouchAction InitialTouchAction() {
    return TouchAction::kTouchActionAuto;
  }
  TouchAction GetEffectiveTouchAction() const {
    return EffectiveTouchActionInternal();
  }
  void SetEffectiveTouchAction(TouchAction t) {
    return SetEffectiveTouchActionInternal(t);
  }

  // vertical-align
  static EVerticalAlign InitialVerticalAlign() {
    return EVerticalAlign::kBaseline;
  }
  EVerticalAlign VerticalAlign() const { return VerticalAlignInternal(); }
  const Length& GetVerticalAlignLength() const {
    return VerticalAlignLengthInternal();
  }
  void SetVerticalAlign(EVerticalAlign v) { SetVerticalAlignInternal(v); }
  void SetVerticalAlignLength(const Length& length) {
    SetVerticalAlignInternal(EVerticalAlign::kLength);
    SetVerticalAlignLengthInternal(length);
  }

  // z-index
  bool HasAutoZIndex() const { return HasAutoZIndexInternal(); }
  void SetZIndex(int v) {
    SetHasAutoZIndexInternal(false);
    SetZIndexInternal(v);
  }
  void SetHasAutoZIndex() {
    SetHasAutoZIndexInternal(true);
    SetZIndexInternal(0);
  }

  // zoom
  float EffectiveZoom() const { return EffectiveZoomInternal(); }
  bool SetZoom(float);
  bool SetEffectiveZoom(float);

  // -webkit-appearance
  static ControlPart InitialAppearance() { return kNoControlPart; }


  // -webkit-clip-path
  bool ClipPathDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ClipPath(), other.ClipPath());
  }

  // Mask properties.
  // -webkit-mask-box-image-outset
  const BorderImageLengthBox& MaskBoxImageOutset() const {
    return MaskBoxImageInternal().Outset();
  }
  void SetMaskBoxImageOutset(const BorderImageLengthBox& outset) {
    MutableMaskBoxImageInternal().SetOutset(outset);
  }

  // -webkit-mask-box-image-slice
  const LengthBox& MaskBoxImageSlices() const {
    return MaskBoxImageInternal().ImageSlices();
  }
  void SetMaskBoxImageSlices(const LengthBox& slices) {
    MutableMaskBoxImageInternal().SetImageSlices(slices);
  }

  // -webkit-mask-box-image-source
  static StyleImage* InitialMaskBoxImageSource() { return 0; }
  StyleImage* MaskBoxImageSource() const {
    return MaskBoxImageInternal().GetImage();
  }
  void SetMaskBoxImageSource(StyleImage* v) {
    MutableMaskBoxImageInternal().SetImage(v);
  }

  // -webkit-mask-box-image-width
  const BorderImageLengthBox& MaskBoxImageWidth() const {
    return MaskBoxImageInternal().BorderSlices();
  }
  void SetMaskBoxImageWidth(const BorderImageLengthBox& slices) {
    MutableMaskBoxImageInternal().SetBorderSlices(slices);
  }

  // Inherited properties.

  // color
  static Color InitialColor() { return Color::kBlack; }
  void SetColor(const Color&);

  // line-height
  static Length InitialLineHeight() { return Length(-100.0, kPercent); }
  Length LineHeight() const;
  CORE_EXPORT void SetLineHeight(const Length& specified_line_height);

  // List style properties.
  // list-style-image
  static StyleImage* InitialListStyleImage() { return 0; }
  StyleImage* ListStyleImage() const;
  void SetListStyleImage(StyleImage*);

  // quotes
  bool QuotesDataEquivalent(const ComputedStyle&) const;

  // text-shadow
  bool TextShadowDataEquivalent(const ComputedStyle&) const;

  // Text emphasis properties.
  static TextEmphasisMark InitialTextEmphasisMark() {
    return TextEmphasisMark::kNone;
  }
  TextEmphasisMark GetTextEmphasisMark() const;
  void SetTextEmphasisMark(TextEmphasisMark mark) {
    SetTextEmphasisMarkInternal(mark);
  }
  const AtomicString& TextEmphasisMarkString() const;

  // -webkit-text-emphasis-color (aka -epub-text-emphasis-color)
  void SetTextEmphasisColor(const StyleColor& color) {
    SetTextEmphasisColorInternal(color.Resolve(Color()));
    SetTextEmphasisColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // -webkit-line-clamp
  static LineClampValue InitialLineClamp() { return LineClampValue(); }
  const LineClampValue& LineClamp() const { return LineClampInternal(); }
  void SetLineClamp(LineClampValue c) { SetLineClampInternal(c); }

  // -webkit-text-fill-color
  void SetTextFillColor(const StyleColor& color) {
    SetTextFillColorInternal(color.Resolve(Color()));
    SetTextFillColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // -webkit-text-stroke-color
  void SetTextStrokeColor(const StyleColor& color) {
    SetTextStrokeColorInternal(color.Resolve(Color()));
    SetTextStrokeColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // caret-color
  void SetCaretColor(const StyleAutoColor& color) {
    SetCaretColorInternal(color.Resolve(Color()));
    SetCaretColorIsCurrentColorInternal(color.IsCurrentColor());
    SetCaretColorIsAutoInternal(color.IsAutoColor());
  }

  // Font properties.
  CORE_EXPORT const Font& GetFont() const;
  CORE_EXPORT void SetFont(const Font&);
  CORE_EXPORT const FontDescription& GetFontDescription() const;
  CORE_EXPORT bool SetFontDescription(const FontDescription&);
  bool HasIdenticalAscentDescentAndLineGap(const ComputedStyle& other) const;

  // font-size
  int FontSize() const;
  CORE_EXPORT float SpecifiedFontSize() const;
  CORE_EXPORT float ComputedFontSize() const;
  LayoutUnit ComputedFontSizeAsFixed() const;

  // font-size-adjust
  float FontSizeAdjust() const;
  bool HasFontSizeAdjust() const;

  // font-weight
  CORE_EXPORT FontWeight GetFontWeight() const;

  // font-stretch
  FontStretch GetFontStretch() const;

  // -webkit-locale
  const AtomicString& Locale() const {
    return LayoutLocale::LocaleString(GetFontDescription().Locale());
  }
  AtomicString LocaleForLineBreakIterator() const;

  // FIXME: Remove letter-spacing/word-spacing and replace them with respective
  // FontBuilder calls.  letter-spacing
  static float InitialLetterWordSpacing() { return 0.0f; }
  float LetterSpacing() const;
  void SetLetterSpacing(float);

  // word-spacing
  float WordSpacing() const;
  void SetWordSpacing(float);

  // SVG properties.
  const SVGComputedStyle& SvgStyle() const { return *svg_style_.Get(); }
  SVGComputedStyle& AccessSVGStyle() { return *svg_style_.Access(); }

  // baseline-shift
  EBaselineShift BaselineShift() const { return SvgStyle().BaselineShift(); }
  const Length& BaselineShiftValue() const {
    return SvgStyle().BaselineShiftValue();
  }
  void SetBaselineShiftValue(const Length& value) {
    SVGComputedStyle& svg_style = AccessSVGStyle();
    svg_style.SetBaselineShift(BS_LENGTH);
    svg_style.SetBaselineShiftValue(value);
  }

  // cx
  void SetCx(const Length& cx) { AccessSVGStyle().SetCx(cx); }

  // cy
  void SetCy(const Length& cy) { AccessSVGStyle().SetCy(cy); }

  // d
  void SetD(RefPtr<StylePath> d) { AccessSVGStyle().SetD(std::move(d)); }

  // x
  void SetX(const Length& x) { AccessSVGStyle().SetX(x); }

  // y
  void SetY(const Length& y) { AccessSVGStyle().SetY(y); }

  // r
  void SetR(const Length& r) { AccessSVGStyle().SetR(r); }

  // rx
  void SetRx(const Length& rx) { AccessSVGStyle().SetRx(rx); }

  // ry
  void SetRy(const Length& ry) { AccessSVGStyle().SetRy(ry); }

  // fill-opacity
  float FillOpacity() const { return SvgStyle().FillOpacity(); }
  void SetFillOpacity(float f) { AccessSVGStyle().SetFillOpacity(f); }

  // Fill utiltiy functions.
  const SVGPaintType& FillPaintType() const {
    return SvgStyle().FillPaintType();
  }
  Color FillPaintColor() const { return SvgStyle().FillPaintColor(); }

  // stop-color
  void SetStopColor(const Color& c) { AccessSVGStyle().SetStopColor(c); }

  // flood-color
  void SetFloodColor(const Color& c) { AccessSVGStyle().SetFloodColor(c); }

  // lighting-color
  void SetLightingColor(const Color& c) {
    AccessSVGStyle().SetLightingColor(c);
  }

  // flood-opacity
  float FloodOpacity() const { return SvgStyle().FloodOpacity(); }
  void SetFloodOpacity(float f) { AccessSVGStyle().SetFloodOpacity(f); }

  // stop-opacity
  float StopOpacity() const { return SvgStyle().StopOpacity(); }
  void SetStopOpacity(float f) { AccessSVGStyle().SetStopOpacity(f); }

  // stroke
  const SVGPaintType& StrokePaintType() const {
    return SvgStyle().StrokePaintType();
  }
  Color StrokePaintColor() const { return SvgStyle().StrokePaintColor(); }

  // stroke-dasharray
  SVGDashArray* StrokeDashArray() const { return SvgStyle().StrokeDashArray(); }
  void SetStrokeDashArray(RefPtr<SVGDashArray> array) {
    AccessSVGStyle().SetStrokeDashArray(std::move(array));
  }

  // stroke-dashoffset
  const Length& StrokeDashOffset() const {
    return SvgStyle().StrokeDashOffset();
  }
  void SetStrokeDashOffset(const Length& d) {
    AccessSVGStyle().SetStrokeDashOffset(d);
  }

  // stroke-miterlimit
  float StrokeMiterLimit() const { return SvgStyle().StrokeMiterLimit(); }
  void SetStrokeMiterLimit(float f) { AccessSVGStyle().SetStrokeMiterLimit(f); }

  // stroke-opacity
  float StrokeOpacity() const { return SvgStyle().StrokeOpacity(); }
  void SetStrokeOpacity(float f) { AccessSVGStyle().SetStrokeOpacity(f); }

  // stroke-width
  const UnzoomedLength& StrokeWidth() const { return SvgStyle().StrokeWidth(); }
  void SetStrokeWidth(const UnzoomedLength& w) {
    AccessSVGStyle().SetStrokeWidth(w);
  }

  // Comparison operators
  // TODO(shend): Replace callers of operator== wth a named method instead, e.g.
  // inheritedEquals().
  CORE_EXPORT bool operator==(const ComputedStyle& other) const;
  bool operator!=(const ComputedStyle& other) const {
    return !(*this == other);
  }

  bool InheritedEqual(const ComputedStyle&) const;
  bool NonInheritedEqual(const ComputedStyle&) const;
  inline bool IndependentInheritedEqual(const ComputedStyle&) const;
  inline bool NonIndependentInheritedEqual(const ComputedStyle&) const;
  bool LoadingCustomFontsEqual(const ComputedStyle&) const;
  bool InheritedDataShared(const ComputedStyle&) const;

  bool HasChildDependentFlags() const {
    return EmptyStateInternal() || HasExplicitlyInheritedProperties();
  }
  void CopyChildDependentFlagsFrom(const ComputedStyle&);

  // Counters.
  const CounterDirectiveMap* GetCounterDirectives() const;
  CounterDirectiveMap& AccessCounterDirectives();
  const CounterDirectives GetCounterDirectives(
      const AtomicString& identifier) const;
  bool CounterDirectivesEqual(const ComputedStyle& other) const {
    // If the counter directives change, trigger a relayout to re-calculate
    // counter values and rebuild the counter node tree.
    return DataEquivalent(CounterDirectivesInternal().get(),
                          other.CounterDirectivesInternal().get());
  }
  void ClearIncrementDirectives();
  void ClearResetDirectives();

  // Variables.
  static StyleInheritedVariables* InitialInheritedVariables() {
    return nullptr;
  }
  static StyleNonInheritedVariables* InitialNonInheritedVariables() {
    return nullptr;
  }

  StyleInheritedVariables* InheritedVariables() const;
  StyleNonInheritedVariables* NonInheritedVariables() const;

  void SetUnresolvedInheritedVariable(const AtomicString&,
                                      RefPtr<CSSVariableData>);
  void SetUnresolvedNonInheritedVariable(const AtomicString&,
                                         RefPtr<CSSVariableData>);

  void SetResolvedUnregisteredVariable(const AtomicString&,
                                       RefPtr<CSSVariableData>);
  void SetResolvedInheritedVariable(const AtomicString&,
                                    RefPtr<CSSVariableData>,
                                    const CSSValue*);
  void SetResolvedNonInheritedVariable(const AtomicString&,
                                       RefPtr<CSSVariableData>,
                                       const CSSValue*);

  void RemoveVariable(const AtomicString&, bool is_inherited_property);

  // Handles both inherited and non-inherited variables
  CSSVariableData* GetVariable(const AtomicString&) const;

  CSSVariableData* GetVariable(const AtomicString&,
                               bool is_inherited_property) const;

  const CSSValue* GetRegisteredVariable(const AtomicString&,
                                        bool is_inherited_property) const;

  const CSSValue* GetRegisteredVariable(const AtomicString&) const;

  // Animations.
  CSSAnimationData& AccessAnimations();
  const CSSAnimationData* Animations() const {
    return AnimationsInternal().get();
  }

  // Transitions.
  const CSSTransitionData* Transitions() const {
    return TransitionsInternal().get();
  }
  CSSTransitionData& AccessTransitions();

  // Callback selectors.
  const Vector<String>& CallbackSelectors() const {
    return CallbackSelectorsInternal();
  }
  void AddCallbackSelector(const String& selector);

  // Non-property flags.
  bool EmptyState() const { return EmptyStateInternal(); }
  void SetEmptyState(bool b) {
    SetUnique();
    SetEmptyStateInternal(b);
  }

  float TextAutosizingMultiplier() const {
    return TextAutosizingMultiplierInternal();
  }
  CORE_EXPORT void SetTextAutosizingMultiplier(float);

  // Column utility functions.
  void ClearMultiCol();
  bool SpecifiesColumns() const {
    return !HasAutoColumnCount() || !HasAutoColumnWidth();
  }
  bool ColumnRuleIsTransparent() const {
    return !ColumnRuleColorIsCurrentColor() &&
           !ColumnRuleColorInternal().Alpha();
  }
  bool ColumnRuleEquivalent(const ComputedStyle& other_style) const;
  void InheritColumnPropertiesFrom(const ComputedStyle& parent) {
    SetColumnGapInternal(parent.ColumnGap());
    SetColumnWidthInternal(parent.ColumnWidth());
    SetVisitedLinkColumnRuleColorInternal(
        parent.VisitedLinkColumnRuleColorInternal());
    SetColumnRuleColorInternal(parent.ColumnRuleColorInternal());
    SetColumnCountInternal(parent.ColumnCount());
    SetColumnRuleStyle(parent.ColumnRuleStyle());
    SetColumnAutoCountInternal(parent.ColumnAutoCountInternal());
    SetColumnAutoWidthInternal(parent.ColumnAutoWidthInternal());
    SetColumnFill(parent.GetColumnFill());
    SetColumnNormalGapInternal(parent.ColumnNormalGapInternal());
    SetColumnRuleColorIsCurrentColor(parent.ColumnRuleColorIsCurrentColor());
    SetColumnSpan(parent.GetColumnSpan());
  }

  // Flex utility functions.
  bool IsColumnFlexDirection() const {
    return FlexDirection() == EFlexDirection::kColumn ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool IsReverseFlexDirection() const {
    return FlexDirection() == EFlexDirection::kRowReverse ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool HasBoxReflect() const { return BoxReflect(); }
  bool ReflectionDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxReflect(), other.BoxReflect());
  }

  // Mask utility functions.
  bool HasMask() const {
    return MaskInternal().HasImage() || MaskBoxImageInternal().HasImage();
  }
  StyleImage* MaskImage() const { return MaskInternal().GetImage(); }
  FillLayer& AccessMaskLayers() { return MutableMaskInternal(); }
  const FillLayer& MaskLayers() const { return MaskInternal(); }
  const NinePieceImage& MaskBoxImage() const { return MaskBoxImageInternal(); }
  bool MaskBoxImageSlicesFill() const { return MaskBoxImageInternal().Fill(); }
  void AdjustMaskLayers() {
    if (MaskLayers().Next()) {
      AccessMaskLayers().CullEmptyLayers();
      AccessMaskLayers().FillUnsetProperties();
    }
  }
  void SetMaskBoxImage(const NinePieceImage& b) { SetMaskBoxImageInternal(b); }
  void SetMaskBoxImageSlicesFill(bool fill) {
    MutableMaskBoxImageInternal().SetFill(fill);
  }

  // Text-combine utility functions.
  bool HasTextCombine() const { return TextCombine() != ETextCombine::kNone; }

  // Grid utility functions.
  GridAutoFlow GetGridAutoFlow() const { return GridAutoFlowInternal(); }
  bool IsGridAutoFlowDirectionRow() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowDirectionRow) ==
           kInternalAutoFlowDirectionRow;
  }
  bool IsGridAutoFlowDirectionColumn() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowDirectionColumn) ==
           kInternalAutoFlowDirectionColumn;
  }
  bool IsGridAutoFlowAlgorithmSparse() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowAlgorithmSparse) ==
           kInternalAutoFlowAlgorithmSparse;
  }
  bool IsGridAutoFlowAlgorithmDense() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowAlgorithmDense) ==
           kInternalAutoFlowAlgorithmDense;
  }

  // align-content utility functions.
  ContentPosition AlignContentPosition() const {
    return AlignContent().GetPosition();
  }
  ContentDistributionType AlignContentDistribution() const {
    return AlignContent().Distribution();
  }
  OverflowAlignment AlignContentOverflowAlignment() const {
    return AlignContent().Overflow();
  }
  void SetAlignContentPosition(ContentPosition position) {
    MutableAlignContentInternal().SetPosition(position);
  }
  void SetAlignContentDistribution(ContentDistributionType distribution) {
    MutableAlignContentInternal().SetDistribution(distribution);
  }
  void SetAlignContentOverflow(OverflowAlignment overflow) {
    MutableAlignContentInternal().SetOverflow(overflow);
  }

  // justify-content utility functions.
  ContentPosition JustifyContentPosition() const {
    return JustifyContent().GetPosition();
  }
  ContentDistributionType JustifyContentDistribution() const {
    return JustifyContent().Distribution();
  }
  OverflowAlignment JustifyContentOverflowAlignment() const {
    return JustifyContent().Overflow();
  }
  void SetJustifyContentPosition(ContentPosition position) {
    MutableJustifyContentInternal().SetPosition(position);
  }
  void SetJustifyContentDistribution(ContentDistributionType distribution) {
    MutableJustifyContentInternal().SetDistribution(distribution);
  }
  void SetJustifyContentOverflow(OverflowAlignment overflow) {
    MutableJustifyContentInternal().SetOverflow(overflow);
  }

  // align-items utility functions.
  ItemPosition AlignItemsPosition() const { return AlignItems().GetPosition(); }
  OverflowAlignment AlignItemsOverflowAlignment() const {
    return AlignItems().Overflow();
  }
  void SetAlignItemsPosition(ItemPosition position) {
    MutableAlignItemsInternal().SetPosition(position);
  }
  void SetAlignItemsOverflow(OverflowAlignment overflow) {
    MutableAlignItemsInternal().SetOverflow(overflow);
  }

  // justify-items utility functions.
  ItemPosition JustifyItemsPosition() const {
    return JustifyItems().GetPosition();
  }
  OverflowAlignment JustifyItemsOverflowAlignment() const {
    return JustifyItems().Overflow();
  }
  ItemPositionType JustifyItemsPositionType() const {
    return JustifyItems().PositionType();
  }
  void SetJustifyItemsPosition(ItemPosition position) {
    MutableJustifyItemsInternal().SetPosition(position);
  }
  void SetJustifyItemsOverflow(OverflowAlignment overflow) {
    MutableJustifyItemsInternal().SetOverflow(overflow);
  }
  void SetJustifyItemsPositionType(ItemPositionType position_type) {
    MutableJustifyItemsInternal().SetPositionType(position_type);
  }

  // align-self utility functions.
  ItemPosition AlignSelfPosition() const { return AlignSelf().GetPosition(); }
  OverflowAlignment AlignSelfOverflowAlignment() const {
    return AlignSelf().Overflow();
  }
  void SetAlignSelfPosition(ItemPosition position) {
    MutableAlignSelfInternal().SetPosition(position);
  }
  void SetAlignSelfOverflow(OverflowAlignment overflow) {
    MutableAlignSelfInternal().SetOverflow(overflow);
  }

  // justify-self utility functions.
  ItemPosition JustifySelfPosition() const {
    return JustifySelf().GetPosition();
  }
  OverflowAlignment JustifySelfOverflowAlignment() const {
    return JustifySelf().Overflow();
  }
  void SetJustifySelfPosition(ItemPosition position) {
    MutableJustifySelfInternal().SetPosition(position);
  }
  void SetJustifySelfOverflow(OverflowAlignment overflow) {
    MutableJustifySelfInternal().SetOverflow(overflow);
  }

  // Writing mode utility functions.
  bool IsHorizontalWritingMode() const {
    return blink::IsHorizontalWritingMode(GetWritingMode());
  }
  bool IsFlippedLinesWritingMode() const {
    return blink::IsFlippedLinesWritingMode(GetWritingMode());
  }
  bool IsFlippedBlocksWritingMode() const {
    return blink::IsFlippedBlocksWritingMode(GetWritingMode());
  }

  // Will-change utility functions.
  bool HasWillChangeCompositingHint() const;
  bool HasWillChangeOpacityHint() const {
    return WillChangeProperties().Contains(CSSPropertyOpacity);
  }
  bool HasWillChangeTransformHint() const;

  // Hyphen utility functions.
  Hyphenation* GetHyphenation() const;
  const AtomicString& HyphenString() const;

  // text-align utility functions.
  using ComputedStyleBase::GetTextAlign;
  ETextAlign GetTextAlign(bool is_last_line) const;

  // text-indent utility functions.
  bool ShouldUseTextIndent(bool is_first_line,
                           bool is_after_forced_break) const;

  // Line-height utility functions.
  const Length& SpecifiedLineHeight() const;
  int ComputedLineHeight() const;
  LayoutUnit ComputedLineHeightAsFixed() const;

  // Width/height utility functions.
  const Length& LogicalWidth() const {
    return IsHorizontalWritingMode() ? Width() : Height();
  }
  const Length& LogicalHeight() const {
    return IsHorizontalWritingMode() ? Height() : Width();
  }
  void SetLogicalWidth(const Length& v) {
    if (IsHorizontalWritingMode()) {
      SetWidth(v);
    } else {
      SetHeight(v);
    }
  }

  void SetLogicalHeight(const Length& v) {
    if (IsHorizontalWritingMode()) {
      SetHeight(v);
    } else {
      SetWidth(v);
    }
  }
  const Length& LogicalMaxWidth() const {
    return IsHorizontalWritingMode() ? MaxWidth() : MaxHeight();
  }
  const Length& LogicalMaxHeight() const {
    return IsHorizontalWritingMode() ? MaxHeight() : MaxWidth();
  }
  const Length& LogicalMinWidth() const {
    return IsHorizontalWritingMode() ? MinWidth() : MinHeight();
  }
  const Length& LogicalMinHeight() const {
    return IsHorizontalWritingMode() ? MinHeight() : MinWidth();
  }

  // Margin utility functions.
  bool HasMargin() const {
    return !MarginLeft().IsZero() || !MarginRight().IsZero() ||
           !MarginTop().IsZero() || !MarginBottom().IsZero();
  }
  bool HasMarginBeforeQuirk() const { return MarginBefore().Quirk(); }
  bool HasMarginAfterQuirk() const { return MarginAfter().Quirk(); }
  const Length& MarginBefore() const { return MarginBeforeUsing(*this); }
  const Length& MarginAfter() const { return MarginAfterUsing(*this); }
  const Length& MarginStart() const { return MarginStartUsing(*this); }
  const Length& MarginEnd() const { return MarginEndUsing(*this); }
  const Length& MarginOver() const {
    return PhysicalMarginToLogical(*this).Over();
  }
  const Length& MarginUnder() const {
    return PhysicalMarginToLogical(*this).Under();
  }
  const Length& MarginStartUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).Start();
  }
  const Length& MarginEndUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).End();
  }
  const Length& MarginBeforeUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).Before();
  }
  const Length& MarginAfterUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).After();
  }
  void SetMarginStart(const Length&);
  void SetMarginEnd(const Length&);
  bool MarginEqual(const ComputedStyle& other) const {
    return MarginTop() == other.MarginTop() &&
           MarginLeft() == other.MarginLeft() &&
           MarginRight() == other.MarginRight() &&
           MarginBottom() == other.MarginBottom();
  }

  // Padding utility functions.
  const Length& PaddingBefore() const {
    return PhysicalPaddingToLogical().Before();
  }
  const Length& PaddingAfter() const {
    return PhysicalPaddingToLogical().After();
  }
  const Length& PaddingStart() const {
    return PhysicalPaddingToLogical().Start();
  }
  const Length& PaddingEnd() const { return PhysicalPaddingToLogical().End(); }
  const Length& PaddingOver() const {
    return PhysicalPaddingToLogical().Over();
  }
  const Length& PaddingUnder() const {
    return PhysicalPaddingToLogical().Under();
  }
  bool HasPadding() const {
    return !PaddingLeft().IsZero() || !PaddingRight().IsZero() ||
           !PaddingTop().IsZero() || !PaddingBottom().IsZero();
  }
  void ResetPadding() {
    SetPaddingTop(kFixed);
    SetPaddingBottom(kFixed);
    SetPaddingLeft(kFixed);
    SetPaddingRight(kFixed);
  }
  void SetPadding(const LengthBox& b) {
    SetPaddingTop(b.top_);
    SetPaddingBottom(b.bottom_);
    SetPaddingLeft(b.left_);
    SetPaddingRight(b.right_);
  }
  bool PaddingEqual(const ComputedStyle& other) const {
    return PaddingTop() == other.PaddingTop() &&
           PaddingLeft() == other.PaddingLeft() &&
           PaddingRight() == other.PaddingRight() &&
           PaddingBottom() == other.PaddingBottom();
  }
  bool PaddingEqual(const LengthBox& other) const {
    return PaddingTop() == other.Top() && PaddingLeft() == other.Left() &&
           PaddingRight() == other.Right() && PaddingBottom() == other.Bottom();
  }

  // Border utility functions
  LayoutRectOutsets ImageOutsets(const NinePieceImage&) const;
  bool HasBorderImageOutsets() const {
    return BorderImage().HasImage() && BorderImage().Outset().NonZero();
  }
  LayoutRectOutsets BorderImageOutsets() const {
    return ImageOutsets(BorderImage());
  }
  bool BorderImageSlicesFill() const { return BorderImage().Fill(); }

  void SetBorderImageSlicesFill(bool);
  const BorderValue BorderLeft() const {
    return BorderValue(BorderLeftStyle(), BorderLeftColor(),
                       BorderLeftWidthInternal().ToFloat());
  }
  const BorderValue BorderRight() const {
    return BorderValue(BorderRightStyle(), BorderRightColor(),
                       BorderRightWidthInternal().ToFloat());
  }
  const BorderValue BorderTop() const {
    return BorderValue(BorderTopStyle(), BorderTopColor(),
                       BorderTopWidthInternal().ToFloat());
  }
  const BorderValue BorderBottom() const {
    return BorderValue(BorderBottomStyle(), BorderBottomColor(),
                       BorderBottomWidthInternal().ToFloat());
  }

  bool BorderSizeEquals(const ComputedStyle& o) const {
    return BorderLeftWidthInternal() == o.BorderLeftWidthInternal() &&
           BorderTopWidthInternal() == o.BorderTopWidthInternal() &&
           BorderRightWidthInternal() == o.BorderRightWidthInternal() &&
           BorderBottomWidthInternal() == o.BorderBottomWidthInternal();
  }

  BorderValue BorderBeforeUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).Before();
  }
  BorderValue BorderAfterUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).After();
  }
  BorderValue BorderStartUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).Start();
  }
  BorderValue BorderEndUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).End();
  }

  BorderValue BorderBefore() const { return BorderBeforeUsing(*this); }
  BorderValue BorderAfter() const { return BorderAfterUsing(*this); }
  BorderValue BorderStart() const { return BorderStartUsing(*this); }
  BorderValue BorderEnd() const { return BorderEndUsing(*this); }

  float BorderAfterWidth() const {
    return PhysicalBorderWidthToLogical().After();
  }
  float BorderBeforeWidth() const {
    return PhysicalBorderWidthToLogical().Before();
  }
  float BorderEndWidth() const { return PhysicalBorderWidthToLogical().End(); }
  float BorderStartWidth() const {
    return PhysicalBorderWidthToLogical().Start();
  }
  float BorderOverWidth() const {
    return PhysicalBorderWidthToLogical().Over();
  }
  float BorderUnderWidth() const {
    return PhysicalBorderWidthToLogical().Under();
  }

  EBorderStyle BorderAfterStyle() const {
    return PhysicalBorderStyleToLogical().After();
  }
  EBorderStyle BorderBeforeStyle() const {
    return PhysicalBorderStyleToLogical().Before();
  }
  EBorderStyle BorderEndStyle() const {
    return PhysicalBorderStyleToLogical().End();
  }
  EBorderStyle BorderStartStyle() const {
    return PhysicalBorderStyleToLogical().Start();
  }

  bool HasBorderFill() const {
    return BorderImage().HasImage() && BorderImage().Fill();
  }
  bool HasBorder() const {
    return BorderLeftNonZero() || BorderRightNonZero() || BorderTopNonZero() ||
           BorderBottomNonZero();
  }
  bool HasBorderDecoration() const { return HasBorder() || HasBorderFill(); }
  bool HasBorderRadius() const {
    if (!BorderTopLeftRadius().Width().IsZero())
      return true;
    if (!BorderTopRightRadius().Width().IsZero())
      return true;
    if (!BorderBottomLeftRadius().Width().IsZero())
      return true;
    if (!BorderBottomRightRadius().Width().IsZero())
      return true;
    return false;
  }
  bool HasBorderColorReferencingCurrentColor() const {
    return (BorderLeftNonZero() && BorderLeftColor().IsCurrentColor()) ||
           (BorderRightNonZero() && BorderRightColor().IsCurrentColor()) ||
           (BorderTopNonZero() && BorderTopColor().IsCurrentColor()) ||
           (BorderBottomNonZero() && BorderBottomColor().IsCurrentColor());
  }

  bool RadiiEqual(const ComputedStyle& o) const {
    return BorderTopLeftRadius() == o.BorderTopLeftRadius() &&
           BorderTopRightRadius() == o.BorderTopRightRadius() &&
           BorderBottomLeftRadius() == o.BorderBottomLeftRadius() &&
           BorderBottomRightRadius() == o.BorderBottomRightRadius();
  }

  bool BorderLeftEquals(const ComputedStyle& o) const {
    return BorderLeftWidthInternal() == o.BorderLeftWidthInternal() &&
           BorderLeftStyle() == o.BorderLeftStyle() &&
           BorderLeftColor() == o.BorderLeftColor() &&
           BorderLeftColorIsCurrentColor() == o.BorderLeftColorIsCurrentColor();
  }
  bool BorderLeftEquals(const BorderValue& o) const {
    return BorderLeftWidthInternal().ToFloat() == o.Width() &&
           BorderLeftStyle() == o.Style() &&
           BorderLeftColor() == o.GetColor() &&
           BorderLeftColorIsCurrentColor() == o.ColorIsCurrentColor();
  }

  bool BorderLeftVisuallyEqual(const ComputedStyle& o) const {
    if (BorderLeftStyle() == EBorderStyle::kNone &&
        o.BorderLeftStyle() == EBorderStyle::kNone)
      return true;
    if (BorderLeftStyle() == EBorderStyle::kHidden &&
        o.BorderLeftStyle() == EBorderStyle::kHidden)
      return true;
    return BorderLeftEquals(o);
  }

  bool BorderRightEquals(const ComputedStyle& o) const {
    return BorderRightWidthInternal() == o.BorderRightWidthInternal() &&
           BorderRightStyle() == o.BorderRightStyle() &&
           BorderRightColor() == o.BorderRightColor() &&
           BorderRightColorIsCurrentColor() ==
               o.BorderRightColorIsCurrentColor();
  }
  bool BorderRightEquals(const BorderValue& o) const {
    return BorderRightWidthInternal().ToFloat() == o.Width() &&
           BorderRightStyle() == o.Style() &&
           BorderRightColor() == o.GetColor() &&
           BorderRightColorIsCurrentColor() == o.ColorIsCurrentColor();
  }

  bool BorderRightVisuallyEqual(const ComputedStyle& o) const {
    if (BorderRightStyle() == EBorderStyle::kNone &&
        o.BorderRightStyle() == EBorderStyle::kNone)
      return true;
    if (BorderRightStyle() == EBorderStyle::kHidden &&
        o.BorderRightStyle() == EBorderStyle::kHidden)
      return true;
    return BorderRightEquals(o);
  }

  bool BorderTopVisuallyEqual(const ComputedStyle& o) const {
    if (BorderTopStyle() == EBorderStyle::kNone &&
        o.BorderTopStyle() == EBorderStyle::kNone)
      return true;
    if (BorderTopStyle() == EBorderStyle::kHidden &&
        o.BorderTopStyle() == EBorderStyle::kHidden)
      return true;
    return BorderTopEquals(o);
  }

  bool BorderTopEquals(const ComputedStyle& o) const {
    return BorderTopWidthInternal() == o.BorderTopWidthInternal() &&
           BorderTopStyle() == o.BorderTopStyle() &&
           BorderTopColor() == o.BorderTopColor() &&
           BorderTopColorIsCurrentColor() == o.BorderTopColorIsCurrentColor();
  }
  bool BorderTopEquals(const BorderValue& o) const {
    return BorderTopWidthInternal().ToFloat() == o.Width() &&
           BorderTopStyle() == o.Style() && BorderTopColor() == o.GetColor() &&
           BorderTopColorIsCurrentColor() == o.ColorIsCurrentColor();
  }

  bool BorderBottomVisuallyEqual(const ComputedStyle& o) const {
    if (BorderBottomStyle() == EBorderStyle::kNone &&
        o.BorderBottomStyle() == EBorderStyle::kNone)
      return true;
    if (BorderBottomStyle() == EBorderStyle::kHidden &&
        o.BorderBottomStyle() == EBorderStyle::kHidden)
      return true;
    return BorderBottomEquals(o);
  }

  bool BorderBottomEquals(const ComputedStyle& o) const {
    return BorderBottomWidthInternal() == o.BorderBottomWidthInternal() &&
           BorderBottomStyle() == o.BorderBottomStyle() &&
           BorderBottomColor() == o.BorderBottomColor() &&
           BorderBottomColorIsCurrentColor() ==
               o.BorderBottomColorIsCurrentColor();
  }
  bool BorderBottomEquals(const BorderValue& o) const {
    return BorderBottomWidthInternal().ToFloat() == o.Width() &&
           BorderBottomStyle() == o.Style() &&
           BorderBottomColor() == o.GetColor() &&
           BorderBottomColorIsCurrentColor() == o.ColorIsCurrentColor();
  }

  bool BorderEquals(const ComputedStyle& o) const {
    return BorderLeftEquals(o) && BorderRightEquals(o) && BorderTopEquals(o) &&
           BorderBottomEquals(o) && BorderImage() == o.BorderImage();
  }

  bool BorderVisuallyEqual(const ComputedStyle& o) const {
    return BorderLeftVisuallyEqual(o) && BorderRightVisuallyEqual(o) &&
           BorderTopVisuallyEqual(o) && BorderBottomVisuallyEqual(o) &&
           BorderImage() == o.BorderImage();
  }

  bool BorderVisualOverflowEqual(const ComputedStyle& o) const {
    return BorderImage().Outset() == o.BorderImage().Outset();
  }

  void ResetBorder() {
    ResetBorderImage();
    ResetBorderTop();
    ResetBorderRight();
    ResetBorderBottom();
    ResetBorderLeft();
    ResetBorderTopLeftRadius();
    ResetBorderTopRightRadius();
    ResetBorderBottomLeftRadius();
    ResetBorderBottomRightRadius();
  }

  void ResetBorderTop() {
    SetBorderTopStyle(EBorderStyle::kNone);
    SetBorderTopWidth(3);
    SetBorderTopColorInternal(0);
    SetBorderTopColorIsCurrentColor(true);
  }
  void ResetBorderRight() {
    SetBorderRightStyle(EBorderStyle::kNone);
    SetBorderRightWidth(3);
    SetBorderRightColorInternal(0);
    SetBorderRightColorIsCurrentColor(true);
  }
  void ResetBorderBottom() {
    SetBorderBottomStyle(EBorderStyle::kNone);
    SetBorderBottomWidth(3);
    SetBorderBottomColorInternal(0);
    SetBorderBottomColorIsCurrentColor(true);
  }
  void ResetBorderLeft() {
    SetBorderLeftStyle(EBorderStyle::kNone);
    SetBorderLeftWidth(3);
    SetBorderLeftColorInternal(0);
    SetBorderLeftColorIsCurrentColor(true);
  }

  void SetBorderRadius(const LengthSize& s) {
    SetBorderTopLeftRadius(s);
    SetBorderTopRightRadius(s);
    SetBorderBottomLeftRadius(s);
    SetBorderBottomRightRadius(s);
  }
  void SetBorderRadius(const IntSize& s) {
    SetBorderRadius(
        LengthSize(Length(s.Width(), kFixed), Length(s.Height(), kFixed)));
  }

  FloatRoundedRect GetRoundedBorderFor(
      const LayoutRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true) const;
  FloatRoundedRect GetRoundedInnerBorderFor(
      const LayoutRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true) const;
  FloatRoundedRect GetRoundedInnerBorderFor(
      const LayoutRect& border_rect,
      const LayoutRectOutsets& insets,
      bool include_logical_left_edge,
      bool include_logical_right_edge) const;

  bool CanRenderBorderImage() const;

  // Float utility functions.
  bool IsFloating() const { return Floating() != EFloat::kNone; }

  // Mix-blend-mode utility functions.
  bool HasBlendMode() const { return BlendMode() != WebBlendMode::kNormal; }

  // Motion utility functions.
  bool HasOffset() const {
    return (OffsetPosition().X() != Length(kAuto)) || OffsetPath();
  }

  // Direction utility functions.
  bool IsLeftToRightDirection() const {
    return Direction() == TextDirection::kLtr;
  }

  // Perspective utility functions.
  bool HasPerspective() const { return Perspective() > 0; }

  // Outline utility functions.
  bool HasOutline() const {
    return OutlineWidth() > 0 && OutlineStyle() > EBorderStyle::kHidden;
  }
  CORE_EXPORT int OutlineOutsetExtent() const;
  CORE_EXPORT float GetOutlineStrokeWidthForFocusRing() const;
  bool HasOutlineWithCurrentColor() const {
    return HasOutline() && OutlineColor().IsCurrentColor();
  }

  // Position utility functions.
  bool HasOutOfFlowPosition() const {
    return GetPosition() == EPosition::kAbsolute ||
           GetPosition() == EPosition::kFixed;
  }
  bool HasInFlowPosition() const {
    return GetPosition() == EPosition::kRelative ||
           GetPosition() == EPosition::kSticky;
  }
  bool HasViewportConstrainedPosition() const {
    return GetPosition() == EPosition::kFixed;
  }
  bool HasStickyConstrainedPosition() const {
    return GetPosition() == EPosition::kSticky &&
           (!Top().IsAuto() || !Left().IsAuto() || !Right().IsAuto() ||
            !Bottom().IsAuto());
  }

  // Clip utility functions.
  const Length& ClipLeft() const { return Clip().Left(); }
  const Length& ClipRight() const { return Clip().Right(); }
  const Length& ClipTop() const { return Clip().Top(); }
  const Length& ClipBottom() const { return Clip().Bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
  const Length& LogicalLeft() const {
    return PhysicalBoundsToLogical().LineLeft();
  }
  const Length& LogicalRight() const {
    return PhysicalBoundsToLogical().LineRight();
  }
  const Length& LogicalTop() const {
    return PhysicalBoundsToLogical().Before();
  }
  const Length& LogicalBottom() const {
    return PhysicalBoundsToLogical().After();
  }
  bool OffsetEqual(const ComputedStyle& other) const {
    return Left() == other.Left() && Right() == other.Right() &&
           Top() == other.Top() && Bottom() == other.Bottom();
  }

  // Whether or not a positioned element requires normal flow x/y to be computed
  // to determine its position.
  bool HasAutoLeftAndRight() const {
    return Left().IsAuto() && Right().IsAuto();
  }
  bool HasAutoTopAndBottom() const {
    return Top().IsAuto() && Bottom().IsAuto();
  }
  bool HasStaticInlinePosition(bool horizontal) const {
    return horizontal ? HasAutoLeftAndRight() : HasAutoTopAndBottom();
  }
  bool HasStaticBlockPosition(bool horizontal) const {
    return horizontal ? HasAutoTopAndBottom() : HasAutoLeftAndRight();
  }

  // Content utility functions.
  bool ContentDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(GetContentData(), other.GetContentData());
  }

  // Contain utility functions.
  bool ContainsPaint() const { return Contain() & kContainsPaint; }
  bool ContainsStyle() const { return Contain() & kContainsStyle; }
  bool ContainsLayout() const { return Contain() & kContainsLayout; }
  bool ContainsSize() const { return Contain() & kContainsSize; }

  // Display utility functions.
  bool IsDisplayReplacedType() const {
    return IsDisplayReplacedType(Display());
  }
  bool IsDisplayInlineType() const { return IsDisplayInlineType(Display()); }
  bool IsOriginalDisplayInlineType() const {
    return IsDisplayInlineType(OriginalDisplay());
  }
  bool IsDisplayBlockContainer() const {
    return IsDisplayBlockContainer(Display());
  }
  bool IsDisplayFlexibleOrGridBox() const {
    return IsDisplayFlexibleBox(Display()) || IsDisplayGridBox(Display());
  }
  bool IsDisplayFlexibleBox() const { return IsDisplayFlexibleBox(Display()); }

  // Isolation utility functions.
  bool HasIsolation() const { return Isolation() != EIsolation::kAuto; }

  // Content utility functions.
  bool HasContent() const { return GetContentData(); }

  // Cursor utility functions.
  CursorList* Cursors() const { return CursorDataInternal().Get(); }
  void AddCursor(StyleImage*,
                 bool hot_spot_specified,
                 const IntPoint& hot_spot = IntPoint());
  void SetCursorList(CursorList*);
  void ClearCursorList();

  // Text decoration utility functions.
  void ApplyTextDecorations(const Color& parent_text_decoration_color,
                            bool override_existing_colors);
  void ClearAppliedTextDecorations();
  void RestoreParentTextDecorations(const ComputedStyle& parent_style);
  const Vector<AppliedTextDecoration>& AppliedTextDecorations() const;
  TextDecoration TextDecorationsInEffect() const;

  // Overflow utility functions.

  EOverflow OverflowInlineDirection() const {
    return IsHorizontalWritingMode() ? OverflowX() : OverflowY();
  }
  EOverflow OverflowBlockDirection() const {
    return IsHorizontalWritingMode() ? OverflowY() : OverflowX();
  }

  // It's sufficient to just check one direction, since it's illegal to have
  // visible on only one overflow value.
  bool IsOverflowVisible() const {
    DCHECK(OverflowX() != EOverflow::kVisible || OverflowX() == OverflowY());
    return OverflowX() == EOverflow::kVisible;
  }
  bool IsOverflowPaged() const {
    return OverflowY() == EOverflow::kWebkitPagedX ||
           OverflowY() == EOverflow::kWebkitPagedY;
  }

  bool IsDisplayTableRowOrColumnType() const {
    return Display() == EDisplay::kTableRow ||
           Display() == EDisplay::kTableRowGroup ||
           Display() == EDisplay::kTableColumn ||
           Display() == EDisplay::kTableColumnGroup;
  }

  bool HasAutoHorizontalScroll() const {
    return OverflowX() == EOverflow::kAuto ||
           OverflowX() == EOverflow::kOverlay;
  }

  bool HasAutoVerticalScroll() const {
    return OverflowY() == EOverflow::kAuto ||
           OverflowY() == EOverflow::kWebkitPagedY ||
           OverflowY() == EOverflow::kOverlay;
  }

  bool ScrollsOverflowX() const {
    return OverflowX() == EOverflow::kScroll || HasAutoHorizontalScroll();
  }

  bool ScrollsOverflowY() const {
    return OverflowY() == EOverflow::kScroll || HasAutoVerticalScroll();
  }

  bool ScrollsOverflow() const {
    return ScrollsOverflowX() || ScrollsOverflowY();
  }

  // Visibility utility functions.
  bool VisibleToHitTesting() const {
    return Visibility() == EVisibility::kVisible &&
           PointerEvents() != EPointerEvents::kNone;
  }

  // Animation utility functions.
  bool ShouldCompositeForCurrentAnimations() const {
    return HasCurrentOpacityAnimation() || HasCurrentTransformAnimation() ||
           HasCurrentFilterAnimation() || HasCurrentBackdropFilterAnimation();
  }
  bool IsRunningAnimationOnCompositor() const {
    return IsRunningOpacityAnimationOnCompositor() ||
           IsRunningTransformAnimationOnCompositor() ||
           IsRunningFilterAnimationOnCompositor() ||
           IsRunningBackdropFilterAnimationOnCompositor();
  }

  // Opacity utility functions.
  bool HasOpacity() const { return Opacity() < 1.0f; }

  // Table layout utility functions.
  bool IsFixedTableLayout() const {
    return TableLayout() == ETableLayout::kFixed && !LogicalWidth().IsAuto();
  }

  // Filter/transform utility functions.
  bool Has3DTransform() const {
    return Transform().Has3DOperation() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
           (Scale() && Scale()->Z() != 1);
  }
  bool HasTransform() const {
    return HasTransformOperations() || HasOffset() ||
           HasCurrentTransformAnimation() || Translate() || Rotate() || Scale();
  }
  bool HasTransformOperations() const {
    return !Transform().Operations().IsEmpty();
  }
  ETransformStyle3D UsedTransformStyle3D() const {
    return HasGroupingProperty() ? ETransformStyle3D::kFlat
                                 : TransformStyle3D();
  }
  // Returns whether the transform operations for |otherStyle| differ from the
  // operations for this style instance. Note that callers may want to also
  // check hasTransform(), as it is possible for two styles to have matching
  // transform operations but differ in other transform-impacting style
  // respects.
  bool TransformDataEquivalent(const ComputedStyle& other) const {
    return !DiffTransformData(*this, other);
  }
  bool Preserves3D() const {
    return UsedTransformStyle3D() != ETransformStyle3D::kFlat;
  }
  enum ApplyTransformOrigin {
    kIncludeTransformOrigin,
    kExcludeTransformOrigin
  };
  enum ApplyMotionPath { kIncludeMotionPath, kExcludeMotionPath };
  enum ApplyIndependentTransformProperties {
    kIncludeIndependentTransformProperties,
    kExcludeIndependentTransformProperties
  };
  void ApplyTransform(TransformationMatrix&,
                      const LayoutSize& border_box_data_size,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;
  void ApplyTransform(TransformationMatrix&,
                      const FloatRect& bounding_box,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;

  bool HasFilters() const;

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  bool HasFilterInducingProperty() const {
    return HasFilter() || HasBoxReflect();
  }

  // Returns |true| if opacity should be considered to have non-initial value
  // for the purpose of creating stacking contexts.
  bool HasNonInitialOpacity() const {
    return HasOpacity() || HasWillChangeOpacityHint() ||
           HasCurrentOpacityAnimation();
  }

  // Returns whether this style contains any grouping property as defined by
  // [css-transforms].  The main purpose of this is to adjust the used value of
  // transform-style property.
  // Note: We currently don't include every grouping property on the spec to
  // maintain backward compatibility.  [css-transforms]
  // https://drafts.csswg.org/css-transforms/#grouping-property-values
  bool HasGroupingProperty() const {
    return !IsOverflowVisible() || HasFilterInducingProperty() ||
           HasNonInitialOpacity();
  }

  // Return true if any transform related property (currently
  // transform/motionPath, transformStyle3D, perspective, or
  // will-change:transform) indicates that we are transforming.
  // will-change:transform should result in the same rendering behavior as
  // having a transform, including the creation of a containing block for fixed
  // position descendants.
  bool HasTransformRelatedProperty() const {
    return HasTransform() || Preserves3D() || HasPerspective() ||
           HasWillChangeTransformHint();
  }

  // Paint utility functions.
  void AddPaintImage(StyleImage*);

  // FIXME: reflections should belong to this helper function but they are
  // currently handled through their self-painting layers. So the layout code
  // doesn't account for them.
  bool HasVisualOverflowingEffect() const {
    return BoxShadow() || HasBorderImageOutsets() || HasOutline();
  }

  // Stacking contexts and positioned elements[1] are stacked (sorted in
  // negZOrderList
  // and posZOrderList) in their enclosing stacking contexts.
  //
  // [1] According to CSS2.1, Appendix E.2.8
  // (https://www.w3.org/TR/CSS21/zindex.html),
  // positioned elements with 'z-index: auto' are "treated as if it created a
  // new stacking context" and z-ordered together with other elements with
  // 'z-index: 0'.  The difference of them from normal stacking contexts is that
  // they don't determine the stacking of the elements underneath them.  (Note:
  // There are also other elements treated as stacking context during painting,
  // but not managed in stacks. See ObjectPainter::PaintAllPhasesAtomically().)
  CORE_EXPORT void UpdateIsStackingContext(bool is_document_element,
                                           bool is_in_top_layer);
  bool IsStacked() const {
    return IsStackingContext() || GetPosition() != EPosition::kStatic;
  }

  // Pseudo-styles
  bool HasAnyPublicPseudoStyles() const;
  bool HasPseudoStyle(PseudoId) const;
  void SetHasPseudoStyle(PseudoId);
  bool HasUniquePseudoStyle() const;
  bool HasPseudoElementStyle() const;

  // Note: CanContainAbsolutePositionObjects should return true if
  // CanContainFixedPositionObjects.  We currently never use this value
  // directly, always OR'ing it with CanContainFixedPositionObjects.
  bool CanContainAbsolutePositionObjects() const {
    return GetPosition() != EPosition::kStatic;
  }
  bool CanContainFixedPositionObjects() const {
    return HasTransformRelatedProperty() || ContainsPaint();
  }

  // Whitespace utility functions.
  static bool AutoWrap(EWhiteSpace ws) {
    // Nowrap and pre don't automatically wrap.
    return ws != EWhiteSpace::kNowrap && ws != EWhiteSpace::kPre;
  }

  bool AutoWrap() const { return AutoWrap(WhiteSpace()); }

  static bool PreserveNewline(EWhiteSpace ws) {
    // Normal and nowrap do not preserve newlines.
    return ws != EWhiteSpace::kNormal && ws != EWhiteSpace::kNowrap;
  }

  bool PreserveNewline() const { return PreserveNewline(WhiteSpace()); }

  static bool BorderStyleIsVisible(EBorderStyle style) {
    return style != EBorderStyle::kNone && style != EBorderStyle::kHidden;
  }

  static bool CollapseWhiteSpace(EWhiteSpace ws) {
    // Pre and prewrap do not collapse whitespace.
    return ws != EWhiteSpace::kPre && ws != EWhiteSpace::kPreWrap;
  }

  bool CollapseWhiteSpace() const { return CollapseWhiteSpace(WhiteSpace()); }

  bool IsCollapsibleWhiteSpace(UChar c) const {
    switch (c) {
      case ' ':
      case '\t':
        return CollapseWhiteSpace();
      case '\n':
        return !PreserveNewline();
    }
    return false;
  }
  bool BreakOnlyAfterWhiteSpace() const {
    return WhiteSpace() == EWhiteSpace::kPreWrap ||
           GetLineBreak() == LineBreak::kAfterWhiteSpace;
  }

  bool BreakWords() const {
    return (WordBreak() == EWordBreak::kBreakWord ||
            OverflowWrap() == EOverflowWrap::kBreakWord) &&
           WhiteSpace() != EWhiteSpace::kPre &&
           WhiteSpace() != EWhiteSpace::kNowrap;
  }

  // Text direction utility functions.
  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return !IsLeftToRightDirection() && IsHorizontalWritingMode();
  }
  bool HasInlinePaginationAxis() const {
    // If the pagination axis is parallel with the writing mode inline axis,
    // columns may be laid out along the inline axis, just like for regular
    // multicol. Otherwise, we need to lay out along the block axis.
    if (IsOverflowPaged()) {
      return (OverflowY() == EOverflow::kWebkitPagedX) ==
             IsHorizontalWritingMode();
    }
    return false;
  }

  // Border utility functions.
  bool BorderObscuresBackground() const;
  void GetBorderEdgeInfo(BorderEdge edges[],
                         bool include_logical_left_edge = true,
                         bool include_logical_right_edge = true) const;

  bool HasBoxDecorations() const {
    return HasBorderDecoration() || HasBorderRadius() || HasOutline() ||
           HasAppearance() || BoxShadow() || HasFilterInducingProperty() ||
           HasBackdropFilter() || Resize() != EResize::kNone;
  }

  // "Box decoration background" includes all box decorations and backgrounds
  // that are painted as the background of the object. It includes borders,
  // box-shadows, background-color and background-image, etc.
  bool HasBoxDecorationBackground() const {
    return HasBackground() || HasBorderDecoration() || HasAppearance() ||
           BoxShadow();
  }

  // Background utility functions.
  FillLayer& AccessBackgroundLayers() { return MutableBackgroundInternal(); }
  const FillLayer& BackgroundLayers() const { return BackgroundInternal(); }
  void AdjustBackgroundLayers() {
    if (BackgroundLayers().Next()) {
      AccessBackgroundLayers().CullEmptyLayers();
      AccessBackgroundLayers().FillUnsetProperties();
    }
  }
  bool HasBackgroundRelatedColorReferencingCurrentColor() const {
    if (BackgroundColor().IsCurrentColor() ||
        VisitedLinkBackgroundColor().IsCurrentColor())
      return true;
    if (!BoxShadow())
      return false;
    return ShadowListHasCurrentColor(BoxShadow());
  }
  bool HasBackground() const {
    Color color = VisitedDependentColor(CSSPropertyBackgroundColor);
    if (color.Alpha())
      return true;
    return HasBackgroundImage();
  }

  // Color utility functions.
  // TODO(sashab): Rename this to just getColor(), and add a comment explaining
  // how it works.
  CORE_EXPORT Color VisitedDependentColor(int color_property) const;

  // -webkit-appearance utility functions.
  bool HasAppearance() const { return Appearance() != kNoControlPart; }

  // Other utility functions.
  bool IsStyleAvailable() const;
  bool IsSharable() const;

  bool RequireTransformOrigin(ApplyTransformOrigin apply_origin,
                              ApplyMotionPath) const;

  InterpolationQuality GetInterpolationQuality() const;

 private:
  void SetVisitedLinkBackgroundColor(const StyleColor& v) {
    SetVisitedLinkBackgroundColorInternal(v);
  }
  void SetVisitedLinkBorderLeftColor(const StyleColor& v) {
    SetVisitedLinkBorderLeftColorInternal(v);
  }
  void SetVisitedLinkBorderRightColor(const StyleColor& v) {
    SetVisitedLinkBorderRightColorInternal(v);
  }
  void SetVisitedLinkBorderBottomColor(const StyleColor& v) {
    SetVisitedLinkBorderBottomColorInternal(v);
  }
  void SetVisitedLinkBorderTopColor(const StyleColor& v) {
    SetVisitedLinkBorderTopColorInternal(v);
  }
  void SetVisitedLinkOutlineColor(const StyleColor& v) {
    SetVisitedLinkOutlineColorInternal(v);
  }
  void SetVisitedLinkColumnRuleColor(const StyleColor& v) {
    SetVisitedLinkColumnRuleColorInternal(v);
  }
  void SetVisitedLinkTextDecorationColor(const StyleColor& v) {
    SetVisitedLinkTextDecorationColorInternal(v);
  }
  void SetVisitedLinkTextEmphasisColor(const StyleColor& color) {
    SetVisitedLinkTextEmphasisColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextEmphasisColorIsCurrentColorInternal(
        color.IsCurrentColor());
  }
  void SetVisitedLinkTextFillColor(const StyleColor& color) {
    SetVisitedLinkTextFillColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextFillColorIsCurrentColorInternal(color.IsCurrentColor());
  }
  void SetVisitedLinkTextStrokeColor(const StyleColor& color) {
    SetVisitedLinkTextStrokeColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextStrokeColorIsCurrentColorInternal(color.IsCurrentColor());
  }
  void SetVisitedLinkCaretColor(const StyleAutoColor& color) {
    SetVisitedLinkCaretColorInternal(color.Resolve(Color()));
    SetVisitedLinkCaretColorIsCurrentColorInternal(color.IsCurrentColor());
    SetVisitedLinkCaretColorIsAutoInternal(color.IsAutoColor());
  }

  static bool IsDisplayBlockContainer(EDisplay display) {
    return display == EDisplay::kBlock || display == EDisplay::kListItem ||
           display == EDisplay::kInlineBlock ||
           display == EDisplay::kFlowRoot || display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  static bool IsDisplayFlexibleBox(EDisplay display) {
    return display == EDisplay::kFlex || display == EDisplay::kInlineFlex;
  }

  static bool IsDisplayGridBox(EDisplay display) {
    return display == EDisplay::kGrid || display == EDisplay::kInlineGrid;
  }

  static bool IsDisplayReplacedType(EDisplay display) {
    return display == EDisplay::kInlineBlock ||
           display == EDisplay::kWebkitInlineBox ||
           display == EDisplay::kInlineFlex ||
           display == EDisplay::kInlineTable ||
           display == EDisplay::kInlineGrid;
  }

  static bool IsDisplayInlineType(EDisplay display) {
    return display == EDisplay::kInline || IsDisplayReplacedType(display);
  }

  static bool IsDisplayTableType(EDisplay display) {
    return display == EDisplay::kTable || display == EDisplay::kInlineTable ||
           display == EDisplay::kTableRowGroup ||
           display == EDisplay::kTableHeaderGroup ||
           display == EDisplay::kTableFooterGroup ||
           display == EDisplay::kTableRow ||
           display == EDisplay::kTableColumnGroup ||
           display == EDisplay::kTableColumn ||
           display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  // Color accessors are all private to make sure callers use
  // VisitedDependentColor instead to access them.
  StyleColor BorderLeftColor() const {
    return BorderLeftColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderLeftColorInternal());
  }
  StyleColor BorderRightColor() const {
    return BorderRightColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderRightColorInternal());
  }
  StyleColor BorderTopColor() const {
    return BorderTopColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderTopColorInternal());
  }
  StyleColor BorderBottomColor() const {
    return BorderBottomColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderBottomColorInternal());
  }

  StyleColor BackgroundColor() const { return BackgroundColorInternal(); }
  StyleAutoColor CaretColor() const {
    if (CaretColorIsCurrentColorInternal())
      return StyleAutoColor::CurrentColor();
    if (CaretColorIsAutoInternal())
      return StyleAutoColor::AutoColor();
    return StyleAutoColor(CaretColorInternal());
  }
  Color GetColor() const;
  StyleColor ColumnRuleColor() const {
    return ColumnRuleColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(ColumnRuleColorInternal());
  }
  StyleColor OutlineColor() const {
    return OutlineColorIsCurrentColor() ? StyleColor::CurrentColor()
                                        : StyleColor(OutlineColorInternal());
  }
  StyleColor TextEmphasisColor() const {
    return TextEmphasisColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextEmphasisColorInternal());
  }
  StyleColor TextFillColor() const {
    return TextFillColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextFillColorInternal());
  }
  StyleColor TextStrokeColor() const {
    return TextStrokeColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextStrokeColorInternal());
  }
  StyleAutoColor VisitedLinkCaretColor() const {
    if (VisitedLinkCaretColorIsCurrentColorInternal())
      return StyleAutoColor::CurrentColor();
    if (VisitedLinkCaretColorIsAutoInternal())
      return StyleAutoColor::AutoColor();
    return StyleAutoColor(VisitedLinkCaretColorInternal());
  }
  StyleColor VisitedLinkBackgroundColor() const {
    return VisitedLinkBackgroundColorInternal();
  }
  StyleColor VisitedLinkBorderLeftColor() const {
    return VisitedLinkBorderLeftColorInternal();
  }
  bool VisitedLinkBorderLeftColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderLeftColor() ==
                other.VisitedLinkBorderLeftColor() ||
            !BorderLeftWidth());
  }
  StyleColor VisitedLinkBorderRightColor() const {
    return VisitedLinkBorderRightColorInternal();
  }
  bool VisitedLinkBorderRightColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderRightColor() ==
                other.VisitedLinkBorderRightColor() ||
            !BorderRightWidth());
  }
  StyleColor VisitedLinkBorderBottomColor() const {
    return VisitedLinkBorderBottomColorInternal();
  }
  bool VisitedLinkBorderBottomColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderBottomColor() ==
                other.VisitedLinkBorderBottomColor() ||
            !BorderBottomWidth());
  }
  StyleColor VisitedLinkBorderTopColor() const {
    return VisitedLinkBorderTopColorInternal();
  }
  bool VisitedLinkBorderTopColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderTopColor() == other.VisitedLinkBorderTopColor() ||
            !BorderTopWidth());
  }
  StyleColor VisitedLinkOutlineColor() const {
    return VisitedLinkOutlineColorInternal();
  }
  bool VisitedLinkOutlineColorHasNotChanged(const ComputedStyle& other) const {
    return (VisitedLinkOutlineColor() == other.VisitedLinkOutlineColor() ||
            !OutlineWidth());
  }
  StyleColor VisitedLinkColumnRuleColor() const {
    return VisitedLinkColumnRuleColorInternal();
  }
  StyleColor VisitedLinkTextDecorationColor() const {
    return VisitedLinkTextDecorationColorInternal();
  }
  StyleColor VisitedLinkTextEmphasisColor() const {
    return VisitedLinkTextEmphasisColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextEmphasisColorInternal());
  }
  StyleColor VisitedLinkTextFillColor() const {
    return VisitedLinkTextFillColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextFillColorInternal());
  }
  StyleColor VisitedLinkTextStrokeColor() const {
    return VisitedLinkTextStrokeColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextStrokeColorInternal());
  }

  StyleColor DecorationColorIncludingFallback(bool visited_link) const;
  Color ColorIncludingFallback(int color_property, bool visited_link) const;

  Color StopColor() const { return SvgStyle().StopColor(); }
  Color FloodColor() const { return SvgStyle().FloodColor(); }
  Color LightingColor() const { return SvgStyle().LightingColor(); }

  void AddAppliedTextDecoration(const AppliedTextDecoration&);
  void OverrideTextDecorationColors(Color propagated_color);
  void ApplyMotionPathTransform(float origin_x,
                                float origin_y,
                                const FloatRect& bounding_box,
                                TransformationMatrix&) const;

  bool ScrollAnchorDisablingPropertyChanged(const ComputedStyle& other,
                                            const StyleDifference&) const;
  bool DiffNeedsFullLayoutAndPaintInvalidation(
      const ComputedStyle& other) const;
  bool DiffNeedsFullLayout(const ComputedStyle& other) const;
  bool DiffNeedsPaintInvalidationSubtree(const ComputedStyle& other) const;
  bool DiffNeedsPaintInvalidationObject(const ComputedStyle& other) const;
  bool DiffNeedsPaintInvalidationObjectForPaintImage(
      const StyleImage&,
      const ComputedStyle& other) const;
  bool DiffNeedsVisualRectUpdate(const ComputedStyle& other) const;
  CORE_EXPORT void UpdatePropertySpecificDifferences(const ComputedStyle& other,
                                                     StyleDifference&) const;

  static bool ShadowListHasCurrentColor(const ShadowList*);

  StyleInheritedVariables& MutableInheritedVariables();
  StyleNonInheritedVariables& MutableNonInheritedVariables();

  PhysicalToLogical<const Length&> PhysicalMarginToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<const Length&>(
        other.GetWritingMode(), other.Direction(), MarginTop(), MarginRight(),
        MarginBottom(), MarginLeft());
  }

  PhysicalToLogical<const Length&> PhysicalPaddingToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingMode(), Direction(),
                                            PaddingTop(), PaddingRight(),
                                            PaddingBottom(), PaddingLeft());
  }

  PhysicalToLogical<BorderValue> PhysicalBorderToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<BorderValue>(
        other.GetWritingMode(), other.Direction(), BorderTop(), BorderRight(),
        BorderBottom(), BorderLeft());
  }

  PhysicalToLogical<float> PhysicalBorderWidthToLogical() const {
    return PhysicalToLogical<float>(GetWritingMode(), Direction(),
                                    BorderTopWidth(), BorderRightWidth(),
                                    BorderBottomWidth(), BorderLeftWidth());
  }

  PhysicalToLogical<EBorderStyle> PhysicalBorderStyleToLogical() const {
    return PhysicalToLogical<EBorderStyle>(
        GetWritingMode(), Direction(), BorderTopStyle(), BorderRightStyle(),
        BorderBottomStyle(), BorderLeftStyle());
  }

  PhysicalToLogical<const Length&> PhysicalBoundsToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingMode(), Direction(),
                                            Top(), Right(), Bottom(), Left());
  }

  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsTransformAnimation);
};

// FIXME: Reduce/remove the dependency on zoom adjusted int values.
// The float or LayoutUnit versions of layout values should be used.
int AdjustForAbsoluteZoom(int value, float zoom_factor);

inline int AdjustForAbsoluteZoom(int value, const ComputedStyle& style) {
  float zoom_factor = style.EffectiveZoom();
  if (zoom_factor == 1)
    return value;
  return AdjustForAbsoluteZoom(value, zoom_factor);
}

inline float AdjustFloatForAbsoluteZoom(float value,
                                        const ComputedStyle& style) {
  return value / style.EffectiveZoom();
}

inline double AdjustDoubleForAbsoluteZoom(double value,
                                          const ComputedStyle& style) {
  return value / style.EffectiveZoom();
}

inline LayoutUnit AdjustLayoutUnitForAbsoluteZoom(LayoutUnit value,
                                                  const ComputedStyle& style) {
  return LayoutUnit(value / style.EffectiveZoom());
}

inline float AdjustScrollForAbsoluteZoom(float scroll_offset,
                                         float zoom_factor) {
  return scroll_offset / zoom_factor;
}

inline float AdjustScrollForAbsoluteZoom(float scroll_offset,
                                         const ComputedStyle& style) {
  return AdjustScrollForAbsoluteZoom(scroll_offset, style.EffectiveZoom());
}

inline bool ComputedStyle::SetZoom(float f) {
  if (Zoom() == f)
    return false;
  SetZoomInternal(f);
  SetEffectiveZoom(EffectiveZoom() * Zoom());
  return true;
}

inline bool ComputedStyle::SetEffectiveZoom(float f) {
  // Clamp the effective zoom value to a smaller (but hopeful still large
  // enough) range, to avoid overflow in derived computations.
  float clamped_effective_zoom = clampTo<float>(f, 1e-6, 1e6);
  if (EffectiveZoomInternal() == clamped_effective_zoom)
    return false;
  SetEffectiveZoomInternal(clamped_effective_zoom);
  return true;
}

inline bool ComputedStyle::IsSharable() const {
  if (Unique())
    return false;
  if (HasUniquePseudoStyle())
    return false;
  return true;
}

inline bool ComputedStyle::HasAnyPublicPseudoStyles() const {
  return PseudoBitsInternal() != kPseudoIdNone;
}

inline bool ComputedStyle::HasPseudoStyle(PseudoId pseudo) const {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  return (1 << (pseudo - kFirstPublicPseudoId)) & PseudoBitsInternal();
}

inline void ComputedStyle::SetHasPseudoStyle(PseudoId pseudo) {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  // TODO: Fix up this code. It is hard to understand.
  SetPseudoBitsInternal(static_cast<PseudoId>(
      PseudoBitsInternal() | 1 << (pseudo - kFirstPublicPseudoId)));
}

inline bool ComputedStyle::HasPseudoElementStyle() const {
  return PseudoBitsInternal() & kElementPseudoIdMask;
}

}  // namespace blink

#endif  // ComputedStyle_h
