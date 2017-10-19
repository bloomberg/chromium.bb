// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementAudioOutputDevice_h
#define HTMLMediaElementAudioOutputDevice_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/media/HTMLMediaElement.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;
class ScriptState;

class MODULES_EXPORT HTMLMediaElementAudioOutputDevice final
    : public GarbageCollectedFinalized<HTMLMediaElementAudioOutputDevice>,
      public Supplement<HTMLMediaElement> {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementAudioOutputDevice);

 public:
  virtual void Trace(blink::Visitor*);
  static String sinkId(HTMLMediaElement&);
  static ScriptPromise setSinkId(ScriptState*,
                                 HTMLMediaElement&,
                                 const String& sink_id);
  static HTMLMediaElementAudioOutputDevice& From(HTMLMediaElement&);
  void setSinkId(const String&);

 private:
  HTMLMediaElementAudioOutputDevice();
  static const char* SupplementName();

  String sink_id_;
};

}  // namespace blink

#endif
