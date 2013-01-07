// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_android.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/video_frame.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "webkit/media/android/stream_texture_factory_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"
#include "webkit/media/webmediaplayer_util.h"
#include "webkit/media/webvideoframe_impl.h"

static const uint32 kGLTextureExternalOES = 0x8D65;

using WebKit::WebMediaPlayer;
using WebKit::WebSize;
using WebKit::WebTimeRanges;
using WebKit::WebURL;
using WebKit::WebVideoFrame;
using media::MediaPlayerBridge;
using media::VideoFrame;

namespace webkit_media {

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    WebKit::WebMediaPlayerClient* client,
    WebMediaPlayerManagerAndroid* manager,
    StreamTextureFactory* factory)
    : client_(client),
      buffered_(1u),
      main_loop_(MessageLoop::current()),
      pending_seek_(0),
      seeking_(false),
      did_loading_progress_(false),
      manager_(manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      is_playing_(false),
      needs_establish_peer_(true),
      has_size_info_(false),
      stream_texture_factory_(factory) {
  main_loop_->AddDestructionObserver(this);
  if (manager_)
    player_id_ = manager_->RegisterMediaPlayer(this);

  if (stream_texture_factory_.get()) {
    stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
    stream_id_ = stream_texture_factory_->CreateStreamTexture(&texture_id_);
    ReallocateVideoFrame();
  }
}

WebMediaPlayerAndroid::~WebMediaPlayerAndroid() {
  if (stream_id_)
    stream_texture_factory_->DestroyStreamTexture(texture_id_);

  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);

  if (main_loop_)
    main_loop_->RemoveDestructionObserver(this);
}

void WebMediaPlayerAndroid::load(const WebURL& url, CORSMode cors_mode) {
  if (cors_mode != CORSModeUnspecified)
    NOTIMPLEMENTED() << "No CORS support";

  url_ = url;

  InitializeMediaPlayer(url_);
}

