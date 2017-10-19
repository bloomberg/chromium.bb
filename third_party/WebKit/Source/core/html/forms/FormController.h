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

#ifndef FormController_h
#define FormController_h

#include <memory>
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class FormKeyGenerator;
class HTMLFormControlElementWithState;
class HTMLFormElement;
class SavedFormState;

class FormControlState {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FormControlState() : type_(kTypeSkip) {}
  explicit FormControlState(const String& value) : type_(kTypeRestore) {
    values_.push_back(value);
  }
  static FormControlState Deserialize(const Vector<String>& state_vector,
                                      size_t& index);
  FormControlState(const FormControlState& another)
      : type_(another.type_), values_(another.values_) {}
  FormControlState& operator=(const FormControlState&);

  bool IsFailure() const { return type_ == kTypeFailure; }
  size_t ValueSize() const { return values_.size(); }
  const String& operator[](size_t i) const { return values_[i]; }
  void Append(const String&);
  void SerializeTo(Vector<String>& state_vector) const;

 private:
  enum Type { kTypeSkip, kTypeRestore, kTypeFailure };
  explicit FormControlState(Type type) : type_(type) {}

  Type type_;
  Vector<String> values_;
};

inline FormControlState& FormControlState::operator=(
    const FormControlState& another) {
  type_ = another.type_;
  values_ = another.values_;
  return *this;
}

inline void FormControlState::Append(const String& value) {
  type_ = kTypeRestore;
  values_.push_back(value);
}

using SavedFormStateMap =
    HashMap<AtomicString, std::unique_ptr<SavedFormState>>;

class DocumentState final : public GarbageCollected<DocumentState> {
 public:
  static DocumentState* Create();
  void Trace(blink::Visitor*);

  void AddControl(HTMLFormControlElementWithState*);
  void RemoveControl(HTMLFormControlElementWithState*);
  Vector<String> ToStateVector();

 private:
  using FormElementListHashSet =
      HeapListHashSet<Member<HTMLFormControlElementWithState>, 64>;
  FormElementListHashSet form_controls_;
};

class FormController final : public GarbageCollectedFinalized<FormController> {
 public:
  static FormController* Create() { return new FormController; }
  ~FormController();
  void Trace(blink::Visitor*);

  void RegisterStatefulFormControl(HTMLFormControlElementWithState&);
  void UnregisterStatefulFormControl(HTMLFormControlElementWithState&);
  // This should be callled only by Document::formElementsState().
  DocumentState* FormElementsState() const;
  // This should be callled only by Document::setStateForNewFormElements().
  void SetStateForNewFormElements(const Vector<String>&);
  // Returns true if saved state is set to this object and there are entries
  // which are not consumed yet.
  bool HasFormStates() const;
  void WillDeleteForm(HTMLFormElement*);
  void RestoreControlStateFor(HTMLFormControlElementWithState&);
  void RestoreControlStateIn(HTMLFormElement&);

  static Vector<String> GetReferencedFilePaths(
      const Vector<String>& state_vector);

 private:
  FormController();
  FormControlState TakeStateForFormElement(
      const HTMLFormControlElementWithState&);
  static void FormStatesFromStateVector(const Vector<String>&,
                                        SavedFormStateMap&);

  Member<DocumentState> document_state_;
  SavedFormStateMap saved_form_state_map_;
  Member<FormKeyGenerator> form_key_generator_;
};

}  // namespace blink
#endif
