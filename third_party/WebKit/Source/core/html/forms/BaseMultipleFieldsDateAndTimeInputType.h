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

#ifndef BaseMultipleFieldsDateAndTimeInputType_h
#define BaseMultipleFieldsDateAndTimeInputType_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/forms/BaseDateAndTimeInputType.h"

#include "core/html/shadow/ClearButtonElement.h"
#include "core/html/shadow/DateTimeEditElement.h"
#include "core/html/shadow/PickerIndicatorElement.h"
#include "core/html/shadow/SpinButtonElement.h"

namespace blink {

struct DateTimeChooserParameters;

class BaseMultipleFieldsDateAndTimeInputType
    : public BaseDateAndTimeInputType
    , protected DateTimeEditElement::EditControlOwner
    , protected PickerIndicatorElement::PickerIndicatorOwner
    , protected SpinButtonElement::SpinButtonOwner
    , protected ClearButtonElement::ClearButtonOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BaseMultipleFieldsDateAndTimeInputType);

public:
    virtual bool isValidFormat(bool hasYear, bool hasMonth, bool hasWeek, bool hasDay, bool hasAMPM, bool hasHour, bool hasMinute, bool hasSecond) const = 0;

    virtual void trace(Visitor* visitor) override { BaseDateAndTimeInputType::trace(visitor); }

protected:
    BaseMultipleFieldsDateAndTimeInputType(HTMLInputElement&);
    virtual ~BaseMultipleFieldsDateAndTimeInputType();

    virtual void setupLayoutParameters(DateTimeEditElement::LayoutParameters&, const DateComponents&) const = 0;
    bool shouldHaveSecondField(const DateComponents&) const;

private:
    // DateTimeEditElement::EditControlOwner functions
    virtual void didBlurFromControl() override final;
    virtual void didFocusOnControl() override final;
    virtual void editControlValueChanged() override final;
    virtual bool isEditControlOwnerDisabled() const override final;
    virtual bool isEditControlOwnerReadOnly() const override final;
    virtual AtomicString localeIdentifier() const override final;
    virtual void editControlDidChangeValueByKeyboard() override final;

    // SpinButtonElement::SpinButtonOwner functions.
    virtual void focusAndSelectSpinButtonOwner() override;
    virtual bool shouldSpinButtonRespondToMouseEvents() override;
    virtual bool shouldSpinButtonRespondToWheelEvents() override;
    virtual void spinButtonStepDown() override;
    virtual void spinButtonStepUp() override;
    virtual void spinButtonDidReleaseMouseCapture(SpinButtonElement::EventDispatch) override;

    // PickerIndicatorElement::PickerIndicatorOwner functions
    virtual bool isPickerIndicatorOwnerDisabledOrReadOnly() const override final;
    virtual void pickerIndicatorChooseValue(const String&) override final;
    virtual void pickerIndicatorChooseValue(double) override final;
    virtual Element& pickerOwnerElement() const override final;
    virtual bool setupDateTimeChooserParameters(DateTimeChooserParameters&) override final;

    // ClearButtonElement::ClearButtonOwner functions.
    virtual void focusAndSelectClearButtonOwner() override;
    virtual bool shouldClearButtonRespondToMouseEvents() override;
    virtual void clearValue() override;

    // InputType functions
    virtual String badInputText() const override;
    virtual void blur() override final;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(PassRefPtr<RenderStyle>) override;
    virtual void createShadowSubtree() override final;
    virtual void destroyShadowSubtree() override final;
    virtual void disabledAttributeChanged() override final;
    virtual void forwardEvent(Event*) override final;
    virtual void handleFocusInEvent(Element* oldFocusedElement, FocusType) override final;
    virtual void handleKeydownEvent(KeyboardEvent*) override final;
    virtual bool hasBadInput() const override;
    virtual bool hasCustomFocusLogic() const override final;
    virtual void minOrMaxAttributeChanged() override final;
    virtual void readonlyAttributeChanged() override final;
    virtual void requiredAttributeChanged() override final;
    virtual void restoreFormControlState(const FormControlState&) override final;
    virtual FormControlState saveFormControlState() const override final;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override final;
    virtual void stepAttributeChanged() override final;
    virtual void updateView() override final;
    virtual void valueAttributeChanged() override;
    virtual void listAttributeTargetChanged() override final;
    virtual void updateClearButtonVisibility() override final;
    virtual TextDirection computedTextDirection() override final;
    virtual AXObject* popupRootAXObject() override final;

    DateTimeEditElement* dateTimeEditElement() const;
    SpinButtonElement* spinButtonElement() const;
    ClearButtonElement* clearButtonElement() const;
    PickerIndicatorElement* pickerIndicatorElement() const;
    bool containsFocusedShadowElement() const;
    void showPickerIndicator();
    void hidePickerIndicator();
    void updatePickerIndicatorVisibility();

    bool m_isDestroyingShadowSubtree;
    bool m_pickerIndicatorIsVisible;
    bool m_pickerIndicatorIsAlwaysVisible;
};

} // namespace blink

#endif
#endif // BaseMultipleFieldsDateAndTimeInputType_h
