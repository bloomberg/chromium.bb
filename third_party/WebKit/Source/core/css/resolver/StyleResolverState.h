/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef StyleResolverState_h
#define StyleResolverState_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/animation/css/CSSAnimationUpdate.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/css/resolver/ElementResolveContext.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/style/CachedUAStyle.h"
#include "core/style/ComputedStyle.h"
#include <memory>

namespace blink {

class FontDescription;

class CORE_EXPORT StyleResolverState {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(StyleResolverState);

 public:
  StyleResolverState(Document&,
                     const ElementResolveContext&,
                     const ComputedStyle* parent_style,
                     const ComputedStyle* layout_parent_style);
  StyleResolverState(Document&,
                     Element*,
                     const ComputedStyle* parent_style = nullptr,
                     const ComputedStyle* layout_parent_style = nullptr);
  ~StyleResolverState();

  // In FontFaceSet and CanvasRenderingContext2D, we don't have an element to
  // grab the document from.  This is why we have to store the document
  // separately.
  Document& GetDocument() const { return *document_; }
  // These are all just pass-through methods to ElementResolveContext.
  Element* GetElement() const { return element_context_.GetElement(); }
  const ContainerNode* ParentNode() const {
    return element_context_.ParentNode();
  }
  const ComputedStyle* RootElementStyle() const {
    return element_context_.RootElementStyle();
  }
  EInsideLink ElementLinkState() const {
    return element_context_.ElementLinkState();
  }
  bool DistributedToInsertionPoint() const {
    return element_context_.DistributedToInsertionPoint();
  }

  const ElementResolveContext& ElementContext() const {
    return element_context_;
  }

  void SetStyle(PassRefPtr<ComputedStyle>);
  const ComputedStyle* Style() const { return style_.Get(); }
  ComputedStyle* Style() { return style_.Get(); }
  PassRefPtr<ComputedStyle> TakeStyle() { return std::move(style_); }

  ComputedStyle& MutableStyleRef() const { return *style_; }
  const ComputedStyle& StyleRef() const { return MutableStyleRef(); }

  const CSSToLengthConversionData& CssToLengthConversionData() const {
    return css_to_length_conversion_data_;
  }
  CSSToLengthConversionData FontSizeConversionData() const;

  void SetConversionFontSizes(
      const CSSToLengthConversionData::FontSizes& font_sizes) {
    css_to_length_conversion_data_.SetFontSizes(font_sizes);
  }
  void SetConversionZoom(float zoom) {
    css_to_length_conversion_data_.SetZoom(zoom);
  }

  CSSAnimationUpdate& AnimationUpdate() { return animation_update_; }

  bool IsAnimationInterpolationMapReady() const {
    return is_animation_interpolation_map_ready_;
  }
  void SetIsAnimationInterpolationMapReady() {
    is_animation_interpolation_map_ready_ = true;
  }

  bool IsAnimatingCustomProperties() const {
    return is_animating_custom_properties_;
  }
  void SetIsAnimatingCustomProperties(bool value) {
    is_animating_custom_properties_ = value;
  }

  void SetParentStyle(RefPtr<const ComputedStyle> parent_style) {
    parent_style_ = std::move(parent_style);
  }
  const ComputedStyle* ParentStyle() const { return parent_style_.Get(); }

  void SetLayoutParentStyle(RefPtr<const ComputedStyle> parent_style) {
    layout_parent_style_ = std::move(parent_style);
  }
  const ComputedStyle* LayoutParentStyle() const {
    return layout_parent_style_.Get();
  }

  // FIXME: These are effectively side-channel "out parameters" for the various
  // map functions. When we map from CSS to style objects we use this state
  // object to track various meta-data about that mapping (e.g. if it's
  // cache-able).  We need to move this data off of StyleResolverState and
  // closer to the objects it applies to. Possibly separating (immutable) inputs
  // from (mutable) outputs.
  void SetApplyPropertyToRegularStyle(bool is_apply) {
    apply_property_to_regular_style_ = is_apply;
  }
  void SetApplyPropertyToVisitedLinkStyle(bool is_apply) {
    apply_property_to_visited_link_style_ = is_apply;
  }
  bool ApplyPropertyToRegularStyle() const {
    return apply_property_to_regular_style_;
  }
  bool ApplyPropertyToVisitedLinkStyle() const {
    return apply_property_to_visited_link_style_;
  }

