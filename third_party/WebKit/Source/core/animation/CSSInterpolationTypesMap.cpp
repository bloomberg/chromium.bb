// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/css_angle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_border_image_length_box_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_clip_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_size_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_font_weight_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_image_slice_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_length_pair_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_offset_rotate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_paint_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_position_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_ray_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_resolution_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_rotate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_shadow_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_size_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_text_indent_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_time_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_transform_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_transform_origin_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_translate_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_var_cycle_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_visibility_interpolation_type.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"

namespace blink {

static const PropertyRegistration* GetRegistration(
    const PropertyRegistry* registry,
    const PropertyHandle& property) {
  DCHECK(property.IsCSSCustomProperty());
  if (!registry) {
    return nullptr;
  }
  return registry->Registration(property.CustomPropertyName());
}

const InterpolationTypes& CSSInterpolationTypesMap::Get(
    const PropertyHandle& property) const {
  using ApplicableTypesMap =
      HashMap<PropertyHandle, std::unique_ptr<const InterpolationTypes>>;
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, applicable_types_map, ());
  auto entry = applicable_types_map.find(property);
  bool found_entry = entry != applicable_types_map.end();

  // Custom property interpolation types may change over time so don't trust the
  // applicableTypesMap without checking the registry.
  if (registry_ && property.IsCSSCustomProperty()) {
    const auto* registration = GetRegistration(registry_.Get(), property);
    if (registration) {
      if (found_entry) {
        applicable_types_map.erase(entry);
      }
      return registration->GetInterpolationTypes();
    }
  }

  if (found_entry) {
    return *entry->value;
  }

  std::unique_ptr<InterpolationTypes> applicable_types =
      std::make_unique<InterpolationTypes>();

  const CSSProperty& css_property = property.IsCSSProperty()
                                        ? property.GetCSSProperty()
                                        : property.PresentationAttribute();
  // We treat presentation attributes identically to their CSS property
  // equivalents when interpolating.
  PropertyHandle used_property =
      property.IsCSSProperty() ? property : PropertyHandle(css_property);
  switch (css_property.PropertyID()) {
    case CSSPropertyBaselineShift:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyFlexBasis:
    case CSSPropertyHeight:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOffsetDistance:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPerspective:
    case CSSPropertyR:
    case CSSPropertyRight:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyShapeMargin:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTop:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyColumnGap:
    case CSSPropertyRowGap:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyColumnWidth:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWidth:
    case CSSPropertyWordSpacing:
    case CSSPropertyX:
    case CSSPropertyY:
      applicable_types->push_back(
          std::make_unique<CSSLengthInterpolationType>(used_property));
      break;
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFillOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyOpacity:
    case CSSPropertyOrder:
    case CSSPropertyOrphans:
    case CSSPropertyShapeImageThreshold:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyColumnCount:
    case CSSPropertyTextSizeAdjust:
    case CSSPropertyWidows:
    case CSSPropertyZIndex:
      applicable_types->push_back(
          std::make_unique<CSSNumberInterpolationType>(used_property));
      break;
    case CSSPropertyLineHeight:
      applicable_types->push_back(
          std::make_unique<CSSLengthInterpolationType>(used_property));
      applicable_types->push_back(
          std::make_unique<CSSNumberInterpolationType>(used_property));
      break;
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextStrokeColor:
      applicable_types->push_back(
          std::make_unique<CSSColorInterpolationType>(used_property));
      break;
    case CSSPropertyFill:
    case CSSPropertyStroke:
      applicable_types->push_back(
          std::make_unique<CSSPaintInterpolationType>(used_property));
      break;
    case CSSPropertyOffsetPath:
      applicable_types->push_back(
          std::make_unique<CSSRayInterpolationType>(used_property));
      FALLTHROUGH;
    case CSSPropertyD:
      applicable_types->push_back(
          std::make_unique<CSSPathInterpolationType>(used_property));
      break;
    case CSSPropertyBoxShadow:
    case CSSPropertyTextShadow:
      applicable_types->push_back(
          std::make_unique<CSSShadowListInterpolationType>(used_property));
      break;
    case CSSPropertyBorderImageSource:
    case CSSPropertyListStyleImage:
    case CSSPropertyWebkitMaskBoxImageSource:
      applicable_types->push_back(
          std::make_unique<CSSImageInterpolationType>(used_property));
      break;
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      applicable_types->push_back(
          std::make_unique<CSSImageListInterpolationType>(used_property));
      break;
    case CSSPropertyStrokeDasharray:
      applicable_types->push_back(
          std::make_unique<CSSLengthListInterpolationType>(used_property));
      break;
    case CSSPropertyFontWeight:
      applicable_types->push_back(
          std::make_unique<CSSFontWeightInterpolationType>(used_property));
      break;
    case CSSPropertyFontVariationSettings:
      applicable_types->push_back(
          std::make_unique<CSSFontVariationSettingsInterpolationType>(
              used_property));
      break;
    case CSSPropertyVisibility:
      applicable_types->push_back(
          std::make_unique<CSSVisibilityInterpolationType>(used_property));
      break;
    case CSSPropertyClip:
      applicable_types->push_back(
          std::make_unique<CSSClipInterpolationType>(used_property));
      break;
    case CSSPropertyOffsetRotate:
      applicable_types->push_back(
          std::make_unique<CSSOffsetRotateInterpolationType>(used_property));
      break;
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
      applicable_types->push_back(
          std::make_unique<CSSPositionAxisListInterpolationType>(
              used_property));
      break;
    case CSSPropertyObjectPosition:
    case CSSPropertyOffsetAnchor:
    case CSSPropertyOffsetPosition:
    case CSSPropertyPerspectiveOrigin:
      applicable_types->push_back(
          std::make_unique<CSSPositionInterpolationType>(used_property));
      break;
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
      applicable_types->push_back(
          std::make_unique<CSSLengthPairInterpolationType>(used_property));
      break;
    case CSSPropertyTranslate:
      applicable_types->push_back(
          std::make_unique<CSSTranslateInterpolationType>(used_property));
      break;
    case CSSPropertyTransformOrigin:
      applicable_types->push_back(
          std::make_unique<CSSTransformOriginInterpolationType>(used_property));
      break;
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize:
      applicable_types->push_back(
          std::make_unique<CSSSizeListInterpolationType>(used_property));
      break;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageWidth:
      applicable_types->push_back(
          std::make_unique<CSSBorderImageLengthBoxInterpolationType>(
              used_property));
      break;
    case CSSPropertyScale:
      applicable_types->push_back(
          std::make_unique<CSSScaleInterpolationType>(used_property));
      break;
    case CSSPropertyFontSize:
      applicable_types->push_back(
          std::make_unique<CSSFontSizeInterpolationType>(used_property));
      break;
    case CSSPropertyTextIndent:
      applicable_types->push_back(
          std::make_unique<CSSTextIndentInterpolationType>(used_property));
      break;
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice:
      applicable_types->push_back(
          std::make_unique<CSSImageSliceInterpolationType>(used_property));
      break;
    case CSSPropertyClipPath:
    case CSSPropertyShapeOutside:
      applicable_types->push_back(
          std::make_unique<CSSBasicShapeInterpolationType>(used_property));
      break;
    case CSSPropertyRotate:
      applicable_types->push_back(
          std::make_unique<CSSRotateInterpolationType>(used_property));
      break;
    case CSSPropertyBackdropFilter:
    case CSSPropertyFilter:
      applicable_types->push_back(
          std::make_unique<CSSFilterListInterpolationType>(used_property));
      break;
    case CSSPropertyTransform:
      applicable_types->push_back(
          std::make_unique<CSSTransformInterpolationType>(used_property));
      break;
    case CSSPropertyVariable:
      DCHECK_EQ(GetRegistration(registry_.Get(), property), nullptr);
      break;
    default:
      DCHECK(!css_property.IsInterpolable());
      break;
  }

