// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static const char kContentHintStringNone[] = "";
static const char kContentHintStringAudioSpeech[] = "speech";
static const char kContentHintStringAudioMusic[] = "music";
static const char kContentHintStringVideoMotion[] = "motion";
static const char kContentHintStringVideoDetail[] = "detail";
static const char kContentHintStringVideoText[] = "text";

class AudioSourceProvider;
class ImageCapture;
class MediaTrackCapabilities;
class MediaTrackConstraints;
class MediaStream;
class MediaTrackSettings;
class ScriptState;

struct TransferredValues {
  base::UnguessableToken session_id;
  String kind;
  String id;
  String label;
  bool enabled;
  bool muted;
  WebMediaStreamTrack::ContentHintType content_hint;
  MediaStreamSource::ReadyState ready_state;
};

String ContentHintToString(
    const WebMediaStreamTrack::ContentHintType& content_hint);

String ReadyStateToString(const MediaStreamSource::ReadyState& ready_state);

class MODULES_EXPORT MediaStreamTrack
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaStreamTrack> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class MODULES_EXPORT Observer : public GarbageCollectedMixin {
   public:
    virtual ~Observer() = default;
    virtual void TrackChangedState() = 0;
  };

  // Create a MediaStreamTrack instance as a result of a transfer into this
  // context, eg when receiving a postMessage() with an MST in the transfer
  // list.
  // TODO(https://crbug.com/1288839): Implement to recreate MST after transfer
  static MediaStreamTrack* FromTransferredState(ScriptState* script_state,
                                                const TransferredValues& data);

  // MediaStreamTrack.idl
  virtual String kind() const = 0;
  virtual String id() const = 0;
  virtual String label() const = 0;
  virtual bool enabled() const = 0;
  virtual void setEnabled(bool) = 0;
  virtual bool muted() const = 0;
  virtual String ContentHint() const = 0;
  virtual String readyState() const = 0;
  virtual void SetContentHint(const String&) = 0;
  virtual void stopTrack(ExecutionContext*) = 0;
  virtual MediaStreamTrack* clone(ScriptState*) = 0;
  virtual MediaTrackCapabilities* getCapabilities() const = 0;
  virtual MediaTrackConstraints* getConstraints() const = 0;
  virtual MediaTrackSettings* getSettings() const = 0;
  virtual CaptureHandle* getCaptureHandle() const = 0;
  virtual ScriptPromise applyConstraints(ScriptState*,
                                         const MediaTrackConstraints*) = 0;

  virtual void applyConstraints(ScriptPromiseResolver*,
                                const MediaTrackConstraints*) = 0;
  virtual void SetConstraints(const MediaConstraints&) = 0;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(mute, kMute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute, kUnmute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(capturehandlechange, kCapturehandlechange)

  virtual MediaStreamSource::ReadyState GetReadyState() = 0;

  virtual MediaStreamComponent* Component() const = 0;
  virtual bool Ended() const = 0;

  virtual void RegisterMediaStream(MediaStream*) = 0;
  virtual void UnregisterMediaStream(MediaStream*) = 0;

  // EventTarget
  const AtomicString& InterfaceName() const override = 0;
  ExecutionContext* GetExecutionContext() const override = 0;
  void AddedEventListener(const AtomicString&,
                          RegisteredEventListener&) override = 0;

  // ScriptWrappable
  bool HasPendingActivity() const override = 0;

  virtual std::unique_ptr<AudioSourceProvider> CreateWebAudioSource(
      int context_sample_rate) = 0;

  virtual ImageCapture* GetImageCapture() = 0;
  virtual absl::optional<base::UnguessableToken> serializable_session_id()
      const = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Only relevant for focusable streams (FocusableMediaStreamTrack).
  // When called on one of these, it signals that Conditional Focus
  // no longer applies - the browser will now decide whether
  // the captured display surface should be captured. Later calls to
  // FocusableMediaStreamTrack.focus() will now raise an exception.
  virtual void CloseFocusWindowOfOpportunity() = 0;
#endif

  virtual void AddObserver(Observer*) = 0;

  void Trace(Visitor* visitor) const override {
    EventTargetWithInlineData::Trace(visitor);
  }
};

typedef HeapVector<Member<MediaStreamTrack>> MediaStreamTrackVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_
