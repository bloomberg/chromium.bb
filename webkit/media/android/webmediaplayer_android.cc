// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_android.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/video_layer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/video_frame.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"
#include "webkit/media/android/webmediaplayer_proxy_android.h"
#include "webkit/media/media_switches.h"
#include "webkit/media/webmediaplayer_util.h"

#if defined(GOOGLE_TV)
#include "webkit/media/android/media_source_delegate.h"
#endif

static const uint32 kGLTextureExternalOES = 0x8D65;

using WebKit::WebMediaPlayer;
using WebKit::WebMediaSource;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTimeRanges;
using WebKit::WebURL;
using media::MediaPlayerBridge;
using media::VideoFrame;

namespace webkit_media {

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    WebMediaPlayerManagerAndroid* manager,
    WebMediaPlayerProxyAndroid* proxy,
    StreamTextureFactory* factory,
    media::MediaLog* media_log)
    : frame_(frame),
      client_(client),
      buffered_(1u),
      main_loop_(base::MessageLoop::current()),
      pending_seek_(0),
      seeking_(false),
      did_loading_progress_(false),
      manager_(manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      is_playing_(false),
      needs_establish_peer_(true),
      has_size_info_(false),
      stream_texture_factory_(factory),
      needs_external_surface_(false),
      video_frame_provider_client_(NULL),
      proxy_(proxy),
      current_time_(0),
      media_log_(media_log) {
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
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  if (proxy_)
    proxy_->DestroyPlayer(player_id_);

  if (stream_id_)
    stream_texture_factory_->DestroyStreamTexture(texture_id_);

  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);

  if (main_loop_)
    main_loop_->RemoveDestructionObserver(this);
}

void WebMediaPlayerAndroid::load(const WebURL& url, CORSMode cors_mode) {
  load(url, NULL, cors_mode);
}

void WebMediaPlayerAndroid::load(const WebURL& url,
                                 WebMediaSource* media_source,
                                 CORSMode cors_mode) {
  if (cors_mode != CORSModeUnspecified)
    NOTIMPLEMENTED() << "No CORS support";

  scoped_ptr<WebKit::WebMediaSource> scoped_media_source(media_source);
#if defined(GOOGLE_TV)
  if (media_source) {
    media_source_delegate_.reset(
        new MediaSourceDelegate(
            frame_, client_, proxy_, player_id_, media_log_));
    // |media_source_delegate_| is owned, so Unretained() is safe here.
    media_source_delegate_->Initialize(
        scoped_media_source.Pass(),
        base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                   base::Unretained(this)));
  }
#endif

  url_ = url;
  GURL first_party_url = frame_->document().firstPartyForCookies();
  if (proxy_) {
    proxy_->Initialize(player_id_, url_, media_source != NULL, first_party_url);
    if (manager_->IsInFullscreen(frame_))
      proxy_->EnterFullscreen(player_id_);
  }

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);
}

