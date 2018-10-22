/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights
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

#include "third_party/blink/renderer/core/events/registered_event_listener.h"

#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"

namespace blink {

RegisteredEventListener::RegisteredEventListener()
    : use_capture_(false),
      passive_(false),
      once_(false),
      blocked_event_warning_emitted_(false),
      passive_forced_for_document_target_(false),
      passive_specified_(false) {}

RegisteredEventListener::RegisteredEventListener(
    EventListener* listener,
    const AddEventListenerOptionsResolved& options)
    : callback_(listener),
      use_capture_(options.capture()),
      passive_(options.passive()),
      once_(options.once()),
      blocked_event_warning_emitted_(false),
      passive_forced_for_document_target_(
          options.PassiveForcedForDocumentTarget()),
      passive_specified_(options.PassiveSpecified()) {}

RegisteredEventListener& RegisteredEventListener::operator=(
    const RegisteredEventListener& that) = default;

void RegisteredEventListener::Trace(Visitor* visitor) {
  visitor->Trace(callback_);
}

AddEventListenerOptionsResolved RegisteredEventListener::Options() const {
  AddEventListenerOptionsResolved result;
  result.setCapture(use_capture_);
  result.setPassive(passive_);
  result.SetPassiveForcedForDocumentTarget(passive_forced_for_document_target_);
  result.setOnce(once_);
  result.SetPassiveSpecified(passive_specified_);
  return result;
}

void RegisteredEventListener::SetCallback(EventListener* listener) {
  callback_ = listener;
}

bool RegisteredEventListener::Matches(
    const EventListener* listener,
    const EventListenerOptions& options) const {
  // Equality is soley based on the listener and useCapture flags.
  DCHECK(callback_);
  DCHECK(listener);
  return *callback_ == *listener &&
         static_cast<bool>(use_capture_) == options.capture();
}

bool RegisteredEventListener::operator==(
    const RegisteredEventListener& other) const {
  // Equality is soley based on the listener and useCapture flags.
  DCHECK(callback_);
  DCHECK(other.callback_);
  return *callback_ == *other.callback_ && use_capture_ == other.use_capture_;
}

}  // namespace blink
