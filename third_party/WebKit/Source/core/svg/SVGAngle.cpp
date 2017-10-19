/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGAngle.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGMarkerOrientType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(kSVGMarkerOrientAuto, "auto"));
    entries.push_back(std::make_pair(kSVGMarkerOrientAngle, "angle"));
    entries.push_back(
        std::make_pair(kSVGMarkerOrientAutoStartReverse, "auto-start-reverse"));
  }
  return entries;
}

template <>
unsigned short GetMaxExposedEnumValue<SVGMarkerOrientType>() {
  return kSVGMarkerOrientAngle;
}

SVGMarkerOrientEnumeration::SVGMarkerOrientEnumeration(SVGAngle* angle)
    : SVGEnumeration<SVGMarkerOrientType>(kSVGMarkerOrientAngle),
      angle_(angle) {}

SVGMarkerOrientEnumeration::~SVGMarkerOrientEnumeration() {}

void SVGMarkerOrientEnumeration::Trace(blink::Visitor* visitor) {
  visitor->Trace(angle_);
  SVGEnumeration<SVGMarkerOrientType>::Trace(visitor);
}

void SVGMarkerOrientEnumeration::NotifyChange() {
  DCHECK(angle_);
  angle_->OrientTypeChanged();
}

void SVGMarkerOrientEnumeration::Add(SVGPropertyBase*, SVGElement*) {
  // SVGMarkerOrientEnumeration is only animated via SVGAngle
  NOTREACHED();
}

void SVGMarkerOrientEnumeration::CalculateAnimatedValue(
    SVGAnimationElement*,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from,
    SVGPropertyBase* to,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  // SVGMarkerOrientEnumeration is only animated via SVGAngle
  NOTREACHED();
}

float SVGMarkerOrientEnumeration::CalculateDistance(
    SVGPropertyBase* to,
    SVGElement* context_element) {
  // SVGMarkerOrientEnumeration is only animated via SVGAngle
  NOTREACHED();
  return -1.0;
}

SVGAngle::SVGAngle()
    : unit_type_(kSvgAngletypeUnspecified),
      value_in_specified_units_(0),
      orient_type_(SVGMarkerOrientEnumeration::Create(this)) {}

SVGAngle::SVGAngle(SVGAngleType unit_type,
                   float value_in_specified_units,
                   SVGMarkerOrientType orient_type)
    : unit_type_(unit_type),
      value_in_specified_units_(value_in_specified_units),
      orient_type_(SVGMarkerOrientEnumeration::Create(this)) {
  orient_type_->SetEnumValue(orient_type);
}

SVGAngle::~SVGAngle() {}

void SVGAngle::Trace(blink::Visitor* visitor) {
  visitor->Trace(orient_type_);
  SVGPropertyHelper<SVGAngle>::Trace(visitor);
}

SVGAngle* SVGAngle::Clone() const {
  return new SVGAngle(unit_type_, value_in_specified_units_,
                      orient_type_->EnumValue());
}

float SVGAngle::Value() const {
  switch (unit_type_) {
    case kSvgAngletypeGrad:
      return grad2deg(value_in_specified_units_);
    case kSvgAngletypeRad:
      return rad2deg(value_in_specified_units_);
    case kSvgAngletypeTurn:
      return turn2deg(value_in_specified_units_);
    case kSvgAngletypeUnspecified:
    case kSvgAngletypeUnknown:
    case kSvgAngletypeDeg:
      return value_in_specified_units_;
  }

  NOTREACHED();
  return 0;
}

void SVGAngle::SetValue(float value) {
  switch (unit_type_) {
    case kSvgAngletypeGrad:
      value_in_specified_units_ = deg2grad(value);
      break;
    case kSvgAngletypeRad:
      value_in_specified_units_ = deg2rad(value);
      break;
    case kSvgAngletypeTurn:
      value_in_specified_units_ = deg2turn(value);
      break;
    case kSvgAngletypeUnspecified:
    case kSvgAngletypeUnknown:
    case kSvgAngletypeDeg:
      value_in_specified_units_ = value;
      break;
  }
  orient_type_->SetEnumValue(kSVGMarkerOrientAngle);
}

template <typename CharType>
static SVGAngle::SVGAngleType StringToAngleType(const CharType*& ptr,
                                                const CharType* end) {
  // If there's no unit given, the angle type is unspecified.
  if (ptr == end)
    return SVGAngle::kSvgAngletypeUnspecified;

  SVGAngle::SVGAngleType type = SVGAngle::kSvgAngletypeUnknown;
  if (IsHTMLSpace<CharType>(ptr[0])) {
    type = SVGAngle::kSvgAngletypeUnspecified;
    ptr++;
  } else if (end - ptr >= 3) {
    if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'g') {
      type = SVGAngle::kSvgAngletypeDeg;
      ptr += 3;
    } else if (ptr[0] == 'r' && ptr[1] == 'a' && ptr[2] == 'd') {
      type = SVGAngle::kSvgAngletypeRad;
      ptr += 3;
    } else if (end - ptr >= 4) {
      if (ptr[0] == 'g' && ptr[1] == 'r' && ptr[2] == 'a' && ptr[3] == 'd') {
        type = SVGAngle::kSvgAngletypeGrad;
        ptr += 4;
      } else if (ptr[0] == 't' && ptr[1] == 'u' && ptr[2] == 'r' &&
                 ptr[3] == 'n') {
        type = SVGAngle::kSvgAngletypeTurn;
        ptr += 4;
      }
    }
  }

  if (!SkipOptionalSVGSpaces(ptr, end))
    return type;

  return SVGAngle::kSvgAngletypeUnknown;
}

