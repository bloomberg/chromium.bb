// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_in_process_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_player_bridge_manager.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCookieJar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "webkit/media/android/stream_texture_factory_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"

using WebKit::WebMediaPlayerClient;
using WebKit::WebMediaPlayer;
using WebKit::WebURL;
using media::MediaPlayerBridge;

namespace webkit_media {

InProcessMediaResourceGetter::InProcessMediaResourceGetter(
    WebKit::WebCookieJar* cookie_jar)
    : cookie_jar_(cookie_jar) {
}

InProcessMediaResourceGetter::~InProcessMediaResourceGetter() {}

void InProcessMediaResourceGetter::GetCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    const GetCookieCB& callback) {
  std::string cookies;
  if (cookie_jar_ != NULL) {
    cookies = UTF16ToUTF8(
        cookie_jar_->cookies(url, first_party_for_cookies));
  }
  callback.Run(cookies);
}

void InProcessMediaResourceGetter::GetPlatformPathFromFileSystemURL(
    const GURL& url, const GetPlatformPathCB& callback) {
  callback.Run(std::string());
}

WebMediaPlayerInProcessAndroid::WebMediaPlayerInProcessAndroid(
    WebKit::WebFrame* frame,
    WebMediaPlayerClient* client,
    WebKit::WebCookieJar* cookie_jar,
    WebMediaPlayerManagerAndroid* manager,
    media::MediaPlayerBridgeManager* resource_manager,
    StreamTextureFactory* factory,
    bool disable_media_history_logging)
    : WebMediaPlayerAndroid(client, manager, factory),
      frame_(frame),
      playback_completed_(false),
      cookie_jar_(cookie_jar),
      resource_manager_(resource_manager),
      disable_history_logging_(disable_media_history_logging) {
}

WebMediaPlayerInProcessAndroid::~WebMediaPlayerInProcessAndroid() {}

void WebMediaPlayerInProcessAndroid::PlayInternal() {
  if (paused())
    media_player_->Start();
}

void WebMediaPlayerInProcessAndroid::PauseInternal() {
  if (!paused())
    media_player_->Pause();
}

void WebMediaPlayerInProcessAndroid::SeekInternal(base::TimeDelta time) {
  playback_completed_ = false;
  media_player_->SeekTo(time);
}

bool WebMediaPlayerInProcessAndroid::paused() const {
  return !media_player_->IsPlaying();
}

float WebMediaPlayerInProcessAndroid::GetCurrentTimeInternal() const {
  // When playback is about to finish, android media player often stops
  // at a time which is smaller than the duration. This makes webkit never
  // know that the playback has finished. To solve this, we set the
  // current time to media duration when OnPlaybackComplete() get called.
  if (playback_completed_)
    return duration();
  return static_cast<float>(media_player_->GetCurrentTime().InSecondsF());
}

void WebMediaPlayerInProcessAndroid::ReleaseResourcesInternal() {
  media_player_->Release();
}

void WebMediaPlayerInProcessAndroid::MediaPreparedCallback(
    int player_id, base::TimeDelta duration) {
  OnMediaPrepared(duration);
}

void WebMediaPlayerInProcessAndroid::PlaybackCompleteCallback(int player_id) {
  // Set the current time equal to duration to let webkit know that play back
  // is completed.
  playback_completed_ = true;
  client()->timeChanged();
}

void WebMediaPlayerInProcessAndroid::SeekCompleteCallback(
    int player_id, base::TimeDelta current_time) {
  OnSeekComplete(current_time);
}

void WebMediaPlayerInProcessAndroid::MediaInterruptedCallback(int player_id) {
  PauseInternal();
}

void WebMediaPlayerInProcessAndroid::MediaErrorCallback(int player_id,
                                                        int error_type) {
  OnMediaError(error_type);
}

void WebMediaPlayerInProcessAndroid::VideoSizeChangedCallback(
    int player_id, int width, int height) {
  OnVideoSizeChanged(width, height);
}

void WebMediaPlayerInProcessAndroid::BufferingUpdateCallback(
    int player_id, int percent) {
  OnBufferingUpdate(percent);
}

void WebMediaPlayerInProcessAndroid::SetVideoSurface(jobject j_surface) {
  media_player_->SetVideoSurface(j_surface);
}

void WebMediaPlayerInProcessAndroid::InitializeMediaPlayer(GURL url) {
  GURL first_party_url = frame_->document().firstPartyForCookies();
  media_player_.reset(new MediaPlayerBridge(
      player_id(), url, first_party_url,
      new InProcessMediaResourceGetter(cookie_jar_),
      disable_history_logging_,
      resource_manager_,
      base::Bind(&WebMediaPlayerInProcessAndroid::MediaErrorCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::VideoSizeChangedCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::BufferingUpdateCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::MediaPreparedCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::PlaybackCompleteCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::SeekCompleteCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::TimeUpdateCallback,
                 base::Unretained(this)),
      base::Bind(&WebMediaPlayerInProcessAndroid::MediaInterruptedCallback,
                 base::Unretained(this))));

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);

  // Calling Prepare() will cause android mediaplayer to start
  // buffering and decoding the data. On mobile devices, this costs a lot of
  // data usage and could even introduce performance issues. So we don't
  // initialize the player unless it is a local file. We will start loading
  // the media only when play/seek/fullsceen button is clicked.
  if (url.SchemeIs("file")) {
    media_player_->Prepare();
    return;
  }

  // Pretend everything has been loaded so that webkit can
  // still call play() and seek().
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
}

void WebMediaPlayerInProcessAndroid::OnTimeUpdate(
    base::TimeDelta current_time) {}

void WebMediaPlayerInProcessAndroid::Destroy() {}

void WebMediaPlayerInProcessAndroid::RequestExternalSurface() {
  NOTIMPLEMENTED();
}

}  // namespace webkit_media