void WebMediaPlayerAndroid::cancelLoad() {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::play() {
#if defined(GOOGLE_TV)
  if (hasVideo() && needs_external_surface_) {
    DCHECK(!needs_establish_peer_);
    if (proxy_)
      proxy_->RequestExternalSurface(player_id_);
  }
#endif
  if (hasVideo() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  if (paused() && proxy_)
    proxy_->Start(player_id_);
  is_playing_ = true;
}

void WebMediaPlayerAndroid::pause() {
  if (proxy_)
    proxy_->Pause(player_id_);
  is_playing_ = false;
}

void WebMediaPlayerAndroid::seek(double seconds) {
  pending_seek_ = seconds;
  seeking_ = true;

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    media_source_delegate_->Seek(seek_time);
#endif
  if (proxy_)
    proxy_->Seek(player_id_, seek_time);
}

bool WebMediaPlayerAndroid::supportsFullscreen() const {
  return true;
}

bool WebMediaPlayerAndroid::supportsSave() const {
  return false;
}

void WebMediaPlayerAndroid::setRate(double rate) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::setVolume(double volume) {
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
  if(!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
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

double WebMediaPlayerAndroid::duration() const {
  return duration_.InSecondsF();
}

double WebMediaPlayerAndroid::currentTime() const {
  // If the player is pending for a seek, return the seek time.
  if (seeking())
    return pending_seek_;

  return current_time_;
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
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    return media_source_delegate_->Buffered();
#endif
  return buffered_;
}

double WebMediaPlayerAndroid::maxTimeSeekable() const {
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

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    WebKit::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    bool premultiply_alpha,
    bool flip_y) {
  if (!texture_id_)
    return false;

  // The video is stored in an unmultiplied format, so premultiply if
  // necessary.
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    premultiply_alpha);

  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  web_graphics_context->copyTextureCHROMIUM(GL_TEXTURE_2D, texture_id_,
                                            texture, level, internal_format);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);
  return true;
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  // TODO(qinmin): fix this for urls that are not file.
  // https://code.google.com/p/chromium/issues/detail?id=28353
  if (url_.SchemeIsFile())
    return true;
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

double WebMediaPlayerAndroid::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerAndroid::decodedFrameCount() const {
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    return media_source_delegate_->DecodedFrameCount();
#endif
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    return media_source_delegate_->DroppedFrameCount();
#endif
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::audioDecodedByteCount() const {
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    return media_source_delegate_->AudioDecodedByteCount();
#endif
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::videoDecodedByteCount() const {
#if defined(GOOGLE_TV)
  if (media_source_delegate_)
    return media_source_delegate_->VideoDecodedByteCount();
#endif
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerAndroid::OnMediaMetadataChanged(
    base::TimeDelta duration, int width, int height, bool success) {
  if (url_.SchemeIs("file"))
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);

  if (ready_state_ != WebMediaPlayer::ReadyStateHaveEnoughData) {
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }

  if (success)
    OnVideoSizeChanged(width, height);

  if (hasVideo() && !video_weblayer_ && client_->needsWebLayerForVideo()) {
    video_weblayer_.reset(
        new webkit::WebLayerImpl(cc::VideoLayer::Create(this)));
    client_->setWebLayer(video_weblayer_.get());
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

#if defined(GOOGLE_TV)
  static bool has_switch = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseExternalVideoSurfaceThresholdInPixels);
  static int threshold = 0;
  static bool parsed_arg =
      has_switch &&
      base::StringToInt(
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kUseExternalVideoSurfaceThresholdInPixels),
          &threshold);

  if ((parsed_arg && threshold <= width * height) ||
      // Use H/W surface for MSE as the content is protected.
      media_source_delegate_) {
    needs_external_surface_ = true;
    SetNeedsEstablishPeer(false);
    if (!paused() && proxy_)
      proxy_->RequestExternalSurface(player_id_);
  }
#endif

  natural_size_.width = width;
  natural_size_.height = height;
  ReallocateVideoFrame();
}

void WebMediaPlayerAndroid::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = static_cast<float>(current_time.InSecondsF());
}

void WebMediaPlayerAndroid::OnDidEnterFullscreen() {
  if (!manager_->IsInFullscreen(frame_)) {
    frame_->view()->willEnterFullScreen();
    frame_->view()->didEnterFullScreen();
    manager_->DidEnterFullscreen(frame_);
  }
}

void WebMediaPlayerAndroid::OnDidExitFullscreen() {
  SetNeedsEstablishPeer(true);
  // We had the fullscreen surface connected to Android MediaPlayer,
  // so reconnect our surface texture for embedded playback.
  if (!paused())
    EstablishSurfaceTexturePeer();

  frame_->view()->willExitFullScreen();
  frame_->view()->didExitFullScreen();
  manager_->DidExitFullscreen();
  client_->repaint();
}

void WebMediaPlayerAndroid::OnMediaPlayerPlay() {
  UpdatePlayingState(true);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnMediaPlayerPause() {
  UpdatePlayingState(false);
  client_->playbackStateChanged();
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
  switch (network_state_) {
    // Pause the media player and inform WebKit if the player is in a good
    // shape.
    case WebMediaPlayer::NetworkStateIdle:
    case WebMediaPlayer::NetworkStateLoading:
    case WebMediaPlayer::NetworkStateLoaded:
      pause();
      client_->playbackStateChanged();
      break;
    // If a WebMediaPlayer instance has entered into one of these states,
    // the internal network state in HTMLMediaElement could be set to empty.
    // And calling playbackStateChanged() could get this object deleted.
    case WebMediaPlayer::NetworkStateEmpty:
    case WebMediaPlayer::NetworkStateFormatError:
    case WebMediaPlayer::NetworkStateNetworkError:
    case WebMediaPlayer::NetworkStateDecodeError:
      break;
  }
  if (proxy_)
    proxy_->ReleaseResources(player_id_);
  OnPlayerReleased();
}

void WebMediaPlayerAndroid::WillDestroyCurrentMessageLoop() {
  if (manager_)
    manager_->UnregisterMediaPlayer(player_id_);
  Detach();
  main_loop_ = NULL;
}

void WebMediaPlayerAndroid::Detach() {
  if (stream_id_) {
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
  }

  current_frame_ = NULL;
  manager_ = NULL;
  proxy_ = NULL;
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  if (needs_external_surface_) {
    // VideoFrame::CreateHoleFrame is only defined under GOOGLE_TV.
#if defined(GOOGLE_TV)
    if (!natural_size_.isEmpty())
      current_frame_ = VideoFrame::CreateHoleFrame(natural_size_);
#else
    NOTIMPLEMENTED() << "Hole punching not supported outside of Google TV";
#endif
  } else if (texture_id_) {
    current_frame_ = VideoFrame::WrapNativeTexture(
        texture_id_, kGLTextureExternalOES, natural_size_,
        gfx::Rect(natural_size_), natural_size_, base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure());
  }
}

void WebMediaPlayerAndroid::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;

  // Set the callback target when a frame is produced.
  if (stream_texture_proxy_)
    stream_texture_proxy_->SetClient(client);
}

scoped_refptr<media::VideoFrame> WebMediaPlayerAndroid::GetCurrentFrame() {
  if (stream_texture_proxy_ && !stream_texture_proxy_->IsBoundToThread() &&
      stream_id_ && !needs_external_surface_) {
    gfx::Size natural_size = current_frame_->natural_size();
    stream_texture_proxy_->BindToCurrentThread(
        stream_id_, natural_size.width(), natural_size.height());
  }
  return current_frame_;
}

void WebMediaPlayerAndroid::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
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

#if defined(GOOGLE_TV)
bool WebMediaPlayerAndroid::RetrieveGeometryChange(gfx::RectF* rect) {
  if (!video_weblayer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_weblayer_->layer();
  rect->set_size(layer->bounds());
  while (layer) {
    rect->Offset(layer->position().OffsetFromOrigin());
    layer = layer->parent();
  }

  // Return false when the geometry hasn't been changed from the last time.
  if (last_computed_rect_ == *rect)
    return false;

  // Store the changed geometry information when it is actually changed.
  last_computed_rect_ = *rect;
  return true;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::generateKeyRequest(const WebString& key_system,
                                              const unsigned char* init_data,
                                              unsigned init_data_length) {
  if (media_source_delegate_) {
    return media_source_delegate_->GenerateKeyRequest(
        key_system, init_data, init_data_length);
  }
  return MediaKeyExceptionKeySystemNotSupported;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  if (media_source_delegate_) {
    return media_source_delegate_->AddKey(
        key_system, key, key_length, init_data, init_data_length, session_id);
  }
  return MediaKeyExceptionKeySystemNotSupported;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  if (media_source_delegate_)
    return media_source_delegate_->CancelKeyRequest(key_system, session_id);
  return MediaKeyExceptionKeySystemNotSupported;
}

void WebMediaPlayerAndroid::OnReadFromDemuxer(
    media::DemuxerStream::Type type, bool seek_done) {
  if (media_source_delegate_)
    media_source_delegate_->OnReadFromDemuxer(type, seek_done);
  else
    NOTIMPLEMENTED();
}
#endif

void WebMediaPlayerAndroid::enterFullscreen() {
  if (proxy_ && manager_->CanEnterFullscreen(frame_)) {
    proxy_->EnterFullscreen(player_id_);
    SetNeedsEstablishPeer(false);
  }
}

void WebMediaPlayerAndroid::exitFullscreen() {
  if (proxy_)
    proxy_->ExitFullscreen(player_id_);
}

bool WebMediaPlayerAndroid::canEnterFullscreen() const {
  return manager_->CanEnterFullscreen(frame_);
}

}  // namespace webkit_media
