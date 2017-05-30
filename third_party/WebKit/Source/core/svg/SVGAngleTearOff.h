/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGAngleTearOff_h
#define SVGAngleTearOff_h

#include "core/svg/SVGAngle.h"
#include "core/svg/properties/SVGPropertyTearOff.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class SVGAngleTearOff final : public SVGPropertyTearOff<SVGAngle>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SVGAngleTearOff* Create(SVGAngle* target,
                                 SVGElement* context_element,
                                 PropertyIsAnimValType property_is_anim_val,
                                 const QualifiedName& attribute_name) {
    return new SVGAngleTearOff(target, context_element, property_is_anim_val,
                               attribute_name);
  }
  static SVGAngleTearOff* CreateDetached();

  enum {
    kSvgAngletypeUnknown = SVGAngle::kSvgAngletypeUnknown,
    kSvgAngletypeUnspecified = SVGAngle::kSvgAngletypeUnspecified,
    kSvgAngletypeDeg = SVGAngle::kSvgAngletypeDeg,
    kSvgAngletypeRad = SVGAngle::kSvgAngletypeRad,
    kSvgAngletypeGrad = SVGAngle::kSvgAngletypeGrad
  };

  ~SVGAngleTearOff() override;

  unsigned short unitType() {
    return HasExposedAngleUnit() ? Target()->UnitType()
                                 : SVGAngle::kSvgAngletypeUnknown;
  }

  void setValue(float, ExceptionState&);
  float value() { return Target()->Value(); }

  void setValueInSpecifiedUnits(float, ExceptionState&);
  float valueInSpecifiedUnits() { return Target()->ValueInSpecifiedUnits(); }

  void newValueSpecifiedUnits(unsigned short unit_type,
                              float value_in_specified_units,
                              ExceptionState&);
  void convertToSpecifiedUnits(unsigned short unit_type, ExceptionState&);

  String valueAsString() {
    return HasExposedAngleUnit() ? Target()->ValueAsString()
                                 : String::Number(0);
  }
  void setValueAsString(const String&, ExceptionState&);

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  SVGAngleTearOff(SVGAngle*,
                  SVGElement*,
                  PropertyIsAnimValType,
                  const QualifiedName&);

  bool HasExposedAngleUnit() {
    return Target()->UnitType() <= SVGAngle::kSvgAngletypeGrad;
  }
};

}  // namespace blink

#endif  // SVGAngleTearOff_h
