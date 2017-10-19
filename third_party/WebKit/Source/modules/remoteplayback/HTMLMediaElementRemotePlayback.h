// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementRemotePlayback_h
#define HTMLMediaElementRemotePlayback_h

#include "core/html/media/HTMLMediaElement.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;
class QualifiedName;
class RemotePlayback;

// Class used to implement the Remote Playback API. It is a supplement to
// HTMLMediaElement.
class MODULES_EXPORT HTMLMediaElementRemotePlayback final
    : public GarbageCollected<HTMLMediaElementRemotePlayback>,
      public Supplement<HTMLMediaElement> {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementRemotePlayback);

 public:
  static bool FastHasAttribute(const QualifiedName&, const HTMLMediaElement&);
  static void SetBooleanAttribute(const QualifiedName&,
                                  HTMLMediaElement&,
                                  bool);

  static HTMLMediaElementRemotePlayback& From(HTMLMediaElement&);
  static RemotePlayback* remote(HTMLMediaElement&);

  virtual void Trace(blink::Visitor*);

 private:
  static const char* SupplementName();

  Member<RemotePlayback> remote_;
};

}  // namespace blink

#endif  // HTMLMediaElementRemotePlayback_h
