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

#ifndef MultipleFieldsTemporalInputTypeView_h
#define MultipleFieldsTemporalInputTypeView_h

#include "core/html/forms/ClearButtonElement.h"
#include "core/html/forms/DateTimeEditElement.h"
#include "core/html/forms/InputTypeView.h"
#include "core/html/forms/PickerIndicatorElement.h"
#include "core/html/forms/SpinButtonElement.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class BaseTemporalInputType;
struct DateTimeChooserParameters;

class MultipleFieldsTemporalInputTypeView final
    : public GarbageCollectedFinalized<MultipleFieldsTemporalInputTypeView>,
      public InputTypeView,
      protected DateTimeEditElement::EditControlOwner,
      protected PickerIndicatorElement::PickerIndicatorOwner,
      protected SpinButtonElement::SpinButtonOwner,
      protected ClearButtonElement::ClearButtonOwner {
  USING_GARBAGE_COLLECTED_MIXIN(MultipleFieldsTemporalInputTypeView);

 public:
  static MultipleFieldsTemporalInputTypeView* Create(HTMLInputElement&,
                                                     BaseTemporalInputType&);
  ~MultipleFieldsTemporalInputTypeView() override;
  virtual void Trace(blink::Visitor*);

 private:
  MultipleFieldsTemporalInputTypeView(HTMLInputElement&,
                                      BaseTemporalInputType&);

  // DateTimeEditElement::EditControlOwner functions
  void DidBlurFromControl(WebFocusType) final;
  void DidFocusOnControl(WebFocusType) final;
  void EditControlValueChanged() final;
  String FormatDateTimeFieldsState(const DateTimeFieldsState&) const override;
  bool IsEditControlOwnerDisabled() const final;
  bool IsEditControlOwnerReadOnly() const final;
  AtomicString LocaleIdentifier() const final;
  void EditControlDidChangeValueByKeyboard() final;

  // SpinButtonElement::SpinButtonOwner functions.
  void FocusAndSelectSpinButtonOwner() override;
  bool ShouldSpinButtonRespondToMouseEvents() override;
  bool ShouldSpinButtonRespondToWheelEvents() override;
  void SpinButtonStepDown() override;
  void SpinButtonStepUp() override;
  void SpinButtonDidReleaseMouseCapture(
      SpinButtonElement::EventDispatch) override;

  // PickerIndicatorElement::PickerIndicatorOwner functions
  bool IsPickerIndicatorOwnerDisabledOrReadOnly() const final;
  void PickerIndicatorChooseValue(const String&) final;
  void PickerIndicatorChooseValue(double) final;
  Element& PickerOwnerElement() const final;
  bool SetupDateTimeChooserParameters(DateTimeChooserParameters&) final;

  // ClearButtonElement::ClearButtonOwner functions.
  void FocusAndSelectClearButtonOwner() override;
  bool ShouldClearButtonRespondToMouseEvents() override;
  void ClearValue() override;

  // InputTypeView functions
  void Blur() final;
  void ClosePopupView() override;
  RefPtr<ComputedStyle> CustomStyleForLayoutObject(
      RefPtr<ComputedStyle>) override;
  void CreateShadowSubtree() final;
  void DestroyShadowSubtree() final;
  void DisabledAttributeChanged() final;
  void ForwardEvent(Event*) final;
  void HandleFocusInEvent(Element* old_focused_element, WebFocusType) final;
  void HandleKeydownEvent(KeyboardEvent*) final;
  bool HasBadInput() const override;
  bool HasCustomFocusLogic() const final;
  void MinOrMaxAttributeChanged() final;
  void ReadonlyAttributeChanged() final;
  void RequiredAttributeChanged() final;
  void RestoreFormControlState(const FormControlState&) final;
  FormControlState SaveFormControlState() const final;
  void DidSetValue(const String&, bool value_changed) final;
  void StepAttributeChanged() final;
  void UpdateView() final;
  void ValueAttributeChanged() override;
  void ListAttributeTargetChanged() final;
  void UpdateClearButtonVisibility() final;
  TextDirection ComputedTextDirection() final;
  AXObject* PopupRootAXObject() final;

  DateTimeEditElement* GetDateTimeEditElement() const;
  SpinButtonElement* GetSpinButtonElement() const;
  ClearButtonElement* GetClearButtonElement() const;
  PickerIndicatorElement* GetPickerIndicatorElement() const;
  bool ContainsFocusedShadowElement() const;
  void ShowPickerIndicator();
  void HidePickerIndicator();
  void UpdatePickerIndicatorVisibility();

  Member<BaseTemporalInputType> input_type_;
  bool is_destroying_shadow_subtree_;
  bool picker_indicator_is_visible_;
  bool picker_indicator_is_always_visible_;
};

}  // namespace blink

#endif  // MultipleFieldsTemporalInputTypeView_h
