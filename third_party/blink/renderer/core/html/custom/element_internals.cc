// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/element_internals.h"

#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/validity_state_flags.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

namespace {
bool IsValidityStateFlagsValid(const ValidityStateFlags* flags) {
  if (!flags)
    return true;
  if (flags->badInput() || flags->customError() || flags->patternMismatch() ||
      flags->rangeOverflow() || flags->rangeUnderflow() ||
      flags->stepMismatch() || flags->tooLong() || flags->tooShort() ||
      flags->typeMismatch() || flags->valueMissing())
    return false;
  return true;
}
}  // anonymous namespace

ElementInternals::ElementInternals(HTMLElement& target) : target_(target) {
  value_.SetUSVString(String());
}

void ElementInternals::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(value_);
  visitor->Trace(entry_source_);
  visitor->Trace(validity_flags_);
  ListedElement::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void ElementInternals::setFormValue(const FileOrUSVString& value,
                                    ExceptionState& exception_state) {
  setFormValue(value, nullptr, exception_state);
}

void ElementInternals::setFormValue(const FileOrUSVString& value,
                                    FormData* entry_source,
                                    ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return;
  }
  if (!entry_source) {
    value_ = value;
    entry_source_ = nullptr;
    return;
  }
  value_ = value;
  entry_source_ = MakeGarbageCollected<FormData>(*entry_source);
}

HTMLFormElement* ElementInternals::form(ExceptionState& exception_state) const {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return nullptr;
  }
  return ListedElement::Form();
}

void ElementInternals::setValidity(ValidityStateFlags* flags,
                                   ExceptionState& exception_state) {
  setValidity(flags, String(), exception_state);
}

void ElementInternals::setValidity(ValidityStateFlags* flags,
                                   const String& message,
                                   ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return;
  }
  // Custom element authors should provide a message. They can omit the message
  // argument only if nothing if | flags| is true.
  if (!IsValidityStateFlagsValid(flags) && message.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTypeMismatchError,
        "The second argument should not be empty if one or more flags in the "
        "first argument are true.");
    return;
  }
  validity_flags_ = flags;
  SetCustomValidationMessage(message);
  SetNeedsValidityCheck();
}

bool ElementInternals::willValidate(ExceptionState& exception_state) const {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return WillValidate();
}

ValidityState* ElementInternals::validity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return nullptr;
  }
  return ListedElement::validity();
}

String ElementInternals::ValidationMessageForBinding(
    ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return String();
  }
  return validationMessage();
}

String ElementInternals::validationMessage() const {
  if (IsValidityStateFlagsValid(validity_flags_))
    return String();
  return CustomValidationMessage();
}

String ElementInternals::ValidationSubMessage() const {
  if (PatternMismatch())
    return Target().FastGetAttribute(html_names::kTitleAttr).GetString();
  return String();
}

bool ElementInternals::checkValidity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return ListedElement::checkValidity();
}

bool ElementInternals::reportValidity(ExceptionState& exception_state) {
  if (!IsTargetFormAssociated()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The target element is not a form-associated custom element.");
    return false;
  }
  return ListedElement::reportValidity();
}

LabelsNodeList* ElementInternals::labels() {
  return Target().labels();
}

void ElementInternals::DidUpgrade() {
  ContainerNode* parent = Target().parentNode();
  if (!parent)
    return;
  InsertedInto(*parent);
  if (auto* owner_form = Form()) {
    if (auto* lists = owner_form->NodeLists())
      lists->InvalidateCaches(nullptr);
  }
  for (ContainerNode* node = parent; node; node = node->parentNode()) {
    if (IsHTMLFieldSetElement(node)) {
      // TODO(tkent): Invalidate only HTMLFormControlsCollections.
      if (auto* lists = node->NodeLists())
        lists->InvalidateCaches(nullptr);
    }
  }
}

bool ElementInternals::IsTargetFormAssociated() const {
  if (Target().IsFormAssociatedCustomElement())
    return true;
  if (Target().GetCustomElementState() != CustomElementState::kUndefined)
    return false;
  // An element is in "undefined" state in its constructor JavaScript code.
  // ElementInternals needs to handle elements to be form-associated same as
  // form-associated custom elements because web authors want to call
  // form-related operations of ElementInternals in constructors.
  CustomElementRegistry* registry = CustomElement::Registry(Target());
  if (!registry)
    return false;
  auto* definition = registry->DefinitionForName(Target().localName());
  return definition && definition->IsFormAssociated();
}

bool ElementInternals::IsFormControlElement() const {
  return false;
}

bool ElementInternals::IsElementInternals() const {
  return true;
}

bool ElementInternals::IsEnumeratable() const {
  return true;
}

void ElementInternals::AppendToFormData(FormData& form_data) {
  if (Target().IsDisabledFormControl())
    return;
  const AtomicString& name = Target().FastGetAttribute(html_names::kNameAttr);
  if (!entry_source_) {
    if (name.IsNull())
      return;
    if (value_.IsFile())
      form_data.AppendFromElement(name, value_.GetAsFile());
    else if (value_.IsUSVString())
      form_data.AppendFromElement(name, value_.GetAsUSVString());
    else
      form_data.AppendFromElement(name, g_empty_string);
    return;
  }
  for (const auto& entry : entry_source_->Entries()) {
    if (entry->isFile())
      form_data.append(entry->name(), entry->GetFile());
    else
      form_data.append(entry->name(), entry->Value());
  }
}

void ElementInternals::DidChangeForm() {
  ListedElement::DidChangeForm();
  CustomElement::EnqueueFormAssociatedCallback(Target(), Form());
}

bool ElementInternals::HasBadInput() const {
  return validity_flags_ && validity_flags_->badInput();
}

bool ElementInternals::PatternMismatch() const {
  return validity_flags_ && validity_flags_->patternMismatch();
}

bool ElementInternals::RangeOverflow() const {
  return validity_flags_ && validity_flags_->rangeOverflow();
}

bool ElementInternals::RangeUnderflow() const {
  return validity_flags_ && validity_flags_->rangeUnderflow();
}

bool ElementInternals::StepMismatch() const {
  return validity_flags_ && validity_flags_->stepMismatch();
}

bool ElementInternals::TooLong() const {
  return validity_flags_ && validity_flags_->tooLong();
}

bool ElementInternals::TooShort() const {
  return validity_flags_ && validity_flags_->tooShort();
}

bool ElementInternals::TypeMismatch() const {
  return validity_flags_ && validity_flags_->typeMismatch();
}

bool ElementInternals::ValueMissing() const {
  return validity_flags_ && validity_flags_->valueMissing();
}

bool ElementInternals::CustomError() const {
  return validity_flags_ && validity_flags_->customError();
}

void ElementInternals::DisabledStateMightBeChanged() {
  bool new_disabled = IsActuallyDisabled();
  if (is_disabled_ == new_disabled)
    return;
  is_disabled_ = new_disabled;
  CustomElement::EnqueueDisabledStateChangedCallback(Target(), new_disabled);
}

}  // namespace blink
