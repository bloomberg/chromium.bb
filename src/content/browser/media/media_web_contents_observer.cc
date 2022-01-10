// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/audible_metrics.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

AudibleMetrics* GetAudibleMetrics() {
  static AudibleMetrics* metrics = new AudibleMetrics();
  return metrics;
}

static void OnAudioOutputDeviceIdTranslated(
    base::WeakPtr<MediaWebContentsObserver> observer,
    const MediaPlayerId& player_id,
    const absl::optional<std::string>& raw_device_id) {
  if (!raw_device_id)
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaWebContentsObserver::OnReceivedTranslatedDeviceId,
                     std::move(observer), player_id, raw_device_id.value()));
}

}  // anonymous namespace

// Maintains state for a single player.  Issues WebContents and power-related
// notifications appropriate for state changes.
class MediaWebContentsObserver::PlayerInfo {
 public:
  PlayerInfo(const MediaPlayerId& id, MediaWebContentsObserver* observer)
      : id_(id), observer_(observer) {}

  ~PlayerInfo() {
    if (is_playing_) {
      NotifyPlayerStopped(WebContentsObserver::MediaStoppedReason::kUnspecified,
                          MediaPowerExperimentManager::NotificationMode::kSkip);
    }
  }

  PlayerInfo(const PlayerInfo&) = delete;
  PlayerInfo& operator=(const PlayerInfo&) = delete;

  void set_has_audio(bool has_audio) { has_audio_ = has_audio; }

  bool has_video() const { return has_video_; }
  void set_has_video(bool has_video) { has_video_ = has_video; }

  void set_muted(bool muted) { muted_ = muted; }

  bool is_playing() const { return is_playing_; }

  void SetIsPlaying() {
    DCHECK(!is_playing_);
    is_playing_ = true;

    NotifyPlayerStarted();
  }

  void SetIsStopped(bool reached_end_of_stream) {
    DCHECK(is_playing_);
    is_playing_ = false;

    NotifyPlayerStopped(
        reached_end_of_stream
            ? WebContentsObserver::MediaStoppedReason::kReachedEndOfStream
            : WebContentsObserver::MediaStoppedReason::kUnspecified,
        MediaPowerExperimentManager::NotificationMode::kNotify);
  }

  bool IsAudible() const { return has_audio_ && is_playing_ && !muted_; }

 private:
  void NotifyPlayerStarted() {
    observer_->web_contents_impl()->MediaStartedPlaying(
        WebContentsObserver::MediaPlayerInfo(has_video_, has_audio_), id_);

    if (observer_->power_experiment_manager_) {
      auto* render_frame_host = RenderFrameHost::FromID(id_.frame_routing_id);
      DCHECK(render_frame_host);

      // Bind the callback to a WeakPtr for the frame, so that we won't try to
      // notify the frame after it's been destroyed.
      observer_->power_experiment_manager_->PlayerStarted(
          id_, base::BindRepeating(
                   &MediaWebContentsObserver::OnExperimentStateChanged,
                   observer_->GetWeakPtrForFrame(render_frame_host), id_));
    }
  }

  void NotifyPlayerStopped(
      WebContentsObserver::MediaStoppedReason stopped_reason,
      MediaPowerExperimentManager::NotificationMode notification_mode) {
    observer_->web_contents_impl()->MediaStoppedPlaying(
        WebContentsObserver::MediaPlayerInfo(has_video_, has_audio_), id_,
        stopped_reason);

    if (observer_->power_experiment_manager_) {
      observer_->power_experiment_manager_->PlayerStopped(id_,
                                                          notification_mode);
    }
  }

  const MediaPlayerId id_;
  const raw_ptr<MediaWebContentsObserver> observer_;

  bool has_audio_ = false;
  bool has_video_ = false;
  bool muted_ = false;
  bool is_playing_ = false;
};