  applicable_types->push_back(
      std::make_unique<CSSDefaultInterpolationType>(used_property));

  auto add_result =
      applicable_types_map.insert(property, std::move(applicable_types));
  return *add_result.stored_value->value;
}

size_t CSSInterpolationTypesMap::Version() const {
  // Property registrations are never removed so the number of registered
  // custom properties is equivalent to how many changes there have been to the
  // property registry.
  return registry_ ? registry_->RegistrationCount() : 0;
}

InterpolationTypes
CSSInterpolationTypesMap::CreateInterpolationTypesForCSSSyntax(
    const AtomicString& property_name,
    const CSSSyntaxDescriptor& descriptor,
    const PropertyRegistration& registration) {
  PropertyHandle property(property_name);
  InterpolationTypes result;

  // All custom properties may encounter var() dependency cycles.
  result.push_back(
      std::make_unique<CSSVarCycleInterpolationType>(property, registration));

  for (const CSSSyntaxComponent& component : descriptor.Components()) {
    if (component.repeatable_) {
      // TODO(alancutter): Support animation of repeatable types.
      continue;
    }

    switch (component.type_) {
      case CSSSyntaxType::kAngle:
        result.push_back(std::make_unique<CSSAngleInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kColor:
        result.push_back(std::make_unique<CSSColorInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kLength:
      case CSSSyntaxType::kLengthPercentage:
      case CSSSyntaxType::kPercentage:
        result.push_back(std::make_unique<CSSLengthInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kNumber:
        result.push_back(std::make_unique<CSSNumberInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kResolution:
        result.push_back(std::make_unique<CSSResolutionInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kTime:
        result.push_back(std::make_unique<CSSTimeInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kImage:
        result.push_back(std::make_unique<CSSImageInterpolationType>(
            property, &registration));
        break;
      case CSSSyntaxType::kInteger:
        result.push_back(std::make_unique<CSSNumberInterpolationType>(
            property, &registration, true));
        break;
      case CSSSyntaxType::kTransformList:
        // TODO(alancutter): Support smooth interpolation of these types.
        break;
      case CSSSyntaxType::kCustomIdent:
      case CSSSyntaxType::kIdent:
      case CSSSyntaxType::kTokenStream:
      case CSSSyntaxType::kUrl:
        // No interpolation behaviour defined, uses the
        // CSSDefaultInterpolationType added below.
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  result.push_back(std::make_unique<CSSDefaultInterpolationType>(property));
  return result;
}

}  // namespace blink
