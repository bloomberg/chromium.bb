// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/media/android/webmediaplayer_android.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/video_layer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/android/media_player_android.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"
#include "webkit/renderer/media/android/webmediaplayer_manager_android.h"
#include "webkit/renderer/media/android/webmediaplayer_proxy_android.h"
#include "webkit/renderer/media/crypto/key_systems.h"
#include "webkit/renderer/media/webmediaplayer_util.h"

static const uint32 kGLTextureExternalOES = 0x8D65;

using WebKit::WebMediaPlayer;
using WebKit::WebMediaSource;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTimeRanges;
using WebKit::WebURL;
using media::MediaPlayerAndroid;
using media::VideoFrame;

namespace {
// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";
}  // namespace

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
      stream_texture_proxy_initialized_(false),
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

  if (stream_texture_factory_) {
    stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
    stream_id_ = stream_texture_factory_->CreateStreamTexture(&texture_id_);
    ReallocateVideoFrame();
  }

#if defined(GOOGLE_TV)
  if (WebKit::WebRuntimeFeatures::isEncryptedMediaEnabled()) {
    // |decryptor_| is owned, so Unretained() is safe here.
    decryptor_.reset(new ProxyDecryptor(
        client,
        frame,
        base::Bind(&WebMediaPlayerAndroid::OnKeyAdded, base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::OnKeyError, base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::OnKeyMessage,
                   base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::OnNeedKey, base::Unretained(this))));
  }
#endif  // defined(GOOGLE_TV)
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

  if (media_source) {
    media_source_delegate_.reset(
        new MediaSourceDelegate(proxy_, player_id_, media_log_));
    // |media_source_delegate_| is owned, so Unretained() is safe here.
    media_source_delegate_->Initialize(
        media_source,
        base::Bind(&WebMediaPlayerAndroid::OnNeedKey, base::Unretained(this)),
        base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                   base::Unretained(this)));
  }

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
      proxy_->RequestExternalSurface(player_id_, last_computed_rect_);
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
  if (seeking_ && media_source_delegate_)
    media_source_delegate_->CancelPendingSeek();
  seeking_ = true;

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);
#if defined(GOOGLE_TV)
  // TODO(qinmin): check if GTV can also defer the seek until the browser side
  // player is ready.
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
  if (media_source_delegate_)
    return media_source_delegate_->Buffered();
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
                                  unsigned char alpha) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    WebKit::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
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
                                            texture, level, internal_format,
                                            type);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);
  return true;
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  // TODO(qinmin): fix this for urls that are not file.
  // https://code.google.com/p/chromium/issues/detail?id=234710
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
  if (media_source_delegate_)
    return media_source_delegate_->DecodedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->DroppedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::audioDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->AudioDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::videoDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->VideoDecodedByteCount();
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
    case MediaPlayerAndroid::MEDIA_ERROR_FORMAT:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_DECODE:
      UpdateNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_INVALID_CODE:
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
      proxy_->RequestExternalSurface(player_id_, last_computed_rect_);
  }
#endif

  natural_size_.width = width;
  natural_size_.height = height;
  ReallocateVideoFrame();
}

void WebMediaPlayerAndroid::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = current_time.InSecondsF();
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

void WebMediaPlayerAndroid::OnMediaSeekRequest(base::TimeDelta time_to_seek,
                                               bool request_texture_peer) {
  if (!media_source_delegate_)
    return;

  if (!seeking_)
    media_source_delegate_->CancelPendingSeek();
  media_source_delegate_->Seek(time_to_seek);
  OnTimeUpdate(time_to_seek);
  if (request_texture_peer)
    EstablishSurfaceTexturePeer();
}

void WebMediaPlayerAndroid::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing &&
      (state == WebMediaPlayer::NetworkStateNetworkError ||
       state == WebMediaPlayer::NetworkStateDecodeError)) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    network_state_ = WebMediaPlayer::NetworkStateFormatError;
  } else {
    network_state_ = state;
  }
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

  media_source_delegate_.reset();
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
  if (!stream_texture_proxy_initialized_ && stream_texture_proxy_ &&
      stream_id_ && !needs_external_surface_) {
    gfx::Size natural_size = current_frame_->natural_size();
    stream_texture_proxy_->BindToCurrentThread(
        stream_id_, natural_size.width(), natural_size.height());
    stream_texture_proxy_initialized_ = true;
  }
  return current_frame_;
}

void WebMediaPlayerAndroid::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  if (media_source_delegate_ && stream_texture_factory_) {
    // MediaCodec will release the old surface when it goes away, we need to
    // recreate a new one each time this is called.
    stream_texture_factory_->DestroyStreamTexture(texture_id_);
    stream_id_ = 0;
    texture_id_ = 0;
    stream_id_ = stream_texture_factory_->CreateStreamTexture(&texture_id_);
    ReallocateVideoFrame();
    stream_texture_proxy_initialized_ = false;
  }
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