MediaWebContentsObserver::MediaWebContentsObserver(
    WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      audible_metrics_(GetAudibleMetrics()),
      session_controllers_manager_(
          std::make_unique<MediaSessionControllersManager>(web_contents)),
      power_experiment_manager_(MediaPowerExperimentManager::Instance()) {}

MediaWebContentsObserver::~MediaWebContentsObserver() = default;

void MediaWebContentsObserver::WebContentsDestroyed() {
  use_after_free_checker_.check();
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  audible_metrics_->WebContentsDestroyed(
      web_contents(), audio_stream_monitor->WasRecentlyAudible() &&
                          !web_contents()->IsAudioMuted());

  // Remove all players so that the experiment manager is notified.
  player_info_map_.clear();

  // Remove all the mojo receivers and remotes associated to the media players
  // handled by this WebContents to prevent from handling/sending any more
  // messages after this point, plus properly cleaning things up.
  media_player_hosts_.clear();
  media_player_observer_hosts_.clear();
  media_player_remotes_.clear();

  session_controllers_manager_.reset();
  fullscreen_player_.reset();
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  use_after_free_checker_.check();

  GlobalRenderFrameHostId frame_routing_id = render_frame_host->GetGlobalId();

  base::EraseIf(
      player_info_map_,
      [frame_routing_id](const PlayerInfoMap::value_type& id_and_player_info) {
        return frame_routing_id == id_and_player_info.first.frame_routing_id;
      });

  base::EraseIf(media_player_hosts_,
                [frame_routing_id](const MediaPlayerHostImplMap::value_type&
                                       media_player_hosts_value_type) {
                  return frame_routing_id ==
                         media_player_hosts_value_type.first;
                });

  base::EraseIf(
      media_player_observer_hosts_,
      [frame_routing_id](const MediaPlayerObserverHostImplMap::value_type&
                             media_player_observer_hosts_value_type) {
        return frame_routing_id ==
               media_player_observer_hosts_value_type.first.frame_routing_id;
      });

  base::EraseIf(media_player_remotes_,
                [frame_routing_id](const MediaPlayerRemotesMap::value_type&
                                       media_player_remotes_value_type) {
                  return frame_routing_id ==
                         media_player_remotes_value_type.first.frame_routing_id;
                });

  session_controllers_manager_->RenderFrameDeleted(render_frame_host);

  if (fullscreen_player_ &&
      fullscreen_player_->frame_routing_id == frame_routing_id) {
    picture_in_picture_allowed_in_fullscreen_.reset();
    fullscreen_player_.reset();
  }

  // Cancel any pending callbacks for players from this frame.
  use_after_free_checker_.check();
  per_frame_factory_.erase(render_frame_host);
}

void MediaWebContentsObserver::MaybeUpdateAudibleState() {
  AudioStreamMonitor* audio_stream_monitor =
      web_contents_impl()->audio_stream_monitor();

  if (audio_stream_monitor->WasRecentlyAudible())
    LockAudio();
  else
    CancelAudioLock();

  audible_metrics_->UpdateAudibleWebContentsState(
      web_contents(), audio_stream_monitor->IsCurrentlyAudible() &&
                          !web_contents()->IsAudioMuted());
}

bool MediaWebContentsObserver::HasActiveEffectivelyFullscreenVideo() const {
  if (!web_contents()->IsFullscreen() || !fullscreen_player_)
    return false;

  // Check that the player is active.
  if (const PlayerInfo* player_info = GetPlayerInfo(*fullscreen_player_))
    return player_info->is_playing() && player_info->has_video();

  return false;
}

bool MediaWebContentsObserver::IsPictureInPictureAllowedForFullscreenVideo()
    const {
  DCHECK(picture_in_picture_allowed_in_fullscreen_.has_value());

  return *picture_in_picture_allowed_in_fullscreen_;
}

const absl::optional<MediaPlayerId>&
MediaWebContentsObserver::GetFullscreenVideoMediaPlayerId() const {
  return fullscreen_player_;
}

void MediaWebContentsObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  session_controllers_manager_->PictureInPictureStateChanged(
      is_picture_in_picture);
}

void MediaWebContentsObserver::DidUpdateAudioMutingState(bool muted) {
  session_controllers_manager_->WebContentsMutedStateChanged(muted);
}

void MediaWebContentsObserver::GetHasPlayedBefore(
    GetHasPlayedBeforeCallback callback) {
  std::move(callback).Run(has_played_before_);
}

