// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/CSSVariableResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
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

bool CSSVariableResolver::ResolveFallback(
    CSSParserTokenRange range,
    bool disallow_animation_tainted,
    Vector<CSSParserToken>& result,
    Vector<String>& result_backing_strings,
    bool& result_is_animation_tainted) {
  if (range.AtEnd())
    return false;
  DCHECK_EQ(range.Peek().GetType(), kCommaToken);
  range.Consume();
  return ResolveTokenRange(range, disallow_animation_tainted, result,
                           result_backing_strings, result_is_animation_tainted);
}

CSSVariableData* CSSVariableResolver::ValueForCustomProperty(
    AtomicString name) {
  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    return nullptr;
  }

  DCHECK(registry_ || !RuntimeEnabledFeatures::CSSVariables2Enabled());
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

  bool unused_cycle_detected;
  RefPtr<CSSVariableData> new_variable_data =
      ResolveCustomProperty(name, *variable_data, unused_cycle_detected);
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
    const CSSVariableData& variable_data,
    bool& cycle_detected) {
  DCHECK(variable_data.NeedsVariableResolution());

  bool disallow_animation_tainted = false;
  bool is_animation_tainted = variable_data.IsAnimationTainted();
  Vector<CSSParserToken> tokens;
  Vector<String> backing_strings;
  backing_strings.AppendVector(variable_data.BackingStrings());
  DCHECK(!variables_seen_.Contains(name));
  variables_seen_.insert(name);
  bool success =
      ResolveTokenRange(variable_data.Tokens(), disallow_animation_tainted,
                        tokens, backing_strings, is_animation_tainted);
  variables_seen_.erase(name);

  if (!success || !cycle_start_points_.IsEmpty()) {
    cycle_start_points_.erase(name);
    cycle_detected = true;
    return nullptr;
  }
  cycle_detected = false;
  return CSSVariableData::CreateResolved(tokens, std::move(backing_strings),
                                         is_animation_tainted);
}

bool CSSVariableResolver::ResolveVariableReference(
    CSSParserTokenRange range,
    bool disallow_animation_tainted,
    Vector<CSSParserToken>& result,
    Vector<String>& result_backing_strings,
    bool& result_is_animation_tainted) {
  range.ConsumeWhitespace();
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  AtomicString variable_name =
      range.ConsumeIncludingWhitespace().Value().ToAtomicString();
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  PropertyHandle property(variable_name);
  if (state_.AnimationPendingCustomProperties().Contains(property) &&
      !variables_seen_.Contains(variable_name)) {
    // We make the StyleResolverState mutable for animated custom properties as
    // an optimisation. Without this we would need to compute animated values on
    // the stack without saving the result or perform an expensive and complex
    // value dependency graph analysis to compute them in the required order.
    StyleResolver::ApplyAnimatedCustomProperty(
        const_cast<StyleResolverState&>(state_), *this, property);
    // Null custom property storage may become non-null after application, we
    // must refresh these cached values.
    inherited_variables_ = state_.Style()->InheritedVariables();
    non_inherited_variables_ = state_.Style()->NonInheritedVariables();
  }
  CSSVariableData* variable_data = ValueForCustomProperty(variable_name);
  if (!variable_data ||
      (disallow_animation_tainted && variable_data->IsAnimationTainted())) {
    // TODO(alancutter): Append the registered initial custom property value if
    // we are disallowing an animation tainted value.
    return ResolveFallback(range, disallow_animation_tainted, result,
                           result_backing_strings, result_is_animation_tainted);
  }

  result.AppendVector(variable_data->Tokens());
  // TODO(alancutter): Avoid adding backing strings multiple times in a row.
  result_backing_strings.AppendVector(variable_data->BackingStrings());
  result_is_animation_tainted |= variable_data->IsAnimationTainted();

  Vector<CSSParserToken> trash;
  Vector<String> trash_backing_strings;
  bool trash_is_animation_tainted;
  ResolveFallback(range, disallow_animation_tainted, trash,
                  trash_backing_strings, trash_is_animation_tainted);
  return true;
}

void CSSVariableResolver::ResolveApplyAtRule(
    CSSParserTokenRange& range,
    Vector<CSSParserToken>& result,
    Vector<String>& result_backing_strings) {
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
  result_backing_strings.AppendVector(variable_data->BackingStrings());
}

