/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2012 Apple Inc. All rights
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
 */

#include "core/dom/Attr.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/dom/events/ScopedEventQueue.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

Attr::Attr(Element& element, const QualifiedName& name)
    : Node(&element.GetDocument(), kCreateOther),
      element_(&element),
      name_(name) {}

Attr::Attr(Document& document,
           const QualifiedName& name,
           const AtomicString& standalone_value)
    : Node(&document, kCreateOther),
      name_(name),
      standalone_value_or_attached_local_name_(standalone_value) {}

Attr* Attr::Create(Element& element, const QualifiedName& name) {
  return new Attr(element, name);
}

Attr* Attr::Create(Document& document,
                   const QualifiedName& name,
                   const AtomicString& value) {
  return new Attr(document, name, value);
}

Attr::~Attr() {}

const QualifiedName Attr::GetQualifiedName() const {
  if (element_ && !standalone_value_or_attached_local_name_.IsNull()) {
    // In the unlikely case the Element attribute has a local name
    // that differs by case, construct the qualified name based on
    // it. This is the qualified name that must be used when
    // looking up the attribute on the element.
    return QualifiedName(name_.Prefix(),
                         standalone_value_or_attached_local_name_,
                         name_.NamespaceURI());
  }

  return name_;
}

const AtomicString& Attr::value() const {
  if (element_)
    return element_->getAttribute(GetQualifiedName());
  return standalone_value_or_attached_local_name_;
}

void Attr::setValue(const AtomicString& value) {
  // Element::setAttribute will remove the attribute if value is null.
  DCHECK(!value.IsNull());
  if (element_)
    element_->setAttribute(GetQualifiedName(), value);
  else
    standalone_value_or_attached_local_name_ = value;
}

void Attr::setNodeValue(const String& v) {
  // Attr uses AtomicString type for its value to save memory as there
  // is duplication among Elements' attributes values.
  setValue(v.IsNull() ? g_empty_atom : AtomicString(v));
}

Node* Attr::cloneNode(bool /*deep*/, ExceptionState&) {
  return new Attr(GetDocument(), name_, value());
}

void Attr::DetachFromElementWithValue(const AtomicString& value) {
  DCHECK(element_);
  standalone_value_or_attached_local_name_ = value;
  element_ = nullptr;
}

void Attr::AttachToElement(Element* element,
                           const AtomicString& attached_local_name) {
  DCHECK(!element_);
  element_ = element;
  standalone_value_or_attached_local_name_ = attached_local_name;
}

void Attr::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  Node::Trace(visitor);
}

void Attr::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(element_);
  Node::TraceWrappers(visitor);
}

}  // namespace blink
