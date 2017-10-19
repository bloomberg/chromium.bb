// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenScreenOrientation_h
#define ScreenScreenOrientation_h

#include "core/frame/Screen.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScreenOrientation;
class Screen;
class ScriptState;

class ScreenScreenOrientation final
    : public GarbageCollected<ScreenScreenOrientation>,
      public Supplement<Screen> {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenScreenOrientation);

 public:
  static ScreenScreenOrientation& From(Screen&);

  static ScreenOrientation* orientation(ScriptState*, Screen&);

  virtual void Trace(blink::Visitor*);

 private:
  static const char* SupplementName();

  Member<ScreenOrientation> orientation_;
};

}  // namespace blink

#endif  // ScreenScreenOrientation_h
