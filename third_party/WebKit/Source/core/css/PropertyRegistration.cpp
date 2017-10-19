// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistration.h"

#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/PropertyDescriptor.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/StyleChangeReason.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

PropertyRegistration::PropertyRegistration(
    const AtomicString& name,
    const CSSSyntaxDescriptor& syntax,
    bool inherits,
    const CSSValue* initial,
    scoped_refptr<CSSVariableData> initial_variable_data)
    : syntax_(syntax),
      inherits_(inherits),
      initial_(initial),
      initial_variable_data_(std::move(initial_variable_data)),
      interpolation_types_(
          CSSInterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
              name,
              syntax,
              *this)) {}

static bool ComputationallyIndependent(const CSSValue& value) {
  DCHECK(!value.IsCSSWideKeyword());

  if (value.IsVariableReferenceValue())
    return !ToCSSVariableReferenceValue(value)
                .VariableDataValue()
                ->NeedsVariableResolution();

  if (value.IsValueList()) {
    for (const CSSValue* inner_value : ToCSSValueList(value)) {
      if (!ComputationallyIndependent(*inner_value))
        return false;
    }
    return true;
  }

  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (!primitive_value.IsLength() &&
        !primitive_value.IsCalculatedPercentageWithLength())
      return true;

    CSSPrimitiveValue::CSSLengthArray length_array;
    primitive_value.AccumulateLengthArray(length_array);
    for (size_t i = 0; i < length_array.values.size(); i++) {
      if (length_array.type_flags.Get(i) &&
          i != CSSPrimitiveValue::kUnitTypePixels &&
          i != CSSPrimitiveValue::kUnitTypePercentage)
        return false;
    }
    return true;
  }

  // TODO(timloh): Images values can also contain lengths.

  return true;
}

void PropertyRegistration::registerProperty(
    ExecutionContext* execution_context,
    const PropertyDescriptor& descriptor,
    ExceptionState& exception_state) {
  // Bindings code ensures these are set.
  DCHECK(descriptor.hasName());
  DCHECK(descriptor.hasInherits());
  DCHECK(descriptor.hasSyntax());

  String name = descriptor.name();
  if (!CSSVariableParser::IsValidVariableName(name)) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Custom property names must start with '--'.");
    return;
  }
  AtomicString atomic_name(name);
  Document* document = ToDocument(execution_context);
  PropertyRegistry& registry = *document->GetPropertyRegistry();
  if (registry.Registration(atomic_name)) {
    exception_state.ThrowDOMException(
        kInvalidModificationError,
        "The name provided has already been registered.");
    return;
  }

  CSSSyntaxDescriptor syntax_descriptor(descriptor.syntax());
  if (!syntax_descriptor.IsValid()) {
    exception_state.ThrowDOMException(
        kSyntaxError,
        "The syntax provided is not a valid custom property syntax.");
    return;
  }

  const CSSValue* initial = nullptr;
  scoped_refptr<CSSVariableData> initial_variable_data;
  if (descriptor.hasInitialValue()) {
    CSSTokenizer tokenizer(descriptor.initialValue());
    const auto tokens = tokenizer.TokenizeToEOF();
    bool is_animation_tainted = false;
    initial = syntax_descriptor.Parse(
        CSSParserTokenRange(tokens),
        document->ElementSheet().Contents()->ParserContext(),
        is_animation_tainted);
    if (!initial) {
      exception_state.ThrowDOMException(
          kSyntaxError,
          "The initial value provided does not parse for the given syntax.");
      return;
    }
    if (!ComputationallyIndependent(*initial)) {
      exception_state.ThrowDOMException(
          kSyntaxError,
          "The initial value provided is not computationally independent.");
      return;
    }
    initial =
        &StyleBuilderConverter::ConvertRegisteredPropertyInitialValue(*initial);
    initial_variable_data = CSSVariableData::Create(
        CSSParserTokenRange(tokens), is_animation_tainted, false);
  } else {
    if (!syntax_descriptor.IsTokenStream()) {
      exception_state.ThrowDOMException(
          kSyntaxError,
          "An initial value must be provided if the syntax is not '*'");
      return;
    }
  }
  registry.RegisterProperty(
      atomic_name, *new PropertyRegistration(atomic_name, syntax_descriptor,
                                             descriptor.inherits(), initial,
                                             std::move(initial_variable_data)));

  document->GetStyleEngine().CustomPropertyRegistered();
}

}  // namespace blink
