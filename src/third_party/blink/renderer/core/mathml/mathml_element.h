// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSToLengthConversionData;
class QualifiedName;

class CORE_EXPORT MathMLElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MathMLElement(const QualifiedName& tagName,
                Document& document,
                ConstructionType constructionType = kCreateMathMLElement);
  ~MathMLElement() override;

  bool HasTagName(const MathMLQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

 protected:
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  base::Optional<Length> AddMathLengthToComputedStyle(
      ComputedStyle&,
      const CSSToLengthConversionData&,
      const QualifiedName&);

 private:
  void ParseAttribute(const AttributeModificationParams&) final;

  bool IsMathMLElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
};

template <typename T>
bool IsElementOfType(const MathMLElement&);
template <>
inline bool IsElementOfType<const MathMLElement>(const MathMLElement&) {
  return true;
}
template <>
inline bool IsElementOfType<const MathMLElement>(const Node& node) {
  return IsA<MathMLElement>(node);
}
template <>
struct DowncastTraits<MathMLElement> {
  static bool AllowFrom(const Node& node) { return node.IsMathMLElement(); }
};

inline bool Node::HasTagName(const MathMLQualifiedName& name) const {
  auto* mathml_element = DynamicTo<MathMLElement>(this);
  return mathml_element && mathml_element->HasTagName(name);
}

}  // namespace blink

#include "third_party/blink/renderer/core/mathml_element_type_helpers.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
