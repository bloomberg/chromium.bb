/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_

#include <cmath>
#include <cstring>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

struct PixelsAndPercent {
  DISALLOW_NEW();
  PixelsAndPercent(float pixels, float percent)
      : pixels(pixels), percent(percent) {}
  float pixels;
  float percent;
};

class CalculationValue;

class PLATFORM_EXPORT Length {
  DISALLOW_NEW();

 public:
  enum class ValueRange { kAll, kNonNegative };

  // FIXME: This enum makes it hard to tell in general what values may be
  // appropriate for any given Length.
  enum Type : unsigned char {
    kAuto,
    kPercent,
    kFixed,
    kMinContent,
    kMaxContent,
    kMinIntrinsic,
    kFillAvailable,
    kFitContent,
    kCalculated,
    kExtendToZoom,
    kDeviceWidth,
    kDeviceHeight,
    kNone,    // only valid for max-width, max-height, or contain-intrinsic-size
    kContent  // only valid for flex-basis
  };

  Length() : int_value_(0), quirk_(false), type_(kAuto), is_float_(false) {}

  explicit Length(Length::Type t)
      : int_value_(0), quirk_(false), type_(t), is_float_(false) {
    DCHECK_NE(t, kCalculated);
  }

  Length(int v, Length::Type t, bool q = false)
      : int_value_(v), quirk_(q), type_(t), is_float_(false) {
    DCHECK_NE(t, kCalculated);
  }

  Length(LayoutUnit v, Length::Type t, bool q = false)
      : float_value_(v.ToFloat()), quirk_(q), type_(t), is_float_(true) {
    DCHECK(std::isfinite(v.ToFloat()));
    DCHECK_NE(t, kCalculated);
  }

  Length(float v, Length::Type t, bool q = false)
      : float_value_(v), quirk_(q), type_(t), is_float_(true) {
    DCHECK(std::isfinite(v));
    DCHECK_NE(t, kCalculated);
  }

  Length(double v, Length::Type t, bool q = false)
      : quirk_(q), type_(t), is_float_(true) {
    DCHECK(std::isfinite(v));
    float_value_ = ClampTo<float>(v);
  }

  explicit Length(scoped_refptr<const CalculationValue>);

  Length(const Length& length) {
    memcpy(this, &length, sizeof(Length));
    if (IsCalculated())
      IncrementCalculatedRef();
  }

  Length& operator=(const Length& length) {
    if (length.IsCalculated())
      length.IncrementCalculatedRef();
    if (IsCalculated())
      DecrementCalculatedRef();
    memcpy(this, &length, sizeof(Length));
    return *this;
  }

  ~Length() {
    if (IsCalculated())
      DecrementCalculatedRef();
  }

  bool operator==(const Length& o) const {
    return (type_ == o.type_) && (quirk_ == o.quirk_) &&
           (IsNone() || (GetFloatValue() == o.GetFloatValue()) ||
            IsCalculatedEqual(o));
  }
  bool operator!=(const Length& o) const { return !(*this == o); }

  const Length& operator*=(float v) {
    if (IsCalculated()) {
      NOTREACHED();
      return *this;
    }

    if (is_float_)
      float_value_ = static_cast<float>(float_value_ * v);
    else
      int_value_ = static_cast<int>(int_value_ * v);

    return *this;
  }

  template <typename NUMBER_TYPE>
  static Length Fixed(NUMBER_TYPE number) {
    return Length(number, kFixed);
  }
  static Length Fixed() { return Length(kFixed); }
  static Length Auto() { return Length(kAuto); }
  static Length FillAvailable() { return Length(kFillAvailable); }
  static Length MinContent() { return Length(kMinContent); }
  static Length MaxContent() { return Length(kMaxContent); }
  static Length MinIntrinsic() { return Length(kMinIntrinsic); }
  static Length ExtendToZoom() { return Length(kExtendToZoom); }
  static Length DeviceWidth() { return Length(kDeviceWidth); }
  static Length DeviceHeight() { return Length(kDeviceHeight); }
  static Length None() { return Length(kNone); }
  static Length FitContent() { return Length(kFitContent); }
  static Length Content() { return Length(kContent); }
  template <typename NUMBER_TYPE>
  static Length Percent(NUMBER_TYPE number) {
    return Length(number, kPercent);
  }

  // FIXME: Make this private (if possible) or at least rename it
  // (http://crbug.com/432707).
  inline float Value() const {
    DCHECK(!IsCalculated());
    return GetFloatValue();
  }

  int IntValue() const {
    if (IsCalculated()) {
      NOTREACHED();
      return 0;
    }
    return GetIntValue();
  }

  float Pixels() const {
    DCHECK_EQ(GetType(), kFixed);
    return GetFloatValue();
  }

  float Percent() const {
    DCHECK_EQ(GetType(), kPercent);
    return GetFloatValue();
  }

  PixelsAndPercent GetPixelsAndPercent() const;

