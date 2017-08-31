/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebMediaStreamTrack.h"

#include <memory>
#include "platform/mediastream/MediaStreamComponent.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebAudioSourceProvider.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamSource.h"
#include "public/platform/WebString.h"

namespace blink {

namespace {

class TrackDataContainer : public MediaStreamComponent::TrackData {
 public:
  explicit TrackDataContainer(
      std::unique_ptr<WebMediaStreamTrack::TrackData> extra_data)
      : extra_data_(std::move(extra_data)) {}

  WebMediaStreamTrack::TrackData* GetTrackData() { return extra_data_.get(); }
  void GetSettings(WebMediaStreamTrack::Settings& settings) {
    extra_data_->GetSettings(settings);
  }

 private:
  std::unique_ptr<WebMediaStreamTrack::TrackData> extra_data_;
};

}  // namespace

WebMediaStreamTrack::WebMediaStreamTrack(
    MediaStreamComponent* media_stream_component)
    : private_(media_stream_component) {}

WebMediaStreamTrack& WebMediaStreamTrack::operator=(
    MediaStreamComponent* media_stream_component) {
  private_ = media_stream_component;
  return *this;
}

void WebMediaStreamTrack::Initialize(const WebMediaStreamSource& source) {
  private_ = MediaStreamComponent::Create(source);
}

void WebMediaStreamTrack::Initialize(const WebString& id,
                                     const WebMediaStreamSource& source) {
  private_ = MediaStreamComponent::Create(id, source);
}

void WebMediaStreamTrack::Reset() {
  private_.Reset();
}

WebMediaStreamTrack::operator MediaStreamComponent*() const {
  return private_.Get();
}

bool WebMediaStreamTrack::IsEnabled() const {
  DCHECK(!private_.IsNull());
  return private_->Enabled();
}

bool WebMediaStreamTrack::IsMuted() const {
  DCHECK(!private_.IsNull());
  return private_->Muted();
}

WebMediaStreamTrack::ContentHintType WebMediaStreamTrack::ContentHint() const {
  DCHECK(!private_.IsNull());
  return private_->ContentHint();
}

WebString WebMediaStreamTrack::Id() const {
  DCHECK(!private_.IsNull());
  return private_->Id();
}

int WebMediaStreamTrack::UniqueId() const {
  DCHECK(!private_.IsNull());
  return private_->UniqueId();
}

WebMediaStreamSource WebMediaStreamTrack::Source() const {
  DCHECK(!private_.IsNull());
  return WebMediaStreamSource(private_->Source());
}

WebMediaStreamTrack::TrackData* WebMediaStreamTrack::GetTrackData() const {
  MediaStreamComponent::TrackData* data = private_->GetTrackData();
  if (!data)
    return 0;
  return static_cast<TrackDataContainer*>(data)->GetTrackData();
}

void WebMediaStreamTrack::SetTrackData(TrackData* extra_data) {
  DCHECK(!private_.IsNull());

  private_->SetTrackData(
      WTF::WrapUnique(new TrackDataContainer(WTF::WrapUnique(extra_data))));
}

void WebMediaStreamTrack::SetSourceProvider(WebAudioSourceProvider* provider) {
  DCHECK(!private_.IsNull());
  private_->SetSourceProvider(provider);
}

void WebMediaStreamTrack::Assign(const WebMediaStreamTrack& other) {
  private_ = other.private_;
}

}  // namespace blink