// The following EME related code is copied from WebMediaPlayerImpl.
// TODO(xhwang): Remove duplicate code between WebMediaPlayerAndroid and
// WebMediaPlayerImpl.
// TODO(kjyoun): Update Google TV EME implementation to use IPC.

// Helper functions to report media EME related stats to UMA. They follow the
// convention of more commonly used macros UMA_HISTOGRAM_ENUMERATION and
// UMA_HISTOGRAM_COUNTS. The reason that we cannot use those macros directly is
// that UMA_* macros require the names to be constant throughout the process'
// lifetime.
static void EmeUMAHistogramEnumeration(const std::string& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const std::string& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::MediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(
    const std::string& method,
    const WebString& key_system,
    WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  EmeUMAHistogramEnumeration(
      key_system.utf8(), method, result_id, kMaxMediaKeyException);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::generateKeyRequest(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::GenerateKeyRequestInternal(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.isEmpty())
    current_key_system_ = key_system;
  else if (key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "generateKeyRequest: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!decryptor_->GenerateKeyRequest(key_system.utf8(),
                                      init_data_type_,
                                      init_data, init_data_length)) {
    current_key_system_.reset();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e = AddKeyInternal(
      key_system, key, key_length, init_data, init_data_length, session_id);
  ReportMediaKeyExceptionToUMA("addKey", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::AddKeyInternal(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "addKey: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id.utf8().data() << "]";

  decryptor_->AddKey(key_system.utf8(), key, key_length,
                     init_data, init_data_length, session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(key_system, session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::CancelKeyRequestInternal(
    const WebString& key_system,
    const WebString& session_id) {
  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  decryptor_->CancelKeyRequest(key_system.utf8(), session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerAndroid::OnKeyAdded(const std::string& key_system,
                                       const std::string& session_id) {
  EmeUMAHistogramCounts(key_system, "KeyAdded", 1);

  if (media_source_delegate_)
    media_source_delegate_->NotifyDemuxerReady(key_system);

  client_->keyAdded(WebString::fromUTF8(key_system),
                    WebString::fromUTF8(session_id));
}

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(WebKit::WebMediaPlayerClient::name) == \
                 static_cast<int>(media::MediaKeys::k ## name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(UnknownError);
COMPILE_ASSERT_MATCHING_ENUM(ClientError);
COMPILE_ASSERT_MATCHING_ENUM(ServiceError);
COMPILE_ASSERT_MATCHING_ENUM(OutputError);
COMPILE_ASSERT_MATCHING_ENUM(HardwareChangeError);
COMPILE_ASSERT_MATCHING_ENUM(DomainError);
#undef COMPILE_ASSERT_MATCHING_ENUM

void WebMediaPlayerAndroid::OnKeyError(const std::string& key_system,
                                       const std::string& session_id,
                                       media::MediaKeys::KeyError error_code,
                                       int system_code) {
  EmeUMAHistogramEnumeration(
      key_system, "KeyError", error_code, media::MediaKeys::kMaxKeyError);

  client_->keyError(
      WebString::fromUTF8(key_system),
      WebString::fromUTF8(session_id),
      static_cast<WebKit::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      system_code);
}

void WebMediaPlayerAndroid::OnKeyMessage(const std::string& key_system,
                                         const std::string& session_id,
                                         const std::string& message,
                                         const std::string& default_url) {
  const GURL default_url_gurl(default_url);
  DLOG_IF(WARNING, !default_url.empty() && !default_url_gurl.is_valid())
      << "Invalid URL in default_url: " << default_url;

  client_->keyMessage(WebString::fromUTF8(key_system),
                      WebString::fromUTF8(session_id),
                      reinterpret_cast<const uint8*>(message.data()),
                      message.size(),
                      default_url_gurl);
}
#endif  // defined(GOOGLE_TV)

void WebMediaPlayerAndroid::OnNeedKey(const std::string& key_system,
                                      const std::string& session_id,
                                      const std::string& type,
                                      scoped_ptr<uint8[]> init_data,
                                      int init_data_size) {
  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!decryptor_)
    return;

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  client_->keyNeeded(WebString::fromUTF8(key_system),
                     WebString::fromUTF8(session_id),
                     init_data.get(),
                     init_data_size);
}

void WebMediaPlayerAndroid::OnReadFromDemuxer(
    media::DemuxerStream::Type type, bool seek_done) {
  if (media_source_delegate_)
    media_source_delegate_->OnReadFromDemuxer(type, seek_done);
  else
    NOTIMPLEMENTED();
}

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
