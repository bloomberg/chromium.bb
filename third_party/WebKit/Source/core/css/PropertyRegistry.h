// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistry_h
#define PropertyRegistry_h

#include "core/css/PropertyRegistration.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class PropertyRegistry : public GarbageCollected<PropertyRegistry> {
 public:
  static PropertyRegistry* Create() { return new PropertyRegistry(); }

  void RegisterProperty(const AtomicString&, PropertyRegistration&);
  const PropertyRegistration* Registration(const AtomicString&) const;
  size_t RegistrationCount() const { return registrations_.size(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(registrations_); }

 private:
  HeapHashMap<AtomicString, Member<PropertyRegistration>> registrations_;
};

}  // namespace blink

#endif  // PropertyRegistry_h
