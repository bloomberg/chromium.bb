/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FORM_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FORM_CONTROLLER_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class FormKeyGenerator;
class HTMLFormElement;
class ListedElement;
class SavedFormState;

class FormControlState {
  DISALLOW_NEW();

 public:
  FormControlState() : type_(kTypeSkip) {}
  explicit FormControlState(const String& value) : type_(kTypeRestore) {
    values_.push_back(value);
  }
  static FormControlState Deserialize(const Vector<String>& state_vector,
                                      wtf_size_t& index);
  FormControlState(const FormControlState& another) = default;
  FormControlState& operator=(const FormControlState&);

  bool IsFailure() const { return type_ == kTypeFailure; }
  wtf_size_t ValueSize() const { return values_.size(); }
  const String& operator[](wtf_size_t i) const { return values_[i]; }
  void Append(const String&);
  void SerializeTo(Vector<String>& state_vector) const;

 private:
  enum Type { kTypeSkip, kTypeRestore, kTypeFailure };
  explicit FormControlState(Type type) : type_(type) {}

  Type type_;
  Vector<String> values_;
};

inline FormControlState& FormControlState::operator=(
    const FormControlState& another) = default;

inline void FormControlState::Append(const String& value) {
  type_ = kTypeRestore;
  values_.push_back(value);
}

using SavedFormStateMap =
    HashMap<AtomicString, std::unique_ptr<SavedFormState>>;

class CORE_EXPORT DocumentState final
    : public GarbageCollectedFinalized<DocumentState> {
 public:
  DocumentState(Document& document);
  void Trace(Visitor*);

  void InvalidateControlList();
  Vector<String> ToStateVector();

 private:
  Member<Document> document_;
  using FormElementList = HeapVector<Member<ListedElement>, 64>;
  FormElementList form_controls_;
  bool form_controls_dirty_ = true;
};

class CORE_EXPORT FormController final
    : public GarbageCollectedFinalized<FormController> {
 public:
  FormController(Document& document);
  ~FormController();
  void Trace(Visitor*);

  void InvalidateStatefulFormControlList();
  // This should be callled only by Document::formElementsState().
  DocumentState* FormElementsState() const;
  // This should be callled only by Document::setStateForNewFormElements().
  void SetStateForNewFormElements(const Vector<String>&);
  // Returns true if saved state is set to this object and there are entries
  // which are not consumed yet.
  bool HasFormStates() const;
  void WillDeleteForm(HTMLFormElement*);
  void RestoreControlStateFor(ListedElement&);
  void RestoreControlStateIn(HTMLFormElement&);
  // For a upgraded form-associated custom element.
  void RestoreControlStateOnUpgrade(ListedElement&);

  static Vector<String> GetReferencedFilePaths(
      const Vector<String>& state_vector);

 private:
  FormControlState TakeStateForFormElement(const ListedElement&);
  static void FormStatesFromStateVector(const Vector<String>&,
                                        SavedFormStateMap&);

  Member<DocumentState> document_state_;
  SavedFormStateMap saved_form_state_map_;
  Member<FormKeyGenerator> form_key_generator_;
};

}  // namespace blink
#endif
