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

#include "core/svg/SVGRectTearOff.h"

#include "core/svg/SVGElement.h"

namespace blink {

SVGRectTearOff::SVGRectTearOff(SVGRect* target,
                               SVGElement* context_element,
                               PropertyIsAnimValType property_is_anim_val,
                               const QualifiedName& attribute_name)
    : SVGPropertyTearOff<SVGRect>(target,
                                  context_element,
                                  property_is_anim_val,
                                  attribute_name) {}

void SVGRectTearOff::setX(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetX(f);
  CommitChange();
}

void SVGRectTearOff::setY(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetY(f);
  CommitChange();
}

void SVGRectTearOff::setWidth(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetWidth(f);
  CommitChange();
}

void SVGRectTearOff::setHeight(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetHeight(f);
  CommitChange();
}

SVGRectTearOff* SVGRectTearOff::CreateDetached(const FloatRect& rect) {
  return Create(SVGRect::Create(rect), nullptr, kPropertyIsNotAnimVal,
                QualifiedName::Null());
}

DEFINE_TRACE_WRAPPERS(SVGRectTearOff) {
  SVGPropertyTearOff<SVGRect>::TraceWrappers(visitor);
  ScriptWrappable::TraceWrappers(visitor);
}

}  // namespace blink
