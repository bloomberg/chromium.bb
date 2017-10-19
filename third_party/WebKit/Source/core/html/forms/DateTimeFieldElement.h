/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DateTimeFieldElement_h
#define DateTimeFieldElement_h

#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSpanElement.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class DateComponents;
class DateTimeFieldsState;

// DateTimeFieldElement is base class of date time field element.
class DateTimeFieldElement : public HTMLSpanElement {
  WTF_MAKE_NONCOPYABLE(DateTimeFieldElement);

 public:
  enum EventBehavior {
    kDispatchNoEvent,
    kDispatchEvent,
  };

  // FieldOwner implementer must call removeEventHandler when
  // it doesn't handle event, e.g. at destruction.
  class FieldOwner : public GarbageCollectedMixin {
   public:
    virtual ~FieldOwner();
    virtual void DidBlurFromField(WebFocusType) = 0;
    virtual void DidFocusOnField(WebFocusType) = 0;
    virtual void FieldValueChanged() = 0;
    virtual bool FocusOnNextField(const DateTimeFieldElement&) = 0;
    virtual bool FocusOnPreviousField(const DateTimeFieldElement&) = 0;
    virtual bool IsFieldOwnerDisabled() const = 0;
    virtual bool IsFieldOwnerReadOnly() const = 0;
    virtual AtomicString LocaleIdentifier() const = 0;
    virtual void FieldDidChangeValueByKeyboard() = 0;
  };

  void DefaultEventHandler(Event*) override;
  virtual bool HasValue() const = 0;
  bool IsDisabled() const;
  virtual float MaximumWidth(const ComputedStyle&);
  virtual void PopulateDateTimeFieldsState(DateTimeFieldsState&) = 0;
  void RemoveEventHandler() { field_owner_ = nullptr; }
  void SetDisabled();
  virtual void SetEmptyValue(EventBehavior = kDispatchNoEvent) = 0;
  virtual void SetValueAsDate(const DateComponents&) = 0;
  virtual void SetValueAsDateTimeFieldsState(const DateTimeFieldsState&) = 0;
  virtual void SetValueAsInteger(int, EventBehavior = kDispatchNoEvent) = 0;
  virtual void StepDown() = 0;
  virtual void StepUp() = 0;
  virtual String Value() const = 0;
  virtual String VisibleValue() const = 0;
  virtual void Trace(blink::Visitor*);

  static float ComputeTextWidth(const ComputedStyle&, const String&);

 protected:
  DateTimeFieldElement(Document&, FieldOwner&);
  void FocusOnNextField();
  virtual void HandleKeyboardEvent(KeyboardEvent*) = 0;
  void Initialize(const AtomicString& pseudo,
                  const String& ax_help_text,
                  int ax_minimum,
                  int ax_maximum);
  Locale& LocaleForOwner() const;
  AtomicString LocaleIdentifier() const;
  void UpdateVisibleValue(EventBehavior);
  virtual int ValueAsInteger() const = 0;
  virtual int ValueForARIAValueNow() const;

  // Node functions.
  void SetFocused(bool, WebFocusType) override;

 private:
  void DefaultKeyboardEventHandler(KeyboardEvent*);
  bool IsDateTimeFieldElement() const final;
  bool IsFieldOwnerDisabled() const;
  bool IsFieldOwnerReadOnly() const;
  bool SupportsFocus() const final;

  Member<FieldOwner> field_owner_;
};

}  // namespace blink

#endif
