// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/animation.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace {

// Legacy parsing allows <string>s for animation-name.
CSSValue* ConsumeAnimationValue(CSSPropertyID property,
                                CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  switch (property) {
    case CSSPropertyAnimationDelay:
      return css_property_parser_helpers::ConsumeTime(range, kValueRangeAll);
    case CSSPropertyAnimationDirection:
      return css_property_parser_helpers::ConsumeIdent<
          CSSValueNormal, CSSValueAlternate, CSSValueReverse,
          CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
      return css_property_parser_helpers::ConsumeTime(range,
                                                      kValueRangeNonNegative);
    case CSSPropertyAnimationFillMode:
      return css_property_parser_helpers::ConsumeIdent<
          CSSValueNone, CSSValueForwards, CSSValueBackwards, CSSValueBoth>(
          range);
    case CSSPropertyAnimationIterationCount:
      return css_parsing_utils::ConsumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
      return css_parsing_utils::ConsumeAnimationName(range, context,
                                                     use_legacy_parsing);
    case CSSPropertyAnimationPlayState:
      return css_property_parser_helpers::ConsumeIdent<CSSValueRunning,
                                                       CSSValuePaused>(range);
    case CSSPropertyAnimationTimingFunction:
      return css_parsing_utils::ConsumeAnimationTimingFunction(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace
namespace css_shorthand {

bool Animation::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const StylePropertyShorthand shorthand = animationShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  HeapVector<Member<CSSValueList>, css_parsing_utils::kMaxNumAnimationLonghands>
      longhands(longhand_count);
  if (!css_parsing_utils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeAnimationValue, range, context,
          local_context.UseAliasParsing())) {
    return false;
  }

  for (unsigned i = 0; i < longhand_count; ++i) {
    css_property_parser_helpers::AddProperty(
        shorthand.properties()[i]->PropertyID(), shorthand.id(), *longhands[i],
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return range.AtEnd();
}

const CSSValue* Animation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    CSSValueList* animations_list = CSSValueList::CreateCommaSeparated();
    for (wtf_size_t i = 0; i < animation_data->NameList().size(); ++i) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSCustomIdentValue::Create(animation_data->NameList()[i]));
      list->Append(*CSSPrimitiveValue::Create(
          CSSTimingData::GetRepeated(animation_data->DurationList(), i),
          CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*ComputedStyleUtils::CreateTimingFunctionValue(
          CSSTimingData::GetRepeated(animation_data->TimingFunctionList(), i)
              .get()));
      list->Append(*CSSPrimitiveValue::Create(
          CSSTimingData::GetRepeated(animation_data->DelayList(), i),
          CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
          CSSTimingData::GetRepeated(animation_data->IterationCountList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
          CSSTimingData::GetRepeated(animation_data->DirectionList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
          CSSTimingData::GetRepeated(animation_data->FillModeList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
          CSSTimingData::GetRepeated(animation_data->PlayStateList(), i)));
      animations_list->Append(*list);
    }
    return animations_list;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // animation-name default value.
  list->Append(*CSSIdentifierValue::Create(CSSValueNone));
  list->Append(
      *CSSPrimitiveValue::Create(CSSAnimationData::InitialDuration(),
                                 CSSPrimitiveValue::UnitType::kSeconds));
  list->Append(*ComputedStyleUtils::CreateTimingFunctionValue(
      CSSAnimationData::InitialTimingFunction().get()));
  list->Append(*CSSPrimitiveValue::Create(
      CSSAnimationData::InitialDelay(), CSSPrimitiveValue::UnitType::kSeconds));
  list->Append(
      *CSSPrimitiveValue::Create(CSSAnimationData::InitialIterationCount(),
                                 CSSPrimitiveValue::UnitType::kNumber));
  list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
      CSSAnimationData::InitialDirection()));
  list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
      CSSAnimationData::InitialFillMode()));
  // Initial animation-play-state.
  list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
  return list;
}

}  // namespace css_shorthand
}  // namespace blink