void WebMediaPlayerAndroid::cancelLoad() {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::play() {
  if (hasVideo() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  PlayInternal();
  is_playing_ = true;
}

void WebMediaPlayerAndroid::pause() {
  PauseInternal();
  is_playing_ = false;
}

void WebMediaPlayerAndroid::seek(float seconds) {
  pending_seek_ = seconds;
  seeking_ = true;

  SeekInternal(ConvertSecondsToTimestamp(seconds));
}

bool WebMediaPlayerAndroid::supportsFullscreen() const {
  return true;
}

bool WebMediaPlayerAndroid::supportsSave() const {
  return false;
}

void WebMediaPlayerAndroid::setEndTime(float seconds) {
  // Deprecated.
  // TODO(qinmin): Remove this from WebKit::WebMediaPlayer as it is never used.
}

void WebMediaPlayerAndroid::setRate(float rate) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::setVolume(float volume) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::setVisible(bool visible) {
  // Deprecated.
  // TODO(qinmin): Remove this from WebKit::WebMediaPlayer as it is never used.
}

bool WebMediaPlayerAndroid::totalBytesKnown() {
  NOTIMPLEMENTED();
  return false;
}

bool WebMediaPlayerAndroid::hasVideo() const {
  // If we have obtained video size information before, use it.
  if (has_size_info_)
    return !natural_size_.isEmpty();

  // TODO(qinmin): need a better method to determine whether the current media
  // content contains video. Android does not provide any function to do
  // this.
  // We don't know whether the current media content has video unless
  // the player is prepared. If the player is not prepared, we fall back
  // to the mime-type. There may be no mime-type on a redirect URL.
  // In that case, we conservatively assume it contains video so that
  // enterfullscreen call will not fail.
  if (!url_.has_path())
    return false;
  std::string mime;
  if(!net::GetMimeTypeFromFile(FilePath(url_.path()), &mime))
    return true;
  return mime.find("audio/") == std::string::npos;
}

bool WebMediaPlayerAndroid::hasAudio() const {
  // TODO(hclam): Query status of audio and return the actual value.
  return true;
}

bool WebMediaPlayerAndroid::paused() const {
  return !is_playing_;
}

bool WebMediaPlayerAndroid::seeking() const {
  return seeking_;
}

float WebMediaPlayerAndroid::duration() const {
  return static_cast<float>(duration_.InSecondsF());
}

float WebMediaPlayerAndroid::currentTime() const {
  // If the player is pending for a seek, return the seek time.
  if (seeking())
    return pending_seek_;

  return GetCurrentTimeInternal();
}

int WebMediaPlayerAndroid::dataRate() const {
  // Deprecated.
  // TODO(qinmin): Remove this from WebKit::WebMediaPlayer as it is never used.
  return 0;
}

WebSize WebMediaPlayerAndroid::naturalSize() const {
  return natural_size_;
}

WebMediaPlayer::NetworkState WebMediaPlayerAndroid::networkState() const {
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerAndroid::readyState() const {
  return ready_state_;
}

const WebTimeRanges& WebMediaPlayerAndroid::buffered() {
  return buffered_;
}

float WebMediaPlayerAndroid::maxTimeSeekable() const {
  // TODO(hclam): If this stream is not seekable this should return 0.
  return duration();
}

bool WebMediaPlayerAndroid::didLoadingProgress() const {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

unsigned long long WebMediaPlayerAndroid::totalBytes() const {
  // Deprecated.
  // TODO(qinmin): Remove this from WebKit::WebMediaPlayer as it is never used.
  return 0;
}

void WebMediaPlayerAndroid::setSize(const WebKit::WebSize& size) {
}

void WebMediaPlayerAndroid::paint(WebKit::WebCanvas* canvas,
                                  const WebKit::WebRect& rect,
                                  uint8_t alpha) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  return false;
}

bool WebMediaPlayerAndroid::didPassCORSAccessCheck() const {
  return false;
}

WebMediaPlayer::MovieLoadType WebMediaPlayerAndroid::movieLoadType() const {
  // Deprecated.
  // TODO(qinmin): Remove this from WebKit::WebMediaPlayer as it is never used.
  return WebMediaPlayer::MovieLoadTypeUnknown;
}

float WebMediaPlayerAndroid::mediaTimeForTimeValue(float timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerAndroid::decodedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::audioDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::videoDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerAndroid::OnMediaPrepared(base::TimeDelta duration) {
  if (url_.SchemeIs("file"))
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);

  if (ready_state_ != WebMediaPlayer::ReadyStateHaveEnoughData) {
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  } else {
    // If the status is already set to ReadyStateHaveEnoughData, set it again
    // to make sure that Videolayerchromium will get created.
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }

  // In we have skipped loading, we have to update webkit about the new
  // duration.
  if (duration_ != duration) {
    duration_ = duration;
    client_->durationChanged();
  }
}

void WebMediaPlayerAndroid::OnPlaybackComplete() {
  // When playback is about to finish, android media player often stops
  // at a time which is smaller than the duration. This makes webkit never
  // know that the playback has finished. To solve this, we set the
  // current time to media duration when OnPlaybackComplete() get called.
  OnTimeUpdate(duration_);
  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnBufferingUpdate(int percentage) {
  buffered_[0].end = duration() * percentage / 100;
  did_loading_progress_ = true;
}

void WebMediaPlayerAndroid::OnSeekComplete(base::TimeDelta current_time) {
  seeking_ = false;

  OnTimeUpdate(current_time);

  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnMediaError(int error_type) {
  switch (error_type) {
    case MediaPlayerBridge::MEDIA_ERROR_FORMAT:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerBridge::MEDIA_ERROR_DECODE:
      UpdateNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;
    case MediaPlayerBridge::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerBridge::MEDIA_ERROR_INVALID_CODE:
      break;
  }
  client_->repaint();
}

void WebMediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  has_size_info_ = true;
  if (natural_size_.width == width && natural_size_.height == height)
    return;

  natural_size_.width = width;
  natural_size_.height = height;
  ReallocateVideoFrame();
}

void WebMediaPlayerAndroid::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  network_state_ = state;
  client_->networkStateChanged();
}

void WebMediaPlayerAndroid::UpdateReadyState(
    WebMediaPlayer::ReadyState state) {
  ready_state_ = state;
  client_->readyStateChanged();
}

void WebMediaPlayerAndroid::OnPlayerReleased() {
  needs_establish_peer_ = true;
}

void WebMediaPlayerAndroid::ReleaseMediaResources() {
  // Pause the media player first.
  pause();
  client_->playbackStateChanged();

  ReleaseResourcesInternal();
  OnPlayerReleased();
}

void WebMediaPlayerAndroid::WillDestroyCurrentMessageLoop() {
  Destroy();

  if (stream_id_) {
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
  }

  video_frame_.reset();

  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);

  manager_ = NULL;
  main_loop_ = NULL;
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  if (texture_id_) {
    video_frame_.reset(new WebVideoFrameImpl(VideoFrame::WrapNativeTexture(
        texture_id_, kGLTextureExternalOES, natural_size_,
        gfx::Rect(natural_size_), natural_size_, base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure())));
  }
}

WebVideoFrame* WebMediaPlayerAndroid::getCurrentFrame() {
  if (stream_texture_proxy_.get() && !stream_texture_proxy_->IsInitialized()
      && stream_id_) {
    stream_texture_proxy_->Initialize(
        stream_id_, video_frame_->width(), video_frame_->height());
  }

  return video_frame_.get();
}

void WebMediaPlayerAndroid::putCurrentFrame(
    WebVideoFrame* web_video_frame) {
}

void WebMediaPlayerAndroid::setStreamTextureClient(
    WebKit::WebStreamTextureClient* client) {
  if (stream_texture_proxy_.get())
    stream_texture_proxy_->SetClient(client);
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  if (stream_texture_factory_.get() && stream_id_)
    stream_texture_factory_->EstablishPeer(stream_id_, player_id_);
  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::SetNeedsEstablishPeer(bool needs_establish_peer) {
  needs_establish_peer_ = needs_establish_peer;
}

void WebMediaPlayerAndroid::UpdatePlayingState(bool is_playing) {
  is_playing_ = is_playing;
}

}  // namespace webkit_media
