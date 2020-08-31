// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"

#include <utility>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"

namespace blink {

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("MSAT::" + message);
}

}  // namespace

MediaStreamAudioTrack::MediaStreamAudioTrack(bool is_local_track)
    : WebPlatformMediaStreamTrack(is_local_track), is_enabled_(1) {
  SendLogMessage(
      base::StringPrintf("MediaStreamAudioTrack([this=%p] {is_local_track=%s})",
                         this, (is_local_track ? "true" : "false")));
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("~MediaStreamAudioTrack([this=%p])", this));
  Stop();
}

// static
MediaStreamAudioTrack* MediaStreamAudioTrack::From(
    const WebMediaStreamTrack& track) {
  if (track.IsNull() ||
      track.Source().GetType() != WebMediaStreamSource::kTypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioTrack*>(track.GetPlatformTrack());
}

void MediaStreamAudioTrack::AddSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("AddSink([this=%p])", this));

  // If the track has already stopped, just notify the sink of this fact without
  // adding it.
  if (stop_callback_.is_null()) {
    sink->OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
    return;
  }

  deliverer_.AddConsumer(sink);
  sink->OnEnabledChanged(!!base::subtle::NoBarrier_Load(&is_enabled_));
}

void MediaStreamAudioTrack::RemoveSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("RemoveSink([this=%p])", this));
  deliverer_.RemoveConsumer(sink);
}

media::AudioParameters MediaStreamAudioTrack::GetOutputFormat() const {
  return deliverer_.GetAudioParameters();
}

void MediaStreamAudioTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("SetEnabled([this=%p] {enabled=%s})", this,
                                    (enabled ? "true" : "false")));

  const bool previously_enabled =
      !!base::subtle::NoBarrier_AtomicExchange(&is_enabled_, enabled ? 1 : 0);
  if (enabled == previously_enabled)
    return;

  Vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnEnabledChanged(enabled);
}

void MediaStreamAudioTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnContentHintChanged(content_hint);
}

void* MediaStreamAudioTrack::GetClassIdentifier() const {
  return nullptr;
}

void MediaStreamAudioTrack::Start(base::OnceClosure stop_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!stop_callback.is_null());
  DCHECK(stop_callback_.is_null());
  SendLogMessage(base::StringPrintf("Start([this=%p])", this));
  stop_callback_ = std::move(stop_callback);
}

void MediaStreamAudioTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("StopAndNotify([this=%p])", this));

  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();

  Vector<WebMediaStreamAudioSink*> sinks_to_end;
  deliverer_.GetConsumerList(&sinks_to_end);
  for (WebMediaStreamAudioSink* sink : sinks_to_end) {
    deliverer_.RemoveConsumer(sink);
    sink->OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
  }

  if (callback)
    std::move(callback).Run();
  weak_factory_.InvalidateWeakPtrs();
}

void MediaStreamAudioTrack::OnSetFormat(const media::AudioParameters& params) {
  SendLogMessage(base::StringPrintf("OnSetFormat([this=%p] {params: [%s]})",
                                    this,
                                    params.AsHumanReadableString().c_str()));
  deliverer_.OnSetFormat(params);
}

void MediaStreamAudioTrack::OnData(const media::AudioBus& audio_bus,
                                   base::TimeTicks reference_time) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamAudioTrack::OnData");

  if (!received_audio_callback_) {
    // Add log message with unique this pointer id to mark the audio track as
    // alive at the first data callback.
    SendLogMessage(base::StringPrintf(
        "OnData([this=%p] => (audio track is alive))", this));
    received_audio_callback_ = true;
  }

  // Note: Using NoBarrier_Load because the timing of when the audio thread sees
  // a changed |is_enabled_| value can be relaxed.
  const bool deliver_data = !!base::subtle::NoBarrier_Load(&is_enabled_);

  if (deliver_data) {
    deliverer_.OnData(audio_bus, reference_time);
  } else {
    // The W3C spec requires silent audio to flow while a track is disabled.
    if (!silent_bus_ || silent_bus_->channels() != audio_bus.channels() ||
        silent_bus_->frames() != audio_bus.frames()) {
      silent_bus_ =
          media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
      silent_bus_->Zero();
    }
    deliverer_.OnData(*silent_bus_, reference_time);
  }
}

}  // namespace blink
