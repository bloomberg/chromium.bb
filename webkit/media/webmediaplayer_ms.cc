// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_ms.h"

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/media/media_stream_client.h"
#include "webkit/media/video_frame_provider.h"
#include "webkit/media/webmediaplayer_delegate.h"
#include "webkit/media/webmediaplayer_util.h"
#include "webkit/media/webvideoframe_impl.h"

using WebKit::WebCanvas;
using WebKit::WebMediaPlayer;
using WebKit::WebRect;
using WebKit::WebSize;

namespace webkit_media {

WebMediaPlayerMS::WebMediaPlayerMS(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    MediaStreamClient* media_stream_client,
    media::MediaLog* media_log)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      buffered_(static_cast<size_t>(1)),
      client_(client),
      delegate_(delegate),
      media_stream_client_(media_stream_client),
      video_frame_provider_started_(false),
      paused_(true),
      pending_repaint_(false),
      got_first_frame_(false),
      total_frame_count_(0),
      dropped_frame_count_(0),
      media_log_(media_log) {
  DVLOG(1) << "WebMediaPlayerMS::ctor";
  DCHECK(media_stream_client);
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << "WebMediaPlayerMS::dtor";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (video_frame_provider_) {
    video_frame_provider_->Stop();
  }

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_)
    delegate_->PlayerGone(this);
}

void WebMediaPlayerMS::load(const WebKit::WebURL& url, CORSMode cors_mode) {
  DVLOG(1) << "WebMediaPlayerMS::load";
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL gurl(url);

  setVolume(GetClient()->volume());
  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  // Check if this url is media stream.
  video_frame_provider_ = media_stream_client_->GetVideoFrameProvider(
      url,
      base::Bind(&WebMediaPlayerMS::OnSourceError, AsWeakPtr()),
      base::Bind(&WebMediaPlayerMS::OnFrameAvailable, AsWeakPtr()));
  if (video_frame_provider_) {
    SetNetworkState(WebMediaPlayer::NetworkStateLoaded);
    GetClient()->sourceOpened();
    GetClient()->setOpaque(true);
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
    RepaintInternal();
  } else {
    SetNetworkState(WebMediaPlayer::NetworkStateNetworkError);
  }
}

void WebMediaPlayerMS::cancelLoad() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::play() {
  DVLOG(1) << "WebMediaPlayerMS::play";
  DCHECK(thread_checker_.CalledOnValidThread());

  paused_ = false;
  if (video_frame_provider_) {
    if (video_frame_provider_started_) {
      video_frame_provider_->Play();
    } else {
      video_frame_provider_started_ = true;
      video_frame_provider_->Start();
    }
  }
  // TODO(wjia): add audio. See crbug.com/142988.

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));

  if (delegate_)
    delegate_->DidPlay(this);
}

void WebMediaPlayerMS::pause() {
  DVLOG(1) << "WebMediaPlayerMS::pause";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame_provider_)
    video_frame_provider_->Pause();
  // TODO(wjia): add audio. See crbug.com/142988.
  paused_ = true;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));

  if (delegate_)
    delegate_->DidPause(this);
}

bool WebMediaPlayerMS::supportsFullscreen() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool WebMediaPlayerMS::supportsSave() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

void WebMediaPlayerMS::seek(float seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setEndTime(float seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setRate(float rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setVolume(float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(wjia): set audio volume. See crbug.com/142988.
  NOTIMPLEMENTED();
}

void WebMediaPlayerMS::setVisible(bool visible) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool WebMediaPlayerMS::totalBytesKnown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

bool WebMediaPlayerMS::hasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (video_frame_provider_ != NULL);
}

bool WebMediaPlayerMS::hasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(wjia): add audio support. See crbug.com/142988.
  return false;
}

WebKit::WebSize WebMediaPlayerMS::naturalSize() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Size size;
  if (current_frame_)
    size = current_frame_->natural_size();
  DVLOG(1) << "WebMediaPlayerMS::naturalSize, " << size.ToString();
  return WebKit::WebSize(size);
}

bool WebMediaPlayerMS::paused() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return paused_;
}

