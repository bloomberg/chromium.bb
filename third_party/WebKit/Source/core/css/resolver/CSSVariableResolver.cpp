// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/CSSVariableResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSVariableData.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/StyleEngine.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "platform/wtf/Vector.h"

namespace blink {

bool CSSVariableResolver::ResolveFallback(CSSParserTokenRange range,
                                          bool disallow_animation_tainted,
                                          Vector<CSSParserToken>& result,
                                          bool& result_is_animation_tainted) {
  if (range.AtEnd())
    return false;
  DCHECK_EQ(range.Peek().GetType(), kCommaToken);
  range.Consume();
  return ResolveTokenRange(range, disallow_animation_tainted, result,
                           result_is_animation_tainted);
}

CSSVariableData* CSSVariableResolver::ValueForCustomProperty(
    AtomicString name) {
  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    return nullptr;
  }

  DCHECK(registry_ || !RuntimeEnabledFeatures::cssVariables2Enabled());
  const PropertyRegistration* registration =
      registry_ ? registry_->Registration(name) : nullptr;

  CSSVariableData* variable_data = nullptr;
  if (!registration || registration->Inherits()) {
    if (inherited_variables_)
      variable_data = inherited_variables_->GetVariable(name);
  } else {
    if (non_inherited_variables_)
      variable_data = non_inherited_variables_->GetVariable(name);
  }
  if (!variable_data)
    return registration ? registration->InitialVariableData() : nullptr;
  if (!variable_data->NeedsVariableResolution())
    return variable_data;

  RefPtr<CSSVariableData> new_variable_data =
      ResolveCustomProperty(name, *variable_data);
  if (!registration) {
    inherited_variables_->SetVariable(name, new_variable_data);
    return new_variable_data.Get();
  }

  const CSSValue* parsed_value = nullptr;
  if (new_variable_data) {
    parsed_value = new_variable_data->ParseForSyntax(registration->Syntax());
    if (!parsed_value)
      new_variable_data = nullptr;
  }
  if (registration->Inherits()) {
    inherited_variables_->SetVariable(name, new_variable_data);
    inherited_variables_->SetRegisteredVariable(name, parsed_value);
  } else {
    non_inherited_variables_->SetVariable(name, new_variable_data);
    non_inherited_variables_->SetRegisteredVariable(name, parsed_value);
  }
  if (!new_variable_data)
    return registration->InitialVariableData();
  return new_variable_data.Get();
}

PassRefPtr<CSSVariableData> CSSVariableResolver::ResolveCustomProperty(
    AtomicString name,
    const CSSVariableData& variable_data) {
  DCHECK(variable_data.NeedsVariableResolution());

  bool disallow_animation_tainted = false;
  bool is_animation_tainted = variable_data.IsAnimationTainted();
  Vector<CSSParserToken> tokens;
  variables_seen_.insert(name);
  bool success =
      ResolveTokenRange(variable_data.Tokens(), disallow_animation_tainted,
                        tokens, is_animation_tainted);
  variables_seen_.erase(name);

  // The old variable data holds onto the backing string the new resolved
  // CSSVariableData relies on. Ensure it will live beyond us overwriting the
  // RefPtr in StyleInheritedVariables.
  DCHECK_GT(variable_data.RefCount(), 1);

  if (!success || !cycle_start_points_.IsEmpty()) {
    cycle_start_points_.erase(name);
    return nullptr;
  }
  return CSSVariableData::CreateResolved(tokens, variable_data,
                                         is_animation_tainted);
}

