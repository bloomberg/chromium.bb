/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef RangeInputType_h
#define RangeInputType_h

#include "core/html/forms/InputType.h"

namespace blink {

class ExceptionState;
class SliderThumbElement;

class RangeInputType final : public InputType {
public:
    static PassRefPtrWillBeRawPtr<InputType> create(HTMLInputElement&);

private:
    RangeInputType(HTMLInputElement&);
    virtual void countUsage() override;
    virtual const AtomicString& formControlType() const override;
    virtual double valueAsDouble() const override;
    virtual void setValueAsDouble(double, TextFieldEventBehavior, ExceptionState&) const override;
    virtual bool typeMismatchFor(const String&) const override;
    virtual bool supportsRequired() const override;
    virtual StepRange createStepRange(AnyStepHandling) const override;
    virtual bool isSteppable() const override;
    virtual void handleMouseDownEvent(MouseEvent*) override;
    virtual void handleTouchEvent(TouchEvent*) override;
    virtual bool hasTouchEventHandler() const override;
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    virtual RenderObject* createRenderer(RenderStyle*) const override;
    virtual void createShadowSubtree() override;
    virtual Decimal parseToNumber(const String&, const Decimal&) const override;
    virtual String serialize(const Decimal&) const override;
    virtual void accessKeyAction(bool sendMouseEvents) override;
    virtual void sanitizeValueInResponseToMinOrMaxAttributeChange() override;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual String fallbackValue() const override;
    virtual String sanitizeValue(const String& proposedValue) const override;
    virtual bool shouldRespectListAttribute() override;
    SliderThumbElement* sliderThumbElement() const;
    Element* sliderTrackElement() const;
    virtual void disabledAttributeChanged() override;
    virtual void listAttributeTargetChanged() override;
    void updateTickMarkValues();
    virtual Decimal findClosestTickMarkValue(const Decimal&) override;

    bool m_tickMarkValuesDirty;
    Vector<Decimal> m_tickMarkValues;
};

} // namespace blink

#endif // RangeInputType_h