bool CSSVariableResolver::ResolveTokenRange(
    CSSParserTokenRange range,
    bool disallow_animation_tainted,
    Vector<CSSParserToken>& result,
    Vector<String>& result_backing_strings,
    bool& result_is_animation_tainted) {
  bool success = true;
  while (!range.AtEnd()) {
    if (range.Peek().FunctionId() == CSSValueVar) {
      success &= ResolveVariableReference(
          range.ConsumeBlock(), disallow_animation_tainted, result,
          result_backing_strings, result_is_animation_tainted);
    } else if (range.Peek().GetType() == kAtKeywordToken &&
               EqualIgnoringASCIICase(range.Peek().Value(), "apply") &&
               RuntimeEnabledFeatures::CSSApplyAtRulesEnabled()) {
      ResolveApplyAtRule(range, result, result_backing_strings);
    } else {
      result.push_back(range.Consume());
    }
  }
  return success;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    CSSPropertyID id,
    const CSSValue& value,
    bool disallow_animation_tainted) {
  DCHECK(!isShorthandProperty(id));

  if (value.IsPendingSubstitutionValue()) {
    return ResolvePendingSubstitutions(id, ToCSSPendingSubstitutionValue(value),
                                       disallow_animation_tainted);
  }

  if (value.IsVariableReferenceValue()) {
    return ResolveVariableReferences(id, ToCSSVariableReferenceValue(value),
                                     disallow_animation_tainted);
  }

  NOTREACHED();
  return nullptr;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    CSSPropertyID id,
    const CSSVariableReferenceValue& value,
    bool disallow_animation_tainted) {
  Vector<CSSParserToken> tokens;
  Vector<String> backing_strings;
  bool is_animation_tainted = false;
  if (!ResolveTokenRange(value.VariableDataValue()->Tokens(),
                         disallow_animation_tainted, tokens, backing_strings,
                         is_animation_tainted)) {
    return CSSUnsetValue::Create();
  }
  const CSSValue* result =
      CSSPropertyParser::ParseSingleValue(id, tokens, value.ParserContext());
  if (!result)
    return CSSUnsetValue::Create();
  return result;
}

const CSSValue* CSSVariableResolver::ResolvePendingSubstitutions(
    CSSPropertyID id,
    const CSSPendingSubstitutionValue& pending_value,
    bool disallow_animation_tainted) {
  // Longhands from shorthand references follow this path.
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>& property_cache =
      state_.ParsedPropertiesForPendingSubstitutionCache(pending_value);

  const CSSValue* value = property_cache.at(id);
  if (!value) {
    // TODO(timloh): We shouldn't retry this for all longhands if the shorthand
    // ends up invalid.
    CSSVariableReferenceValue* shorthand_value = pending_value.ShorthandValue();
    CSSPropertyID shorthand_property_id = pending_value.ShorthandPropertyId();

    Vector<CSSParserToken> tokens;
    Vector<String> backing_strings;
    bool is_animation_tainted = false;
    if (ResolveTokenRange(shorthand_value->VariableDataValue()->Tokens(),
                          disallow_animation_tainted, tokens, backing_strings,
                          is_animation_tainted)) {
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

RefPtr<CSSVariableData>
CSSVariableResolver::ResolveCustomPropertyAnimationKeyframe(
    const CSSCustomPropertyDeclaration& keyframe,
    bool& cycle_detected) {
  DCHECK(keyframe.Value());
  DCHECK(keyframe.Value()->NeedsVariableResolution());
  const AtomicString& name = keyframe.GetName();

  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    cycle_detected = true;
    return nullptr;
  }

  return ResolveCustomProperty(name, *keyframe.Value(), cycle_detected);
}

void CSSVariableResolver::ResolveVariableDefinitions() {
  if (!inherited_variables_ && !non_inherited_variables_)
    return;

  int variable_count = 0;
  if (inherited_variables_) {
    for (auto& variable : inherited_variables_->data_)
      ValueForCustomProperty(variable.key);
    variable_count += inherited_variables_->data_.size();
  }
  if (non_inherited_variables_) {
    for (auto& variable : non_inherited_variables_->data_)
      ValueForCustomProperty(variable.key);
    variable_count += non_inherited_variables_->data_.size();
  }
  INCREMENT_STYLE_STATS_COUNTER(state_.GetDocument().GetStyleEngine(),
                                custom_properties_applied, variable_count);
}

void CSSVariableResolver::ComputeRegisteredVariables() {
  // const_cast is needed because Persistent<const ...> doesn't work properly.

  if (inherited_variables_) {
    for (auto& variable : inherited_variables_->registered_data_) {
      if (variable.value) {
        variable.value = const_cast<CSSValue*>(
            &StyleBuilderConverter::ConvertRegisteredPropertyValue(
                state_, *variable.value));
      }
    }
  }

  if (non_inherited_variables_) {
    for (auto& variable : non_inherited_variables_->registered_data_) {
      if (variable.value) {
        variable.value = const_cast<CSSValue*>(
            &StyleBuilderConverter::ConvertRegisteredPropertyValue(
                state_, *variable.value));
      }
    }
  }
}

CSSVariableResolver::CSSVariableResolver(const StyleResolverState& state)
    : state_(state),
      inherited_variables_(state.Style()->InheritedVariables()),
      non_inherited_variables_(state.Style()->NonInheritedVariables()),
      registry_(state.GetDocument().GetPropertyRegistry()) {}

}  // namespace blink
