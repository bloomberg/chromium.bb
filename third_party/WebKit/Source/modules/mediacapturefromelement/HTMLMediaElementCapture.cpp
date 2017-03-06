// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/HTMLMediaElementCapture.h"

#include "core/dom/ExceptionCode.h"
#include "core/events/EventListener.h"
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
  MediaElementEventListener(HTMLMediaElement* element, MediaStream* stream)
      : EventListener(CPPEventListenerType),
        m_mediaElement(element),
        m_mediaStream(stream) {}

  DECLARE_VIRTUAL_TRACE();

 private:
  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  Member<HTMLMediaElement> m_mediaElement;
  Member<MediaStream> m_mediaStream;
};

void MediaElementEventListener::handleEvent(ExecutionContext* context,
                                            Event* event) {
  DVLOG(2) << __func__ << " " << event->type();
  DCHECK(m_mediaStream);

  if (event->type() == EventTypeNames::ended) {
    MediaStreamTrackVector tracks = m_mediaStream->getTracks();
    for (const auto& track : tracks) {
      track->stopTrack(ASSERT_NO_EXCEPTION);
      m_mediaStream->removeTrackByComponent(track->component());
    }

    m_mediaStream->streamEnded();
    return;
  }
  if (event->type() != EventTypeNames::loadedmetadata)
    return;

  WebMediaStream webStream;
  webStream.initialize(WebVector<WebMediaStreamTrack>(),
                       WebVector<WebMediaStreamTrack>());

  if (m_mediaElement->hasVideo()) {
    Platform::current()->createHTMLVideoElementCapturer(
        &webStream, m_mediaElement->webMediaPlayer());
  }
  if (m_mediaElement->hasAudio()) {
    Platform::current()->createHTMLAudioElementCapturer(
        &webStream, m_mediaElement->webMediaPlayer());
  }

  WebVector<WebMediaStreamTrack> videoTracks;
  webStream.videoTracks(videoTracks);
  for (const auto& track : videoTracks)
    m_mediaStream->addTrackByComponent(track);

  WebVector<WebMediaStreamTrack> audioTracks;
  webStream.audioTracks(audioTracks);
  for (const auto& track : audioTracks)
    m_mediaStream->addTrackByComponent(track);

  DVLOG(2) << "#videotracks: " << videoTracks.size()
           << " #audiotracks: " << audioTracks.size();
}

DEFINE_TRACE(MediaElementEventListener) {
  visitor->trace(m_mediaElement);
  visitor->trace(m_mediaStream);
  EventListener::trace(visitor);
}

}  // anonymous namespace

// static
MediaStream* HTMLMediaElementCapture::captureStream(
    HTMLMediaElement& element,
    ExceptionState& exceptionState) {
  // Avoid capturing from EME-protected Media Elements.
  if (HTMLMediaElementEncryptedMedia::mediaKeys(element)) {
    // This exception is not defined in the spec, see
    // https://github.com/w3c/mediacapture-fromelement/issues/20.
    exceptionState.throwDOMException(NotSupportedError,
                                     "Stream capture not supported with EME");
    return nullptr;
  }

  WebMediaStream webStream;
  webStream.initialize(WebVector<WebMediaStreamTrack>(),
                       WebVector<WebMediaStreamTrack>());

  // create() duplicates the MediaStreamTracks inside |webStream|.
  MediaStream* stream =
      MediaStream::create(element.getExecutionContext(), webStream);

  EventListener* listener = new MediaElementEventListener(&element, stream);
  element.addEventListener(EventTypeNames::loadedmetadata, listener, false);
  element.addEventListener(EventTypeNames::ended, listener, false);

  // If |element| is actually playing a MediaStream, just clone it.
  if (!element.currentSrc().isNull() &&
      HTMLMediaElement::isMediaStreamURL(element.currentSrc().getString())) {
    return MediaStream::create(
        element.getExecutionContext(),
        MediaStreamRegistry::registry().lookupMediaStreamDescriptor(
            element.currentSrc().getString()));
  }

  if (element.hasVideo()) {
    Platform::current()->createHTMLVideoElementCapturer(
        &webStream, element.webMediaPlayer());
  }
  if (element.hasAudio()) {
    Platform::current()->createHTMLAudioElementCapturer(
        &webStream, element.webMediaPlayer());
  }

  // If element.currentSrc().isNull() then |stream| will have no tracks, those
  // will be added eventually afterwards via MediaElementEventListener.
  return stream;
}

}  // namespace blink
