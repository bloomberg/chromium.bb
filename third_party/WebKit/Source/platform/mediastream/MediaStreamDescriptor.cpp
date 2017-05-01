/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mediastream/MediaStreamDescriptor.h"

#include "platform/UUID.h"

namespace blink {

MediaStreamDescriptor* MediaStreamDescriptor::Create(
    const MediaStreamSourceVector& audio_sources,
    const MediaStreamSourceVector& video_sources) {
  return new MediaStreamDescriptor(CreateCanonicalUUIDString(), audio_sources,
                                   video_sources);
}

MediaStreamDescriptor* MediaStreamDescriptor::Create(
    const MediaStreamComponentVector& audio_components,
    const MediaStreamComponentVector& video_components) {
  return new MediaStreamDescriptor(CreateCanonicalUUIDString(),
                                   audio_components, video_components);
}

MediaStreamDescriptor* MediaStreamDescriptor::Create(
    const String& id,
    const MediaStreamComponentVector& audio_components,
    const MediaStreamComponentVector& video_components) {
  return new MediaStreamDescriptor(id, audio_components, video_components);
}

void MediaStreamDescriptor::AddComponent(MediaStreamComponent* component) {
  switch (component->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      if (audio_components_.Find(component) == kNotFound)
        audio_components_.push_back(component);
      break;
    case MediaStreamSource::kTypeVideo:
      if (video_components_.Find(component) == kNotFound)
        video_components_.push_back(component);
      break;
  }
}

void MediaStreamDescriptor::RemoveComponent(MediaStreamComponent* component) {
  size_t pos = kNotFound;
  switch (component->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      pos = audio_components_.Find(component);
      if (pos != kNotFound)
        audio_components_.erase(pos);
      break;
    case MediaStreamSource::kTypeVideo:
      pos = video_components_.Find(component);
      if (pos != kNotFound)
        video_components_.erase(pos);
      break;
  }
}

void MediaStreamDescriptor::AddRemoteTrack(MediaStreamComponent* component) {
  if (client_)
    client_->AddTrackByComponent(component);
  else
    AddComponent(component);
}

void MediaStreamDescriptor::RemoveRemoteTrack(MediaStreamComponent* component) {
  if (client_)
    client_->RemoveTrackByComponent(component);
  else
    RemoveComponent(component);
}

MediaStreamDescriptor::MediaStreamDescriptor(
    const String& id,
    const MediaStreamSourceVector& audio_sources,
    const MediaStreamSourceVector& video_sources)
    : client_(nullptr), id_(id), active_(true) {
  DCHECK(id_.length());
  for (size_t i = 0; i < audio_sources.size(); i++)
    audio_components_.push_back(MediaStreamComponent::Create(audio_sources[i]));

  for (size_t i = 0; i < video_sources.size(); i++)
    video_components_.push_back(MediaStreamComponent::Create(video_sources[i]));
}

MediaStreamDescriptor::MediaStreamDescriptor(
    const String& id,
    const MediaStreamComponentVector& audio_components,
    const MediaStreamComponentVector& video_components)
    : client_(nullptr), id_(id), active_(true) {
  DCHECK(id_.length());
  for (MediaStreamComponentVector::const_iterator iter =
           audio_components.begin();
       iter != audio_components.end(); ++iter)
    audio_components_.push_back((*iter));
  for (MediaStreamComponentVector::const_iterator iter =
           video_components.begin();
       iter != video_components.end(); ++iter)
    video_components_.push_back((*iter));
}

DEFINE_TRACE(MediaStreamDescriptor) {
  visitor->Trace(audio_components_);
  visitor->Trace(video_components_);
  visitor->Trace(client_);
}

}  // namespace blink