String SVGAngle::ValueAsString() const {
  switch (unit_type_) {
    case kSvgAngletypeDeg: {
      DEFINE_STATIC_LOCAL(String, deg_string, ("deg"));
      return String::Number(value_in_specified_units_) + deg_string;
    }
    case kSvgAngletypeRad: {
      DEFINE_STATIC_LOCAL(String, rad_string, ("rad"));
      return String::Number(value_in_specified_units_) + rad_string;
    }
    case kSvgAngletypeGrad: {
      DEFINE_STATIC_LOCAL(String, grad_string, ("grad"));
      return String::Number(value_in_specified_units_) + grad_string;
    }
    case kSvgAngletypeTurn: {
      DEFINE_STATIC_LOCAL(String, turn_string, ("turn"));
      return String::Number(value_in_specified_units_) + turn_string;
    }
    case kSvgAngletypeUnspecified:
    case kSvgAngletypeUnknown:
      return String::Number(value_in_specified_units_);
  }

  NOTREACHED();
  return String();
}

template <typename CharType>
static SVGParsingError ParseValue(const String& value,
                                  float& value_in_specified_units,
                                  SVGAngle::SVGAngleType& unit_type) {
  const CharType* ptr = value.GetCharacters<CharType>();
  const CharType* end = ptr + value.length();

  if (!ParseNumber(ptr, end, value_in_specified_units, kAllowLeadingWhitespace))
    return SVGParsingError(SVGParseStatus::kExpectedAngle,
                           ptr - value.GetCharacters<CharType>());

  unit_type = StringToAngleType(ptr, end);
  if (unit_type == SVGAngle::kSvgAngletypeUnknown)
    return SVGParsingError(SVGParseStatus::kExpectedAngle,
                           ptr - value.GetCharacters<CharType>());

  return SVGParseStatus::kNoError;
}

SVGParsingError SVGAngle::SetValueAsString(const String& value) {
  if (value.IsEmpty()) {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    return SVGParseStatus::kNoError;
  }

  if (value == "auto") {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    orient_type_->SetEnumValue(kSVGMarkerOrientAuto);
    return SVGParseStatus::kNoError;
  }
  if (value == "auto-start-reverse") {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    orient_type_->SetEnumValue(kSVGMarkerOrientAutoStartReverse);
    return SVGParseStatus::kNoError;
  }

  float value_in_specified_units = 0;
  SVGAngleType unit_type = kSvgAngletypeUnknown;

  SVGParsingError error;
  if (value.Is8Bit())
    error = ParseValue<LChar>(value, value_in_specified_units, unit_type);
  else
    error = ParseValue<UChar>(value, value_in_specified_units, unit_type);
  if (error != SVGParseStatus::kNoError)
    return error;

  orient_type_->SetEnumValue(kSVGMarkerOrientAngle);
  unit_type_ = unit_type;
  value_in_specified_units_ = value_in_specified_units;
  return SVGParseStatus::kNoError;
}

void SVGAngle::NewValueSpecifiedUnits(SVGAngleType unit_type,
                                      float value_in_specified_units) {
  orient_type_->SetEnumValue(kSVGMarkerOrientAngle);
  unit_type_ = unit_type;
  value_in_specified_units_ = value_in_specified_units;
}

