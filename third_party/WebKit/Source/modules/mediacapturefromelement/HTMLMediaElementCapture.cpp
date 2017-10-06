// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/HTMLMediaElementCapture.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/events/EventListener.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/track/AudioTrackList.h"
#include "core/html/track/VideoTrackList.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamRegistry.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

namespace {

// Class to register to the events of |m_mediaElement|, acting accordingly on
// the tracks of |m_mediaStream|.
class MediaElementEventListener final : public EventListener {
  WTF_MAKE_NONCOPYABLE(MediaElementEventListener);

 public:
  MediaElementEventListener(HTMLMediaElement*, MediaStream*);
  void UpdateSources(ExecutionContext*);

  DECLARE_VIRTUAL_TRACE();

 private:
  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  Member<HTMLMediaElement> media_element_;
  Member<MediaStream> media_stream_;
  HeapHashSet<WeakMember<MediaStreamSource>> sources_;
};

MediaElementEventListener::MediaElementEventListener(HTMLMediaElement* element,
                                                     MediaStream* stream)
    : EventListener(kCPPEventListenerType),
      media_element_(element),
      media_stream_(stream) {
  UpdateSources(element->GetExecutionContext());
}

void MediaElementEventListener::handleEvent(ExecutionContext* context,
                                            Event* event) {
  DVLOG(2) << __func__ << " " << event->type();
  DCHECK(media_stream_);

  if (event->type() == EventTypeNames::ended) {
    const MediaStreamTrackVector tracks = media_stream_->getTracks();
    for (const auto& track : tracks) {
      track->stopTrack(ASSERT_NO_EXCEPTION);
      media_stream_->RemoveTrackByComponent(track->Component());
    }

    media_stream_->StreamEnded();
    return;
  }
  if (event->type() != EventTypeNames::loadedmetadata)
    return;

  // If |media_element_| is a MediaStream, clone the new tracks.
  if (media_element_->GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    const MediaStreamTrackVector tracks = media_stream_->getTracks();
    for (const auto& track : tracks) {
      track->stopTrack(ASSERT_NO_EXCEPTION);
      media_stream_->RemoveTrackByComponent(track->Component());
    }
    MediaStreamDescriptor* const descriptor =
        media_element_->currentSrc().IsEmpty()
            ? media_element_->GetSrcObject()
            : MediaStreamRegistry::Registry().LookupMediaStreamDescriptor(
                  media_element_->currentSrc().GetString());
    DCHECK(descriptor);
    for (size_t i = 0; i < descriptor->NumberOfAudioComponents(); i++)
      media_stream_->AddTrackByComponent(descriptor->AudioComponent(i));
    for (size_t i = 0; i < descriptor->NumberOfVideoComponents(); i++)
      media_stream_->AddTrackByComponent(descriptor->VideoComponent(i));
    UpdateSources(context);
    return;
  }

  WebMediaStream web_stream;
  web_stream.Initialize(WebVector<WebMediaStreamTrack>(),
                        WebVector<WebMediaStreamTrack>());

  if (media_element_->HasVideo()) {
    Platform::Current()->CreateHTMLVideoElementCapturer(
        &web_stream, media_element_->GetWebMediaPlayer());
  }
  if (media_element_->HasAudio()) {
    Platform::Current()->CreateHTMLAudioElementCapturer(
        &web_stream, media_element_->GetWebMediaPlayer());
  }

  WebVector<WebMediaStreamTrack> video_tracks;
  web_stream.VideoTracks(video_tracks);
  for (const auto& track : video_tracks)
    media_stream_->AddTrackByComponent(track);

  WebVector<WebMediaStreamTrack> audio_tracks;
  web_stream.AudioTracks(audio_tracks);
  for (const auto& track : audio_tracks)
    media_stream_->AddTrackByComponent(track);

  DVLOG(2) << "#videotracks: " << video_tracks.size()
           << " #audiotracks: " << audio_tracks.size();

  UpdateSources(context);
}

void MediaElementEventListener::UpdateSources(ExecutionContext* context) {
  for (auto track : media_stream_->getTracks())
    sources_.insert(track->Component()->Source());

  if (!media_element_->IsMediaDataCORSSameOrigin(
          context->GetSecurityOrigin())) {
    for (auto source : sources_)
      MediaStreamCenter::Instance().DidStopMediaStreamSource(source);
  }
}

DEFINE_TRACE(MediaElementEventListener) {
  visitor->Trace(media_element_);
  visitor->Trace(media_stream_);
  visitor->Trace(sources_);
  EventListener::Trace(visitor);
}

}  // anonymous namespace

// static
MediaStream* HTMLMediaElementCapture::captureStream(
    HTMLMediaElement& element,
    ExceptionState& exception_state) {
  // Avoid capturing from EME-protected Media Elements.
  if (HTMLMediaElementEncryptedMedia::mediaKeys(element)) {
    // This exception is not defined in the spec, see
    // https://github.com/w3c/mediacapture-fromelement/issues/20.
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "Stream capture not supported with EME");
    return nullptr;
  }

  if (!(element.currentSrc().IsNull() && !element.GetSrcObject()) &&
      !element.IsMediaDataCORSSameOrigin(
          element.GetExecutionContext()->GetSecurityOrigin())) {
    exception_state.ThrowSecurityError(
        "Cannot capture from element with cross-origin data");
    return nullptr;
  }

  WebMediaStream web_stream;
  web_stream.Initialize(WebVector<WebMediaStreamTrack>(),
                        WebVector<WebMediaStreamTrack>());

  // Create() duplicates the MediaStreamTracks inside |webStream|.
  MediaStream* stream =
      MediaStream::Create(element.GetExecutionContext(), web_stream);

  MediaElementEventListener* listener =
      new MediaElementEventListener(&element, stream);
  element.addEventListener(EventTypeNames::loadedmetadata, listener, false);
  element.addEventListener(EventTypeNames::ended, listener, false);

  // If |element| is actually playing a MediaStream, just clone it.
  if (element.GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    MediaStreamDescriptor* const descriptor =
        element.currentSrc().IsNull()
            ? element.GetSrcObject()
            : MediaStreamRegistry::Registry().LookupMediaStreamDescriptor(
                  element.currentSrc().GetString());
    DCHECK(descriptor);
    return MediaStream::Create(element.GetExecutionContext(), descriptor);
  }

  if (element.HasVideo()) {
    Platform::Current()->CreateHTMLVideoElementCapturer(
        &web_stream, element.GetWebMediaPlayer());
  }
  if (element.HasAudio()) {
    Platform::Current()->CreateHTMLAudioElementCapturer(
        &web_stream, element.GetWebMediaPlayer());
  }
  listener->UpdateSources(element.GetExecutionContext());

  // If element.currentSrc().isNull() then |stream| will have no tracks, those
  // will be added eventually afterwards via MediaElementEventListener.
  return stream;
}

}  // namespace blink
