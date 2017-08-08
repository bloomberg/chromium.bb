// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlayback_h
#define RemotePlayback_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "public/platform/modules/presentation/WebPresentationConnection.h"
#include "public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackAvailability.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"

namespace blink {

class AvailabilityCallbackWrapper;
class HTMLMediaElement;
class RemotePlaybackAvailabilityCallback;
class ScriptPromiseResolver;
class ScriptState;
class WebPresentationConnectionProxy;
struct WebPresentationError;
struct WebPresentationInfo;

class MODULES_EXPORT RemotePlayback final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<RemotePlayback>,
      NON_EXPORTED_BASE(public WebRemotePlaybackClient),
      public WebPresentationAvailabilityObserver,
      public NON_EXPORTED_BASE(WebPresentationConnection) {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RemotePlayback);

 public:
  // Result of WatchAvailabilityInternal that means availability is not
  // supported.
  static const int kWatchAvailabilityNotSupported = -1;

  static RemotePlayback* Create(HTMLMediaElement&);

  // Notifies this object that disableRemotePlayback attribute was set on the
  // corresponding media element.
  void RemotePlaybackDisabled();

  // EventTarget implementation.
  const WTF::AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Starts notifying the page about the changes to the remote playback devices
  // availability via the provided callback. May start the monitoring of remote
  // playback devices if it isn't running yet.
  ScriptPromise watchAvailability(ScriptState*,
                                  RemotePlaybackAvailabilityCallback*);

  // Cancels updating the page via the callback specified by its id.
  ScriptPromise cancelWatchAvailability(ScriptState*, int id);

  // Cancels all the callbacks watching remote playback availability changes
  // registered with this element.
  ScriptPromise cancelWatchAvailability(ScriptState*);

  // Shows the UI allowing user to change the remote playback state of the media
  // element (by picking a remote playback device from the list, for example).
  ScriptPromise prompt(ScriptState*);

  String state() const;

  // The implementation of prompt(). Used by the native remote playback button.
  void PromptInternal();

  // The implementation of watchAvailability() and cancelWatchAvailability().
  // Can return kWatchAvailabilityNotSupported to indicate the availability
  // monitoring is disabled. RemotePlaybackAvailable() will return true then.
  int WatchAvailabilityInternal(AvailabilityCallbackWrapper*);
  bool CancelWatchAvailabilityInternal(int id);

  WebRemotePlaybackState GetState() const { return state_; }

  // Called by RemotePlaybackConnectionCallbacks.
  void OnConnectionSuccess(const WebPresentationInfo&);
  void OnConnectionError(const WebPresentationError&);

  // WebPresentationAvailabilityObserver implementation.
  void AvailabilityChanged(mojom::ScreenAvailability) override;
  const WebVector<WebURL>& Urls() const override;

  // WebPresentationConnection implementation.
  void BindProxy(std::unique_ptr<WebPresentationConnectionProxy>) override;
  void DidReceiveTextMessage(const WebString& message) override;
  void DidReceiveBinaryMessage(const uint8_t* data, size_t length) override;
  void DidChangeState(WebPresentationConnectionState) override;
  void DidClose() override;

  // WebRemotePlaybackClient implementation.
  void StateChanged(WebRemotePlaybackState) override;
  void AvailabilityChanged(WebRemotePlaybackAvailability) override;
  void PromptCancelled() override;
  bool RemotePlaybackAvailable() const override;
  void SourceChanged(const WebURL&, bool is_source_supported) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connecting);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  friend class V8RemotePlayback;
  friend class RemotePlaybackTest;
  friend class MediaControlsImplTest;

  explicit RemotePlayback(HTMLMediaElement&);

  // Calls the specified availability callback with the current availability.
  // Need a void() method to post it as a task.
  void NotifyInitialAvailability(int callback_id);

  // Starts listening for remote playback device availability if there're both
  // registered availability callbacks and a valid source set. May be called
  // more than once in a row.
  void MaybeStartListeningForAvailability();

  // Stops listening for remote playback device availability (unconditionally).
  // May be called more than once in a row.
  void StopListeningForAvailability();

  WebRemotePlaybackState state_;
  WebRemotePlaybackAvailability availability_;
  HeapHashMap<int, TraceWrapperMember<AvailabilityCallbackWrapper>>
      availability_callbacks_;
  Member<HTMLMediaElement> media_element_;
  Member<ScriptPromiseResolver> prompt_promise_resolver_;
  WebVector<WebURL> availability_urls_;
  bool is_listening_;

  // WebPresentationConnection implementation.
  String presentation_id_;
  KURL presentation_url_;
  std::unique_ptr<WebPresentationConnectionProxy> connection_proxy_;
};

}  // namespace blink

#endif  // RemotePlayback_h
