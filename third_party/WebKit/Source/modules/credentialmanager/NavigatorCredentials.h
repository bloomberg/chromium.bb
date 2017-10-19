// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorCredentials_h
#define NavigatorCredentials_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CredentialsContainer;
class Navigator;

class NavigatorCredentials final
    : public GarbageCollected<NavigatorCredentials>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorCredentials);

 public:
  static NavigatorCredentials& From(Navigator&);
  // NavigatorCredentials.idl
  static CredentialsContainer* credentials(Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorCredentials(Navigator&);
  CredentialsContainer* credentials();

  static const char* SupplementName();

  Member<CredentialsContainer> credentials_container_;
};

}  // namespace blink

#endif  // NavigatorCredentials_h