bool CSSVariableResolver::ResolveVariableReference(
    CSSParserTokenRange range,
    bool disallow_animation_tainted,
    Vector<CSSParserToken>& result,
    bool& result_is_animation_tainted) {
  range.ConsumeWhitespace();
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  AtomicString variable_name =
      range.ConsumeIncludingWhitespace().Value().ToAtomicString();
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CSSVariableData* variable_data = ValueForCustomProperty(variable_name);
  if (!variable_data ||
      (disallow_animation_tainted && variable_data->IsAnimationTainted())) {
    // TODO(alancutter): Append the registered initial custom property value if
    // we are disallowing an animation tainted value.
    return ResolveFallback(range, disallow_animation_tainted, result,
                           result_is_animation_tainted);
  }

  result.AppendVector(variable_data->Tokens());
  result_is_animation_tainted |= variable_data->IsAnimationTainted();

  Vector<CSSParserToken> trash;
  bool trash_is_animation_tainted;
  ResolveFallback(range, disallow_animation_tainted, trash,
                  trash_is_animation_tainted);
  return true;
}

void CSSVariableResolver::ResolveApplyAtRule(CSSParserTokenRange& range,
                                             Vector<CSSParserToken>& result) {
  DCHECK(range.Peek().GetType() == kAtKeywordToken &&
         EqualIgnoringASCIICase(range.Peek().Value(), "apply"));
  range.ConsumeIncludingWhitespace();
  const CSSParserToken& variable_name = range.ConsumeIncludingWhitespace();
  // TODO(timloh): Should we actually be consuming this?
  if (range.Peek().GetType() == kSemicolonToken)
    range.Consume();

  CSSVariableData* variable_data =
      ValueForCustomProperty(variable_name.Value().ToAtomicString());
  if (!variable_data)
    return;  // Invalid custom property

  CSSParserTokenRange rule = variable_data->TokenRange();
  rule.ConsumeWhitespace();
  if (rule.Peek().GetType() != kLeftBraceToken)
    return;
  CSSParserTokenRange rule_contents = rule.ConsumeBlock();
  rule.ConsumeWhitespace();
  if (!rule.AtEnd())
    return;

  result.AppendRange(rule_contents.begin(), rule_contents.end());
}

bool CSSVariableResolver::ResolveTokenRange(CSSParserTokenRange range,
                                            bool disallow_animation_tainted,
                                            Vector<CSSParserToken>& result,
                                            bool& result_is_animation_tainted) {
  bool success = true;
  while (!range.AtEnd()) {
    if (range.Peek().FunctionId() == CSSValueVar) {
      success &= ResolveVariableReference(range.ConsumeBlock(),
                                          disallow_animation_tainted, result,
                                          result_is_animation_tainted);
    } else if (range.Peek().GetType() == kAtKeywordToken &&
               EqualIgnoringASCIICase(range.Peek().Value(), "apply") &&
               RuntimeEnabledFeatures::cssApplyAtRulesEnabled()) {
      ResolveApplyAtRule(range, result);
    } else {
      result.push_back(range.Consume());
    }
  }
  return success;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSValue& value,
    bool disallow_animation_tainted) {
  DCHECK(!isShorthandProperty(id));

  if (value.IsPendingSubstitutionValue()) {
    return ResolvePendingSubstitutions(state, id,
                                       ToCSSPendingSubstitutionValue(value),
                                       disallow_animation_tainted);
  }

  if (value.IsVariableReferenceValue()) {
    return ResolveVariableReferences(state, id,
                                     ToCSSVariableReferenceValue(value),
                                     disallow_animation_tainted);
  }

  NOTREACHED();
  return nullptr;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSVariableReferenceValue& value,
    bool disallow_animation_tainted) {
  CSSVariableResolver resolver(state);
  Vector<CSSParserToken> tokens;
  bool is_animation_tainted = false;
  if (!resolver.ResolveTokenRange(value.VariableDataValue()->Tokens(),
                                  disallow_animation_tainted, tokens,
                                  is_animation_tainted))
    return CSSUnsetValue::Create();
  const CSSValue* result =
      CSSPropertyParser::ParseSingleValue(id, tokens, value.ParserContext());
  if (!result)
    return CSSUnsetValue::Create();
  return result;
}