bool WebMediaPlayerMS::seeking() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

float WebMediaPlayerMS::duration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::numeric_limits<float>::infinity();
}

float WebMediaPlayerMS::currentTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_frame_.get()) {
    return current_frame_->GetTimestamp().InSecondsF();
  }
  return 0.0f;
}

int WebMediaPlayerMS::dataRate() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

WebMediaPlayer::NetworkState WebMediaPlayerMS::networkState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::networkState, state:" << network_state_;
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerMS::readyState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::readyState, state:" << ready_state_;
  return ready_state_;
}

const WebKit::WebTimeRanges& WebMediaPlayerMS::buffered() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return buffered_;
}

float WebMediaPlayerMS::maxTimeSeekable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0.0f;
}

bool WebMediaPlayerMS::didLoadingProgress() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

unsigned long long WebMediaPlayerMS::totalBytes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

void WebMediaPlayerMS::setSize(const WebSize& size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Don't need to do anything as we use the dimensions passed in via paint().
}

void WebMediaPlayerMS::paint(WebCanvas* canvas,
                             const WebRect& rect,
                             uint8_t alpha) {
  DVLOG(1) << "WebMediaPlayerMS::paint";
  DCHECK(thread_checker_.CalledOnValidThread());

  video_renderer_.Paint(current_frame_, canvas, rect, alpha);
}

bool WebMediaPlayerMS::hasSingleSecurityOrigin() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool WebMediaPlayerMS::didPassCORSAccessCheck() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

WebMediaPlayer::MovieLoadType WebMediaPlayerMS::movieLoadType() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return WebMediaPlayer::MovieLoadTypeUnknown;
}

float WebMediaPlayerMS::mediaTimeForTimeValue(float timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerMS::decodedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::decodedFrameCount, " << total_frame_count_;
  return total_frame_count_;
}

unsigned WebMediaPlayerMS::droppedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::droppedFrameCount, " << dropped_frame_count_;
  return dropped_frame_count_;
}

unsigned WebMediaPlayerMS::audioDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerMS::videoDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

WebKit::WebVideoFrame* WebMediaPlayerMS::getCurrentFrame() {
  DVLOG(1) << "WebMediaPlayerMS::getCurrentFrame";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!pending_repaint_);
  pending_repaint_ = true;
  if (current_frame_.get())
    return new webkit_media::WebVideoFrameImpl(current_frame_);
  return NULL;
}

void WebMediaPlayerMS::putCurrentFrame(
    WebKit::WebVideoFrame* web_video_frame) {
  DVLOG(1) << "WebMediaPlayerMS::putCurrentFrame";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pending_repaint_);
  pending_repaint_ = false;
  if (web_video_frame) {
    delete web_video_frame;
  }
}

void WebMediaPlayerMS::OnFrameAvailable(
    const scoped_refptr<media::VideoFrame>& frame) {
  DVLOG(1) << "WebMediaPlayerMS::OnFrameAvailable";
  DCHECK(thread_checker_.CalledOnValidThread());
  ++total_frame_count_;
  if (!got_first_frame_) {
    got_first_frame_ = true;
    start_time_ = frame->GetTimestamp();
  }
  current_frame_ = frame;
  current_frame_->SetTimestamp(frame->GetTimestamp() - start_time_);
  if (pending_repaint_) {
    // TODO(wjia): Figure out how to calculate dropped frame count for
    // both S/W and H/W compositing.
    // ++dropped_frame_count_;
  } else {
    GetClient()->repaint();
  }
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << "WebMediaPlayerMS::RepaintInternal";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pending_repaint_) {
    GetClient()->repaint();
  }
}

void WebMediaPlayerMS::OnSourceError() {
  DVLOG(1) << "WebMediaPlayerMS::OnSourceError";
  DCHECK(thread_checker_.CalledOnValidThread());
  SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
  RepaintInternal();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->networkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->readyStateChanged();
}

WebKit::WebMediaPlayerClient* WebMediaPlayerMS::GetClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_);
  return client_;
}

}  // namespace webkit_media
