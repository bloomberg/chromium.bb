// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_impl_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/media_player_bridge.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "webkit/media/android/stream_texture_factory_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"
#include "webkit/media/android/webmediaplayer_proxy_android.h"

using WebKit::WebMediaPlayerClient;
using WebKit::WebMediaPlayer;

namespace webkit_media {

WebMediaPlayerImplAndroid::WebMediaPlayerImplAndroid(
    WebKit::WebFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerManagerAndroid* manager,
    WebMediaPlayerProxyAndroid* proxy,
    StreamTextureFactory* factory)
    : WebMediaPlayerAndroid(client, manager, factory),
      frame_(frame),
      proxy_(proxy),
      current_time_(0) {
}

WebMediaPlayerImplAndroid::~WebMediaPlayerImplAndroid() {
  Destroy();
}

void WebMediaPlayerImplAndroid::InitializeMediaPlayer(GURL url) {
  GURL first_party_url = frame_->document().firstPartyForCookies();
  if (proxy_) {
    proxy_->Initialize(player_id(), url.spec(), first_party_url.spec());
  }

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);
}

void WebMediaPlayerImplAndroid::PlayInternal() {
  if (paused() && proxy_)
    proxy_->Start(player_id());
}

void WebMediaPlayerImplAndroid::PauseInternal() {
  if (proxy_)
    proxy_->Pause(player_id());
}

void WebMediaPlayerImplAndroid::SeekInternal(base::TimeDelta time) {
  if (proxy_)
    proxy_->Seek(player_id(), time);
}

float WebMediaPlayerImplAndroid::GetCurrentTimeInternal() const {
  return current_time_;
}

void WebMediaPlayerImplAndroid::ReleaseResourcesInternal() {
  if (proxy_)
    proxy_->ReleaseResources(player_id());
}

void WebMediaPlayerImplAndroid::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = static_cast<float>(current_time.InSecondsF());
}

void WebMediaPlayerImplAndroid::Destroy() {
  proxy_->DestroyPlayer(player_id());
  proxy_ = NULL;
}

void WebMediaPlayerImplAndroid::SetVideoSurface(jobject j_surface) {}

}  // namespace webkit_media
