/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef HashChangeEvent_h
#define HashChangeEvent_h

#include "core/dom/events/Event.h"
#include "core/events/HashChangeEventInit.h"

namespace blink {

class HashChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HashChangeEvent* Create() { return new HashChangeEvent; }

  static HashChangeEvent* Create(const String& old_url, const String& new_url) {
    return new HashChangeEvent(old_url, new_url);
  }

  static HashChangeEvent* Create(const AtomicString& type,
                                 const HashChangeEventInit& initializer) {
    return new HashChangeEvent(type, initializer);
  }

  const String& oldURL() const { return old_url_; }
  const String& newURL() const { return new_url_; }

  const AtomicString& InterfaceName() const override {
    return EventNames::HashChangeEvent;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::Trace(visitor); }

 private:
  HashChangeEvent() {}

  HashChangeEvent(const String& old_url, const String& new_url)
      : Event(EventTypeNames::hashchange, false, false),
        old_url_(old_url),
        new_url_(new_url) {}

  HashChangeEvent(const AtomicString& type,
                  const HashChangeEventInit& initializer)
      : Event(type, initializer) {
    if (initializer.hasOldURL())
      old_url_ = initializer.oldURL();
    if (initializer.hasNewURL())
      new_url_ = initializer.newURL();
  }

  String old_url_;
  String new_url_;
};

}  // namespace blink

#endif  // HashChangeEvent_h
