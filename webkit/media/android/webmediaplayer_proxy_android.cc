// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_proxy_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "webkit/media/android/webmediaplayer_android.h"

namespace webkit_media {

WebMediaPlayerProxyAndroid::WebMediaPlayerProxyAndroid(
    const scoped_refptr<base::MessageLoopProxy>& render_loop,
    base::WeakPtr<WebMediaPlayerAndroid> webmediaplayer)
    : render_loop_(render_loop),
      webmediaplayer_(webmediaplayer) {
  DCHECK(render_loop_);
  DCHECK(webmediaplayer_);
}

WebMediaPlayerProxyAndroid::~WebMediaPlayerProxyAndroid() {
}

void WebMediaPlayerProxyAndroid::MediaErrorCallback(int error_type) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnMediaError, webmediaplayer_, error_type));
}

void WebMediaPlayerProxyAndroid::MediaInfoCallback(int info_type) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnMediaInfo, webmediaplayer_, info_type));
}

void WebMediaPlayerProxyAndroid::VideoSizeChangedCallback(
    int width, int height) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnVideoSizeChanged, webmediaplayer_,
      width, height));
}

void WebMediaPlayerProxyAndroid::BufferingUpdateCallback(int percent) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnBufferingUpdate, webmediaplayer_, percent));
}

void WebMediaPlayerProxyAndroid::PlaybackCompleteCallback() {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnPlaybackComplete, webmediaplayer_));
}

void WebMediaPlayerProxyAndroid::SeekCompleteCallback() {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnSeekComplete, webmediaplayer_));
}

void WebMediaPlayerProxyAndroid::MediaPreparedCallback() {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerAndroid::OnMediaPrepared, webmediaplayer_));
}

}  // namespace webkit_media
