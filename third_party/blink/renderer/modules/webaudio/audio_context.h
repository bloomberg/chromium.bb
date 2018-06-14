// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AudioContextOptions;
class AudioTimestamp;
class Document;
class ExceptionState;
class ScriptState;
class WebAudioLatencyHint;

// This is an BaseAudioContext which actually plays sound, unlike an
// OfflineAudioContext which renders sound into a buffer.
class MODULES_EXPORT AudioContext : public BaseAudioContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioContext* Create(Document&,
                              const AudioContextOptions&,
                              ExceptionState&);

  ~AudioContext() override;
  void Trace(blink::Visitor*) override;

  ScriptPromise closeContext(ScriptState*);
  bool IsContextClosed() const final;

  ScriptPromise suspendContext(ScriptState*) final;
  ScriptPromise resumeContext(ScriptState*) final;

  bool HasRealtimeConstraint() final { return true; }

  void getOutputTimestamp(ScriptState*, AudioTimestamp&);
  double baseLatency() const;

  // For metrics purpose, records when start() is called on a
  // AudioScheduledSourceHandler or a AudioBufferSourceHandler without a user
  // gesture while the AudioContext requires a user gesture.
  void MaybeRecordStartAttempt() final;

 protected:
  AudioContext(Document&, const WebAudioLatencyHint&);
  void Uninitialize() final;

  void DidClose() final;

 private:
  friend class AudioContextAutoplayTest;

  // Do not change the order of this enum, it is used for metrics.
  enum AutoplayStatus {
    // The AudioContext failed to activate because of user gesture requirements.
    kAutoplayStatusFailed = 0,
    // Same as AutoplayStatusFailed but start() on a node was called with a user
    // gesture.
    kAutoplayStatusFailedWithStart = 1,
    // The AudioContext had user gesture requirements and was able to activate
    // with a user gesture.
    kAutoplayStatusSucceeded = 2,

    // Keep at the end.
    kAutoplayStatusCount
  };

  // Returns the AutoplayPolicy currently applying to this instance.
  AutoplayPolicy::Type GetAutoplayPolicy() const;

  // Returns whether the autoplay requirements are fulfilled.
  bool AreAutoplayRequirementsFulfilled() const;

  // If any, unlock user gesture requirements if a user gesture is being
  // processed.
  void MaybeUnlockUserGesture();

  // Returns whether the AudioContext is allowed to start rendering.
  bool IsAllowedToStart() const;

  // Record the current autoplay status and clear it.
  void RecordAutoplayStatus();

  void StopRendering();

  unsigned context_id_;
  Member<ScriptPromiseResolver> close_resolver_;

  // Whether a user gesture is required to start this AudioContext.
  bool user_gesture_required_;
  base::Optional<AutoplayStatus> autoplay_status_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
