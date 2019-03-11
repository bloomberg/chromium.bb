/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"

#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/style/border_image_length_box.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/fill_layer.h"

namespace blink {

void CSSToStyleMap::MapFillAttachment(StyleResolverState&,
                                      FillLayer* layer,
                                      const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetAttachment(FillLayer::InitialFillAttachment(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  switch (identifier_value->GetValueID()) {
    case CSSValueFixed:
      layer->SetAttachment(EFillAttachment::kFixed);
      break;
    case CSSValueScroll:
      layer->SetAttachment(EFillAttachment::kScroll);
      break;
    case CSSValueLocal:
      layer->SetAttachment(EFillAttachment::kLocal);
      break;
    default:
      return;
  }
}

void CSSToStyleMap::MapFillClip(StyleResolverState&,
                                FillLayer* layer,
                                const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetClip(FillLayer::InitialFillClip(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetClip(identifier_value->ConvertTo<EFillBox>());
}

void CSSToStyleMap::MapFillComposite(StyleResolverState&,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetComposite(FillLayer::InitialFillComposite(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetComposite(identifier_value->ConvertTo<CompositeOperator>());
}

void CSSToStyleMap::MapFillBlendMode(StyleResolverState&,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetBlendMode(FillLayer::InitialFillBlendMode(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetBlendMode(identifier_value->ConvertTo<BlendMode>());
}

void CSSToStyleMap::MapFillOrigin(StyleResolverState&,
                                  FillLayer* layer,
                                  const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetOrigin(FillLayer::InitialFillOrigin(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetOrigin(identifier_value->ConvertTo<EFillBox>());
}

void CSSToStyleMap::MapFillImage(StyleResolverState& state,
                                 FillLayer* layer,
                                 const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetImage(FillLayer::InitialFillImage(layer->GetType()));
    return;
  }

  CSSPropertyID property = layer->GetType() == EFillLayerType::kBackground
                               ? CSSPropertyBackgroundImage
                               : CSSPropertyWebkitMaskImage;
  layer->SetImage(state.GetStyleImage(property, value));
}

void CSSToStyleMap::MapFillRepeatX(StyleResolverState&,
                                   FillLayer* layer,
                                   const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetRepeatX(FillLayer::InitialFillRepeatX(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetRepeatX(identifier_value->ConvertTo<EFillRepeat>());
}

void CSSToStyleMap::MapFillRepeatY(StyleResolverState&,
                                   FillLayer* layer,
                                   const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetRepeatY(FillLayer::InitialFillRepeatY(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  layer->SetRepeatY(identifier_value->ConvertTo<EFillRepeat>());
}

void CSSToStyleMap::MapFillSize(StyleResolverState& state,
                                FillLayer* layer,
                                const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetSizeType(FillLayer::InitialFillSizeType(layer->GetType()));
    layer->SetSizeLength(FillLayer::InitialFillSizeLength(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair())
    return;

  if (identifier_value && identifier_value->GetValueID() == CSSValueContain)
    layer->SetSizeType(EFillSizeType::kContain);
  else if (identifier_value && identifier_value->GetValueID() == CSSValueCover)
    layer->SetSizeType(EFillSizeType::kCover);
  else
    layer->SetSizeType(EFillSizeType::kSizeLength);

  LengthSize b = FillLayer::InitialFillSizeLength(layer->GetType());

  if (identifier_value && (identifier_value->GetValueID() == CSSValueContain ||
                           identifier_value->GetValueID() == CSSValueCover)) {
    layer->SetSizeLength(b);
    return;
  }

  Length first_length;
  Length second_length;

  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    first_length =
        StyleBuilderConverter::ConvertLengthOrAuto(state, pair.First());
    second_length =
        StyleBuilderConverter::ConvertLengthOrAuto(state, pair.Second());
  } else {
    DCHECK(value.IsPrimitiveValue() || value.IsIdentifierValue());
    first_length = StyleBuilderConverter::ConvertLengthOrAuto(state, value);
    second_length = Length();
  }

  b.SetWidth(first_length);
  b.SetHeight(second_length);
  layer->SetSizeLength(b);
}

void CSSToStyleMap::MapFillPositionX(StyleResolverState& state,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetPositionX(FillLayer::InitialFillPositionX(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair())
    return;

  Length length;
  if (value.IsValuePair())
    length = ToCSSPrimitiveValue(ToCSSValuePair(value).Second())
                 .ConvertToLength(state.CssToLengthConversionData());
  else
    length = StyleBuilderConverter::ConvertPositionLength<CSSValueLeft,
                                                          CSSValueRight>(state,
                                                                         value);

  layer->SetPositionX(length);
  if (value.IsValuePair())
    layer->SetBackgroundXOrigin(
        To<CSSIdentifierValue>(ToCSSValuePair(value).First())
            .ConvertTo<BackgroundEdgeOrigin>());
}

void CSSToStyleMap::MapFillPositionY(StyleResolverState& state,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetPositionY(FillLayer::InitialFillPositionY(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair())
    return;

  Length length;
  if (value.IsValuePair())
    length = ToCSSPrimitiveValue(ToCSSValuePair(value).Second())
                 .ConvertToLength(state.CssToLengthConversionData());
  else
    length = StyleBuilderConverter::ConvertPositionLength<CSSValueTop,
                                                          CSSValueBottom>(
        state, value);

  layer->SetPositionY(length);
  if (value.IsValuePair())
    layer->SetBackgroundYOrigin(
        To<CSSIdentifierValue>(ToCSSValuePair(value).First())
            .ConvertTo<BackgroundEdgeOrigin>());
}

void CSSToStyleMap::MapFillMaskSourceType(StyleResolverState&,
                                          FillLayer* layer,
                                          const CSSValue& value) {
  EMaskSourceType type = FillLayer::InitialFillMaskSourceType(layer->GetType());
  if (value.IsInitialValue()) {
    layer->SetMaskSourceType(type);
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return;

  switch (identifier_value->GetValueID()) {
    case CSSValueAlpha:
      type = EMaskSourceType::kAlpha;
      break;
    case CSSValueLuminance:
      type = EMaskSourceType::kLuminance;
      break;
    case CSSValueAuto:
      break;
    default:
      NOTREACHED();
  }

  layer->SetMaskSourceType(type);
}

double CSSToStyleMap::MapAnimationDelay(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSTimingData::InitialDelay();
  return ToCSSPrimitiveValue(value).ComputeSeconds();
}

Timing::PlaybackDirection CSSToStyleMap::MapAnimationDirection(
    const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSAnimationData::InitialDirection();

  switch (To<CSSIdentifierValue>(value).GetValueID()) {
    case CSSValueNormal:
      return Timing::PlaybackDirection::NORMAL;
    case CSSValueAlternate:
      return Timing::PlaybackDirection::ALTERNATE_NORMAL;
    case CSSValueReverse:
      return Timing::PlaybackDirection::REVERSE;
    case CSSValueAlternateReverse:
      return Timing::PlaybackDirection::ALTERNATE_REVERSE;
    default:
      NOTREACHED();
      return CSSAnimationData::InitialDirection();
  }
}

double CSSToStyleMap::MapAnimationDuration(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSTimingData::InitialDuration();
  return ToCSSPrimitiveValue(value).ComputeSeconds();
}

Timing::FillMode CSSToStyleMap::MapAnimationFillMode(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSAnimationData::InitialFillMode();

  switch (To<CSSIdentifierValue>(value).GetValueID()) {
    case CSSValueNone:
      return Timing::FillMode::NONE;
    case CSSValueForwards:
      return Timing::FillMode::FORWARDS;
    case CSSValueBackwards:
      return Timing::FillMode::BACKWARDS;
    case CSSValueBoth:
      return Timing::FillMode::BOTH;
    default:
      NOTREACHED();
      return CSSAnimationData::InitialFillMode();
  }
}

double CSSToStyleMap::MapAnimationIterationCount(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSAnimationData::InitialIterationCount();
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueInfinite)
    return std::numeric_limits<double>::infinity();
  return ToCSSPrimitiveValue(value).GetFloatValue();
}

AtomicString CSSToStyleMap::MapAnimationName(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSAnimationData::InitialName();
  if (auto* custom_ident_value = DynamicTo<CSSCustomIdentValue>(value))
    return AtomicString(custom_ident_value->Value());
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueNone);
  return CSSAnimationData::InitialName();
}

EAnimPlayState CSSToStyleMap::MapAnimationPlayState(const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSAnimationData::InitialPlayState();
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValuePaused)
    return EAnimPlayState::kPaused;
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueRunning);
  return EAnimPlayState::kPlaying;
}

CSSTransitionData::TransitionProperty CSSToStyleMap::MapAnimationProperty(
    const CSSValue& value) {
  if (value.IsInitialValue())
    return CSSTransitionData::InitialProperty();
  if (const auto* custom_ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    if (custom_ident_value->IsKnownPropertyID()) {
      return CSSTransitionData::TransitionProperty(
          custom_ident_value->ValueAsPropertyID());
    }
    return CSSTransitionData::TransitionProperty(custom_ident_value->Value());
  }
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueAll)
    return CSSTransitionData::InitialProperty();
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueNone);
  return CSSTransitionData::TransitionProperty(
      CSSTransitionData::kTransitionNone);
}

scoped_refptr<TimingFunction> CSSToStyleMap::MapAnimationTimingFunction(
    const CSSValue& value) {
  // FIXME: We should probably only call into this function with a valid
  // single timing function value which isn't initial or inherit. We can
  // currently get into here with initial since the parser expands unset
  // properties in shorthands to initial.

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueLinear:
        return LinearTimingFunction::Shared();
      case CSSValueEase:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE);
      case CSSValueEaseIn:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN);
      case CSSValueEaseOut:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_OUT);
      case CSSValueEaseInOut:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
      case CSSValueStepStart:
        return StepsTimingFunction::Preset(
            StepsTimingFunction::StepPosition::START);
      case CSSValueStepEnd:
        return StepsTimingFunction::Preset(
            StepsTimingFunction::StepPosition::END);
      default:
        NOTREACHED();
        return CSSTimingData::InitialTimingFunction();
    }
  }

  if (value.IsCubicBezierTimingFunctionValue()) {
    const cssvalue::CSSCubicBezierTimingFunctionValue& cubic_timing_function =
        cssvalue::ToCSSCubicBezierTimingFunctionValue(value);
    return CubicBezierTimingFunction::Create(
        cubic_timing_function.X1(), cubic_timing_function.Y1(),
        cubic_timing_function.X2(), cubic_timing_function.Y2());
  }

  if (value.IsInitialValue())
    return CSSTimingData::InitialTimingFunction();

  if (value.IsFramesTimingFunctionValue()) {
    const cssvalue::CSSFramesTimingFunctionValue& frames_timing_function =
        cssvalue::ToCSSFramesTimingFunctionValue(value);
    return FramesTimingFunction::Create(
        frames_timing_function.NumberOfFrames());
  }

  const cssvalue::CSSStepsTimingFunctionValue& steps_timing_function =
      cssvalue::ToCSSStepsTimingFunctionValue(value);
  return StepsTimingFunction::Create(steps_timing_function.NumberOfSteps(),
                                     steps_timing_function.GetStepPosition());
}

void CSSToStyleMap::MapNinePieceImage(StyleResolverState& state,
                                      CSSPropertyID property,
                                      const CSSValue& value,
                                      NinePieceImage& image) {
  // If we're not a value list, then we are "none" and don't need to alter the
  // empty image at all.
  if (!value.IsValueList())
    return;

  // Retrieve the border image value.
  const CSSValueList& border_image = ToCSSValueList(value);

  // Set the image (this kicks off the load).
  CSSPropertyID image_property;
  if (property == CSSPropertyWebkitBorderImage)
    image_property = CSSPropertyBorderImageSource;
  else if (property == CSSPropertyWebkitMaskBoxImage)
    image_property = CSSPropertyWebkitMaskBoxImageSource;
  else
    image_property = property;

  for (unsigned i = 0; i < border_image.length(); ++i) {
    const CSSValue& current = border_image.Item(i);

    if (current.IsImageValue() || current.IsImageGeneratorValue() ||
        current.IsImageSetValue()) {
      image.SetImage(state.GetStyleImage(image_property, current));
    } else if (current.IsBorderImageSliceValue()) {
      MapNinePieceImageSlice(state, current, image);
    } else if (current.IsValueList()) {
      const CSSValueList& slash_list = ToCSSValueList(current);
      size_t length = slash_list.length();
      // Map in the image slices.
      if (length && slash_list.Item(0).IsBorderImageSliceValue())
        MapNinePieceImageSlice(state, slash_list.Item(0), image);

      // Map in the border slices.
      if (length > 1)
        image.SetBorderSlices(MapNinePieceImageQuad(state, slash_list.Item(1)));

      // Map in the outset.
      if (length > 2)
        image.SetOutset(MapNinePieceImageQuad(state, slash_list.Item(2)));
    } else if (current.IsPrimitiveValue() || current.IsValuePair()) {
      // Set the appropriate rules for stretch/round/repeat of the slices.
      MapNinePieceImageRepeat(state, current, image);
    }
  }

  if (property == CSSPropertyWebkitBorderImage) {
    // We have to preserve the legacy behavior of -webkit-border-image and make
    // the border slices also set the border widths. We don't need to worry
    // about percentages, since we don't even support those on real borders yet.
    if (image.BorderSlices().Top().IsLength() &&
        image.BorderSlices().Top().length().IsFixed())
      state.Style()->SetBorderTopWidth(
          image.BorderSlices().Top().length().Value());
    if (image.BorderSlices().Right().IsLength() &&
        image.BorderSlices().Right().length().IsFixed())
      state.Style()->SetBorderRightWidth(
          image.BorderSlices().Right().length().Value());
    if (image.BorderSlices().Bottom().IsLength() &&
        image.BorderSlices().Bottom().length().IsFixed())
      state.Style()->SetBorderBottomWidth(
          image.BorderSlices().Bottom().length().Value());
    if (image.BorderSlices().Left().IsLength() &&
        image.BorderSlices().Left().length().IsFixed())
      state.Style()->SetBorderLeftWidth(
          image.BorderSlices().Left().length().Value());
  }
}

static Length ConvertBorderImageSliceSide(const CSSPrimitiveValue& value) {
  if (value.IsPercentage())
    return Length::Percent(value.GetDoubleValue());
  return Length::Fixed(round(value.GetDoubleValue()));
}

void CSSToStyleMap::MapNinePieceImageSlice(StyleResolverState&,
                                           const CSSValue& value,
                                           NinePieceImage& image) {
  if (!IsA<cssvalue::CSSBorderImageSliceValue>(value))
    return;

  // Retrieve the border image value.
  const auto& border_image_slice =
      To<cssvalue::CSSBorderImageSliceValue>(value);

  // Set up a length box to represent our image slices.
  LengthBox box;
  const CSSQuadValue& slices = border_image_slice.Slices();
  box.top_ = ConvertBorderImageSliceSide(ToCSSPrimitiveValue(*slices.Top()));
  box.bottom_ =
      ConvertBorderImageSliceSide(ToCSSPrimitiveValue(*slices.Bottom()));
  box.left_ = ConvertBorderImageSliceSide(ToCSSPrimitiveValue(*slices.Left()));
  box.right_ =
      ConvertBorderImageSliceSide(ToCSSPrimitiveValue(*slices.Right()));
  image.SetImageSlices(box);

  // Set our fill mode.
  image.SetFill(border_image_slice.Fill());
}

static BorderImageLength ToBorderImageLength(const StyleResolverState& state,
                                             const CSSValue& value) {
  if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (primitive_value.IsNumber())
      return primitive_value.GetDoubleValue();
  }
  return StyleBuilderConverter::ConvertLengthOrAuto(state, value);
}

BorderImageLengthBox CSSToStyleMap::MapNinePieceImageQuad(
    StyleResolverState& state,
    const CSSValue& value) {
  if (!value.IsQuadValue())
    return BorderImageLengthBox(Length::Auto());

  const CSSQuadValue& slices = ToCSSQuadValue(value);
  // Set up a border image length box to represent our image slices.
  return BorderImageLengthBox(ToBorderImageLength(state, *slices.Top()),
                              ToBorderImageLength(state, *slices.Right()),
                              ToBorderImageLength(state, *slices.Bottom()),
                              ToBorderImageLength(state, *slices.Left()));
}

void CSSToStyleMap::MapNinePieceImageRepeat(StyleResolverState&,
                                            const CSSValue& value,
                                            NinePieceImage& image) {
  if (!value.IsValuePair())
    return;

  const CSSValuePair& pair = ToCSSValuePair(value);
  CSSValueID first_identifier =
      To<CSSIdentifierValue>(pair.First()).GetValueID();
  CSSValueID second_identifier =
      To<CSSIdentifierValue>(pair.Second()).GetValueID();

  ENinePieceImageRule horizontal_rule;
  switch (first_identifier) {
    case CSSValueStretch:
      horizontal_rule = kStretchImageRule;
      break;
    case CSSValueRound:
      horizontal_rule = kRoundImageRule;
      break;
    case CSSValueSpace:
      horizontal_rule = kSpaceImageRule;
      break;
    default:  // CSSValueRepeat
      horizontal_rule = kRepeatImageRule;
      break;
  }
  image.SetHorizontalRule(horizontal_rule);

  ENinePieceImageRule vertical_rule;
  switch (second_identifier) {
    case CSSValueStretch:
      vertical_rule = kStretchImageRule;
      break;
    case CSSValueRound:
      vertical_rule = kRoundImageRule;
      break;
    case CSSValueSpace:
      vertical_rule = kSpaceImageRule;
      break;
    default:  // CSSValueRepeat
      vertical_rule = kRepeatImageRule;
      break;
  }
  image.SetVerticalRule(vertical_rule);
}

}  // namespace blink
