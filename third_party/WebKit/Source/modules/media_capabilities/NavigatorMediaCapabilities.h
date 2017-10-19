// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorMediaCapabilities_h
#define NavigatorMediaCapabilities_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace blink {

class MediaCapabilities;
class Navigator;

// Provides MediaCapabilities as a supplement of Navigator as an attribute.
class NavigatorMediaCapabilities final
    : public GarbageCollected<NavigatorMediaCapabilities>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorMediaCapabilities);

 public:
  static MediaCapabilities* mediaCapabilities(Navigator&);

  void Trace(blink::Visitor*);

 private:
  explicit NavigatorMediaCapabilities(Navigator&);

  static NavigatorMediaCapabilities& From(Navigator&);
  static const char* SupplementName();

  // The MediaCapabilities instance of this Navigator.
  Member<MediaCapabilities> capabilities_;
};

}  // namespace blink

#endif  // NavigatorMediaCapabilities_h
