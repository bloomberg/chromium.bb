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
#include "core/layout/LayoutTheme.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/AppliedTextDecorationList.h"
#include "core/style/BorderValue.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/ContentData.h"
#include "core/style/CounterDirectives.h"
#include "core/style/CursorData.h"
#include "core/style/CursorList.h"
#include "core/style/DataRef.h"
#include "core/style/LineClampValue.h"
#include "core/style/NinePieceImage.h"
#include "core/style/QuotesData.h"
#include "core/style/SVGComputedStyle.h"
#include "core/style/ShadowData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleContentAlignmentData.h"
#include "core/style/StyleDifference.h"
#include "core/style/StyleFilterData.h"
#include "core/style/StyleGridData.h"
#include "core/style/StyleGridItemData.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleMultiColData.h"
#include "core/style/StyleOffsetRotation.h"
#include "core/style/StyleReflection.h"
#include "core/style/StyleSelfAlignmentData.h"
#include "core/style/StyleTransformData.h"
#include "core/style/TextSizeAdjust.h"
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
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TabSize.h"
#include "platform/text/TextDirection.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/LeakAnnotations.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefVector.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

template <typename T, typename U>
inline bool compareEqual(const T& t, const U& u) {
  return t == static_cast<T>(u);
}

template <typename T>
inline bool compareEqual(const T& a, const T& b) {
  return a == b;
}

#define SET_VAR(group, variable, value)      \
  if (!compareEqual(group->variable, value)) \
  group.Access()->variable = value

#define SET_NESTED_VAR(group, base, variable, value) \
  if (!compareEqual(group->base->variable, value))   \
  group.Access()->base.Access()->variable = value

#define SET_VAR_WITH_SETTER(group, getter, setter, value) \
  if (!compareEqual(group->getter(), value))              \
  group.Access()->setter(value)

#define SET_BORDERVALUE_COLOR(group, variable, value)   \
  if (!compareEqual(group->variable.GetColor(), value)) \
  group.Access()->variable.SetColor(value)

#define SET_BORDER_WIDTH(group, variable, value) \
  if (!group->variable.WidthEquals(value))       \
  group.Access()->variable.SetWidth(value)

