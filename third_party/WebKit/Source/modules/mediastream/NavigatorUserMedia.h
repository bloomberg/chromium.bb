// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorUserMedia_h
#define NavigatorUserMedia_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class MediaDevices;

class NavigatorUserMedia final : public GarbageCollected<NavigatorUserMedia>,
                                 public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorUserMedia)
 public:
  static MediaDevices* mediaDevices(Navigator&);
  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorUserMedia(Navigator&);
  MediaDevices* GetMediaDevices();
  static const char* SupplementName();
  static NavigatorUserMedia& From(Navigator&);

  Member<MediaDevices> media_devices_;
};

}  // namespace blink

#endif
