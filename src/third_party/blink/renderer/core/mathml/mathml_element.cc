// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

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
  if (name == html_names::kDirAttr || name == mathml_names::kMathsizeAttr ||
      name == mathml_names::kMathcolorAttr ||
      name == mathml_names::kMathbackgroundAttr ||
      name == mathml_names::kMathvariantAttr ||
      name == mathml_names::kScriptlevelAttr ||
      name == mathml_names::kDisplayAttr ||
      name == mathml_names::kDisplaystyleAttr)
    return true;
  return Element::IsPresentationAttribute(name);
}

namespace {

bool ParseScriptLevel(const AtomicString& attributeValue,
                      unsigned& scriptLevel,
                      bool& add) {
  String value = attributeValue;
  if (value.StartsWith("+") || value.StartsWith("-")) {
    add = true;
    value = value.Right(1);
  }

  return WTF::VisitCharacters(
      value, [&](const auto* position, unsigned length) {
        WTF::NumberParsingResult result;
        WTF::NumberParsingOptions options(
            WTF::NumberParsingOptions::kAcceptMinusZeroForUnsigned);
        scriptLevel = CharactersToUInt(position, length, options, &result);
        return result == WTF::NumberParsingResult::kSuccess;
      });
}

}  // namespace

void MathMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
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
  } else if (name == mathml_names::kScriptlevelAttr) {
    unsigned scriptLevel = 0;
    bool add = false;
    if (ParseScriptLevel(value, scriptLevel, add)) {
      if (add) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMathDepth, "add(" + value + ")");
      } else {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMathDepth, scriptLevel,
            CSSPrimitiveValue::UnitType::kNumber);
      }
    }
  } else if (name == mathml_names::kDisplayAttr &&
             HasTagName(mathml_names::kMathTag)) {
    if (EqualIgnoringASCIICase(value, "inline")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                              CSSValueID::kMath);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kCompact);
    } else if (EqualIgnoringASCIICase(value, "block")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                              "block math");
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kNormal);
    }
  } else if (name == mathml_names::kDisplaystyleAttr) {
    if (EqualIgnoringASCIICase(value, "false")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kCompact);
    } else if (EqualIgnoringASCIICase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kNormal);
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
        event_name, JSEventHandlerForContentAttribute::Create(
                        GetExecutionContext(), param.name, param.new_value));
    return;
  }

  Element::ParseAttribute(param);
}

absl::optional<bool> MathMLElement::BooleanAttribute(
    const QualifiedName& name) const {
  const AtomicString& value = FastGetAttribute(name);
  if (EqualIgnoringASCIICase(value, "true"))
    return true;
  if (EqualIgnoringASCIICase(value, "false"))
    return false;
  return absl::nullopt;
}

absl::optional<Length> MathMLElement::AddMathLengthToComputedStyle(
    const CSSToLengthConversionData& conversion_data,
    const QualifiedName& attr_name,
    AllowPercentages allow_percentages) {
  if (!FastHasAttribute(attr_name))
    return absl::nullopt;
  auto value = FastGetAttribute(attr_name);
  const CSSPrimitiveValue* parsed_value = CSSParser::ParseLengthPercentage(
      value,
      StrictCSSParserContext(GetExecutionContext()->GetSecureContextMode()));
  if (!parsed_value || parsed_value->IsCalculated() ||
      (parsed_value->IsPercentage() &&
       (!value.EndsWith('%') || allow_percentages == AllowPercentages::kNo)))
    return absl::nullopt;
  return parsed_value->ConvertToLength(conversion_data);
}

bool MathMLElement::IsTokenElement() const {
  return HasTagName(mathml_names::kMiTag) || HasTagName(mathml_names::kMoTag) ||
         HasTagName(mathml_names::kMnTag) ||
         HasTagName(mathml_names::kMtextTag) ||
         HasTagName(mathml_names::kMsTag);
}

}  // namespace blink
