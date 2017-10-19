// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorClipboard_h
#define NavigatorClipboard_h

#include "core/CoreExport.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Clipboard;
class ScriptState;

class NavigatorClipboard final : public GarbageCollected<NavigatorClipboard>,
                                 public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorClipboard);
  WTF_MAKE_NONCOPYABLE(NavigatorClipboard);

 public:
  static Clipboard* clipboard(ScriptState*, Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorClipboard(Navigator&);
  static const char* SupplementName();

  Member<Clipboard> clipboard_;
};

}  // namespace blink

#endif  // NavigatorClipboard.h
