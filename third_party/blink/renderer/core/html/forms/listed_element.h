/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContainerNode;
class Document;
class FormAttributeTargetObserver;
class FormData;
class HTMLElement;
class HTMLFormElement;
class Node;
class ValidationMessageClient;
class ValidityState;

enum CheckValidityEventBehavior {
  kCheckValidityDispatchNoEvent,
  kCheckValidityDispatchInvalidEvent
};

// https://html.spec.whatwg.org/multipage/forms.html#category-listed
class CORE_EXPORT ListedElement : public GarbageCollectedMixin {
 public:
  virtual ~ListedElement();

  static HTMLFormElement* FindAssociatedForm(const HTMLElement*,
                                             const AtomicString& form_id,
                                             HTMLFormElement* form_ancestor);
  HTMLFormElement* Form() const { return form_.Get(); }
  ValidityState* validity();

  virtual bool IsFormControlElement() const = 0;
  virtual bool IsFormControlElementWithState() const;
  virtual bool IsElementInternals() const;
  virtual bool IsEnumeratable() const = 0;

  // Returns the 'name' attribute value. If this element has no name
  // attribute, it returns an empty string instead of null string.
  // Note that the 'name' IDL attribute doesn't use this function.
  virtual const AtomicString& GetName() const;

  // Override in derived classes to get the encoded name=value pair for
  // submitting.
  virtual void AppendToFormData(FormData&) {}

  void ResetFormOwner();

  void FormRemovedFromTree(const Node& form_root);

  bool WillValidate() const;

  // ValidityState attribute implementations
  bool CustomError() const;

  // Functions for ValidityState interface methods.
  // Override functions for PatterMismatch, RangeOverflow, RangerUnderflow,
  // StepMismatch, TooLong, TooShort and ValueMissing must call WillValidate
  // method.
  virtual bool HasBadInput() const;
  virtual bool PatternMismatch() const;
  virtual bool RangeOverflow() const;
  virtual bool RangeUnderflow() const;
  virtual bool StepMismatch() const;
  virtual bool TooLong() const;
  virtual bool TooShort() const;
  virtual bool TypeMismatch() const;
  virtual bool ValueMissing() const;
  bool Valid() const;

  using List = HeapVector<Member<ListedElement>>;

  virtual String validationMessage() const;
  virtual String ValidationSubMessage() const;
  virtual void setCustomValidity(const String&);
  void UpdateVisibleValidationMessage();
  void HideVisibleValidationMessage();
  bool checkValidity(
      List* unhandled_invalid_controls = nullptr,
      CheckValidityEventBehavior = kCheckValidityDispatchInvalidEvent);
  bool reportValidity();
  // This must be called only after the caller check the element is focusable.
  void ShowValidationMessage();
  bool IsValidationMessageVisible() const;
  void FindCustomValidationMessageTextDirection(const String& message,
                                                TextDirection& message_dir,
                                                String& sub_message,
                                                TextDirection& sub_message_dir);

  // For Element::IsValidElement(), which is for :valid :invalid selectors.
  bool IsValidElement();

  // This must be called when a validation constraint or control value is
  // changed.
  void SetNeedsValidityCheck();

  // This should be called when |disabled| content attribute is changed.
  virtual void DisabledAttributeChanged();
  // This should be called when |form| content attribute is changed.
  void FormAttributeChanged();
  // This is for FormAttributeTargteObserver class.
  void FormAttributeTargetChanged();
  // This should be called in Node::InsertedInto().
  void InsertedInto(ContainerNode&);
  // This should be called in Node::RemovedFrom().
  void RemovedFrom(ContainerNode&);
  // This should be called in Node::DidMoveToDocument().
  void DidMoveToNewDocument(Document& old_document);
  // This is for HTMLFieldSetElement class.
  void AncestorDisabledStateWasChanged();

  // https://html.spec.whatwg.org/multipage/semantics-other.html#concept-element-disabled
  bool IsActuallyDisabled() const;

  void Trace(blink::Visitor*) override;

 protected:
  ListedElement();

  // FIXME: Remove usage of setForm. resetFormOwner should be enough, and
  // setForm is confusing.
  void SetForm(HTMLFormElement*);
  void AssociateByParser(HTMLFormElement*);

  // If you add an override of willChangeForm() or didChangeForm() to a class
  // derived from this one, you will need to add a call to setForm(0) to the
  // destructor of that class.
  virtual void WillChangeForm();
  virtual void DidChangeForm();

  // This must be called any time the result of WillValidate() has changed.
  void UpdateWillValidateCache();
  virtual bool RecalcWillValidate() const;

  String CustomValidationMessage() const;

  // False; There are no FIELDSET ancestors.
  // True; There might be a FIELDSET ancestor, and thre might be no
  //       FIELDSET ancestors.
  mutable bool may_have_field_set_ancestor_ = true;

 private:
  void UpdateAncestorDisabledState() const;
  void SetFormAttributeTargetObserver(FormAttributeTargetObserver*);
  void ResetFormAttributeTargetObserver();
  // Requests validity recalc for the form owner, if one exists.
  void FormOwnerSetNeedsValidityCheck();
  // Requests validity recalc for all ancestor fieldsets, if exist.
  void FieldSetAncestorsSetNeedsValidityCheck(Node*);

  ValidationMessageClient* GetValidationMessageClient() const;

  Member<FormAttributeTargetObserver> form_attribute_target_observer_;
  Member<HTMLFormElement> form_;
  Member<ValidityState> validity_state_;
  String custom_validation_message_;
  bool has_validation_message_ : 1;
  // If form_was_set_by_parser_ is true, form_ is always non-null.
  bool form_was_set_by_parser_ : 1;

  // The initial value of will_validate_ depends on the derived class. We can't
  // initialize it with a virtual function in the constructor. will_validate_
  // is not deterministic as long as will_validate_initialized_ is false.
  mutable bool will_validate_initialized_ : 1;
  mutable bool will_validate_ : 1;

  // Cache of IsValidElement().
  bool is_valid_ : 1;
  bool validity_is_dirty_ : 1;

  enum class AncestorDisabledState { kUnknown, kEnabled, kDisabled };
  mutable AncestorDisabledState ancestor_disabled_state_ =
      AncestorDisabledState::kUnknown;

  enum class DataListAncestorState {
    kUnknown,
    kInsideDataList,
    kNotInsideDataList
  };
  mutable enum DataListAncestorState data_list_ancestor_state_ =
      DataListAncestorState::kUnknown;
};

CORE_EXPORT HTMLElement* ToHTMLElement(ListedElement*);
CORE_EXPORT HTMLElement& ToHTMLElement(ListedElement&);
CORE_EXPORT const HTMLElement* ToHTMLElement(const ListedElement*);
CORE_EXPORT const HTMLElement& ToHTMLElement(const ListedElement&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_
