/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaQueryExp_h
#define MediaQueryExp_h

#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/media_feature_names.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserToken;

struct MediaQueryExpValue {
  DISALLOW_NEW();
  CSSValueID id;
  double value;
  CSSPrimitiveValue::UnitType unit;
  unsigned numerator;
  unsigned denominator;

  bool is_id;
  bool is_value;
  bool is_ratio;

  MediaQueryExpValue()
      : id(CSSValueInvalid),
        value(0),
        unit(CSSPrimitiveValue::UnitType::kUnknown),
        numerator(0),
        denominator(1),
        is_id(false),
        is_value(false),
        is_ratio(false) {}

  bool IsValid() const { return (is_id || is_value || is_ratio); }
  String CssText() const;
  bool Equals(const MediaQueryExpValue& exp_value) const {
    if (is_id)
      return (id == exp_value.id);
    if (is_value)
      return (value == exp_value.value);
    if (is_ratio)
      return (numerator == exp_value.numerator &&
              denominator == exp_value.denominator);
    return !exp_value.IsValid();
  }
};

class CORE_EXPORT MediaQueryExp {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // Returns an invalid MediaQueryExp if the arguments are invalid.
  static MediaQueryExp Create(const String& media_feature,
                              const Vector<CSSParserToken, 4>&);
  static MediaQueryExp Invalid() {
    return MediaQueryExp(String(), MediaQueryExpValue());
  }

  MediaQueryExp(const MediaQueryExp& other);
  ~MediaQueryExp();

  const String& MediaFeature() const { return media_feature_; }

  MediaQueryExpValue ExpValue() const { return exp_value_; }

  bool IsValid() const { return !media_feature_.IsNull(); }

  bool operator==(const MediaQueryExp& other) const;

  bool IsViewportDependent() const;

  bool IsDeviceDependent() const;

  String Serialize() const;

 private:
  MediaQueryExp(const String&, const MediaQueryExpValue&);

  String media_feature_;
  MediaQueryExpValue exp_value_;
};

}  // namespace blink

#endif
