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

#ifndef ColorInputType_h
#define ColorInputType_h

#include "core/html/forms/ColorChooserClient.h"
#include "core/html/forms/InputType.h"
#include "core/html/forms/KeyboardClickableInputTypeView.h"

namespace blink {

class ColorChooser;

class ColorInputType final : public InputType,
                             public KeyboardClickableInputTypeView,
                             public ColorChooserClient {
  USING_GARBAGE_COLLECTED_MIXIN(ColorInputType);

 public:
  static InputType* Create(HTMLInputElement&);
  ~ColorInputType() override;
  virtual void Trace(blink::Visitor*);
  using InputType::GetElement;

  // ColorChooserClient implementation.
  void DidChooseColor(const Color&) override;
  void DidEndChooser() override;
  Element& OwnerElement() const override;
  IntRect ElementRectRelativeToViewport() const override;
  Color CurrentColor() override;
  bool ShouldShowSuggestions() const override;
  Vector<ColorSuggestion> Suggestions() const override;
  ColorChooserClient* GetColorChooserClient() override;

 private:
  explicit ColorInputType(HTMLInputElement&);
  InputTypeView* CreateView() override;
  ValueMode GetValueMode() const override;
  void ValueAttributeChanged() override;
  void CountUsage() override;
  const AtomicString& FormControlType() const override;
  bool SupportsRequired() const override;
  String SanitizeValue(const String&) const override;
  void CreateShadowSubtree() override;
  void DidSetValue(const String&, bool value_changed) override;
  void HandleDOMActivateEvent(Event*) override;
  void ClosePopupView() override;
  bool ShouldRespectListAttribute() override;
  bool TypeMismatchFor(const String&) const override;
  void WarnIfValueIsInvalid(const String&) const override;
  void UpdateView() override;
  AXObject* PopupRootAXObject() override;

  Color ValueAsColor() const;
  void EndColorChooser();
  HTMLElement* ShadowColorSwatch() const;

  Member<ColorChooser> chooser_;
};

}  // namespace blink

#endif  // ColorInputType_h
