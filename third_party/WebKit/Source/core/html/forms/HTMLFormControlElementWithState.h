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

#ifndef HTMLFormControlElementWithState_h
#define HTMLFormControlElementWithState_h

#include "core/CoreExport.h"
#include "core/html/forms/HTMLFormControlElement.h"
#include "platform/wtf/DoublyLinkedList.h"

namespace blink {

class FormControlState;

class HTMLFormControlElementWithState;
// Cannot use eager tracing as HTMLFormControlElementWithState objects are part
// of a HeapDoublyLinkedList and have pointers to the previous and next elements
// in the list, which can cause very deep stacks in eager tracing.
WILL_NOT_BE_EAGERLY_TRACED_CLASS(HTMLFormControlElementWithState);

class CORE_EXPORT HTMLFormControlElementWithState
    : public HTMLFormControlElement,
      public DoublyLinkedListNode<HTMLFormControlElementWithState> {
 public:
  ~HTMLFormControlElementWithState() override;

  bool CanContainRangeEndPoint() const final { return false; }

  virtual bool ShouldAutocomplete() const;
  virtual bool ShouldSaveAndRestoreFormControlState() const;
  virtual FormControlState SaveFormControlState() const;
  // The specified FormControlState must have at least one string value.
  virtual void RestoreFormControlState(const FormControlState&) {}
  void NotifyFormStateChanged();

  void Trace(Visitor*) override;

 protected:
  HTMLFormControlElementWithState(const QualifiedName& tag_name, Document&);

  void FinishParsingChildren() override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;
  bool IsFormControlElementWithState() const final;

 private:
  // Pointers for DoublyLinkedListNode<HTMLFormControlElementWithState>. This
  // is used for adding an instance to a list of form controls stored in
  // DocumentState. Each instance is only added to its containing document's
  // DocumentState list.
  friend class WTF::DoublyLinkedListNode<HTMLFormControlElementWithState>;
  Member<HTMLFormControlElementWithState> prev_;
  Member<HTMLFormControlElementWithState> next_;
};

DEFINE_TYPE_CASTS(HTMLFormControlElementWithState,
                  ListedElement,
                  control,
                  control->IsFormControlElementWithState(),
                  control.IsFormControlElementWithState());

}  // namespace blink

#endif
