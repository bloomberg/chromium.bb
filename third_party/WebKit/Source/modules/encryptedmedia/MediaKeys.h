/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaKeys_h
#define MediaKeys_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayPiece.h"
#include "platform/Timer.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class HTMLMediaElement;
class MediaKeySession;
class ScriptState;
class WebContentDecryptionModule;

// References are held by JS and HTMLMediaElement.
// The WebContentDecryptionModule has the same lifetime as this object.
class MediaKeys : public GarbageCollectedFinalized<MediaKeys>,
                  public ActiveScriptWrappable<MediaKeys>,
                  public ContextLifecycleObserver,
                  public ScriptWrappable {
  USING_GARBAGE_COLLECTED_MIXIN(MediaKeys);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaKeys* Create(
      ExecutionContext*,
      const WebVector<WebEncryptedMediaSessionType>& supported_session_types,
      std::unique_ptr<WebContentDecryptionModule>);
  virtual ~MediaKeys();

  MediaKeySession* createSession(ScriptState*,
                                 const String& session_type_string,
                                 ExceptionState&);

  ScriptPromise setServerCertificate(ScriptState*,
                                     const DOMArrayPiece& server_certificate);

  // Indicates that the provided HTMLMediaElement wants to use this object.
  // Returns true if no other HTMLMediaElement currently references this
  // object, false otherwise. If true, will take a weak reference to
  // HTMLMediaElement and expects the reservation to be accepted/cancelled
  // later.
  bool ReserveForMediaElement(HTMLMediaElement*);
  // Indicates that SetMediaKeys completed successfully.
  void AcceptReservation();
  // Indicates that SetMediaKeys failed, so HTMLMediaElement did not
  // successfully link to this object.
  void CancelReservation();

  // The previously reserved and accepted HTMLMediaElement is no longer
  // using this object.
  void ClearMediaElement();

  WebContentDecryptionModule* ContentDecryptionModule();

  DECLARE_VIRTUAL_TRACE();

  // ContextLifecycleObserver implementation.
  // FIXME: This class could derive from ContextLifecycleObserver
  // again (http://crbug.com/483722).
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

 private:
  MediaKeys(
      ExecutionContext*,
      const WebVector<WebEncryptedMediaSessionType>& supported_session_types,
      std::unique_ptr<WebContentDecryptionModule>);
  class PendingAction;

  bool SessionTypeSupported(WebEncryptedMediaSessionType);
  void TimerFired(TimerBase*);

  const WebVector<WebEncryptedMediaSessionType> supported_session_types_;
  std::unique_ptr<WebContentDecryptionModule> cdm_;

  // Keep track of the HTMLMediaElement that references this object. Keeping
  // a WeakMember so that HTMLMediaElement's lifetime isn't dependent on
  // this object.
  // Note that the referenced HTMLMediaElement must be destroyed
  // before this object. This is due to WebMediaPlayerImpl (owned by
  // HTMLMediaElement) possibly having a pointer to Decryptor created
  // by WebContentDecryptionModuleImpl (owned by this object).
  WeakMember<HTMLMediaElement> media_element_;

  // Keep track of whether this object has been reserved by HTMLMediaElement
  // (i.e. a setMediaKeys operation is in progress). Destruction of this
  // object will be prevented until the setMediaKeys() completes.
  bool reserved_for_media_element_;

  HeapDeque<Member<PendingAction>> pending_actions_;
  TaskRunnerTimer<MediaKeys> timer_;
};

}  // namespace blink

#endif  // MediaKeys_h
