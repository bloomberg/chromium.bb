// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

MathMLElement::MathMLElement(const QualifiedName& tagName,
                             Document& document,
                             ConstructionType constructionType)
    : Element(tagName, &document, constructionType) {}

MathMLElement::~MathMLElement() {}

static inline bool IsValidDirAttribute(const AtomicString& value) {
  return EqualIgnoringASCIICase(value, "ltr") ||
         EqualIgnoringASCIICase(value, "rtl");
}

// Keywords from MathML3 and CSS font-size are skipped.
static inline bool IsDisallowedMathSizeAttribute(const AtomicString& value) {
  return EqualIgnoringASCIICase(value, "medium") ||
         value.EndsWith("large", kTextCaseASCIIInsensitive) ||
         value.EndsWith("small", kTextCaseASCIIInsensitive) ||
         EqualIgnoringASCIICase(value, "smaller") ||
         EqualIgnoringASCIICase(value, "larger");
}

bool MathMLElement::IsPresentationAttribute(const QualifiedName& name) const {
  // TODO(crbug.com/1023292): add support for displaystyle and scriptlevel.
  if (name == html_names::kDirAttr || name == mathml_names::kMathsizeAttr ||
      name == mathml_names::kMathcolorAttr ||
      name == mathml_names::kMathbackgroundAttr ||
      name == mathml_names::kMathvariantAttr ||
      name == mathml_names::kDisplayAttr ||
      name == mathml_names::kDisplaystyleAttr)
    return true;
  return Element::IsPresentationAttribute(name);
}

void MathMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  // TODO(crbug.com/1023292, crbug.com/1023296): add support for display,
  // displaystyle and scriptlevel.
  if (name == html_names::kDirAttr) {
    if (IsValidDirAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDirection,
                                              value);
    }
  } else if (name == mathml_names::kMathsizeAttr) {
    if (!IsDisallowedMathSizeAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFontSize,
                                              value);
    }
  } else if (name == mathml_names::kMathbackgroundAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == mathml_names::kMathcolorAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kColor,
                                            value);
  } else if (name == mathml_names::kDisplayAttr &&
             HasTagName(mathml_names::kMathTag)) {
    if (EqualIgnoringASCIICase(value, "inline")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                              CSSValueID::kInlineMath);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kInline);
    } else if (EqualIgnoringASCIICase(value, "block")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                              CSSValueID::kMath);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kDisplay);
    }
  } else if (name == mathml_names::kDisplaystyleAttr) {
    if (EqualIgnoringASCIICase(value, "false")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kInline);
    } else if (EqualIgnoringASCIICase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kDisplay);
    }
  } else if (name == mathml_names::kMathvariantAttr) {
    // TODO(crbug.com/1076420): this needs to handle all mathvariant values.
    if (EqualIgnoringASCIICase(value, "normal")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kTextTransform, CSSValueID::kNone);
    }
  } else {
    Element::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void MathMLElement::ParseAttribute(const AttributeModificationParams& param) {
  const AtomicString& event_name =
      HTMLElement::EventNameForAttributeName(param.name);
  if (!event_name.IsNull()) {
    SetAttributeEventListener(
        event_name,
        CreateAttributeEventListener(this, param.name, param.new_value));
    return;
  }

  Element::ParseAttribute(param);
}

base::Optional<Length> MathMLElement::AddMathLengthToComputedStyle(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data,
    const QualifiedName& attr_name) {
  if (!FastHasAttribute(attr_name))
    return base::nullopt;
  auto value = FastGetAttribute(attr_name);
  const CSSPrimitiveValue* parsed_value = CSSParser::ParseLengthPercentage(
      value, StrictCSSParserContext(GetDocument().GetSecureContextMode()));
  if (!parsed_value || parsed_value->IsCalculated())
    return base::nullopt;
  return parsed_value->ConvertToLength(conversion_data);
}

}  // namespace blink
