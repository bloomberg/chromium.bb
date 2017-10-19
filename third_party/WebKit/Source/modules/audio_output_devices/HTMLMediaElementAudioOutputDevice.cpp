// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"

#include <memory>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "modules/audio_output_devices/SetSinkIdCallbacks.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

namespace {

class SetSinkIdResolver : public ScriptPromiseResolver {
  WTF_MAKE_NONCOPYABLE(SetSinkIdResolver);

 public:
  static SetSinkIdResolver* Create(ScriptState*,
                                   HTMLMediaElement&,
                                   const String& sink_id);
  ~SetSinkIdResolver() override = default;
  void StartAsync();

  virtual void Trace(blink::Visitor*);

 private:
  SetSinkIdResolver(ScriptState*, HTMLMediaElement&, const String& sink_id);
  void TimerFired(TimerBase*);

  Member<HTMLMediaElement> element_;
  String sink_id_;
  TaskRunnerTimer<SetSinkIdResolver> timer_;
};

SetSinkIdResolver* SetSinkIdResolver::Create(ScriptState* script_state,
                                             HTMLMediaElement& element,
                                             const String& sink_id) {
  SetSinkIdResolver* resolver =
      new SetSinkIdResolver(script_state, element, sink_id);
  resolver->SuspendIfNeeded();
  resolver->KeepAliveWhilePending();
  return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(ScriptState* script_state,
                                     HTMLMediaElement& element,
                                     const String& sink_id)
    : ScriptPromiseResolver(script_state),
      element_(element),
      sink_id_(sink_id),
      timer_(TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state),
             this,
             &SetSinkIdResolver::TimerFired) {}

void SetSinkIdResolver::StartAsync() {
  timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void SetSinkIdResolver::TimerFired(TimerBase* timer) {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  DCHECK(context->IsDocument());
  std::unique_ptr<SetSinkIdCallbacks> callbacks =
      WTF::WrapUnique(new SetSinkIdCallbacks(this, *element_, sink_id_));
  WebMediaPlayer* web_media_player = element_->GetWebMediaPlayer();
  if (web_media_player) {
    // Using release() to transfer ownership because |webMediaPlayer| is a
    // platform object that takes raw pointers.
    web_media_player->SetSinkId(sink_id_,
                                WebSecurityOrigin(context->GetSecurityOrigin()),
                                callbacks.release());
  } else {
    if (AudioOutputDeviceClient* client =
            AudioOutputDeviceClient::From(context)) {
      client->CheckIfAudioSinkExistsAndIsAuthorized(context, sink_id_,
                                                    std::move(callbacks));
    } else {
      // The context has been detached. Impossible to get a security origin to
      // check.
      DCHECK(context->IsContextDestroyed());
      Reject(DOMException::Create(
          kSecurityError,
          "Impossible to authorize device for detached context"));
    }
  }
}

void SetSinkIdResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  ScriptPromiseResolver::Trace(visitor);
}

}  // namespace

HTMLMediaElementAudioOutputDevice::HTMLMediaElementAudioOutputDevice()
    : sink_id_("") {}

String HTMLMediaElementAudioOutputDevice::sinkId(HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(element);
  return aod_element.sink_id_;
}

void HTMLMediaElementAudioOutputDevice::setSinkId(const String& sink_id) {
  sink_id_ = sink_id;
}

ScriptPromise HTMLMediaElementAudioOutputDevice::setSinkId(
    ScriptState* script_state,
    HTMLMediaElement& element,
    const String& sink_id) {
  SetSinkIdResolver* resolver =
      SetSinkIdResolver::Create(script_state, element, sink_id);
  ScriptPromise promise = resolver->Promise();
  if (sink_id == HTMLMediaElementAudioOutputDevice::sinkId(element))
    resolver->Resolve();
  else
    resolver->StartAsync();

  return promise;
}

const char* HTMLMediaElementAudioOutputDevice::SupplementName() {
  return "HTMLMediaElementAudioOutputDevice";
}

HTMLMediaElementAudioOutputDevice& HTMLMediaElementAudioOutputDevice::From(
    HTMLMediaElement& element) {
  HTMLMediaElementAudioOutputDevice* supplement =
      static_cast<HTMLMediaElementAudioOutputDevice*>(
          Supplement<HTMLMediaElement>::From(element, SupplementName()));
  if (!supplement) {
    supplement = new HTMLMediaElementAudioOutputDevice();
    ProvideTo(element, SupplementName(), supplement);
  }
  return *supplement;
}

void HTMLMediaElementAudioOutputDevice::Trace(blink::Visitor* visitor) {
  Supplement<HTMLMediaElement>::Trace(visitor);
}

}  // namespace blink