  const CalculationValue& GetCalculationValue() const;

  // If |this| is calculated, returns the underlying |CalculationValue|. If not,
  // returns a |CalculationValue| constructed from |GetPixelsAndPercent()|. Hits
  // a DCHECK if |this| is not a specified value (e.g., 'auto').
  scoped_refptr<const CalculationValue> AsCalculationValue() const;

  Length::Type GetType() const { return static_cast<Length::Type>(type_); }
  bool Quirk() const { return quirk_; }

  void SetQuirk(bool quirk) { quirk_ = quirk; }

  bool IsNone() const { return GetType() == kNone; }

  // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated
  // Length always contains a percentage, and without a maxValue passed to these
  // functions it's impossible to determine the sign or zero-ness. We assume all
  // calc values are positive and non-zero for now.
  bool IsZero() const {
    DCHECK(!IsNone());
    if (IsCalculated())
      return false;

    return is_float_ ? !float_value_ : !int_value_;
  }
  bool IsPositive() const {
    if (IsNone())
      return false;
    if (IsCalculated())
      return true;

    return GetFloatValue() > 0;
  }
  bool IsNegative() const {
    if (IsNone() || IsCalculated())
      return false;

    return GetFloatValue() < 0;
  }

  // For the layout purposes, if this |Length| is a block-axis size, see
  // |IsAutoOrContentOrIntrinsic()|, it is usually a better choice.
  bool IsAuto() const { return GetType() == kAuto; }
  bool IsFixed() const { return GetType() == kFixed; }

  // For the block axis, intrinsic sizes such as `min-content` behave the same
  // as `auto`. https://www.w3.org/TR/css-sizing-3/#valdef-width-min-content
  bool IsContentOrIntrinsic() const {
    return GetType() == kMinContent || GetType() == kMaxContent ||
           GetType() == kFitContent || GetType() == kMinIntrinsic ||
           GetType() == kContent;
  }
  bool IsAutoOrContentOrIntrinsic() const {
    return GetType() == kAuto || IsContentOrIntrinsic();
  }

  // NOTE: This shouldn't be use in NG code.
  bool IsContentOrIntrinsicOrFillAvailable() const {
    return IsContentOrIntrinsic() || GetType() == kFillAvailable;
  }

  bool IsSpecified() const {
    return GetType() == kFixed || GetType() == kPercent ||
           GetType() == kCalculated;
  }

  bool IsCalculated() const { return GetType() == kCalculated; }
  bool IsCalculatedEqual(const Length&) const;
  bool IsMinContent() const { return GetType() == kMinContent; }
  bool IsMaxContent() const { return GetType() == kMaxContent; }
  bool IsContent() const { return GetType() == kContent; }
  bool IsMinIntrinsic() const { return GetType() == kMinIntrinsic; }
  bool IsFillAvailable() const { return GetType() == kFillAvailable; }
  bool IsFitContent() const { return GetType() == kFitContent; }
  bool IsPercent() const { return GetType() == kPercent; }
  bool IsPercentOrCalc() const {
    return GetType() == kPercent || GetType() == kCalculated;
  }
  bool IsExtendToZoom() const { return GetType() == kExtendToZoom; }
  bool IsDeviceWidth() const { return GetType() == kDeviceWidth; }
  bool IsDeviceHeight() const { return GetType() == kDeviceHeight; }

  Length Blend(const Length& from, double progress, ValueRange range) const {
    DCHECK(IsSpecified());
    DCHECK(from.IsSpecified());

    if (progress == 0.0)
      return from;

    if (progress == 1.0)
      return *this;

    if (from.GetType() == kCalculated || GetType() == kCalculated)
      return BlendMixedTypes(from, progress, range);

    if (!from.IsZero() && !IsZero() && from.GetType() != GetType())
      return BlendMixedTypes(from, progress, range);

    if (from.IsZero() && IsZero())
      return *this;

    return BlendSameTypes(from, progress, range);
  }

  float GetFloatValue() const {
    DCHECK(!IsNone());
    return is_float_ ? float_value_ : int_value_;
  }
  float NonNanCalculatedValue(LayoutUnit max_value) const;

  Length SubtractFromOneHundredPercent() const;

  Length Zoom(double factor) const;

  String ToString() const;

 private:
  int GetIntValue() const {
    DCHECK(!IsNone());
    return is_float_ ? static_cast<int>(float_value_) : int_value_;
  }

  Length BlendMixedTypes(const Length& from, double progress, ValueRange) const;

  Length BlendSameTypes(const Length& from, double progress, ValueRange) const;

  int CalculationHandle() const {
    DCHECK(IsCalculated());
    return GetIntValue();
  }
  void IncrementCalculatedRef() const;
  void DecrementCalculatedRef() const;

  union {
    int int_value_;
    float float_value_;
  };
  bool quirk_;
  unsigned char type_;
  bool is_float_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const Length&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LENGTH_H_
