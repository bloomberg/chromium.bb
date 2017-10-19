// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPresentation_h
#define NavigatorPresentation_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class Presentation;

class NavigatorPresentation final
    : public GarbageCollected<NavigatorPresentation>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorPresentation);

 public:
  static NavigatorPresentation& From(Navigator&);
  static Presentation* presentation(Navigator&);

  virtual void Trace(blink::Visitor*);

 private:
  NavigatorPresentation();

  static const char* SupplementName();
  Presentation* presentation();

  Member<Presentation> presentation_;
};

}  // namespace blink

#endif  // NavigatorPresentation_h
