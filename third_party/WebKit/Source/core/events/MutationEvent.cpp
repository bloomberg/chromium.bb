/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/events/MutationEvent.h"

namespace blink {

MutationEvent::MutationEvent() : attr_change_(0) {}

MutationEvent::MutationEvent(const AtomicString& type,
                             bool can_bubble,
                             bool cancelable,
                             Node* related_node,
                             const String& prev_value,
                             const String& new_value,
                             const String& attr_name,
                             unsigned short attr_change)
    : Event(type, can_bubble, cancelable),
      related_node_(related_node),
      prev_value_(prev_value),
      new_value_(new_value),
      attr_name_(attr_name),
      attr_change_(attr_change) {}

MutationEvent::~MutationEvent() {}

void MutationEvent::initMutationEvent(const AtomicString& type,
                                      bool can_bubble,
                                      bool cancelable,
                                      Node* related_node,
                                      const String& prev_value,
                                      const String& new_value,
                                      const String& attr_name,
                                      unsigned short attr_change) {
  if (IsBeingDispatched())
    return;

  initEvent(type, can_bubble, cancelable);

  related_node_ = related_node;
  prev_value_ = prev_value;
  new_value_ = new_value;
  attr_name_ = attr_name;
  attr_change_ = attr_change;
}

const AtomicString& MutationEvent::InterfaceName() const {
  return EventNames::MutationEvent;
}

void MutationEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(related_node_);
  Event::Trace(visitor);
}

}  // namespace blink
