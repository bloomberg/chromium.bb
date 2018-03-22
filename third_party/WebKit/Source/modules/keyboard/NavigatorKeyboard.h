// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorKeyboard_h
#define NavigatorKeyboard_h

#include "core/CoreExport.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Keyboard;

// Navigator supplement which exposes keyboard related functionality.
class NavigatorKeyboard final : public GarbageCollected<NavigatorKeyboard>,
                                public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorKeyboard);
  WTF_MAKE_NONCOPYABLE(NavigatorKeyboard);

 public:
  static const char kSupplementName[];
  static Keyboard* keyboard(Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorKeyboard(Navigator&);

  Member<Keyboard> keyboard_;
};

}  // namespace blink

#endif  // NavigatorKeyboard_h
