// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPermissions_h
#define NavigatorPermissions_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class Permissions;

class NavigatorPermissions final
    : public GarbageCollected<NavigatorPermissions>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorPermissions);

 public:
  static NavigatorPermissions& From(Navigator&);
  static Permissions* permissions(Navigator&);

  virtual void Trace(blink::Visitor*);

 private:
  NavigatorPermissions();

  static const char* SupplementName();

  Member<Permissions> permissions_;
};

}  // namespace blink

#endif  // NavigatorPermissions_h
