// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_

#include <memory>
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ComputedStyle;
class CrossThreadStyleValue;
class LayoutObject;
class StylePropertyShorthand;
class SVGComputedStyle;

enum PhysicalBoxSide { kTopSide, kRightSide, kBottomSide, kLeftSide };

class CORE_EXPORT CSSProperty : public CSSUnresolvedProperty {
 public:
  static const CSSProperty& Get(CSSPropertyID);

  // For backwards compatibility when passing around CSSUnresolvedProperty
  // references. In case we need to call a function that hasn't been converted
  // to using property classes yet.
  virtual CSSPropertyID PropertyID() const {
    NOTREACHED();
    return CSSPropertyInvalid;
  }
  virtual CSSPropertyName GetCSSPropertyName() const {
    return CSSPropertyName(PropertyID());
  }
  bool IDEquals(CSSPropertyID id) const { return PropertyID() == id; }
  bool IsResolvedProperty() const override { return true; }
  virtual bool IsInterpolable() const { return false; }
  virtual bool IsInherited() const { return false; }
  virtual bool IsCompositableProperty() const { return false; }
  virtual bool IsRepeated() const { return false; }
  virtual char RepetitionSeparator() const {
    NOTREACHED();
    return 0;
  }
  virtual bool IsDescriptor() const { return false; }
  virtual bool SupportsPercentage() const { return false; }
  virtual bool IsProperty() const { return true; }
  virtual bool IsAffectedByAll() const { return IsEnabled() && IsProperty(); }
  virtual bool IsLayoutDependentProperty() const { return false; }
  virtual bool IsLayoutDependent(const ComputedStyle* style,
                                 LayoutObject* layout_object) const {
    return false;
  }
  virtual bool IsValidForVisitedLink() const { return false; }

  virtual const CSSValue* CSSValueFromComputedStyleInternal(
      const ComputedStyle&,
      const SVGComputedStyle&,
      const LayoutObject*,
      Node*,
      bool allow_visited_style) const {
    return nullptr;
  }
  // TODO: Resolve computed auto alignment in applyProperty/ComputedStyle and
  // remove this non-const Node parameter.
  const CSSValue* CSSValueFromComputedStyle(const ComputedStyle&,
                                            const LayoutObject*,
                                            Node*,
                                            bool allow_visited_style) const;
  virtual std::unique_ptr<CrossThreadStyleValue>
  CrossThreadStyleValueFromComputedStyle(const ComputedStyle& computed_style,
                                         const LayoutObject* layout_object,
                                         Node* node,
                                         bool allow_visited_style) const;
  virtual const CSSProperty& ResolveDirectionAwareProperty(TextDirection,
                                                           WritingMode) const {
    return *this;
  }
  virtual bool IsShorthand() const { return false; }
  virtual bool IsLonghand() const { return false; }
  static void FilterEnabledCSSPropertiesIntoVector(const CSSPropertyID*,
                                                   size_t length,
                                                   Vector<const CSSProperty*>&);

 protected:
  constexpr CSSProperty() : CSSUnresolvedProperty() {}

  static const StylePropertyShorthand& BorderDirections();
  static const CSSProperty& ResolveAfterToPhysicalProperty(
      TextDirection,
      WritingMode,
      const StylePropertyShorthand&);
  static const CSSProperty& ResolveBeforeToPhysicalProperty(
      TextDirection,
      WritingMode,
      const StylePropertyShorthand&);
  static const CSSProperty& ResolveEndToPhysicalProperty(
      TextDirection,
      WritingMode,
      const StylePropertyShorthand&);
  static const CSSProperty& ResolveStartToPhysicalProperty(
      TextDirection,
      WritingMode,
      const StylePropertyShorthand&);
};

template <>
struct DowncastTraits<CSSProperty> {
  static bool AllowFrom(const CSSUnresolvedProperty& unresolved) {
    return unresolved.IsResolvedProperty();
  }
};

CORE_EXPORT const CSSProperty& GetCSSPropertyVariable();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
