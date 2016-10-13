// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlayback_h
#define RemotePlayback_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "wtf/Compiler.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class HTMLMediaElement;
class LocalFrame;
class RemotePlaybackAvailability;
class ScriptPromiseResolver;

class MODULES_EXPORT RemotePlayback final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable,
      WTF_NON_EXPORTED_BASE(private WebRemotePlaybackClient) {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RemotePlayback);

 public:
  static RemotePlayback* create(HTMLMediaElement&);

  // EventTarget implementation.
  const WTF::AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  ScriptPromise getAvailability(ScriptState*);
  ScriptPromise prompt(ScriptState*);

  String state() const;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connecting);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class RemotePlaybackTest;

  explicit RemotePlayback(HTMLMediaElement&);

  // WebRemotePlaybackClient implementation.
  void stateChanged(WebRemotePlaybackState) override;
  void availabilityChanged(bool available) override;
  void promptCancelled() override;

  WebRemotePlaybackState m_state;
  bool m_availability;
  HeapVector<Member<RemotePlaybackAvailability>> m_availabilityObjects;
  Member<HTMLMediaElement> m_mediaElement;
  Member<ScriptPromiseResolver> m_promptPromiseResolver;
};

}  // namespace blink

#endif  // RemotePlayback_h
