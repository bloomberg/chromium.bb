// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpReceiver.h"

#include "platform/bindings/Microtask.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebRTCRtpContributingSource.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver> receiver,
                               MediaStreamTrack* track,
                               MediaStreamVector streams)
    : receiver_(std::move(receiver)),
      track_(track),
      streams_(std::move(streams)) {
  DCHECK(receiver_);
  DCHECK(track_);
  // Some bots require #if around the DCHECK to avoid compile error about
  // |StateMatchesWebReceiver| (which is behind #if) not being defined.
#if DCHECK_IS_ON()
  DCHECK(StateMatchesWebReceiver());
#endif  // DCHECK_IS_ON()
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  return track_;
}

const HeapVector<Member<RTCRtpContributingSource>>&
RTCRtpReceiver::getContributingSources() {
  UpdateSourcesIfNeeded();
  return contributing_sources_;
}

const WebRTCRtpReceiver& RTCRtpReceiver::web_receiver() const {
  return *receiver_;
}

MediaStreamVector RTCRtpReceiver::streams() const {
  return streams_;
}

#if DCHECK_IS_ON()

bool RTCRtpReceiver::StateMatchesWebReceiver() const {
  if (track_->Component() !=
      static_cast<MediaStreamComponent*>(receiver_->Track())) {
    return false;
  }
  WebVector<WebMediaStream> web_streams = receiver_->Streams();
  if (streams_.size() != web_streams.size())
    return false;
  for (size_t i = 0; i < streams_.size(); ++i) {
    if (streams_[i]->Descriptor() !=
        static_cast<MediaStreamDescriptor*>(web_streams[i])) {
      return false;
    }
  }
  return true;
}

#endif  // DCHECK_IS_ON()

void RTCRtpReceiver::UpdateSourcesIfNeeded() {
  if (!contributing_sources_needs_updating_)
    return;
  contributing_sources_.clear();
  for (const std::unique_ptr<WebRTCRtpContributingSource>&
           web_contributing_source : receiver_->GetSources()) {
    if (web_contributing_source->SourceType() ==
        WebRTCRtpContributingSourceType::SSRC) {
      // TODO(hbos): When |getSynchronizationSources| is added to get SSRC
      // sources don't ignore SSRCs here.
      continue;
    }
    DCHECK_EQ(web_contributing_source->SourceType(),
              WebRTCRtpContributingSourceType::CSRC);
    auto it = contributing_sources_by_source_id_.find(
        web_contributing_source->Source());
    if (it == contributing_sources_by_source_id_.end()) {
      RTCRtpContributingSource* contributing_source =
          new RTCRtpContributingSource(this, *web_contributing_source);
      contributing_sources_by_source_id_.insert(contributing_source->source(),
                                                contributing_source);
      contributing_sources_.push_back(contributing_source);
    } else {
      it->value->UpdateMembers(*web_contributing_source);
      contributing_sources_.push_back(it->value);
    }
  }
  // Clear the flag and schedule a microtask to reset it to true. This makes
  // the cache valid until the next microtask checkpoint. As such, sources
  // represent a snapshot and can be compared reliably in .js code, no risk of
  // being updated due to an RTP packet arriving. E.g.
  // "source.timestamp == source.timestamp" will always be true.
  contributing_sources_needs_updating_ = false;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RTCRtpReceiver::SetContributingSourcesNeedsUpdating,
                WrapWeakPersistent(this)));
}

void RTCRtpReceiver::SetContributingSourcesNeedsUpdating() {
  contributing_sources_needs_updating_ = true;
}

DEFINE_TRACE(RTCRtpReceiver) {
  visitor->Trace(track_);
  visitor->Trace(streams_);
  visitor->Trace(contributing_sources_by_source_id_);
  visitor->Trace(contributing_sources_);
}

}  // namespace blink