void SVGAngle::ConvertToSpecifiedUnits(SVGAngleType unit_type) {
  if (unit_type == unit_type_)
    return;

  switch (unit_type_) {
    case kSvgAngletypeTurn:
      switch (unit_type) {
        case kSvgAngletypeGrad:
          value_in_specified_units_ = turn2grad(value_in_specified_units_);
          break;
        case kSvgAngletypeUnspecified:
        case kSvgAngletypeDeg:
          value_in_specified_units_ = turn2deg(value_in_specified_units_);
          break;
        case kSvgAngletypeRad:
          value_in_specified_units_ =
              deg2rad(turn2deg(value_in_specified_units_));
          break;
        case kSvgAngletypeTurn:
        case kSvgAngletypeUnknown:
          NOTREACHED();
          break;
      }
      break;
    case kSvgAngletypeRad:
      switch (unit_type) {
        case kSvgAngletypeGrad:
          value_in_specified_units_ = rad2grad(value_in_specified_units_);
          break;
        case kSvgAngletypeUnspecified:
        case kSvgAngletypeDeg:
          value_in_specified_units_ = rad2deg(value_in_specified_units_);
          break;
        case kSvgAngletypeTurn:
          value_in_specified_units_ =
              deg2turn(rad2deg(value_in_specified_units_));
          break;
        case kSvgAngletypeRad:
        case kSvgAngletypeUnknown:
          NOTREACHED();
          break;
      }
      break;
    case kSvgAngletypeGrad:
      switch (unit_type) {
        case kSvgAngletypeRad:
          value_in_specified_units_ = grad2rad(value_in_specified_units_);
          break;
        case kSvgAngletypeUnspecified:
        case kSvgAngletypeDeg:
          value_in_specified_units_ = grad2deg(value_in_specified_units_);
          break;
        case kSvgAngletypeTurn:
          value_in_specified_units_ = grad2turn(value_in_specified_units_);
          break;
        case kSvgAngletypeGrad:
        case kSvgAngletypeUnknown:
          NOTREACHED();
          break;
      }
      break;
    case kSvgAngletypeUnspecified:
    // Spec: For angles, a unitless value is treated the same as if degrees were
    // specified.
    case kSvgAngletypeDeg:
      switch (unit_type) {
        case kSvgAngletypeRad:
          value_in_specified_units_ = deg2rad(value_in_specified_units_);
          break;
        case kSvgAngletypeGrad:
          value_in_specified_units_ = deg2grad(value_in_specified_units_);
          break;
        case kSvgAngletypeTurn:
          value_in_specified_units_ = deg2turn(value_in_specified_units_);
          break;
        case kSvgAngletypeUnspecified:
        case kSvgAngletypeDeg:
          break;
        case kSvgAngletypeUnknown:
          NOTREACHED();
          break;
      }
      break;
    case kSvgAngletypeUnknown:
      NOTREACHED();
      break;
  }

  unit_type_ = unit_type;
  orient_type_->SetEnumValue(kSVGMarkerOrientAngle);
}

void SVGAngle::Add(SVGPropertyBase* other, SVGElement*) {
  SVGAngle* other_angle = ToSVGAngle(other);

  // Only respect by animations, if from and by are both specified in angles
  // (and not, for example, 'auto').
  if (OrientType()->EnumValue() != kSVGMarkerOrientAngle ||
      other_angle->OrientType()->EnumValue() != kSVGMarkerOrientAngle)
    return;

  SetValue(Value() + other_angle->Value());
}

void SVGAngle::Assign(const SVGAngle& other) {
  SVGMarkerOrientType other_orient_type = other.OrientType()->EnumValue();
  if (other_orient_type == kSVGMarkerOrientAngle)
    NewValueSpecifiedUnits(other.UnitType(), other.ValueInSpecifiedUnits());
  else
    orient_type_->SetEnumValue(other_orient_type);
}

void SVGAngle::CalculateAnimatedValue(SVGAnimationElement* animation_element,
                                      float percentage,
                                      unsigned repeat_count,
                                      SVGPropertyBase* from,
                                      SVGPropertyBase* to,
                                      SVGPropertyBase* to_at_end_of_duration,
                                      SVGElement*) {
  DCHECK(animation_element);
  bool is_to_animation = animation_element->GetAnimationMode() == kToAnimation;

  SVGAngle* from_angle = is_to_animation ? this : ToSVGAngle(from);
  SVGAngle* to_angle = ToSVGAngle(to);
  SVGMarkerOrientType from_orient_type = from_angle->OrientType()->EnumValue();
  SVGMarkerOrientType to_orient_type = to_angle->OrientType()->EnumValue();

  if (from_orient_type != to_orient_type) {
    // Fall back to discrete animation.
    Assign(percentage < 0.5f ? *from_angle : *to_angle);
    return;
  }

  switch (from_orient_type) {
    // From 'auto' to 'auto', or 'auto-start-reverse' to 'auto-start-reverse'
    case kSVGMarkerOrientAuto:
    case kSVGMarkerOrientAutoStartReverse:
      OrientType()->SetEnumValue(from_orient_type);
      return;

    // Regular from angle to angle animation, with all features like additive
    // etc.
    case kSVGMarkerOrientAngle: {
      float animated_value = Value();
      SVGAngle* to_at_end_of_duration_angle = ToSVGAngle(to_at_end_of_duration);

      animation_element->AnimateAdditiveNumber(
          percentage, repeat_count, from_angle->Value(), to_angle->Value(),
          to_at_end_of_duration_angle->Value(), animated_value);
      OrientType()->SetEnumValue(kSVGMarkerOrientAngle);
      SetValue(animated_value);
    }
      return;

    // If the enumeration value is not angle or auto, its unknown.
    default:
      value_in_specified_units_ = 0;
      OrientType()->SetEnumValue(kSVGMarkerOrientUnknown);
      return;
  }
}

float SVGAngle::CalculateDistance(SVGPropertyBase* other, SVGElement*) {
  return fabsf(Value() - ToSVGAngle(other)->Value());
}

void SVGAngle::OrientTypeChanged() {
  if (OrientType()->EnumValue() == kSVGMarkerOrientAuto ||
      OrientType()->EnumValue() == kSVGMarkerOrientAutoStartReverse) {
    unit_type_ = kSvgAngletypeUnspecified;
    value_in_specified_units_ = 0;
  }
}

}  // namespace blink