  void CacheUserAgentBorderAndBackground() {
    // LayoutTheme only needs the cached style if it has an appearance,
    // and constructing it is expensive so we avoid it if possible.
    if (!Style()->HasAppearance())
      return;

    cached_ua_style_ = CachedUAStyle::Create(Style());
  }

  const CachedUAStyle* GetCachedUAStyle() const {
    return cached_ua_style_.get();
  }

  ElementStyleResources& GetElementStyleResources() {
    return element_style_resources_;
  }

  void LoadPendingResources();

  // FIXME: Once styleImage can be made to not take a StyleResolverState
  // this convenience function should be removed. As-is, without this, call
  // sites are extremely verbose.
  StyleImage* GetStyleImage(CSSPropertyID property_id, const CSSValue& value) {
    return element_style_resources_.GetStyleImage(property_id, value);
  }

  FontBuilder& GetFontBuilder() { return font_builder_; }
  const FontBuilder& GetFontBuilder() const { return font_builder_; }
  // FIXME: These exist as a primitive way to track mutations to font-related
  // properties on a ComputedStyle. As designed, these are very error-prone, as
  // some callers set these directly on the ComputedStyle w/o telling us.
  // Presumably we'll want to design a better wrapper around ComputedStyle for
  // tracking these mutations and separate it from StyleResolverState.
  const FontDescription& ParentFontDescription() const {
    return parent_style_->GetFontDescription();
  }

  void SetZoom(float f) {
    if (style_->SetZoom(f))
      font_builder_.DidChangeEffectiveZoom();
  }
  void SetEffectiveZoom(float f) {
    if (style_->SetEffectiveZoom(f))
      font_builder_.DidChangeEffectiveZoom();
  }
  void SetWritingMode(WritingMode new_writing_mode) {
    if (style_->GetWritingMode() == new_writing_mode) {
      return;
    }
    style_->SetWritingMode(new_writing_mode);
    font_builder_.DidChangeWritingMode();
  }
  void SetTextOrientation(ETextOrientation text_orientation) {
    if (style_->GetTextOrientation() != text_orientation) {
      style_->SetTextOrientation(text_orientation);
      font_builder_.DidChangeTextOrientation();
    }
  }

  void SetHasDirAutoAttribute(bool value) { has_dir_auto_attribute_ = value; }
  bool HasDirAutoAttribute() const { return has_dir_auto_attribute_; }

  void SetCustomPropertySetForApplyAtRule(const String&, StylePropertySet*);
  StylePropertySet* CustomPropertySetForApplyAtRule(const String&);

  HeapHashMap<CSSPropertyID, Member<const CSSValue>>&
  ParsedPropertiesForPendingSubstitutionCache(
      const CSSPendingSubstitutionValue&) const;

 private:
  ElementResolveContext element_context_;
  Member<Document> document_;

  // style_ is the primary output for each element's style resolve.
  RefPtr<ComputedStyle> style_;

  CSSToLengthConversionData css_to_length_conversion_data_;

  // parent_style_ is not always just ElementResolveContext::ParentStyle(),
  // so we keep it separate.
  RefPtr<const ComputedStyle> parent_style_;
  // This will almost-always be the same that parent_style_, except in the
  // presence of display: contents. This is the style against which we have to
  // do adjustment.
  RefPtr<const ComputedStyle> layout_parent_style_;

  CSSAnimationUpdate animation_update_;
  bool is_animation_interpolation_map_ready_;
  bool is_animating_custom_properties_;

  bool apply_property_to_regular_style_;
  bool apply_property_to_visited_link_style_;
  bool has_dir_auto_attribute_;

  FontBuilder font_builder_;

  std::unique_ptr<CachedUAStyle> cached_ua_style_;

  ElementStyleResources element_style_resources_;

  HeapHashMap<String, Member<StylePropertySet>>
      custom_property_sets_for_apply_at_rule_;

  mutable HeapHashMap<
      Member<const CSSPendingSubstitutionValue>,
      Member<HeapHashMap<CSSPropertyID, Member<const CSSValue>>>>
      parsed_properties_for_pending_substitution_cache_;
};

}  // namespace blink

#endif  // StyleResolverState_h
