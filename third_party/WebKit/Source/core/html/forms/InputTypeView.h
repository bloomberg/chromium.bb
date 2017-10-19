/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef InputTypeView_h
#define InputTypeView_h

#include "core/CoreExport.h"
#include "core/dom/events/EventDispatcher.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class AXObject;
class BeforeTextInsertedEvent;
class Element;
class Event;
class FormControlState;
class HTMLFormElement;
class HTMLInputElement;
class KeyboardEvent;
class MouseEvent;
class LayoutObject;
class ComputedStyle;

class ClickHandlingState final : public EventDispatchHandlingState {
 public:
  virtual void Trace(blink::Visitor*);

  bool checked;
  bool indeterminate;
  Member<HTMLInputElement> checked_radio_button;
};

// An InputTypeView object represents the UI-specific part of an
// HTMLInputElement. Do not expose instances of InputTypeView and classes
// derived from it to classes other than HTMLInputElement.
class CORE_EXPORT InputTypeView : public GarbageCollectedMixin {
  WTF_MAKE_NONCOPYABLE(InputTypeView);

 public:
  virtual ~InputTypeView();
  virtual void Trace(blink::Visitor*);

  virtual bool SizeShouldIncludeDecoration(int default_size,
                                           int& preferred_size) const;

  // Event handling functions

  virtual void HandleClickEvent(MouseEvent*);
  virtual void HandleMouseDownEvent(MouseEvent*);
  virtual ClickHandlingState* WillDispatchClick();
  virtual void DidDispatchClick(Event*, const ClickHandlingState&);
  virtual void HandleKeydownEvent(KeyboardEvent*);
  virtual void HandleKeypressEvent(KeyboardEvent*);
  virtual void HandleKeyupEvent(KeyboardEvent*);
  virtual void HandleBeforeTextInsertedEvent(BeforeTextInsertedEvent*);
  virtual void ForwardEvent(Event*);
  virtual bool ShouldSubmitImplicitly(Event*);
  virtual HTMLFormElement* FormForSubmission() const;
  virtual bool HasCustomFocusLogic() const;
  virtual void HandleFocusInEvent(Element* old_focused_element, WebFocusType);
  virtual void HandleBlurEvent();
  virtual void HandleDOMActivateEvent(Event*);
  virtual void AccessKeyAction(bool send_mouse_events);
  virtual void Blur();
  void DispatchSimulatedClickIfActive(KeyboardEvent*) const;

  virtual void SubtreeHasChanged();
  virtual LayoutObject* CreateLayoutObject(const ComputedStyle&) const;
  virtual RefPtr<ComputedStyle> CustomStyleForLayoutObject(
      RefPtr<ComputedStyle>);
  virtual TextDirection ComputedTextDirection();
  virtual void StartResourceLoading();
  virtual void ClosePopupView();
  virtual void CreateShadowSubtree();
  virtual void DestroyShadowSubtree();
  virtual void MinOrMaxAttributeChanged();
  virtual void StepAttributeChanged();
  virtual void AltAttributeChanged();
  virtual void SrcAttributeChanged();
  virtual void UpdateView();
  virtual void AttributeChanged();
  virtual void MultipleAttributeChanged();
  virtual void DisabledAttributeChanged();
  virtual void ReadonlyAttributeChanged();
  virtual void RequiredAttributeChanged();
  virtual void ValueAttributeChanged();
  virtual void DidSetValue(const String&, bool value_changed);
  virtual void ListAttributeTargetChanged();
  virtual void UpdateClearButtonVisibility();
  virtual void UpdatePlaceholderText();
  virtual AXObject* PopupRootAXObject();
  virtual void EnsureFallbackContent() {}
  virtual void EnsurePrimaryContent() {}
  virtual bool HasFallbackContent() const { return false; }
  virtual FormControlState SaveFormControlState() const;
  virtual void RestoreFormControlState(const FormControlState&);

  // Validation functions
  virtual bool HasBadInput() const;

 protected:
  InputTypeView(HTMLInputElement& element) : element_(&element) {}
  HTMLInputElement& GetElement() const { return *element_; }

 private:
  Member<HTMLInputElement> element_;
};

}  // namespace blink
#endif