#define SET_NESTED_BORDER_WIDTH(group, base, variable, value) \
  if (!group->base->variable.WidthEquals(value))              \
  group.Access()->base.Access()->variable.SetWidth(value)

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
class RotateTransformOperation;
class ScaleTransformOperation;
class ShadowList;
class ShapeValue;
class StyleImage;
class StylePath;
class StyleResolver;
class TransformationMatrix;
class TranslateTransformOperation;

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
class CORE_EXPORT ComputedStyle : public ComputedStyleBase,
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
  static ComputedStyle& MutableInitialStyle();

 public:
  static RefPtr<ComputedStyle> Create();
  static RefPtr<ComputedStyle> CreateAnonymousStyleWithDisplay(
      const ComputedStyle& parent_style,
      EDisplay);
  static RefPtr<ComputedStyle> Clone(const ComputedStyle&);
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

  // Content alignment properties.
  static StyleContentAlignmentData InitialContentAlignment() {
    return StyleContentAlignmentData(kContentPositionNormal,
                                     kContentDistributionDefault,
                                     kOverflowAlignmentDefault);
  }

  // align-content (aka -webkit-align-content)
  const StyleContentAlignmentData& AlignContent() const {
    return rare_non_inherited_data_->align_content_;
  }
  void SetAlignContent(const StyleContentAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, align_content_, data);
  }

  // justify-content (aka -webkit-justify-content)
  const StyleContentAlignmentData& JustifyContent() const {
    return rare_non_inherited_data_->justify_content_;
  }
  void SetJustifyContent(const StyleContentAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, justify_content_, data);
  }

  // Default-Alignment properties.
  static StyleSelfAlignmentData InitialDefaultAlignment() {
    return StyleSelfAlignmentData(RuntimeEnabledFeatures::CSSGridLayoutEnabled()
                                      ? kItemPositionNormal
                                      : kItemPositionStretch,
                                  kOverflowAlignmentDefault);
  }

  // align-items (aka -webkit-align-items)
  const StyleSelfAlignmentData& AlignItems() const {
    return rare_non_inherited_data_->align_items_;
  }
  void SetAlignItems(const StyleSelfAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, align_items_, data);
  }

  // justify-items
  const StyleSelfAlignmentData& JustifyItems() const {
    return rare_non_inherited_data_->justify_items_;
  }
  void SetJustifyItems(const StyleSelfAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, justify_items_, data);
  }

  // Self-Alignment properties.
  static StyleSelfAlignmentData InitialSelfAlignment() {
    return StyleSelfAlignmentData(kItemPositionAuto, kOverflowAlignmentDefault);
  }

  // align-self (aka -webkit-align-self)
  const StyleSelfAlignmentData& AlignSelf() const {
    return rare_non_inherited_data_->align_self_;
  }
  void SetAlignSelf(const StyleSelfAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, align_self_, data);
  }

  // justify-self
  const StyleSelfAlignmentData& JustifySelf() const {
    return rare_non_inherited_data_->justify_self_;
  }
  void SetJustifySelf(const StyleSelfAlignmentData& data) {
    SET_VAR(rare_non_inherited_data_, justify_self_, data);
  }

  // Filter properties.

  // backdrop-filter
  static const FilterOperations& InitialBackdropFilter();
  const FilterOperations& BackdropFilter() const {
    return rare_non_inherited_data_->backdrop_filter_->operations_;
  }
  FilterOperations& MutableBackdropFilter() {
    return rare_non_inherited_data_.Access()
        ->backdrop_filter_.Access()
        ->operations_;
  }
  bool HasBackdropFilter() const {
    return !rare_non_inherited_data_->backdrop_filter_->operations_.Operations()
                .IsEmpty();
  }
  void SetBackdropFilter(const FilterOperations& ops) {
    SET_NESTED_VAR(rare_non_inherited_data_, backdrop_filter_, operations_,
                   ops);
  }

  // filter (aka -webkit-filter)
  static const FilterOperations& InitialFilter();
  FilterOperations& MutableFilter() {
    return rare_non_inherited_data_.Access()->filter_.Access()->operations_;
  }
  const FilterOperations& Filter() const {
    return rare_non_inherited_data_->filter_->operations_;
  }
  bool HasFilter() const {
    return !rare_non_inherited_data_->filter_->operations_.Operations()
                .IsEmpty();
  }
  void SetFilter(const FilterOperations& ops) {
    SET_NESTED_VAR(rare_non_inherited_data_, filter_, operations_, ops);
  }

  // backface-visibility (aka -webkit-backface-visibility)
  static EBackfaceVisibility InitialBackfaceVisibility() {
    return kBackfaceVisibilityVisible;
  }
  EBackfaceVisibility BackfaceVisibility() const {
    return static_cast<EBackfaceVisibility>(
        rare_non_inherited_data_->backface_visibility_);
  }
  void SetBackfaceVisibility(EBackfaceVisibility b) {
    SET_VAR(rare_non_inherited_data_, backface_visibility_, b);
  }

  // Background properties.
  // background-color
  static Color InitialBackgroundColor() { return Color::kTransparent; }
  void SetBackgroundColor(const StyleColor& v) {
    SetBackgroundColorInternal(v);
  }

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

  static EBorderStyle InitialColumnRuleStyle() { return EBorderStyle::kNone; }

  // Border color properties.
  // border-left-color
  void SetBorderLeftColor(const StyleColor& color) {
    if (!compareEqual(BorderLeftColor(), color)) {
      SetBorderLeftColorInternal(color.Resolve(Color()));
      SetBorderLeftColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-right-color
  void SetBorderRightColor(const StyleColor& color) {
    if (!compareEqual(BorderRightColor(), color)) {
      SetBorderRightColorInternal(color.Resolve(Color()));
      SetBorderRightColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-top-color
  void SetBorderTopColor(const StyleColor& color) {
    if (!compareEqual(BorderTopColor(), color)) {
      SetBorderTopColorInternal(color.Resolve(Color()));
      SetBorderTopColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-bottom-color
  void SetBorderBottomColor(const StyleColor& color) {
    if (!compareEqual(BorderBottomColor(), color)) {
      SetBorderBottomColorInternal(color.Resolve(Color()));
      SetBorderBottomColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // box-shadow (aka -webkit-box-shadow)
  static ShadowList* InitialBoxShadow() { return 0; }
  ShadowList* BoxShadow() const {
    return rare_non_inherited_data_->box_shadow_.Get();
  }
  void SetBoxShadow(RefPtr<ShadowList>);
  bool BoxShadowDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxShadow(), other.BoxShadow());
  }

  // clip
  static LengthBox InitialClip() { return LengthBox(); }
  const LengthBox& Clip() const { return ClipInternal(); }
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
  unsigned short ColumnCount() const {
    return rare_non_inherited_data_->multi_col_data_->count_;
  }
  void SetColumnCount(unsigned short c) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, auto_count_,
                   false);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, count_, c);
  }
  bool HasAutoColumnCount() const {
    return rare_non_inherited_data_->multi_col_data_->auto_count_;
  }
  void SetHasAutoColumnCount() {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, auto_count_,
                   true);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, count_,
                   InitialColumnCount());
  }

  // column-fill
  static ColumnFill InitialColumnFill() { return kColumnFillBalance; }
  ColumnFill GetColumnFill() const {
    return static_cast<ColumnFill>(
        rare_non_inherited_data_->multi_col_data_->fill_);
  }
  void SetColumnFill(ColumnFill column_fill) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, fill_,
                   column_fill);
  }

  // column-gap (aka -webkit-column-gap)
  float ColumnGap() const {
    return rare_non_inherited_data_->multi_col_data_->gap_;
  }
  void SetColumnGap(float f) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, normal_gap_,
                   false);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, gap_, f);
  }
  bool HasNormalColumnGap() const {
    return rare_non_inherited_data_->multi_col_data_->normal_gap_;
  }
  void SetHasNormalColumnGap() {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, normal_gap_,
                   true);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, gap_, 0);
  }

  // column-rule-color (aka -webkit-column-rule-color)
  void SetColumnRuleColor(const StyleColor& c) {
    SET_BORDERVALUE_COLOR(rare_non_inherited_data_.Access()->multi_col_data_,
                          rule_, c);
  }

  // column-rule-style (aka -webkit-column-rule-style)
  EBorderStyle ColumnRuleStyle() const {
    return rare_non_inherited_data_->multi_col_data_->rule_.Style();
  }
  void SetColumnRuleStyle(EBorderStyle b) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, rule_.style_,
                   static_cast<unsigned>(b));
  }

  // column-rule-width (aka -webkit-column-rule-width)
  static unsigned short InitialColumnRuleWidth() { return 3; }
  unsigned short ColumnRuleWidth() const {
    const BorderValue& rule = rare_non_inherited_data_->multi_col_data_->rule_;
    if (rule.Style() == EBorderStyle::kNone ||
        rule.Style() == EBorderStyle::kHidden)
      return 0;
    return rule.Width();
  }
  void SetColumnRuleWidth(unsigned short w) {
    SET_NESTED_BORDER_WIDTH(rare_non_inherited_data_, multi_col_data_, rule_,
                            w);
  }

  // column-span (aka -webkit-column-span)
  static ColumnSpan InitialColumnSpan() { return kColumnSpanNone; }
  ColumnSpan GetColumnSpan() const {
    return static_cast<ColumnSpan>(
        rare_non_inherited_data_->multi_col_data_->column_span_);
  }
  void SetColumnSpan(ColumnSpan column_span) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, column_span_,
                   column_span);
  }

  // column-width (aka -webkit-column-width)
  float ColumnWidth() const {
    return rare_non_inherited_data_->multi_col_data_->width_;
  }
  void SetColumnWidth(float f) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, auto_width_,
                   false);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, width_, f);
  }
  bool HasAutoColumnWidth() const {
    return rare_non_inherited_data_->multi_col_data_->auto_width_;
  }
  void SetHasAutoColumnWidth() {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, auto_width_,
                   true);
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_, width_, 0);
  }

  // contain
  static Containment InitialContain() { return kContainsNone; }
  Containment Contain() const {
    return static_cast<Containment>(rare_non_inherited_data_->contain_);
  }
  void SetContain(Containment contain) {
    SET_VAR(rare_non_inherited_data_, contain_, contain);
  }

  // content
  ContentData* GetContentData() const {
    return rare_non_inherited_data_->content_.Get();
  }
  void SetContent(ContentData*);

  // Flex properties.
  // flex-basis (aka -webkit-flex-basis)
  static Length InitialFlexBasis() { return Length(kAuto); }
  const Length& FlexBasis() const {
    return rare_non_inherited_data_->flexible_box_data_->flex_basis_;
  }
  void SetFlexBasis(const Length& length) {
    SET_NESTED_VAR(rare_non_inherited_data_, flexible_box_data_, flex_basis_,
                   length);
  }

  // flex-direction (aka -webkit-flex-direction)
  static EFlexDirection InitialFlexDirection() { return kFlowRow; }
  EFlexDirection FlexDirection() const {
    return static_cast<EFlexDirection>(
        rare_non_inherited_data_->flexible_box_data_->flex_direction_);
  }
  void SetFlexDirection(EFlexDirection direction) {
    SET_NESTED_VAR(rare_non_inherited_data_, flexible_box_data_,
                   flex_direction_, direction);
  }

  // flex-grow (aka -webkit-flex-grow)
  static float InitialFlexGrow() { return 0; }
  float FlexGrow() const {
    return rare_non_inherited_data_->flexible_box_data_->flex_grow_;
  }
  void SetFlexGrow(float f) {
    SET_NESTED_VAR(rare_non_inherited_data_, flexible_box_data_, flex_grow_, f);
  }

  // flex-shrink (aka -webkit-flex-shrink)
  static float InitialFlexShrink() { return 1; }
  float FlexShrink() const {
    return rare_non_inherited_data_->flexible_box_data_->flex_shrink_;
  }
  void SetFlexShrink(float f) {
    SET_NESTED_VAR(rare_non_inherited_data_, flexible_box_data_, flex_shrink_,
                   f);
  }

  // flex-wrap (aka -webkit-flex-wrap)
  static EFlexWrap InitialFlexWrap() { return kFlexNoWrap; }
  EFlexWrap FlexWrap() const {
    return static_cast<EFlexWrap>(
        rare_non_inherited_data_->flexible_box_data_->flex_wrap_);
  }
  void SetFlexWrap(EFlexWrap w) {
    SET_NESTED_VAR(rare_non_inherited_data_, flexible_box_data_, flex_wrap_, w);
  }

  // -webkit-box-flex
  static float InitialBoxFlex() { return 0.0f; }
  float BoxFlex() const {
    return rare_non_inherited_data_->deprecated_flexible_box_data_->box_flex_;
  }
  void SetBoxFlex(float f) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_flex_, f);
  }

  // -webkit-box-flex-group
  static unsigned InitialBoxFlexGroup() { return 1; }
  unsigned BoxFlexGroup() const {
    return rare_non_inherited_data_->deprecated_flexible_box_data_
        ->box_flex_group_;
  }
  void SetBoxFlexGroup(unsigned fg) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_flex_group_, fg);
  }

  // -webkit-box-align
  // For valid values of box-align see
  // http://www.w3.org/TR/2009/WD-css3-flexbox-20090723/#alignment
  static EBoxAlignment InitialBoxAlign() { return BSTRETCH; }
  EBoxAlignment BoxAlign() const {
    return static_cast<EBoxAlignment>(
        rare_non_inherited_data_->deprecated_flexible_box_data_->box_align_);
  }
  void SetBoxAlign(EBoxAlignment a) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_align_, a);
  }

  // -webkit-box-lines
  static EBoxLines InitialBoxLines() { return SINGLE; }
  EBoxLines BoxLines() const {
    return static_cast<EBoxLines>(
        rare_non_inherited_data_->deprecated_flexible_box_data_->box_lines_);
  }
  void SetBoxLines(EBoxLines lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_lines_, lines);
  }

  // -webkit-box-ordinal-group
  static unsigned InitialBoxOrdinalGroup() { return 1; }
  unsigned BoxOrdinalGroup() const {
    return rare_non_inherited_data_->deprecated_flexible_box_data_
        ->box_ordinal_group_;
  }
  void SetBoxOrdinalGroup(unsigned og) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_ordinal_group_,
                   std::min(std::numeric_limits<unsigned>::max() - 1, og));
  }

  // -webkit-box-orient
  static EBoxOrient InitialBoxOrient() { return HORIZONTAL; }
  EBoxOrient BoxOrient() const {
    return static_cast<EBoxOrient>(
        rare_non_inherited_data_->deprecated_flexible_box_data_->box_orient_);
  }
  void SetBoxOrient(EBoxOrient o) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_orient_, o);
  }

  // -webkit-box-pack
  static EBoxPack InitialBoxPack() { return kBoxPackStart; }
  EBoxPack BoxPack() const {
    return static_cast<EBoxPack>(
        rare_non_inherited_data_->deprecated_flexible_box_data_->box_pack_);
  }
  void SetBoxPack(EBoxPack p) {
    SET_NESTED_VAR(rare_non_inherited_data_, deprecated_flexible_box_data_,
                   box_pack_, p);
  }

  // -webkit-box-reflect
  static StyleReflection* InitialBoxReflect() { return 0; }
  StyleReflection* BoxReflect() const {
    return rare_non_inherited_data_->box_reflect_.Get();
  }
  void SetBoxReflect(RefPtr<StyleReflection> reflect) {
    if (rare_non_inherited_data_->box_reflect_ != reflect)
      rare_non_inherited_data_.Access()->box_reflect_ = std::move(reflect);
  }

  // Grid properties.
  static Vector<GridTrackSize> InitialGridAutoRepeatTracks() {
    return Vector<GridTrackSize>(); /* none */
  }
  static size_t InitialGridAutoRepeatInsertionPoint() { return 0; }
  static AutoRepeatType InitialGridAutoRepeatType() { return kNoAutoRepeat; }
  static NamedGridLinesMap InitialNamedGridColumnLines() {
    return NamedGridLinesMap();
  }
  static NamedGridLinesMap InitialNamedGridRowLines() {
    return NamedGridLinesMap();
  }
  static OrderedNamedGridLines InitialOrderedNamedGridColumnLines() {
    return OrderedNamedGridLines();
  }
  static OrderedNamedGridLines InitialOrderedNamedGridRowLines() {
    return OrderedNamedGridLines();
  }
  static NamedGridAreaMap InitialNamedGridArea() { return NamedGridAreaMap(); }
  static size_t InitialNamedGridAreaCount() { return 0; }

  // grid-auto-columns
  static Vector<GridTrackSize> InitialGridAutoColumns();
  const Vector<GridTrackSize>& GridAutoColumns() const {
    return rare_non_inherited_data_->grid_data_->grid_auto_columns_;
  }
  void SetGridAutoColumns(const Vector<GridTrackSize>& track_size_list) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_auto_columns_,
                   track_size_list);
  }

  // grid-auto-flow
  static GridAutoFlow InitialGridAutoFlow() { return kAutoFlowRow; }
  void SetGridAutoFlow(GridAutoFlow flow) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_auto_flow_, flow);
  }

  // grid-auto-rows
  static Vector<GridTrackSize> InitialGridAutoRows();
  const Vector<GridTrackSize>& GridAutoRows() const {
    return rare_non_inherited_data_->grid_data_->grid_auto_rows_;
  }
  void SetGridAutoRows(const Vector<GridTrackSize>& track_size_list) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_auto_rows_,
                   track_size_list);
  }

  // grid-column-gap
  static Length InitialGridColumnGap() { return Length(kFixed); }
  const Length& GridColumnGap() const {
    return rare_non_inherited_data_->grid_data_->grid_column_gap_;
  }
  void SetGridColumnGap(const Length& v) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_column_gap_, v);
  }

  // grid-column-start
  static GridPosition InitialGridColumnStart() {
    return GridPosition(); /* auto */
  }
  const GridPosition& GridColumnStart() const {
    return rare_non_inherited_data_->grid_item_data_->grid_column_start_;
  }
  void SetGridColumnStart(const GridPosition& column_start_position) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_item_data_,
                   grid_column_start_, column_start_position);
  }

  // grid-column-end
  static GridPosition InitialGridColumnEnd() {
    return GridPosition(); /* auto */
  }
  const GridPosition& GridColumnEnd() const {
    return rare_non_inherited_data_->grid_item_data_->grid_column_end_;
  }
  void SetGridColumnEnd(const GridPosition& column_end_position) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_item_data_, grid_column_end_,
                   column_end_position);
  }

  // grid-row-gap
  static Length InitialGridRowGap() { return Length(kFixed); }
  const Length& GridRowGap() const {
    return rare_non_inherited_data_->grid_data_->grid_row_gap_;
  }
  void SetGridRowGap(const Length& v) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_row_gap_, v);
  }

  // grid-row-start
  static GridPosition InitialGridRowStart() {
    return GridPosition(); /* auto */
  }
  const GridPosition& GridRowStart() const {
    return rare_non_inherited_data_->grid_item_data_->grid_row_start_;
  }
  void SetGridRowStart(const GridPosition& row_start_position) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_item_data_, grid_row_start_,
                   row_start_position);
  }

  // grid-row-end
  static GridPosition InitialGridRowEnd() { return GridPosition(); /* auto */ }
  const GridPosition& GridRowEnd() const {
    return rare_non_inherited_data_->grid_item_data_->grid_row_end_;
  }
  void SetGridRowEnd(const GridPosition& row_end_position) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_item_data_, grid_row_end_,
                   row_end_position);
  }

  // grid-template-columns
  static Vector<GridTrackSize> InitialGridTemplateColumns() {
    return Vector<GridTrackSize>(); /* none */
  }
  const Vector<GridTrackSize>& GridTemplateColumns() const {
    return rare_non_inherited_data_->grid_data_->grid_template_columns_;
  }
  void SetGridTemplateColumns(const Vector<GridTrackSize>& lengths) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_template_columns_,
                   lengths);
  }

  // grid-template-rows
  static Vector<GridTrackSize> InitialGridTemplateRows() {
    return Vector<GridTrackSize>(); /* none */
  }
  const Vector<GridTrackSize>& GridTemplateRows() const {
    return rare_non_inherited_data_->grid_data_->grid_template_rows_;
  }
  void SetGridTemplateRows(const Vector<GridTrackSize>& lengths) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_template_rows_,
                   lengths);
  }

  // image-orientation
  static RespectImageOrientationEnum InitialRespectImageOrientation() {
    return kDoNotRespectImageOrientation;
  }
  RespectImageOrientationEnum RespectImageOrientation() const {
    return static_cast<RespectImageOrientationEnum>(
        RespectImageOrientationInternal());
  }
  void SetRespectImageOrientation(RespectImageOrientationEnum v) {
    SetRespectImageOrientationInternal(v);
  }

  // mix-blend-mode
  static WebBlendMode InitialBlendMode() { return kWebBlendModeNormal; }
  WebBlendMode BlendMode() const {
    return static_cast<WebBlendMode>(
        rare_non_inherited_data_->effective_blend_mode_);
  }
  void SetBlendMode(WebBlendMode v) {
    rare_non_inherited_data_.Access()->effective_blend_mode_ = v;
  }

  // offset-anchor
  static LengthPoint InitialOffsetAnchor() {
    return LengthPoint(Length(kAuto), Length(kAuto));
  }
  const LengthPoint& OffsetAnchor() const {
    return rare_non_inherited_data_->transform_data_->motion_.anchor_;
  }
  void SetOffsetAnchor(const LengthPoint& offset_anchor) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, motion_.anchor_,
                   offset_anchor);
  }

  // offset-distance
  static Length InitialOffsetDistance() { return Length(0, kFixed); }
  const Length& OffsetDistance() const {
    return rare_non_inherited_data_->transform_data_->motion_.distance_;
  }
  void SetOffsetDistance(const Length& offset_distance) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, motion_.distance_,
                   offset_distance);
  }

  // offset-path
  static BasicShape* InitialOffsetPath() { return nullptr; }
  BasicShape* OffsetPath() const {
    return rare_non_inherited_data_->transform_data_->motion_.path_.Get();
  }
  void SetOffsetPath(RefPtr<BasicShape>);

  // offset-position
  static LengthPoint InitialOffsetPosition() {
    return LengthPoint(Length(kAuto), Length(kAuto));
  }
  const LengthPoint& OffsetPosition() const {
    return rare_non_inherited_data_->transform_data_->motion_.position_;
  }
  void SetOffsetPosition(const LengthPoint& offset_position) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, motion_.position_,
                   offset_position);
  }

  // offset-rotate
  static StyleOffsetRotation InitialOffsetRotate() {
    return StyleOffsetRotation(0, kOffsetRotationAuto);
  }
  const StyleOffsetRotation& OffsetRotate() const {
    return rare_non_inherited_data_->transform_data_->motion_.rotation_;
  }
  void SetOffsetRotate(const StyleOffsetRotation& offset_rotate) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, motion_.rotation_,
                   offset_rotate);
  }

  // opacity (aka -webkit-opacity)
  static float InitialOpacity() { return 1.0f; }
  float Opacity() const { return rare_non_inherited_data_->opacity_; }
  void SetOpacity(float f) {
    float v = clampTo<float>(f, 0, 1);
    SET_VAR(rare_non_inherited_data_, opacity_, v);
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
  int Order() const { return rare_non_inherited_data_->order_; }
  // We restrict the smallest value to int min + 2 because we use int min and
  // int min + 1 as special values in a hash set.
  void SetOrder(int o) {
    SET_VAR(rare_non_inherited_data_, order_,
            max(std::numeric_limits<int>::min() + 2, o));
  }

  // Outline properties.

  bool OutlineVisuallyEqual(const ComputedStyle& other) const {
    return rare_non_inherited_data_->outline_.VisuallyEqual(
        other.rare_non_inherited_data_->outline_);
  }

  // outline-color
  void SetOutlineColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(rare_non_inherited_data_, outline_, v);
  }

  // outline-style
  EBorderStyle OutlineStyle() const {
    return rare_non_inherited_data_->outline_.Style();
  }
  void SetOutlineStyle(EBorderStyle v) {
    SET_VAR(rare_non_inherited_data_, outline_.style_,
            static_cast<unsigned>(v));
  }
  static OutlineIsAuto InitialOutlineStyleIsAuto() { return kOutlineIsAutoOff; }
  OutlineIsAuto OutlineStyleIsAuto() const {
    return static_cast<OutlineIsAuto>(
        rare_non_inherited_data_->outline_.IsAuto());
  }
  void SetOutlineStyleIsAuto(OutlineIsAuto is_auto) {
    SET_VAR(rare_non_inherited_data_, outline_.is_auto_, is_auto);
  }

  // outline-width
  static unsigned short InitialOutlineWidth() { return 3; }
  unsigned short OutlineWidth() const {
    if (rare_non_inherited_data_->outline_.Style() == EBorderStyle::kNone)
      return 0;
    return rare_non_inherited_data_->outline_.Width();
  }
  void SetOutlineWidth(unsigned short v) {
    SET_BORDER_WIDTH(rare_non_inherited_data_, outline_, v);
  }

  // outline-offset
  static int InitialOutlineOffset() { return 0; }
  int OutlineOffset() const {
    if (rare_non_inherited_data_->outline_.Style() == EBorderStyle::kNone)
      return 0;
    return rare_non_inherited_data_->outline_.Offset();
  }
  void SetOutlineOffset(int v) {
    SET_VAR(rare_non_inherited_data_, outline_.offset_, v);
  }

  // perspective (aka -webkit-perspective)
  static float InitialPerspective() { return 0; }
  float Perspective() const { return rare_non_inherited_data_->perspective_; }
  void SetPerspective(float p) {
    SET_VAR(rare_non_inherited_data_, perspective_, p);
  }

  // perspective-origin (aka -webkit-perspective-origin)
  static LengthPoint InitialPerspectiveOrigin() {
    return LengthPoint(Length(50.0, kPercent), Length(50.0, kPercent));
  }
  const LengthPoint& PerspectiveOrigin() const {
    return rare_non_inherited_data_->perspective_origin_;
  }
  void SetPerspectiveOrigin(const LengthPoint& p) {
    SET_VAR(rare_non_inherited_data_, perspective_origin_, p);
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

  // resize
  static EResize InitialResize() { return RESIZE_NONE; }
  EResize Resize() const {
    return static_cast<EResize>(rare_non_inherited_data_->resize_);
  }
  void SetResize(EResize r) { SET_VAR(rare_non_inherited_data_, resize_, r); }

  // Transform properties.
  // transform (aka -webkit-transform)
  static EmptyTransformOperations InitialTransform() {
    return EmptyTransformOperations();
  }
  const TransformOperations& Transform() const {
    return rare_non_inherited_data_->transform_data_->operations_;
  }
  void SetTransform(const TransformOperations& ops) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, operations_, ops);
  }

  // transform-origin (aka -webkit-transform-origin)
  static TransformOrigin InitialTransformOrigin() {
    return TransformOrigin(Length(50.0, kPercent), Length(50.0, kPercent), 0);
  }
  const TransformOrigin& GetTransformOrigin() const {
    return rare_non_inherited_data_->transform_data_->origin_;
  }
  void SetTransformOrigin(const TransformOrigin& o) {
    SET_NESTED_VAR(rare_non_inherited_data_, transform_data_, origin_, o);
  }

  // transform-style (aka -webkit-transform-style)
  static ETransformStyle3D InitialTransformStyle3D() {
    return kTransformStyle3DFlat;
  }
  ETransformStyle3D TransformStyle3D() const {
    return static_cast<ETransformStyle3D>(
        rare_non_inherited_data_->transform_style_3d_);
  }
  void SetTransformStyle3D(ETransformStyle3D b) {
    SET_VAR(rare_non_inherited_data_, transform_style_3d_, b);
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

  // Independent transform properties.
  // translate
  static RefPtr<TranslateTransformOperation> InitialTranslate() {
    return nullptr;
  }
  TranslateTransformOperation* Translate() const {
    return rare_non_inherited_data_->transform_data_->translate_.Get();
  }
  void SetTranslate(RefPtr<TranslateTransformOperation> v) {
    rare_non_inherited_data_.Access()->transform_data_.Access()->translate_ =
        std::move(v);
  }

  // rotate
  static RefPtr<RotateTransformOperation> InitialRotate() { return nullptr; }
  RotateTransformOperation* Rotate() const {
    return rare_non_inherited_data_->transform_data_->rotate_.Get();
  }
  void SetRotate(RefPtr<RotateTransformOperation> v) {
    rare_non_inherited_data_.Access()->transform_data_.Access()->rotate_ =
        std::move(v);
  }

  // scale
  static RefPtr<ScaleTransformOperation> InitialScale() { return nullptr; }
  ScaleTransformOperation* Scale() const {
    return rare_non_inherited_data_->transform_data_->scale_.Get();
  }
  void SetScale(RefPtr<ScaleTransformOperation> v) {
    rare_non_inherited_data_.Access()->transform_data_.Access()->scale_ =
        std::move(v);
  }

  // Scroll properties.
  // scroll-behavior
  static ScrollBehavior InitialScrollBehavior() { return kScrollBehaviorAuto; }
  ScrollBehavior GetScrollBehavior() const {
    return static_cast<ScrollBehavior>(
        rare_non_inherited_data_->scroll_behavior_);
  }
  void SetScrollBehavior(ScrollBehavior b) {
    SET_VAR(rare_non_inherited_data_, scroll_behavior_, b);
  }

  // scroll-snap-coordinate
  static Vector<LengthPoint> InitialScrollSnapCoordinate() {
    return Vector<LengthPoint>();
  }
  const Vector<LengthPoint>& ScrollSnapCoordinate() const {
    return rare_non_inherited_data_->scroll_snap_data_->coordinates_;
  }
  void SetScrollSnapCoordinate(const Vector<LengthPoint>& b) {
    SET_NESTED_VAR(rare_non_inherited_data_, scroll_snap_data_, coordinates_,
                   b);
  }

  // scroll-snap-destination
  static LengthPoint InitialScrollSnapDestination() {
    return LengthPoint(Length(0, kFixed), Length(0, kFixed));
  }
  const LengthPoint& ScrollSnapDestination() const {
    return rare_non_inherited_data_->scroll_snap_data_->destination_;
  }
  void SetScrollSnapDestination(const LengthPoint& b) {
    SET_NESTED_VAR(rare_non_inherited_data_, scroll_snap_data_, destination_,
                   b);
  }

  // scroll-snap-points-x
  static ScrollSnapPoints InitialScrollSnapPointsX() {
    return ScrollSnapPoints();
  }
  const ScrollSnapPoints& ScrollSnapPointsX() const {
    return rare_non_inherited_data_->scroll_snap_data_->x_points_;
  }
  void SetScrollSnapPointsX(const ScrollSnapPoints& b) {
    SET_NESTED_VAR(rare_non_inherited_data_, scroll_snap_data_, x_points_, b);
  }

  // scroll-snap-points-y
  static ScrollSnapPoints InitialScrollSnapPointsY() {
    return ScrollSnapPoints();
  }
  const ScrollSnapPoints& ScrollSnapPointsY() const {
    return rare_non_inherited_data_->scroll_snap_data_->y_points_;
  }
  void SetScrollSnapPointsY(const ScrollSnapPoints& b) {
    SET_NESTED_VAR(rare_non_inherited_data_, scroll_snap_data_, y_points_, b);
  }

  // scroll-snap-type
  static ScrollSnapType InitialScrollSnapType() { return kScrollSnapTypeNone; }
  ScrollSnapType GetScrollSnapType() const {
    return static_cast<ScrollSnapType>(
        rare_non_inherited_data_->scroll_snap_type_);
  }
  void SetScrollSnapType(ScrollSnapType b) {
    SET_VAR(rare_non_inherited_data_, scroll_snap_type_, b);
  }

  // shape-image-threshold (aka -webkit-shape-image-threshold)
  static float InitialShapeImageThreshold() { return 0; }
  float ShapeImageThreshold() const {
    return rare_non_inherited_data_->shape_image_threshold_;
  }
  void SetShapeImageThreshold(float shape_image_threshold) {
    float clamped_shape_image_threshold =
        clampTo<float>(shape_image_threshold, 0, 1);
    SET_VAR(rare_non_inherited_data_, shape_image_threshold_,
            clamped_shape_image_threshold);
  }

  // shape-outside (aka -webkit-shape-outside)
  static ShapeValue* InitialShapeOutside() { return 0; }
  ShapeValue* ShapeOutside() const {
    return rare_non_inherited_data_->shape_outside_.Get();
  }
  void SetShapeOutside(ShapeValue* value) {
    if (rare_non_inherited_data_->shape_outside_ == value)
      return;
    rare_non_inherited_data_.Access()->shape_outside_ = value;
  }
  bool ShapeOutsideDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ShapeOutside(), other.ShapeOutside());
  }

  // size
  const FloatSize& PageSize() const {
    return rare_non_inherited_data_->page_size_;
  }
  PageSizeType GetPageSizeType() const {
    return static_cast<PageSizeType>(rare_non_inherited_data_->page_size_type_);
  }
  void SetPageSize(const FloatSize& s) {
    SET_VAR(rare_non_inherited_data_, page_size_, s);
  }
  void SetPageSizeType(PageSizeType t) {
    SET_VAR(rare_non_inherited_data_, page_size_type_,
            static_cast<unsigned>(t));
  }

  // Text decoration properties.
  // text-decoration-line
  static TextDecoration InitialTextDecoration() {
    return TextDecoration::kNone;
  }
  TextDecoration GetTextDecoration() const {
    return static_cast<TextDecoration>(TextDecorationInternal());
  }
  void SetTextDecoration(TextDecoration v) { SetTextDecorationInternal(v); }

  // text-decoration-color
  void SetTextDecorationColor(const StyleColor& c) {
    SET_VAR(rare_non_inherited_data_, text_decoration_color_, c);
  }

  // text-decoration-style
  static TextDecorationStyle InitialTextDecorationStyle() {
    return kTextDecorationStyleSolid;
  }
  TextDecorationStyle GetTextDecorationStyle() const {
    return static_cast<TextDecorationStyle>(
        rare_non_inherited_data_->text_decoration_style_);
  }
  void SetTextDecorationStyle(TextDecorationStyle v) {
    SET_VAR(rare_non_inherited_data_, text_decoration_style_, v);
  }

  // text-decoration-skip
  static TextDecorationSkip InitialTextDecorationSkip() {
    return TextDecorationSkip::kObjects;
  }
  TextDecorationSkip GetTextDecorationSkip() const {
    return TextDecorationSkipInternal();
  }
  void SetTextDecorationSkip(TextDecorationSkip v) {
    SetTextDecorationSkipInternal(v);
  }

  // text-overflow
  static TextOverflow InitialTextOverflow() { return kTextOverflowClip; }
  TextOverflow GetTextOverflow() const {
    return static_cast<TextOverflow>(rare_non_inherited_data_->text_overflow_);
  }
  void SetTextOverflow(TextOverflow overflow) {
    SET_VAR(rare_non_inherited_data_, text_overflow_, overflow);
  }

  // touch-action
  static TouchAction InitialTouchAction() {
    return TouchAction::kTouchActionAuto;
  }
  TouchAction GetTouchAction() const {
    return static_cast<TouchAction>(rare_non_inherited_data_->touch_action_);
  }
  void SetTouchAction(TouchAction t) {
    SET_VAR(rare_non_inherited_data_, touch_action_, t);
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

  // will-change
  const Vector<CSSPropertyID>& WillChangeProperties() const {
    return rare_non_inherited_data_->will_change_data_->will_change_properties_;
  }
  bool WillChangeContents() const {
    return rare_non_inherited_data_->will_change_data_->will_change_contents_;
  }
  bool WillChangeScrollPosition() const {
    return rare_non_inherited_data_->will_change_data_
        ->will_change_scroll_position_;
  }
  void SetWillChangeProperties(const Vector<CSSPropertyID>& properties) {
    SET_NESTED_VAR(rare_non_inherited_data_, will_change_data_,
                   will_change_properties_, properties);
  }
  void SetWillChangeContents(bool b) {
    SET_NESTED_VAR(rare_non_inherited_data_, will_change_data_,
                   will_change_contents_, b);
  }
  void SetWillChangeScrollPosition(bool b) {
    SET_NESTED_VAR(rare_non_inherited_data_, will_change_data_,
                   will_change_scroll_position_, b);
  }

  // z-index
  int ZIndex() const { return ZIndexInternal(); }
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
  static float InitialZoom() { return 1.0f; }
  float Zoom() const { return ZoomInternal(); }
  float EffectiveZoom() const { return EffectiveZoomInternal(); }
  bool SetZoom(float);
  bool SetEffectiveZoom(float);

  // -webkit-app-region
  DraggableRegionMode GetDraggableRegionMode() const {
    return static_cast<DraggableRegionMode>(
        rare_non_inherited_data_->draggable_region_mode_);
  }
  void SetDraggableRegionMode(DraggableRegionMode v) {
    SET_VAR(rare_non_inherited_data_, draggable_region_mode_,
            static_cast<unsigned>(v));
  }

  // -webkit-appearance
  static ControlPart InitialAppearance() { return kNoControlPart; }
  ControlPart Appearance() const {
    return static_cast<ControlPart>(rare_non_inherited_data_->appearance_);
  }
  void SetAppearance(ControlPart a) {
    SET_VAR(rare_non_inherited_data_, appearance_, a);
  }

  // -webkit-clip-path
  static ClipPathOperation* InitialClipPath() { return 0; }
  ClipPathOperation* ClipPath() const {
    return rare_non_inherited_data_->clip_path_.Get();
  }
  void SetClipPath(RefPtr<ClipPathOperation> operation) {
    if (rare_non_inherited_data_->clip_path_ != operation)
      rare_non_inherited_data_.Access()->clip_path_ = std::move(operation);
  }
  bool ClipPathDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ClipPath(), other.ClipPath());
  }

  // Mask properties.
  // -webkit-mask-box-image-outset
  const BorderImageLengthBox& MaskBoxImageOutset() const {
    return rare_non_inherited_data_->mask_box_image_.Outset();
  }
  void SetMaskBoxImageOutset(const BorderImageLengthBox& outset) {
    rare_non_inherited_data_.Access()->mask_box_image_.SetOutset(outset);
  }

  // -webkit-mask-box-image-slice
  const LengthBox& MaskBoxImageSlices() const {
    return rare_non_inherited_data_->mask_box_image_.ImageSlices();
  }
  void SetMaskBoxImageSlices(const LengthBox& slices) {
    rare_non_inherited_data_.Access()->mask_box_image_.SetImageSlices(slices);
  }

  // -webkit-mask-box-image-source
  static StyleImage* InitialMaskBoxImageSource() { return 0; }
  StyleImage* MaskBoxImageSource() const {
    return rare_non_inherited_data_->mask_box_image_.GetImage();
  }
  void SetMaskBoxImageSource(StyleImage* v) {
    rare_non_inherited_data_.Access()->mask_box_image_.SetImage(v);
  }

  // -webkit-mask-box-image-width
  const BorderImageLengthBox& MaskBoxImageWidth() const {
    return rare_non_inherited_data_->mask_box_image_.BorderSlices();
  }
  void SetMaskBoxImageWidth(const BorderImageLengthBox& slices) {
    rare_non_inherited_data_.Access()->mask_box_image_.SetBorderSlices(slices);
  }

  // Inherited properties.

  // color
  static Color InitialColor() { return Color::kBlack; }
  void SetColor(const Color&);

  // line-height
  static Length InitialLineHeight() { return Length(-100.0, kPercent); }
  Length LineHeight() const;
  void SetLineHeight(const Length& specified_line_height);

  // List style properties.
  // list-style-image
  static StyleImage* InitialListStyleImage() { return 0; }
  StyleImage* ListStyleImage() const;
  void SetListStyleImage(StyleImage*);

  // quotes
  static QuotesData* InitialQuotes() { return 0; }
  QuotesData* Quotes() const { return QuotesInternal().Get(); }
  void SetQuotes(RefPtr<QuotesData>);

  bool QuotesDataEquivalent(const ComputedStyle&) const;

  // text-shadow
  static ShadowList* InitialTextShadow() { return 0; }
  ShadowList* TextShadow() const { return TextShadowInternal().Get(); }
  void SetTextShadow(RefPtr<ShadowList>);

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
  const LineClampValue& LineClamp() const {
    return rare_non_inherited_data_->line_clamp_;
  }
  void SetLineClamp(LineClampValue c) {
    SET_VAR(rare_non_inherited_data_, line_clamp_, c);
  }

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

  // -webkit-user-drag
  static EUserDrag InitialUserDrag() { return DRAG_AUTO; }
  EUserDrag UserDrag() const {
    return static_cast<EUserDrag>(rare_non_inherited_data_->user_drag_);
  }
  void SetUserDrag(EUserDrag d) {
    SET_VAR(rare_non_inherited_data_, user_drag_, d);
  }

  // caret-color
  void SetCaretColor(const StyleAutoColor& color) {
    SetCaretColorInternal(color.Resolve(Color()));
    SetCaretColorIsCurrentColorInternal(color.IsCurrentColor());
    SetCaretColorIsAutoInternal(color.IsAutoColor());
  }

  // Font properties.
  const Font& GetFont() const;
  void SetFont(const Font&);
  const FontDescription& GetFontDescription() const;
  bool SetFontDescription(const FontDescription&);
  bool HasIdenticalAscentDescentAndLineGap(const ComputedStyle& other) const;

  // font-size
  int FontSize() const;
  float SpecifiedFontSize() const;
  float ComputedFontSize() const;
  LayoutUnit ComputedFontSizeAsFixed() const;

  // font-size-adjust
  float FontSizeAdjust() const;
  bool HasFontSizeAdjust() const;

  // font-weight
  FontWeight GetFontWeight() const;

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
  bool operator==(const ComputedStyle& other) const;
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
    return rare_non_inherited_data_->animations_.get();
  }

  // Transitions.
  const CSSTransitionData* Transitions() const {
    return rare_non_inherited_data_->transitions_.get();
  }
  CSSTransitionData& AccessTransitions();

  // Callback selectors.
  const Vector<String>& CallbackSelectors() const {
    return rare_non_inherited_data_->callback_selectors_;
  }
  void AddCallbackSelector(const String& selector);

  // Non-property flags.
  bool EmptyState() const { return EmptyStateInternal(); }
  void SetEmptyState(bool b) {
    SetUnique();
    SetEmptyStateInternal(b);
  }

  bool HasInlineTransform() const {
    return rare_non_inherited_data_->has_inline_transform_;
  }
  void SetHasInlineTransform(bool b) {
    SET_VAR(rare_non_inherited_data_, has_inline_transform_, b);
  }

  bool HasCompositorProxy() const {
    return rare_non_inherited_data_->has_compositor_proxy_;
  }
  void SetHasCompositorProxy(bool b) {
    SET_VAR(rare_non_inherited_data_, has_compositor_proxy_, b);
  }

  bool RequiresAcceleratedCompositingForExternalReasons(bool b) {
    return rare_non_inherited_data_
        ->requires_accelerated_compositing_for_external_reasons_;
  }
  void SetRequiresAcceleratedCompositingForExternalReasons(bool b) {
    SET_VAR(rare_non_inherited_data_,
            requires_accelerated_compositing_for_external_reasons_, b);
  }

  bool HasAuthorBackground() const {
    return rare_non_inherited_data_->has_author_background_;
  };
  void SetHasAuthorBackground(bool author_background) {
    SET_VAR(rare_non_inherited_data_, has_author_background_,
            author_background);
  }

  bool HasAuthorBorder() const {
    return rare_non_inherited_data_->has_author_border_;
  };
  void SetHasAuthorBorder(bool author_border) {
    SET_VAR(rare_non_inherited_data_, has_author_border_, author_border);
  }

  // A stacking context is painted atomically and defines a stacking order,
  // whereas a containing stacking context defines in which order the stacking
  // contexts below are painted.
  // See CSS 2.1, Appendix E (https://www.w3.org/TR/CSS21/zindex.html) for more
  // details.
  bool IsStackingContext() const {
    return rare_non_inherited_data_->is_stacking_context_;
  }
  void SetIsStackingContext(bool b) {
    SET_VAR(rare_non_inherited_data_, is_stacking_context_, b);
  }

  float TextAutosizingMultiplier() const {
    return TextAutosizingMultiplierInternal();
  }
  void SetTextAutosizingMultiplier(float);

  bool SelfOrAncestorHasDirAutoAttribute() const {
    return SelfOrAncestorHasDirAutoAttributeInternal();
  }
  void SetSelfOrAncestorHasDirAutoAttribute(bool v) {
    SetSelfOrAncestorHasDirAutoAttributeInternal(v);
  }

  // Animation flags.
  bool HasCurrentOpacityAnimation() const {
    return rare_non_inherited_data_->has_current_opacity_animation_;
  }
  void SetHasCurrentOpacityAnimation(bool b = true) {
    SET_VAR(rare_non_inherited_data_, has_current_opacity_animation_, b);
  }

  bool HasCurrentTransformAnimation() const {
    return rare_non_inherited_data_->has_current_transform_animation_;
  }
  void SetHasCurrentTransformAnimation(bool b = true) {
    SET_VAR(rare_non_inherited_data_, has_current_transform_animation_, b);
  }

  bool HasCurrentFilterAnimation() const {
    return rare_non_inherited_data_->has_current_filter_animation_;
  }
  void SetHasCurrentFilterAnimation(bool b = true) {
    SET_VAR(rare_non_inherited_data_, has_current_filter_animation_, b);
  }

  bool HasCurrentBackdropFilterAnimation() const {
    return rare_non_inherited_data_->has_current_backdrop_filter_animation_;
  }
  void SetHasCurrentBackdropFilterAnimation(bool b = true) {
    SET_VAR(rare_non_inherited_data_, has_current_backdrop_filter_animation_,
            b);
  }

  bool IsRunningOpacityAnimationOnCompositor() const {
    return rare_non_inherited_data_->running_opacity_animation_on_compositor_;
  }
  void SetIsRunningOpacityAnimationOnCompositor(bool b = true) {
    SET_VAR(rare_non_inherited_data_, running_opacity_animation_on_compositor_,
            b);
  }

  bool IsRunningTransformAnimationOnCompositor() const {
    return rare_non_inherited_data_->running_transform_animation_on_compositor_;
  }
  void SetIsRunningTransformAnimationOnCompositor(bool b = true) {
    SET_VAR(rare_non_inherited_data_,
            running_transform_animation_on_compositor_, b);
  }

  bool IsRunningFilterAnimationOnCompositor() const {
    return rare_non_inherited_data_->running_filter_animation_on_compositor_;
  }
  void SetIsRunningFilterAnimationOnCompositor(bool b = true) {
    SET_VAR(rare_non_inherited_data_, running_filter_animation_on_compositor_,
            b);
  }

  bool IsRunningBackdropFilterAnimationOnCompositor() const {
    return rare_non_inherited_data_
        ->running_backdrop_filter_animation_on_compositor_;
  }
  void SetIsRunningBackdropFilterAnimationOnCompositor(bool b = true) {
    SET_VAR(rare_non_inherited_data_,
            running_backdrop_filter_animation_on_compositor_, b);
  }

  // Column utility functions.
  void ClearMultiCol();
  bool SpecifiesColumns() const {
    return !HasAutoColumnCount() || !HasAutoColumnWidth();
  }
  bool ColumnRuleIsTransparent() const {
    return rare_non_inherited_data_->multi_col_data_->rule_.IsTransparent();
  }
  bool ColumnRuleEquivalent(const ComputedStyle* other_style) const;
  void InheritColumnPropertiesFrom(const ComputedStyle& parent) {
    rare_non_inherited_data_.Access()->multi_col_data_ =
        parent.rare_non_inherited_data_->multi_col_data_;
  }

  // Flex utility functions.
  bool IsColumnFlexDirection() const {
    return FlexDirection() == kFlowColumn ||
           FlexDirection() == kFlowColumnReverse;
  }
  bool IsReverseFlexDirection() const {
    return FlexDirection() == kFlowRowReverse ||
           FlexDirection() == kFlowColumnReverse;
  }
  bool HasBoxReflect() const { return BoxReflect(); }
  bool ReflectionDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxReflect(), other.BoxReflect());
  }

  // Mask utility functions.
  bool HasMask() const {
    return rare_non_inherited_data_->mask_.HasImage() ||
           rare_non_inherited_data_->mask_box_image_.HasImage();
  }
  StyleImage* MaskImage() const {
    return rare_non_inherited_data_->mask_.GetImage();
  }
  FillLayer& AccessMaskLayers() {
    return rare_non_inherited_data_.Access()->mask_;
  }
  const FillLayer& MaskLayers() const {
    return rare_non_inherited_data_->mask_;
  }
  const NinePieceImage& MaskBoxImage() const {
    return rare_non_inherited_data_->mask_box_image_;
  }
  bool MaskBoxImageSlicesFill() const {
    return rare_non_inherited_data_->mask_box_image_.Fill();
  }
  void AdjustMaskLayers() {
    if (MaskLayers().Next()) {
      AccessMaskLayers().CullEmptyLayers();
      AccessMaskLayers().FillUnsetProperties();
    }
  }
  void SetMaskBoxImage(const NinePieceImage& b) {
    SET_VAR(rare_non_inherited_data_, mask_box_image_, b);
  }
  void SetMaskBoxImageSlicesFill(bool fill) {
    rare_non_inherited_data_.Access()->mask_box_image_.SetFill(fill);
  }

  // Text-combine utility functions.
  bool HasTextCombine() const { return TextCombine() != ETextCombine::kNone; }

  // Grid utility functions.
  const Vector<GridTrackSize>& GridAutoRepeatColumns() const {
    return rare_non_inherited_data_->grid_data_->grid_auto_repeat_columns_;
  }
  const Vector<GridTrackSize>& GridAutoRepeatRows() const {
    return rare_non_inherited_data_->grid_data_->grid_auto_repeat_rows_;
  }
  size_t GridAutoRepeatColumnsInsertionPoint() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_columns_insertion_point_;
  }
  size_t GridAutoRepeatRowsInsertionPoint() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_rows_insertion_point_;
  }
  AutoRepeatType GridAutoRepeatColumnsType() const {
    return static_cast<AutoRepeatType>(
        rare_non_inherited_data_->grid_data_->auto_repeat_columns_type_);
  }
  AutoRepeatType GridAutoRepeatRowsType() const {
    return static_cast<AutoRepeatType>(
        rare_non_inherited_data_->grid_data_->auto_repeat_rows_type_);
  }
  const NamedGridLinesMap& NamedGridColumnLines() const {
    return rare_non_inherited_data_->grid_data_->named_grid_column_lines_;
  }
  const NamedGridLinesMap& NamedGridRowLines() const {
    return rare_non_inherited_data_->grid_data_->named_grid_row_lines_;
  }
  const OrderedNamedGridLines& OrderedNamedGridColumnLines() const {
    return rare_non_inherited_data_->grid_data_
        ->ordered_named_grid_column_lines_;
  }
  const OrderedNamedGridLines& OrderedNamedGridRowLines() const {
    return rare_non_inherited_data_->grid_data_->ordered_named_grid_row_lines_;
  }
  const NamedGridLinesMap& AutoRepeatNamedGridColumnLines() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_named_grid_column_lines_;
  }
  const NamedGridLinesMap& AutoRepeatNamedGridRowLines() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_named_grid_row_lines_;
  }
  const OrderedNamedGridLines& AutoRepeatOrderedNamedGridColumnLines() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_ordered_named_grid_column_lines_;
  }
  const OrderedNamedGridLines& AutoRepeatOrderedNamedGridRowLines() const {
    return rare_non_inherited_data_->grid_data_
        ->auto_repeat_ordered_named_grid_row_lines_;
  }
  const NamedGridAreaMap& NamedGridArea() const {
    return rare_non_inherited_data_->grid_data_->named_grid_area_;
  }
  size_t NamedGridAreaRowCount() const {
    return rare_non_inherited_data_->grid_data_->named_grid_area_row_count_;
  }
  size_t NamedGridAreaColumnCount() const {
    return rare_non_inherited_data_->grid_data_->named_grid_area_column_count_;
  }
  GridAutoFlow GetGridAutoFlow() const {
    return static_cast<GridAutoFlow>(
        rare_non_inherited_data_->grid_data_->grid_auto_flow_);
  }
  bool IsGridAutoFlowDirectionRow() const {
    return (rare_non_inherited_data_->grid_data_->grid_auto_flow_ &
            kInternalAutoFlowDirectionRow) == kInternalAutoFlowDirectionRow;
  }
  bool IsGridAutoFlowDirectionColumn() const {
    return (rare_non_inherited_data_->grid_data_->grid_auto_flow_ &
            kInternalAutoFlowDirectionColumn) ==
           kInternalAutoFlowDirectionColumn;
  }
  bool IsGridAutoFlowAlgorithmSparse() const {
    return (rare_non_inherited_data_->grid_data_->grid_auto_flow_ &
            kInternalAutoFlowAlgorithmSparse) ==
           kInternalAutoFlowAlgorithmSparse;
  }
  bool IsGridAutoFlowAlgorithmDense() const {
    return (rare_non_inherited_data_->grid_data_->grid_auto_flow_ &
            kInternalAutoFlowAlgorithmDense) == kInternalAutoFlowAlgorithmDense;
  }
  void SetGridAutoRepeatColumns(const Vector<GridTrackSize>& track_sizes) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   grid_auto_repeat_columns_, track_sizes);
  }
  void SetGridAutoRepeatRows(const Vector<GridTrackSize>& track_sizes) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, grid_auto_repeat_rows_,
                   track_sizes);
  }
  void SetGridAutoRepeatColumnsInsertionPoint(const size_t insertion_point) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_columns_insertion_point_, insertion_point);
  }
  void SetGridAutoRepeatRowsInsertionPoint(const size_t insertion_point) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_rows_insertion_point_, insertion_point);
  }
  void SetGridAutoRepeatColumnsType(const AutoRepeatType auto_repeat_type) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_columns_type_, auto_repeat_type);
  }
  void SetGridAutoRepeatRowsType(const AutoRepeatType auto_repeat_type) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, auto_repeat_rows_type_,
                   auto_repeat_type);
  }
  void SetNamedGridColumnLines(
      const NamedGridLinesMap& named_grid_column_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   named_grid_column_lines_, named_grid_column_lines);
  }
  void SetNamedGridRowLines(const NamedGridLinesMap& named_grid_row_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, named_grid_row_lines_,
                   named_grid_row_lines);
  }
  void SetOrderedNamedGridColumnLines(
      const OrderedNamedGridLines& ordered_named_grid_column_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   ordered_named_grid_column_lines_,
                   ordered_named_grid_column_lines);
  }
  void SetOrderedNamedGridRowLines(
      const OrderedNamedGridLines& ordered_named_grid_row_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   ordered_named_grid_row_lines_, ordered_named_grid_row_lines);
  }
  void SetAutoRepeatNamedGridColumnLines(
      const NamedGridLinesMap& named_grid_column_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_named_grid_column_lines_,
                   named_grid_column_lines);
  }
  void SetAutoRepeatNamedGridRowLines(
      const NamedGridLinesMap& named_grid_row_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_named_grid_row_lines_, named_grid_row_lines);
  }
  void SetAutoRepeatOrderedNamedGridColumnLines(
      const OrderedNamedGridLines& ordered_named_grid_column_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_ordered_named_grid_column_lines_,
                   ordered_named_grid_column_lines);
  }
  void SetAutoRepeatOrderedNamedGridRowLines(
      const OrderedNamedGridLines& ordered_named_grid_row_lines) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   auto_repeat_ordered_named_grid_row_lines_,
                   ordered_named_grid_row_lines);
  }
  void SetNamedGridArea(const NamedGridAreaMap& named_grid_area) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_, named_grid_area_,
                   named_grid_area);
  }
  void SetNamedGridAreaRowCount(size_t row_count) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   named_grid_area_row_count_, row_count);
  }
  void SetNamedGridAreaColumnCount(size_t column_count) {
    SET_NESTED_VAR(rare_non_inherited_data_, grid_data_,
                   named_grid_area_column_count_, column_count);
  }

  // align-content utility functions.
  ContentPosition AlignContentPosition() const {
    return rare_non_inherited_data_->align_content_.GetPosition();
  }
  ContentDistributionType AlignContentDistribution() const {
    return rare_non_inherited_data_->align_content_.Distribution();
  }
  OverflowAlignment AlignContentOverflowAlignment() const {
    return rare_non_inherited_data_->align_content_.Overflow();
  }
  void SetAlignContentPosition(ContentPosition position) {
    rare_non_inherited_data_.Access()->align_content_.SetPosition(position);
  }
  void SetAlignContentDistribution(ContentDistributionType distribution) {
    rare_non_inherited_data_.Access()->align_content_.SetDistribution(
        distribution);
  }
  void SetAlignContentOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->align_content_.SetOverflow(overflow);
  }

  // justify-content utility functions.
  ContentPosition JustifyContentPosition() const {
    return rare_non_inherited_data_->justify_content_.GetPosition();
  }
  ContentDistributionType JustifyContentDistribution() const {
    return rare_non_inherited_data_->justify_content_.Distribution();
  }
  OverflowAlignment JustifyContentOverflowAlignment() const {
    return rare_non_inherited_data_->justify_content_.Overflow();
  }
  void SetJustifyContentPosition(ContentPosition position) {
    rare_non_inherited_data_.Access()->justify_content_.SetPosition(position);
  }
  void SetJustifyContentDistribution(ContentDistributionType distribution) {
    rare_non_inherited_data_.Access()->justify_content_.SetDistribution(
        distribution);
  }
  void SetJustifyContentOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->justify_content_.SetOverflow(overflow);
  }

  // align-items utility functions.
  ItemPosition AlignItemsPosition() const {
    return rare_non_inherited_data_->align_items_.GetPosition();
  }
  OverflowAlignment AlignItemsOverflowAlignment() const {
    return rare_non_inherited_data_->align_items_.Overflow();
  }
  void SetAlignItemsPosition(ItemPosition position) {
    rare_non_inherited_data_.Access()->align_items_.SetPosition(position);
  }
  void SetAlignItemsOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->align_items_.SetOverflow(overflow);
  }

  // justify-items utility functions.
  ItemPosition JustifyItemsPosition() const {
    return rare_non_inherited_data_->justify_items_.GetPosition();
  }
  OverflowAlignment JustifyItemsOverflowAlignment() const {
    return rare_non_inherited_data_->justify_items_.Overflow();
  }
  ItemPositionType JustifyItemsPositionType() const {
    return rare_non_inherited_data_->justify_items_.PositionType();
  }
  void SetJustifyItemsPosition(ItemPosition position) {
    rare_non_inherited_data_.Access()->justify_items_.SetPosition(position);
  }
  void SetJustifyItemsOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->justify_items_.SetOverflow(overflow);
  }
  void SetJustifyItemsPositionType(ItemPositionType position_type) {
    rare_non_inherited_data_.Access()->justify_items_.SetPositionType(
        position_type);
  }

  // align-self utility functions.
  ItemPosition AlignSelfPosition() const {
    return rare_non_inherited_data_->align_self_.GetPosition();
  }
  OverflowAlignment AlignSelfOverflowAlignment() const {
    return rare_non_inherited_data_->align_self_.Overflow();
  }
  void SetAlignSelfPosition(ItemPosition position) {
    rare_non_inherited_data_.Access()->align_self_.SetPosition(position);
  }
  void SetAlignSelfOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->align_self_.SetOverflow(overflow);
  }

  // justify-self utility functions.
  ItemPosition JustifySelfPosition() const {
    return rare_non_inherited_data_->justify_self_.GetPosition();
  }
  OverflowAlignment JustifySelfOverflowAlignment() const {
    return rare_non_inherited_data_->justify_self_.Overflow();
  }
  void SetJustifySelfPosition(ItemPosition position) {
    rare_non_inherited_data_.Access()->justify_self_.SetPosition(position);
  }
  void SetJustifySelfOverflow(OverflowAlignment overflow) {
    rare_non_inherited_data_.Access()->justify_self_.SetOverflow(overflow);
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
  const Length& MarginBefore() const { return MarginBeforeUsing(this); }
  const Length& MarginAfter() const { return MarginAfterUsing(this); }
  const Length& MarginStart() const { return MarginStartUsing(this); }
  const Length& MarginEnd() const { return MarginEndUsing(this); }
  const Length& MarginOver() const {
    return LengthBox::Over(GetWritingMode(), MarginTop(), MarginRight());
  }
  const Length& MarginUnder() const {
    return LengthBox::Under(GetWritingMode(), MarginBottom(), MarginLeft());
  }
  const Length& MarginStartUsing(const ComputedStyle* other) const {
    return LengthBox::Start(other->GetWritingMode(), other->Direction(),
                            MarginTop(), MarginLeft(), MarginRight(),
                            MarginBottom());
  }
  const Length& MarginEndUsing(const ComputedStyle* other) const {
    return LengthBox::End(other->GetWritingMode(), other->Direction(),
                          MarginTop(), MarginLeft(), MarginRight(),
                          MarginBottom());
  }
  const Length& MarginBeforeUsing(const ComputedStyle* other) const {
    return LengthBox::Before(other->GetWritingMode(), MarginTop(), MarginLeft(),
                             MarginRight());
  }
  const Length& MarginAfterUsing(const ComputedStyle* other) const {
    return LengthBox::After(other->GetWritingMode(), MarginBottom(),
                            MarginLeft(), MarginRight());
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
    return LengthBox::Before(GetWritingMode(), PaddingTop(), PaddingLeft(),
                             PaddingRight());
  }
  const Length& PaddingAfter() const {
    return LengthBox::After(GetWritingMode(), PaddingBottom(), PaddingLeft(),
                            PaddingRight());
  }
  const Length& PaddingStart() const {
    return LengthBox::Start(GetWritingMode(), Direction(), PaddingTop(),
                            PaddingLeft(), PaddingRight(), PaddingBottom());
  }
  const Length& PaddingEnd() const {
    return LengthBox::End(GetWritingMode(), Direction(), PaddingTop(),
                          PaddingLeft(), PaddingRight(), PaddingBottom());
  }
  const Length& PaddingOver() const {
    return LengthBox::Over(GetWritingMode(), PaddingTop(), PaddingRight());
  }
  const Length& PaddingUnder() const {
    return LengthBox::Under(GetWritingMode(), PaddingBottom(), PaddingLeft());
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
                       BorderLeftWidthInternal().ToFloat(),
                       OutlineStyleIsAuto());
  }
  const BorderValue BorderRight() const {
    return BorderValue(BorderRightStyle(), BorderRightColor(),
                       BorderRightWidthInternal().ToFloat(),
                       OutlineStyleIsAuto());
  }
  const BorderValue BorderTop() const {
    return BorderValue(BorderTopStyle(), BorderTopColor(),
                       BorderTopWidthInternal().ToFloat(),
                       OutlineStyleIsAuto());
  }
  const BorderValue BorderBottom() const {
    return BorderValue(BorderBottomStyle(), BorderBottomColor(),
                       BorderBottomWidthInternal().ToFloat(),
                       OutlineStyleIsAuto());
  }

  bool BorderSizeEquals(const ComputedStyle& o) const {
    return BorderLeftWidthInternal() == o.BorderLeftWidthInternal() &&
           BorderTopWidthInternal() == o.BorderTopWidthInternal() &&
           BorderRightWidthInternal() == o.BorderRightWidthInternal() &&
           BorderBottomWidthInternal() == o.BorderBottomWidthInternal();
  }

  BorderValue BorderBefore() const;
  BorderValue BorderAfter() const;
  BorderValue BorderStart() const;
  BorderValue BorderEnd() const;

  float BorderAfterWidth() const;
  float BorderBeforeWidth() const;
  float BorderEndWidth() const;
  float BorderStartWidth() const;
  float BorderOverWidth() const;
  float BorderUnderWidth() const;

  EBorderStyle BorderAfterStyle() const;
  EBorderStyle BorderBeforeStyle() const;
  EBorderStyle BorderEndStyle() const;
  EBorderStyle BorderStartStyle() const;

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

  // Float utility functions.
  bool IsFloating() const { return Floating() != EFloat::kNone; }

  // Mix-blend-mode utility functions.
  bool HasBlendMode() const { return BlendMode() != kWebBlendModeNormal; }

  // Motion utility functions.
  bool HasOffset() const {
    return (OffsetPosition().X() != Length(kAuto)) || OffsetPath();
  }

  // Direction utility functions.
  bool IsLeftToRightDirection() const {
    return Direction() == TextDirection::kLtr;
  }

  // Perspective utility functions.
  bool HasPerspective() const {
    return rare_non_inherited_data_->perspective_ > 0;
  }

  // Page size utility functions.
  void ResetPageSizeType() {
    SET_VAR(rare_non_inherited_data_, page_size_type_,
            static_cast<unsigned>(PageSizeType::kAuto));
  }

  // Outline utility functions.
  bool HasOutline() const {
    return OutlineWidth() > 0 && OutlineStyle() > EBorderStyle::kHidden;
  }
  int OutlineOutsetExtent() const;
  float GetOutlineStrokeWidthForFocusRing() const;
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
    return GetPosition() == EPosition::kFixed ||
           GetPosition() == EPosition::kSticky;
  }

  // Clip utility functions.
  const Length& ClipLeft() const { return ClipInternal().Left(); }
  const Length& ClipRight() const { return ClipInternal().Right(); }
  const Length& ClipTop() const { return ClipInternal().Top(); }
  const Length& ClipBottom() const { return ClipInternal().Bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
  const Length& LogicalLeft() const {
    return LengthBox::LogicalLeft(GetWritingMode(), Left(), Top());
  }
  const Length& LogicalRight() const {
    return LengthBox::LogicalRight(GetWritingMode(), Right(), Bottom());
  }
  const Length& LogicalTop() const {
    return LengthBox::Before(GetWritingMode(), Top(), Left(), Right());
  }
  const Length& LogicalBottom() const {
    return LengthBox::After(GetWritingMode(), Bottom(), Left(), Right());
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
  bool ContainsPaint() const {
    return rare_non_inherited_data_->contain_ & kContainsPaint;
  }
  bool ContainsStyle() const {
    return rare_non_inherited_data_->contain_ & kContainsStyle;
  }
  bool ContainsLayout() const {
    return rare_non_inherited_data_->contain_ & kContainsLayout;
  }
  bool ContainsSize() const {
    return rare_non_inherited_data_->contain_ & kContainsSize;
  }

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
    return rare_non_inherited_data_->transform_data_->operations_
               .Has3DOperation() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
           (Scale() && Scale()->Z() != 1);
  }
  bool HasTransform() const {
    return HasTransformOperations() || HasOffset() ||
           HasCurrentTransformAnimation() || Translate() || Rotate() || Scale();
  }
  bool HasTransformOperations() const {
    return !rare_non_inherited_data_->transform_data_->operations_.Operations()
                .IsEmpty();
  }
  ETransformStyle3D UsedTransformStyle3D() const {
    return HasGroupingProperty() ? kTransformStyle3DFlat : TransformStyle3D();
  }
  // Returns whether the transform operations for |otherStyle| differ from the
  // operations for this style instance. Note that callers may want to also
  // check hasTransform(), as it is possible for two styles to have matching
  // transform operations but differ in other transform-impacting style
  // respects.
  bool TransformDataEquivalent(const ComputedStyle& other) const {
    return rare_non_inherited_data_->transform_data_ ==
           other.rare_non_inherited_data_->transform_data_;
  }
  bool Preserves3D() const {
    return UsedTransformStyle3D() != kTransformStyle3DFlat;
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
  void UpdateIsStackingContext(bool is_document_element, bool is_in_top_layer);
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
           HasBackdropFilter() || Resize() != RESIZE_NONE;
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
  Color VisitedDependentColor(int color_property) const;

  // -webkit-appearance utility functions.
  bool HasAppearance() const { return Appearance() != kNoControlPart; }

  // Other utility functions.
  bool IsStyleAvailable() const;
  bool IsSharable() const;

  bool RequireTransformOrigin(ApplyTransformOrigin apply_origin,
                              ApplyMotionPath) const;

 private:
  void SetVisitedLinkBackgroundColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_background_color_, v);
  }
  void SetVisitedLinkBorderLeftColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_border_left_color_, v);
  }
  void SetVisitedLinkBorderRightColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_border_right_color_, v);
  }
  void SetVisitedLinkBorderBottomColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_border_bottom_color_, v);
  }
  void SetVisitedLinkBorderTopColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_border_top_color_, v);
  }
  void SetVisitedLinkOutlineColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_outline_color_, v);
  }
  void SetVisitedLinkColumnRuleColor(const StyleColor& v) {
    SET_NESTED_VAR(rare_non_inherited_data_, multi_col_data_,
                   visited_link_column_rule_color_, v);
  }
  void SetVisitedLinkTextDecorationColor(const StyleColor& v) {
    SET_VAR(rare_non_inherited_data_, visited_link_text_decoration_color_, v);
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
    return rare_non_inherited_data_->multi_col_data_->rule_.GetColor();
  }
  StyleColor OutlineColor() const {
    return rare_non_inherited_data_->outline_.GetColor();
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
    return rare_non_inherited_data_->visited_link_background_color_;
  }
  StyleColor VisitedLinkBorderLeftColor() const {
    return rare_non_inherited_data_->visited_link_border_left_color_;
  }
  bool VisitedLinkBorderLeftColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderLeftColor() ==
                other.VisitedLinkBorderLeftColor() ||
            !BorderLeftWidth());
  }
  StyleColor VisitedLinkBorderRightColor() const {
    return rare_non_inherited_data_->visited_link_border_right_color_;
  }
  bool VisitedLinkBorderRightColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderRightColor() ==
                other.VisitedLinkBorderRightColor() ||
            !BorderRightWidth());
  }
  StyleColor VisitedLinkBorderBottomColor() const {
    return rare_non_inherited_data_->visited_link_border_bottom_color_;
  }
  bool VisitedLinkBorderBottomColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderBottomColor() ==
                other.VisitedLinkBorderBottomColor() ||
            !BorderBottomWidth());
  }
  StyleColor VisitedLinkBorderTopColor() const {
    return rare_non_inherited_data_->visited_link_border_top_color_;
  }
  bool VisitedLinkBorderTopColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderTopColor() == other.VisitedLinkBorderTopColor() ||
            !BorderTopWidth());
  }
  StyleColor VisitedLinkOutlineColor() const {
    return rare_non_inherited_data_->visited_link_outline_color_;
  }
  bool VisitedLinkOutlineColorHasNotChanged(const ComputedStyle& other) const {
    return (VisitedLinkOutlineColor() == other.VisitedLinkOutlineColor() ||
            !OutlineWidth());
  }
  StyleColor VisitedLinkColumnRuleColor() const {
    return rare_non_inherited_data_->multi_col_data_
        ->visited_link_column_rule_color_;
  }
  StyleColor TextDecorationColor() const {
    return rare_non_inherited_data_->text_decoration_color_;
  }
  StyleColor VisitedLinkTextDecorationColor() const {
    return rare_non_inherited_data_->visited_link_text_decoration_color_;
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
      const StyleImage*,
      const ComputedStyle& other) const;
  bool DiffNeedsVisualRectUpdate(const ComputedStyle& other) const;
  void UpdatePropertySpecificDifferences(const ComputedStyle& other,
                                         StyleDifference&) const;

  static bool ShadowListHasCurrentColor(const ShadowList*);

  StyleInheritedVariables& MutableInheritedVariables();
  StyleNonInheritedVariables& MutableNonInheritedVariables();

  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsTransformAnimation);
};

// FIXME: Reduce/remove the dependency on zoom adjusted int values.
// The float or LayoutUnit versions of layout values should be used.
int AdjustForAbsoluteZoom(int value, float zoom_factor);

inline int AdjustForAbsoluteZoom(int value, const ComputedStyle* style) {
  float zoom_factor = style->EffectiveZoom();
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
  if (compareEqual(ZoomInternal(), f))
    return false;
  SetZoomInternal(f);
  SetEffectiveZoom(EffectiveZoom() * Zoom());
  return true;
}

inline bool ComputedStyle::SetEffectiveZoom(float f) {
  // Clamp the effective zoom value to a smaller (but hopeful still large
  // enough) range, to avoid overflow in derived computations.
  float clamped_effective_zoom = clampTo<float>(f, 1e-6, 1e6);
  if (compareEqual(EffectiveZoomInternal(), clamped_effective_zoom))
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
