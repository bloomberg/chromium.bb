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

#ifndef TextFieldInputType_h
#define TextFieldInputType_h

#include "core/html/forms/InputType.h"
#include "core/html/shadow/SpinButtonElement.h"

namespace blink {

class FormDataList;

// The class represents types of which UI contain text fields.
// It supports not only the types for BaseTextInputType but also type=number.
class TextFieldInputType : public InputType, protected SpinButtonElement::SpinButtonOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TextFieldInputType);
public:
    virtual void trace(Visitor* visitor) override { InputType::trace(visitor); }

protected:
    TextFieldInputType(HTMLInputElement&);
    virtual ~TextFieldInputType();
    virtual bool canSetSuggestedValue() override;
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    void handleKeydownEventForSpinButton(KeyboardEvent*);

    virtual bool needsContainer() const { return false; }
    bool shouldHaveSpinButton() const;
    virtual void createShadowSubtree() override;
    virtual void destroyShadowSubtree() override;
    virtual void attributeChanged() override;
    virtual void disabledAttributeChanged() override;
    virtual void readonlyAttributeChanged() override;
    virtual bool supportsReadOnly() const override;
    virtual void handleFocusEvent(Element* oldFocusedNode, FocusType) override final;
    virtual void handleBlurEvent() override final;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual void updateView() override;

    virtual String convertFromVisibleValue(const String&) const;
    enum ValueChangeState {
        ValueChangeStateNone,
        ValueChangeStateChanged
    };
    virtual void didSetValueByUserEdit(ValueChangeState);

    Element* containerElement() const;

private:
    virtual bool shouldShowFocusRingOnMouseFocus() const override final;
    virtual bool isTextField() const override final;
    virtual bool valueMissing(const String&) const override;
    virtual void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) override;
    virtual void forwardEvent(Event*) override final;
    virtual bool shouldSubmitImplicitly(Event*) override final;
    virtual RenderObject* createRenderer(RenderStyle*) const override;
    virtual String sanitizeValue(const String&) const override;
    virtual bool shouldRespectListAttribute() override;
    virtual void listAttributeTargetChanged() override;
    virtual void updatePlaceholderText() override final;
    virtual bool appendFormData(FormDataList&, bool multipart) const override;
    virtual void subtreeHasChanged() override final;

    // SpinButtonElement::SpinButtonOwner functions.
    virtual void focusAndSelectSpinButtonOwner() override final;
    virtual bool shouldSpinButtonRespondToMouseEvents() override final;
    virtual bool shouldSpinButtonRespondToWheelEvents() override final;
    virtual void spinButtonStepDown() override final;
    virtual void spinButtonStepUp() override final;
    virtual void spinButtonDidReleaseMouseCapture(SpinButtonElement::EventDispatch) override final;

    SpinButtonElement* spinButtonElement() const;
};

} // namespace blink

#endif // TextFieldInputType_h