const CSSValue* CSSVariableResolver::ResolvePendingSubstitutions(
    const StyleResolverState& state,
    CSSPropertyID id,
    const CSSPendingSubstitutionValue& pending_value,
    bool disallow_animation_tainted) {
  // Longhands from shorthand references follow this path.
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>& property_cache =
      state.ParsedPropertiesForPendingSubstitutionCache(pending_value);

  const CSSValue* value = property_cache.at(id);
  if (!value) {
    // TODO(timloh): We shouldn't retry this for all longhands if the shorthand
    // ends up invalid.
    CSSVariableReferenceValue* shorthand_value = pending_value.ShorthandValue();
    CSSPropertyID shorthand_property_id = pending_value.ShorthandPropertyId();

    CSSVariableResolver resolver(state);

    Vector<CSSParserToken> tokens;
    bool is_animation_tainted = false;
    if (resolver.ResolveTokenRange(
            shorthand_value->VariableDataValue()->Tokens(),
            disallow_animation_tainted, tokens, is_animation_tainted)) {

      HeapVector<CSSProperty, 256> parsed_properties;

      if (CSSPropertyParser::ParseValue(
              shorthand_property_id, false, CSSParserTokenRange(tokens),
              shorthand_value->ParserContext(), parsed_properties,
              StyleRule::RuleType::kStyle)) {
        unsigned parsed_properties_count = parsed_properties.size();
        for (unsigned i = 0; i < parsed_properties_count; ++i) {
          property_cache.Set(parsed_properties[i].Id(),
                             parsed_properties[i].Value());
        }
      }
    }
    value = property_cache.at(id);
  }

  if (value)
    return value;

  return CSSUnsetValue::Create();
}

void CSSVariableResolver::ResolveVariableDefinitions(
    const StyleResolverState& state) {
  StyleInheritedVariables* inherited_variables =
      state.Style()->InheritedVariables();
  StyleNonInheritedVariables* non_inherited_variables =
      state.Style()->NonInheritedVariables();
  if (!inherited_variables && !non_inherited_variables)
    return;

  CSSVariableResolver resolver(state);
  int variable_count = 0;
  if (inherited_variables) {
    for (auto& variable : inherited_variables->data_)
      resolver.ValueForCustomProperty(variable.key);
    variable_count += inherited_variables->data_.size();
  }
  if (non_inherited_variables) {
    for (auto& variable : non_inherited_variables->data_)
      resolver.ValueForCustomProperty(variable.key);
    variable_count += non_inherited_variables->data_.size();
  }
  INCREMENT_STYLE_STATS_COUNTER(state.GetDocument().GetStyleEngine(),
                                custom_properties_applied, variable_count);
}

void CSSVariableResolver::ComputeRegisteredVariables(
    const StyleResolverState& state) {
  // const_cast is needed because Persistent<const ...> doesn't work properly.

  StyleInheritedVariables* inherited_variables =
      state.Style()->InheritedVariables();
  if (inherited_variables) {
    for (auto& variable : inherited_variables->registered_data_) {
      if (variable.value) {
        variable.value = const_cast<CSSValue*>(
            &StyleBuilderConverter::ConvertRegisteredPropertyValue(
                state, *variable.value));
      }
    }
  }

  StyleNonInheritedVariables* non_inherited_variables =
      state.Style()->NonInheritedVariables();
  if (non_inherited_variables) {
    for (auto& variable : non_inherited_variables->registered_data_) {
      if (variable.value) {
        variable.value = const_cast<CSSValue*>(
            &StyleBuilderConverter::ConvertRegisteredPropertyValue(
                state, *variable.value));
      }
    }
  }
}

CSSVariableResolver::CSSVariableResolver(const StyleResolverState& state)
    : inherited_variables_(state.Style()->InheritedVariables()),
      non_inherited_variables_(state.Style()->NonInheritedVariables()),
      registry_(state.GetDocument().GetPropertyRegistry()) {}

}  // namespace blink
