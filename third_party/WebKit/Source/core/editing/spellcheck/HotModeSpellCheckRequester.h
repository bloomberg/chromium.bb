// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HotModeSpellCheckRequester_h
#define HotModeSpellCheckRequester_h

#include "core/editing/Forward.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class SpellCheckRequester;

// This class is only supposed to be used by IdleSpellCheckCallback in hot mode
// invocation. Not to be confused with SpellCheckRequester.
class HotModeSpellCheckRequester {
  STACK_ALLOCATED();

 public:
  explicit HotModeSpellCheckRequester(SpellCheckRequester&);
  void CheckSpellingAt(const Position&);

 private:
  HeapVector<Member<const Element>> processed_root_editables_;
  Member<SpellCheckRequester> requester_;

  DISALLOW_COPY_AND_ASSIGN(HotModeSpellCheckRequester);
};

}  // namespace blink

#endif
