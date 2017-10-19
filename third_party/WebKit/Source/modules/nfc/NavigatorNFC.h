// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorNFC_h
#define NavigatorNFC_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class NFC;
class Navigator;

class NavigatorNFC final : public GarbageCollected<NavigatorNFC>,
                           public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorNFC);

 public:
  // Gets, or creates, NavigatorNFC supplement on Navigator.
  static NavigatorNFC& From(Navigator&);

  static NFC* nfc(Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorNFC(Navigator&);
  static const char* SupplementName();

  Member<NFC> nfc_;
};

}  // namespace blink

#endif  // NavigatorNFC_h