void MediaWebContentsObserver::BindMediaPlayerObserverClient(
    mojo::PendingReceiver<media::mojom::MediaPlayerObserverClient>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void MediaWebContentsObserver::RequestPersistentVideo(bool value) {
  if (!fullscreen_player_)
    return;

  // The message is sent to the renderer even though the video is already the
  // fullscreen element itself. It will eventually be handled by Blink.
  GetMediaPlayerRemote(*fullscreen_player_)->SetPersistentState(value);
}

bool MediaWebContentsObserver::IsPlayerActive(
    const MediaPlayerId& player_id) const {
  if (const PlayerInfo* player_info = GetPlayerInfo(player_id))
    return player_info->is_playing();

  return false;
}

// MediaWebContentsObserver::MediaPlayerHostImpl

MediaWebContentsObserver::MediaPlayerHostImpl::MediaPlayerHostImpl(
    GlobalRenderFrameHostId frame_routing_id,
    MediaWebContentsObserver* media_web_contents_observer)
    : frame_routing_id_(frame_routing_id),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerHostImpl::~MediaPlayerHostImpl() = default;

void MediaWebContentsObserver::MediaPlayerHostImpl::BindMediaPlayerHostReceiver(
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaWebContentsObserver::MediaPlayerHostImpl::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> media_player,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
        media_player_observer,
    int32_t player_id) {
  media_web_contents_observer_->OnMediaPlayerAdded(
      std::move(media_player), std::move(media_player_observer),
      MediaPlayerId(frame_routing_id_, player_id));
}

// MediaWebContentsObserver::MediaPlayerObserverHostImpl

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    MediaPlayerObserverHostImpl(
        const MediaPlayerId& media_player_id,
        MediaWebContentsObserver* media_web_contents_observer)
    : media_player_id_(media_player_id),
      media_web_contents_observer_(media_web_contents_observer) {}

MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    ~MediaPlayerObserverHostImpl() = default;

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    BindMediaPlayerObserverReceiver(
        mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
            media_player_observer) {
  media_player_observer_receiver_.Bind(std::move(media_player_observer));

  // |media_web_contents_observer_| outlives MediaPlayerHostImpl, so it's safe
  // to use base::Unretained().
  media_player_observer_receiver_.set_disconnect_handler(base::BindOnce(
      &MediaWebContentsObserver::OnMediaPlayerObserverDisconnected,
      base::Unretained(media_web_contents_observer_), media_player_id_));
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMutedStatusChanged(bool muted) {
  media_web_contents_observer_->web_contents_impl()->MediaMutedStatusChanged(
      media_player_id_, muted);

  media_web_contents_observer_->session_controllers_manager()
      ->OnMediaMutedStatusChanged(media_player_id_, muted);

  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  player_info->set_muted(muted);
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaMetadataChanged(bool has_audio,
                           bool has_video,
                           media::MediaContentType media_content_type) {
  media_web_contents_observer_->OnMediaMetadataChanged(
      media_player_id_, has_audio, has_video, media_content_type);

  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaPositionStateChanged(
        const media_session::MediaPosition& media_position) {
  media_web_contents_observer_->session_controllers_manager()
      ->OnMediaPositionStateChanged(media_player_id_, media_position);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnMediaEffectivelyFullscreenChanged(
        blink::WebFullscreenVideoStatus status) {
  media_web_contents_observer_->OnMediaEffectivelyFullscreenChanged(
      media_player_id_, status);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaSizeChanged(
    const ::gfx::Size& size) {
  media_web_contents_observer_->web_contents_impl()->MediaResized(
      size, media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnPictureInPictureAvailabilityChanged(bool available) {
  media_web_contents_observer_->session_controllers_manager()
      ->OnPictureInPictureAvailabilityChanged(media_player_id_, available);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnAudioOutputSinkChanged(const std::string& hashed_device_id) {
  media_web_contents_observer_->OnAudioOutputSinkChanged(media_player_id_,
                                                         hashed_device_id);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnUseAudioServiceChanged(bool uses_audio_service) {
  uses_audio_service_ = uses_audio_service;
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    OnAudioOutputSinkChangingDisabled() {
  media_web_contents_observer_->session_controllers_manager()
      ->OnAudioOutputSinkChangingDisabled(media_player_id_);
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPlaying() {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  if (!media_web_contents_observer_->session_controllers_manager()->RequestPlay(
          media_player_id_)) {
    // Return early to avoid spamming WebContents with playing/stopped
    // notifications.  If RequestPlay() fails, media session will send a pause
    // signal right away.
    return;
  }

  if (!player_info->is_playing())
    player_info->SetIsPlaying();

  media_web_contents_observer_->OnMediaPlaying();
  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::OnMediaPaused(
    bool stream_ended) {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info || !player_info->is_playing())
    return;

  player_info->SetIsStopped(stream_ended);

  media_web_contents_observer_->session_controllers_manager()->OnPause(
      media_player_id_, stream_ended);

  NotifyAudioStreamMonitorIfNeeded();
}

void MediaWebContentsObserver::MediaPlayerObserverHostImpl::
    NotifyAudioStreamMonitorIfNeeded() {
  PlayerInfo* player_info = GetPlayerInfo();
  if (!player_info)
    return;

  bool should_add_client = player_info->IsAudible() && !uses_audio_service_;
  auto* audio_stream_monitor =
      media_web_contents_observer_->web_contents_impl()->audio_stream_monitor();

  if (should_add_client && !audio_client_registration_) {
    audio_client_registration_ = audio_stream_monitor->RegisterAudibleClient();
  } else if (!should_add_client && audio_client_registration_) {
    audio_client_registration_.reset();
  }
}

MediaWebContentsObserver::PlayerInfo*
MediaWebContentsObserver::MediaPlayerObserverHostImpl::GetPlayerInfo() {
  return media_web_contents_observer_->GetPlayerInfo(media_player_id_);
}

// MediaWebContentsObserver

MediaWebContentsObserver::PlayerInfo* MediaWebContentsObserver::GetPlayerInfo(
    const MediaPlayerId& id) const {
  const auto it = player_info_map_.find(id);
  return it != player_info_map_.end() ? it->second.get() : nullptr;
}

void MediaWebContentsObserver::OnMediaMetadataChanged(
    const MediaPlayerId& player_id,
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  PlayerInfo* player_info = GetPlayerInfo(player_id);
  if (!player_info) {
    PlayerInfoMap::iterator it;
    std::tie(it, std::ignore) = player_info_map_.emplace(
        player_id, std::make_unique<PlayerInfo>(player_id, this));
    player_info = it->second.get();
  }

  player_info->set_has_audio(has_audio);
  player_info->set_has_video(has_video);

  session_controllers_manager_->OnMetadata(player_id, has_audio, has_video,
                                           media_content_type);
}

void MediaWebContentsObserver::OnMediaEffectivelyFullscreenChanged(
    const MediaPlayerId& player_id,
    blink::WebFullscreenVideoStatus fullscreen_status) {
  switch (fullscreen_status) {
    case blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled:
      fullscreen_player_ = player_id;
      picture_in_picture_allowed_in_fullscreen_ = true;
      break;
    case blink::WebFullscreenVideoStatus::
        kFullscreenAndPictureInPictureDisabled:
      fullscreen_player_ = player_id;
      picture_in_picture_allowed_in_fullscreen_ = false;
      break;
    case blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen:
      if (!fullscreen_player_ || *fullscreen_player_ != player_id)
        return;

      picture_in_picture_allowed_in_fullscreen_.reset();
      fullscreen_player_.reset();
      break;
  }

  bool is_fullscreen =
      (fullscreen_status !=
       blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
  web_contents_impl()->MediaEffectivelyFullscreenChanged(is_fullscreen);
}

void MediaWebContentsObserver::OnMediaPlaying() {
  has_played_before_ = true;
}

void MediaWebContentsObserver::OnAudioOutputSinkChanged(
    const MediaPlayerId& player_id,
    std::string hashed_device_id) {
  auto* render_frame_host = RenderFrameHost::FromID(player_id.frame_routing_id);
  DCHECK(render_frame_host);

  auto salt_and_origin = content::GetMediaDeviceSaltAndOrigin(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());

  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& hashed_device_id,
         base::OnceCallback<void(const absl::optional<std::string>&)>
             callback) {
        MediaStreamManager::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT, salt,
            std::move(origin), hashed_device_id,
            base::SequencedTaskRunnerHandle::Get(), std::move(callback));
      },
      salt_and_origin.device_id_salt, std::move(salt_and_origin.origin),
      hashed_device_id,
      base::BindOnce(&OnAudioOutputDeviceIdTranslated,
                     weak_ptr_factory_.GetWeakPtr(), player_id));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

void MediaWebContentsObserver::OnReceivedTranslatedDeviceId(
    const MediaPlayerId& player_id,
    const std::string& raw_device_id) {
  session_controllers_manager_->OnAudioOutputSinkChanged(player_id,
                                                         raw_device_id);
}

bool MediaWebContentsObserver::IsMediaPlayerRemoteAvailable(
    const MediaPlayerId& player_id) {
  return media_player_remotes_.contains(player_id);
}

mojo::AssociatedRemote<media::mojom::MediaPlayer>&
MediaWebContentsObserver::GetMediaPlayerRemote(const MediaPlayerId& player_id) {
  return media_player_remotes_.at(player_id);
}

void MediaWebContentsObserver::OnMediaPlayerObserverDisconnected(
    const MediaPlayerId& player_id) {
  DCHECK(media_player_observer_hosts_.contains(player_id));
  media_player_observer_hosts_.erase(player_id);
}

device::mojom::WakeLock* MediaWebContentsObserver::GetAudioWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!audio_wake_lock_) {
    mojo::PendingReceiver<device::mojom::WakeLock> receiver =
        audio_wake_lock_.BindNewPipeAndPassReceiver();
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::kPreventAppSuspension,
          device::mojom::WakeLockReason::kAudioPlayback, "Playing audio",
          std::move(receiver));
    }
  }
  return audio_wake_lock_.get();
}

void MediaWebContentsObserver::LockAudio() {
  GetAudioWakeLock()->RequestWakeLock();
  has_audio_wake_lock_for_testing_ = true;
}

void MediaWebContentsObserver::CancelAudioLock() {
  if (audio_wake_lock_)
    GetAudioWakeLock()->CancelWakeLock();
  has_audio_wake_lock_for_testing_ = false;
}

WebContentsImpl* MediaWebContentsObserver::web_contents_impl() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

void MediaWebContentsObserver::BindMediaPlayerHost(
    GlobalRenderFrameHostId frame_routing_id,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
        player_receiver) {
  if (!media_player_hosts_.contains(frame_routing_id)) {
    media_player_hosts_[frame_routing_id] =
        std::make_unique<MediaPlayerHostImpl>(frame_routing_id, this);
  }

  media_player_hosts_[frame_routing_id]->BindMediaPlayerHostReceiver(
      std::move(player_receiver));
}

void MediaWebContentsObserver::OnMediaPlayerAdded(
    mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
        media_player_observer,
    MediaPlayerId player_id) {
  auto* const rfh = RenderFrameHost::FromID(player_id.frame_routing_id);
  DCHECK(rfh);

  if (media_player_remotes_.contains(player_id)) {
    // Original remote associated with |player_id| will be overridden. If the
    // original player is still alive, this will break our ability to control
    // it from the browser process. We don't know that the original player is
    // actually still alive.
    // TODO(https://crbug.com/1172882): Determine the root cause of duplication
    // and/or refactor to make ID purely a browser-side concept.
    LOG(ERROR) << __func__ << " Duplicate media player id ("
               << player_id.delegate_id << ")";
  }

  media_player_remotes_[player_id].Bind(std::move(player_remote));
  media_player_remotes_[player_id].set_disconnect_handler(base::BindOnce(
      [](MediaWebContentsObserver* observer, const MediaPlayerId& player_id) {
        observer->player_info_map_.erase(player_id);
        observer->media_player_remotes_.erase(player_id);
        observer->session_controllers_manager_->OnEnd(player_id);
        if (observer->fullscreen_player_ &&
            *observer->fullscreen_player_ == player_id) {
          observer->fullscreen_player_.reset();
        }
        observer->web_contents_impl()->MediaDestroyed(player_id);
      },
      base::Unretained(this), player_id));

  // Create a new MediaPlayerObserverHostImpl for |player_id|, implementing the
  // media::mojom::MediaPlayerObserver mojo interface, to handle messages sent
  // from the MediaPlayer element in the renderer process.
  if (!media_player_observer_hosts_.contains(player_id)) {
    media_player_observer_hosts_[player_id] =
        std::make_unique<MediaPlayerObserverHostImpl>(player_id, this);
  }
  media_player_observer_hosts_[player_id]->BindMediaPlayerObserverReceiver(
      std::move(media_player_observer));
}

void MediaWebContentsObserver::SuspendAllMediaPlayers() {
  for (auto& media_player_remote : media_player_remotes_) {
    media_player_remote.second->SuspendForFrameClosed();
  }
}

void MediaWebContentsObserver::OnExperimentStateChanged(MediaPlayerId id,
                                                        bool is_starting) {
  use_after_free_checker_.check();

  GetMediaPlayerRemote(id)->SetPowerExperimentState(is_starting);
}

base::WeakPtr<MediaWebContentsObserver>
MediaWebContentsObserver::GetWeakPtrForFrame(
    RenderFrameHost* render_frame_host) {
  auto iter = per_frame_factory_.find(render_frame_host);
  if (iter != per_frame_factory_.end())
    return iter->second->GetWeakPtr();

  auto result = per_frame_factory_.emplace(std::make_pair(
      render_frame_host,
      std::make_unique<base::WeakPtrFactory<MediaWebContentsObserver>>(this)));
  return result.first->second->GetWeakPtr();
}

}  // namespace content
