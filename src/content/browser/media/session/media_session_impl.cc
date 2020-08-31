// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/gfx/favicon_size.h"

#if defined(OS_ANDROID)
#include "content/browser/media/session/media_session_android.h"
#endif  // defined(OS_ANDROID)

namespace content {

using blink::mojom::MediaSessionPlaybackState;
using MediaSessionUserAction = MediaSessionUmaHelper::MediaSessionUserAction;
using media_session::mojom::MediaAudioVideoState;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionImageType;
using media_session::mojom::MediaSessionInfo;

namespace {

const double kUnduckedVolumeMultiplier = 1.0;
const double kDefaultDuckingVolumeMultiplier = 0.2;

const char kDebugInfoOwnerSeparator[] = " - ";

using MapRenderFrameHostToDepth = std::map<RenderFrameHost*, size_t>;

using media_session::mojom::AudioFocusType;

using MediaSessionSuspendedSource =
    MediaSessionUmaHelper::MediaSessionSuspendedSource;

const char kMediaSessionDataName[] = "MediaSessionDataName";

class MediaSessionData : public base::SupportsUserData::Data {
 public:
  MediaSessionData() = default;

  static MediaSessionData* GetOrCreate(BrowserContext* context) {
    auto* data = static_cast<MediaSessionData*>(
        context->GetUserData(kMediaSessionDataName));

    if (!data) {
      auto new_data = std::make_unique<MediaSessionData>();
      data = new_data.get();
      context->SetUserData(kMediaSessionDataName, std::move(new_data));
    }

    return data;
  }

  const base::UnguessableToken& source_id() const { return source_id_; }

 private:
  base::UnguessableToken source_id_ = base::UnguessableToken::Create();

  DISALLOW_COPY_AND_ASSIGN(MediaSessionData);
};

size_t ComputeFrameDepth(RenderFrameHost* rfh,
                         MapRenderFrameHostToDepth* map_rfh_to_depth) {
  DCHECK(rfh);
  size_t depth = 0;
  RenderFrameHost* current_frame = rfh;
  while (current_frame) {
    auto it = map_rfh_to_depth->find(current_frame);
    if (it != map_rfh_to_depth->end()) {
      depth += it->second;
      break;
    }
    ++depth;
    current_frame = current_frame->GetParent();
  }
  (*map_rfh_to_depth)[rfh] = depth;
  return depth;
}

MediaSessionUserAction MediaSessionActionToUserAction(
    media_session::mojom::MediaSessionAction action) {
  switch (action) {
    case media_session::mojom::MediaSessionAction::kPlay:
      return MediaSessionUserAction::Play;
    case media_session::mojom::MediaSessionAction::kPause:
      return MediaSessionUserAction::Pause;
    case media_session::mojom::MediaSessionAction::kPreviousTrack:
      return MediaSessionUserAction::PreviousTrack;
    case media_session::mojom::MediaSessionAction::kNextTrack:
      return MediaSessionUserAction::NextTrack;
    case media_session::mojom::MediaSessionAction::kSeekBackward:
      return MediaSessionUserAction::SeekBackward;
    case media_session::mojom::MediaSessionAction::kSeekForward:
      return MediaSessionUserAction::SeekForward;
    case media_session::mojom::MediaSessionAction::kSkipAd:
      return MediaSessionUserAction::SkipAd;
    case media_session::mojom::MediaSessionAction::kStop:
      return MediaSessionUserAction::Stop;
    case media_session::mojom::MediaSessionAction::kSeekTo:
      return MediaSessionUserAction::SeekTo;
    case media_session::mojom::MediaSessionAction::kScrubTo:
      return MediaSessionUserAction::ScrubTo;
    case media_session::mojom::MediaSessionAction::kEnterPictureInPicture:
      return MediaSessionUserAction::EnterPictureInPicture;
    case media_session::mojom::MediaSessionAction::kExitPictureInPicture:
      return MediaSessionUserAction::ExitPictureInPicture;
  }
  NOTREACHED();
  return MediaSessionUserAction::Play;
}

// If the string is not empty then push it to the back of a vector.
void MaybePushBackString(std::vector<std::string>& vector,
                         const std::string& str) {
  if (!str.empty())
    vector.push_back(str);
}

bool IsSizeAtLeast(const gfx::Size& size, int min_size) {
  return size.width() >= min_size || size.height() >= min_size;
}

base::string16 SanitizeMediaTitle(const base::string16 title) {
  base::string16 out;
  base::TrimString(title, base::ASCIIToUTF16(" "), &out);
  return out;
}

}  // anonymous namespace

MediaSessionImpl::PlayerIdentifier::PlayerIdentifier(
    MediaSessionPlayerObserver* observer,
    int player_id)
    : observer(observer), player_id(player_id) {}

bool MediaSessionImpl::PlayerIdentifier::operator==(
    const PlayerIdentifier& other) const {
  return this->observer == other.observer && this->player_id == other.player_id;
}

bool MediaSessionImpl::PlayerIdentifier::operator<(
    const PlayerIdentifier& other) const {
  return MediaSessionImpl::PlayerIdentifier::Hash()(*this) <
         MediaSessionImpl::PlayerIdentifier::Hash()(other);
}

size_t MediaSessionImpl::PlayerIdentifier::Hash::operator()(
    const PlayerIdentifier& player_identifier) const {
  size_t hash =
      std::hash<MediaSessionPlayerObserver*>()(player_identifier.observer);
  hash += std::hash<int>()(player_identifier.player_id);
  return hash;
}

// static
MediaSession* MediaSession::Get(WebContents* web_contents) {
  return MediaSessionImpl::Get(web_contents);
}

// static
const base::UnguessableToken& MediaSession::GetSourceId(
    BrowserContext* browser_context) {
  return MediaSessionData::GetOrCreate(browser_context)->source_id();
}

// static
WebContents* MediaSession::GetWebContentsFromRequestId(
    const base::UnguessableToken& request_id) {
  DCHECK_NE(base::UnguessableToken::Null(), request_id);
  for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
    MediaSessionImpl* session = MediaSessionImpl::FromWebContents(web_contents);
    if (!session)
      continue;
    if (session->GetRequestId() == request_id)
      return web_contents;
  }
  return nullptr;
}

// static
MediaSessionImpl* MediaSessionImpl::Get(WebContents* web_contents) {
  MediaSessionImpl* session = FromWebContents(web_contents);
  if (!session) {
    CreateForWebContents(web_contents);
    session = FromWebContents(web_contents);
    session->Initialize();
  }
  return session;
}

MediaSessionImpl::~MediaSessionImpl() {
  DCHECK(normal_players_.empty());
  DCHECK(pepper_players_.empty());
  DCHECK(one_shot_players_.empty());
  DCHECK(audio_focus_state_ == State::INACTIVE);
}

void MediaSessionImpl::WebContentsDestroyed() {
  // This should only work for tests. In production, all the players should have
  // already been removed before WebContents is destroyed.

  // TODO(zqzhang): refactor MediaSessionImpl, maybe move the interface used to
  // talk with AudioFocusManager out to a seperate class. The AudioFocusManager
  // unit tests then could mock the interface and abandon audio focus when
  // WebContents is destroyed. See https://crbug.com/651069
  normal_players_.clear();
  pepper_players_.clear();
  one_shot_players_.clear();

  AbandonSystemAudioFocusIfNeeded();
}

void MediaSessionImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (services_.count(rfh))
    OnServiceDestroyed(services_[rfh]);
}

void MediaSessionImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
  if (services_.count(rfh))
    services_[rfh]->DidFinishNavigation();

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::OnWebContentsFocused(RenderWidgetHost*) {
  focused_ = true;

#if !defined(OS_ANDROID) && !defined(OS_MACOSX)
  // If we have just gained focus and we have audio focus we should re-request
  // system audio focus. This will ensure this media session is towards the top
  // of the stack if we have multiple sessions active at the same time.
  if (audio_focus_state_ == State::ACTIVE)
    RequestSystemAudioFocus(desired_audio_focus_type_);
#endif
}

void MediaSessionImpl::OnWebContentsLostFocus(RenderWidgetHost*) {
  focused_ = false;
}

void MediaSessionImpl::TitleWasSet(NavigationEntry* entry) {
  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::DidUpdateFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  std::vector<media_session::MediaImage> icons;

  for (auto& icon : candidates) {
    // We only want either favicons or the touch icons. There is another type of
    // touch icon which is "precomposed". This means it might have rounded
    // corners, etc. but it is not predictable so we cannot show them in the UI.
    if (icon->icon_type != blink::mojom::FaviconIconType::kFavicon &&
        icon->icon_type != blink::mojom::FaviconIconType::kTouchIcon) {
      continue;
    }

    std::vector<gfx::Size> sizes = icon->icon_sizes;

    // If we are a favicon and we do not have a size then we should assume the
    // default size for favicons.
    if (icon->icon_type == blink::mojom::FaviconIconType::kFavicon &&
        sizes.empty())
      sizes.push_back(gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));

    if (sizes.empty() || !icon->icon_url.is_valid())
      continue;

    media_session::MediaImage image;
    image.src = icon->icon_url;
    image.sizes = sizes;
    icons.push_back(image);
  }

  auto it = images_.find(MediaSessionImageType::kSourceIcon);
  if (it != images_.end() && it->second == icons)
    return;

  images_.insert_or_assign(MediaSessionImageType::kSourceIcon, icons);

  for (auto& observer : observers_)
    observer->MediaSessionImagesChanged(this->images_);
}

void MediaSessionImpl::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  RebuildAndNotifyMediaSessionInfoChanged();
}

bool MediaSessionImpl::AddPlayer(MediaSessionPlayerObserver* observer,
                                 int player_id,
                                 media::MediaContentType media_content_type) {
  if (media_content_type == media::MediaContentType::OneShot)
    return AddOneShotPlayer(observer, player_id);
  if (media_content_type == media::MediaContentType::Pepper)
    return AddPepperPlayer(observer, player_id);

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  AudioFocusType required_audio_focus_type;
  if (media_content_type == media::MediaContentType::Persistent)
    required_audio_focus_type = AudioFocusType::kGain;
  else
    required_audio_focus_type = AudioFocusType::kGainTransientMayDuck;

  PlayerIdentifier key(observer, player_id);

  // If the audio focus is already granted and is of type Content, there is
  // nothing to do. If it is granted of type Transient the requested type is
  // also transient, there is also nothing to do. Otherwise, the session needs
  // to request audio focus again.
  if (audio_focus_state_ == State::ACTIVE) {
    base::Optional<AudioFocusType> current_focus_type =
        delegate_->GetCurrentFocusType();
    if (current_focus_type == AudioFocusType::kGain ||
        current_focus_type == required_audio_focus_type) {
      auto iter = normal_players_.find(key);
      if (iter == normal_players_.end())
        normal_players_.emplace(std::move(key), required_audio_focus_type);
      else
        iter->second = required_audio_focus_type;

      UpdateRoutedService();
      RebuildAndNotifyMediaPositionChanged();
      return true;
    }
  }

  State old_audio_focus_state = audio_focus_state_;
  RequestSystemAudioFocus(required_audio_focus_type);

  if (audio_focus_state_ != State::ACTIVE)
    return false;

  // The session should be reset if a player is starting while all players are
  // suspended.
  if (old_audio_focus_state != State::ACTIVE)
    normal_players_.clear();

  auto iter = normal_players_.find(key);
  if (iter == normal_players_.end())
    normal_players_.emplace(std::move(key), required_audio_focus_type);
  else
    iter->second = required_audio_focus_type;

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();

  return true;
}

void MediaSessionImpl::RemovePlayer(MediaSessionPlayerObserver* observer,
                                    int player_id) {
  PlayerIdentifier identifier(observer, player_id);

  auto iter = normal_players_.find(identifier);
  if (iter != normal_players_.end())
    normal_players_.erase(iter);

  auto it = pepper_players_.find(identifier);
  if (it != pepper_players_.end())
    pepper_players_.erase(it);

  it = one_shot_players_.find(identifier);
  if (it != one_shot_players_.end())
    one_shot_players_.erase(it);

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::RemovePlayers(MediaSessionPlayerObserver* observer) {
  for (auto it = normal_players_.begin(); it != normal_players_.end();) {
    if (it->first.observer == observer)
      normal_players_.erase(it++);
    else
      ++it;
  }

  for (auto it = pepper_players_.begin(); it != pepper_players_.end();) {
    if (it->observer == observer)
      pepper_players_.erase(it++);
    else
      ++it;
  }

  for (auto it = one_shot_players_.begin(); it != one_shot_players_.end();) {
    if (it->observer == observer)
      one_shot_players_.erase(it++);
    else
      ++it;
  }

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::RecordSessionDuck() {
  uma_helper_.RecordSessionSuspended(
      MediaSessionSuspendedSource::SystemTransientDuck);
}

void MediaSessionImpl::OnPlayerPaused(MediaSessionPlayerObserver* observer,
                                      int player_id) {
  // If a playback is completed, BrowserMediaPlayerManager will call
  // OnPlayerPaused() after RemovePlayer(). This is a workaround.
  // Also, this method may be called when a player that is not added
  // to this session (e.g. a silent video) is paused. MediaSessionImpl
  // should ignore the paused player for this case.
  PlayerIdentifier identifier(observer, player_id);
  if (!normal_players_.count(identifier) &&
      !pepper_players_.count(identifier) &&
      !one_shot_players_.count(identifier)) {
    return;
  }

  // If the player to be removed is a pepper player, or there is more than one
  // observer, remove the paused one from the session.
  if (pepper_players_.count(identifier) || normal_players_.size() != 1) {
    RemovePlayer(observer, player_id);
    return;
  }

  // If the player is a one-shot player, just remove it since it is not expected
  // to resume a one-shot player via resuming MediaSession.
  if (one_shot_players_.count(identifier)) {
    RemovePlayer(observer, player_id);
    return;
  }

  // Otherwise, suspend the session.
  DCHECK(IsActive());
  OnSuspendInternal(SuspendType::kContent, State::SUSPENDED);
}

void MediaSessionImpl::RebuildAndNotifyMediaPositionChanged() {
  base::Optional<media_session::MediaPosition> position;

  // If there was a position specified from Blink then we should use that.
  if (routed_service_ && routed_service_->position())
    position = routed_service_->position();

  // If we only have a single player then we should use the position from that.
  if (!position && normal_players_.size() == 1 && one_shot_players_.empty() &&
      pepper_players_.empty()) {
    auto& first = normal_players_.begin()->first;
    position = first.observer->GetPosition(first.player_id);
  }

  if (position == position_)
    return;

  position_ = position;

  for (auto& observer : observers_)
    observer->MediaSessionPositionChanged(position_);
}

void MediaSessionImpl::Resume(SuspendType suspend_type) {
  if (!IsSuspended())
    return;

  if (suspend_type == SuspendType::kUI) {
    // If the site has registered an action handler for play then we should
    // pass it to the site and let them handle it.
    if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kPlay)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay);
      return;
    }

    MediaSessionUmaHelper::RecordMediaSessionUserAction(
        MediaSessionUmaHelper::MediaSessionUserAction::PlayDefault, focused_);
  }

  // When the resume requests comes from another source than system, audio focus
  // must be requested.
  if (suspend_type != SuspendType::kSystem) {
    // Request audio focus again in case we lost it because another app started
    // playing while the playback was paused. If the audio focus request is
    // delayed we will resume the player when the request completes.
    AudioFocusDelegate::AudioFocusResult result =
        RequestSystemAudioFocus(desired_audio_focus_type_);

    SetAudioFocusState(result != AudioFocusDelegate::AudioFocusResult::kFailed
                           ? State::ACTIVE
                           : State::INACTIVE);

    if (audio_focus_state_ != State::ACTIVE)
      return;
  }

  OnResumeInternal(suspend_type);
}

void MediaSessionImpl::Suspend(SuspendType suspend_type) {
  if (!IsActive())
    return;

  if (suspend_type == SuspendType::kUI) {
    // If the site has registered an action handler for pause then we should
    // pass it to the site and let them handle it.
    if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kPause)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause);
      return;
    }

    MediaSessionUmaHelper::RecordMediaSessionUserAction(
        MediaSessionUserAction::PauseDefault, focused_);
  }

  OnSuspendInternal(suspend_type, State::SUSPENDED);
}

void MediaSessionImpl::Stop(SuspendType suspend_type) {
  DCHECK(audio_focus_state_ != State::INACTIVE);
  DCHECK(suspend_type != SuspendType::kContent);
  DCHECK(!HasPepper());

  if (suspend_type == SuspendType::kUI) {
    // If the site has registered an action handle for stop then we should
    // notify the site but continue stopping the media session.
    if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kStop)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kStop);
    } else {
      MediaSessionUmaHelper::RecordMediaSessionUserAction(
          MediaSessionUmaHelper::MediaSessionUserAction::StopDefault, focused_);
    }
  }

  if (auto* pip_window_controller_ =
          PictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->Close(false /* should_pause_video */);
  }

  // TODO(mlamouri): merge the logic between UI and SYSTEM.
  if (suspend_type == SuspendType::kSystem) {
    OnSuspendInternal(suspend_type, State::INACTIVE);
    return;
  }

  if (audio_focus_state_ != State::SUSPENDED)
    OnSuspendInternal(suspend_type, State::SUSPENDED);

  DCHECK(audio_focus_state_ == State::SUSPENDED);
  normal_players_.clear();

  AbandonSystemAudioFocusIfNeeded();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::Seek(base::TimeDelta seek_time) {
  DCHECK(!seek_time.is_zero());

  if (seek_time > base::TimeDelta()) {
    // If the site has registered an action handler for seek forward then we
    // should pass it to the site and let them handle it.
    if (ShouldRouteAction(
            media_session::mojom::MediaSessionAction::kSeekForward)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekForward);
      return;
    }

    for (const auto& it : normal_players_)
      it.first.observer->OnSeekForward(it.first.player_id, seek_time);
  } else if (seek_time < base::TimeDelta()) {
    // If the site has registered an action handler for seek backward then we
    // should pass it to the site and let them handle it.
    if (ShouldRouteAction(
            media_session::mojom::MediaSessionAction::kSeekBackward)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekBackward);
      return;
    }

    for (const auto& it : normal_players_)
      it.first.observer->OnSeekBackward(it.first.player_id, seek_time * -1);
  }
}

bool MediaSessionImpl::IsControllable() const {
  // If the session does not have audio focus or it has one shot players then it
  // cannot be controllable.
  if (audio_focus_state_ == State::INACTIVE || !one_shot_players_.empty())
    return false;

#if !defined(OS_ANDROID)
  if (routed_service_ && routed_service_->playback_state() !=
                             blink::mojom::MediaSessionPlaybackState::NONE) {
    return true;
  }
#endif

  return desired_audio_focus_type_ == AudioFocusType::kGain;
}

void MediaSessionImpl::SetDuckingVolumeMultiplier(double multiplier) {
  ducking_volume_multiplier_ = base::ClampToRange(multiplier, 0.0, 1.0);
}

void MediaSessionImpl::SetAudioFocusGroupId(
    const base::UnguessableToken& group_id) {
  audio_focus_group_id_ = group_id;
}

void MediaSessionImpl::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  UpdateVolumeMultiplier();
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  UpdateVolumeMultiplier();
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::UpdateVolumeMultiplier() {
  for (const auto& it : normal_players_) {
    it.first.observer->OnSetVolumeMultiplier(it.first.player_id,
                                             GetVolumeMultiplier());
  }

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());
}

double MediaSessionImpl::GetVolumeMultiplier() const {
  return is_ducking_ ? ducking_volume_multiplier_ : kUnduckedVolumeMultiplier;
}

bool MediaSessionImpl::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

bool MediaSessionImpl::IsSuspended() const {
  return audio_focus_state_ == State::SUSPENDED;
}

bool MediaSessionImpl::HasPepper() const {
  return !pepper_players_.empty();
}

void MediaSessionImpl::SetDelegateForTests(
    std::unique_ptr<AudioFocusDelegate> delegate) {
  delegate_ = std::move(delegate);
}

MediaSessionUmaHelper* MediaSessionImpl::uma_helper_for_test() {
  return &uma_helper_;
}

void MediaSessionImpl::RemoveAllPlayersForTest() {
  normal_players_.clear();
  pepper_players_.clear();
  one_shot_players_.clear();
  AbandonSystemAudioFocusIfNeeded();
}

void MediaSessionImpl::OnImageDownloadComplete(
    GetMediaImageBitmapCallback callback,
    int minimum_size_px,
    int desired_size_px,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  DCHECK(bitmaps.size() == sizes.size());
  SkBitmap image;
  double best_image_score = 0.0;

  // Rank |sizes| and |bitmaps| using MediaImageManager.
  for (size_t i = 0; i < bitmaps.size(); i++) {
    double image_score = media_session::MediaImageManager::GetImageSizeScore(
        minimum_size_px, desired_size_px, sizes.at(i));

    if (image_score > best_image_score)
      image = bitmaps.at(i);
  }

  // If the image is the wrong color type then we should convert it.
  SkBitmap bitmap;
  if (!image.isNull()) {
    if (image.colorType() == kRGBA_8888_SkColorType) {
      bitmap = image;
    } else {
      SkImageInfo info = image.info().makeColorType(kRGBA_8888_SkColorType);
      if (bitmap.tryAllocPixels(info))
        image.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
    }
  }

  std::move(callback).Run(bitmap);
}

void MediaSessionImpl::OnSystemAudioFocusRequested(bool result) {
  uma_helper_.RecordRequestAudioFocusResult(result);
  if (result)
    StopDucking();
}

void MediaSessionImpl::OnSuspendInternal(SuspendType suspend_type,
                                         State new_state) {
  DCHECK(!HasPepper());

  DCHECK(new_state == State::SUSPENDED || new_state == State::INACTIVE);
  // UI suspend cannot use State::INACTIVE.
  DCHECK(suspend_type == SuspendType::kSystem || new_state == State::SUSPENDED);

  if (!one_shot_players_.empty())
    return;

  if (audio_focus_state_ != State::ACTIVE)
    return;

  switch (suspend_type) {
    case SuspendType::kUI:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::UI);
      break;
    case SuspendType::kSystem:
      switch (new_state) {
        case State::SUSPENDED:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemTransient);
          break;
        case State::INACTIVE:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemPermanent);
          break;
        case State::ACTIVE:
          NOTREACHED();
          break;
      }
      break;
    case SuspendType::kContent:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::CONTENT);
      break;
  }

  SetAudioFocusState(new_state);
  suspend_type_ = suspend_type;

  if (suspend_type != SuspendType::kContent) {
    // SuspendType::CONTENT happens when the suspend action came from
    // the page in which case the player is already paused.
    // Otherwise, the players need to be paused.
    for (const auto& it : normal_players_)
      it.first.observer->OnSuspend(it.first.player_id);
  }

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id,
                                       ducking_volume_multiplier_);

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::OnResumeInternal(SuspendType suspend_type) {
  if (suspend_type == SuspendType::kSystem && suspend_type_ != suspend_type)
    return;

  SetAudioFocusState(State::ACTIVE);

  for (const auto& it : normal_players_)
    it.first.observer->OnResume(it.first.player_id);

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());

  RebuildAndNotifyMediaSessionInfoChanged();
}

MediaSessionImpl::MediaSessionImpl(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      audio_focus_state_(State::INACTIVE),
      desired_audio_focus_type_(AudioFocusType::kGainTransientMayDuck),
      is_ducking_(false),
      ducking_volume_multiplier_(kDefaultDuckingVolumeMultiplier),
      routed_service_(nullptr) {
#if defined(OS_ANDROID)
  session_android_.reset(new MediaSessionAndroid(this));
#endif  // defined(OS_ANDROID)

  if (web_contents && web_contents->GetMainFrame() &&
      web_contents->GetMainFrame()->GetView()) {
    focused_ = web_contents->GetMainFrame()->GetView()->HasFocus();
  }

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::Initialize() {
  delegate_ = AudioFocusDelegate::Create(this);
  delegate_->MediaSessionInfoChanged(GetMediaSessionInfoSync());

  DCHECK(web_contents());
  DidUpdateFaviconURL(web_contents()->GetFaviconURLs());
}

AudioFocusDelegate::AudioFocusResult MediaSessionImpl::RequestSystemAudioFocus(
    AudioFocusType audio_focus_type) {
  // |kGainTransient| is not used in MediaSessionImpl.
  DCHECK_NE(media_session::mojom::AudioFocusType::kGainTransient,
            audio_focus_type);

  AudioFocusDelegate::AudioFocusResult result =
      delegate_->RequestAudioFocus(audio_focus_type);
  desired_audio_focus_type_ = audio_focus_type;

  bool success = result != AudioFocusDelegate::AudioFocusResult::kFailed;
  SetAudioFocusState(success ? State::ACTIVE : State::INACTIVE);

  // If we are delayed then we should return now and wait for the response from
  // the audio focus delegate.
  if (result == AudioFocusDelegate::AudioFocusResult::kDelayed)
    return result;

  OnSystemAudioFocusRequested(success);
  return result;
}

mojo::PendingRemote<media_session::mojom::MediaSession>
MediaSessionImpl::AddRemote() {
  mojo::PendingRemote<media_session::mojom::MediaSession> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void MediaSessionImpl::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());

  // Add the title and the url to the owner.
  std::vector<std::string> owner_parts;
  MaybePushBackString(owner_parts,
                      base::UTF16ToUTF8(web_contents()->GetTitle()));
  MaybePushBackString(owner_parts,
                      web_contents()->GetLastCommittedURL().spec());
  info->owner = base::JoinString(owner_parts, kDebugInfoOwnerSeparator);

  std::move(callback).Run(std::move(info));
}

media_session::mojom::MediaSessionInfoPtr
MediaSessionImpl::GetMediaSessionInfoSync() {
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());

  switch (audio_focus_state_) {
    case State::ACTIVE:
      info->state = MediaSessionInfo::SessionState::kActive;
      break;
    case State::SUSPENDED:
      info->state = MediaSessionInfo::SessionState::kSuspended;
      break;
    case State::INACTIVE:
      info->state = MediaSessionInfo::SessionState::kInactive;
      break;
  }

  // The state should always be kDucked if we are ducked.
  if (is_ducking_)
    info->state = MediaSessionInfo::SessionState::kDucking;

  // If we have Pepper players then we should force ducking.
  info->force_duck = HasPepper();

  // The playback state should use |IsActive| to determine whether we are
  // playing or not. However, if there is a |routed_service_| which is playing
  // then we should force the playback state to be playing.
  info->playback_state =
      IsActive() ? MediaPlaybackState::kPlaying : MediaPlaybackState::kPaused;
  if (routed_service_ &&
      routed_service_->playback_state() == MediaSessionPlaybackState::PLAYING) {
    info->playback_state = MediaPlaybackState::kPlaying;
  }

  info->audio_video_state = GetMediaAudioVideoState();
  info->is_controllable = IsControllable();

  // If the browser context is off the record then it should be sensitive.
  info->is_sensitive = web_contents()->GetBrowserContext()->IsOffTheRecord();

  info->picture_in_picture_state =
      web_contents()->HasPictureInPictureVideo()
          ? media_session::mojom::MediaPictureInPictureState::
                kInPictureInPicture
          : media_session::mojom::MediaPictureInPictureState::
                kNotInPictureInPicture;

  return info;
}

void MediaSessionImpl::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoSync());
}

void MediaSessionImpl::AddObserver(
    mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer) {
  mojo::Remote<media_session::mojom::MediaSessionObserver>
      media_session_observer(std::move(observer));
  media_session_observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());
  media_session_observer->MediaSessionMetadataChanged(metadata_);
  media_session_observer->MediaSessionImagesChanged(images_);
  media_session_observer->MediaSessionPositionChanged(position_);

  std::vector<media_session::mojom::MediaSessionAction> actions(
      actions_.begin(), actions_.end());
  media_session_observer->MediaSessionActionsChanged(actions);

  observers_.Add(std::move(media_session_observer));
}

void MediaSessionImpl::FinishSystemAudioFocusRequest(
    AudioFocusType audio_focus_type,
    bool result) {
  // If the media session is not active then we do not need to enforce the
  // result of the audio focus request.
  if (audio_focus_state_ != State::ACTIVE) {
    AbandonSystemAudioFocusIfNeeded();
    return;
  }

  OnSystemAudioFocusRequested(result);

  if (!result && !HasPepper()) {
    switch (audio_focus_type) {
      case AudioFocusType::kGain:
        // If the gain audio focus request failed then we should suspend the
        // media session.
        OnSuspendInternal(SuspendType::kSystem, State::SUSPENDED);
        break;
      case AudioFocusType::kAmbient:
      case AudioFocusType::kGainTransient:
        // MediaSessionImpl does not use |kGainTransient| or |kAmbient|.
        NOTREACHED();
        break;
      case AudioFocusType::kGainTransientMayDuck:
        // The focus request failed, we should suspend any players that have
        // the same audio focus type.
        for (auto& player : normal_players_) {
          if (audio_focus_type == player.second)
            player.first.observer->OnSuspend(player.first.player_id);
        }
        break;
    }
  }
}

void MediaSessionImpl::PreviousTrack() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kPreviousTrack);
}

void MediaSessionImpl::NextTrack() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kNextTrack);
}

void MediaSessionImpl::SkipAd() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kSkipAd);
}

void MediaSessionImpl::SeekTo(base::TimeDelta seek_time) {
  DidReceiveAction(
      media_session::mojom::MediaSessionAction::kSeekTo,
      blink::mojom::MediaSessionActionDetails::NewSeekTo(
          blink::mojom::MediaSessionSeekToDetails::New(seek_time, false)));
}

void MediaSessionImpl::ScrubTo(base::TimeDelta seek_time) {
  DidReceiveAction(
      media_session::mojom::MediaSessionAction::kSeekTo,
      blink::mojom::MediaSessionActionDetails::NewSeekTo(
          blink::mojom::MediaSessionSeekToDetails::New(seek_time, true)));
}

void MediaSessionImpl::EnterPictureInPicture() {
  if (ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kEnterPictureInPicture)) {
    DidReceiveAction(
        media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
    return;
  }

  // There should be one and only one player when we enter picture-in-picture.
  DCHECK_EQ(normal_players_.size(), 1u);
  normal_players_.begin()->first.observer->OnEnterPictureInPicture(
      normal_players_.begin()->first.player_id);
}

void MediaSessionImpl::ExitPictureInPicture() {
  if (ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kExitPictureInPicture)) {
    DidReceiveAction(
        media_session::mojom::MediaSessionAction::kExitPictureInPicture);
    return;
  }

  // There should be one and only one player when we exit picture-in-picture.
  DCHECK_EQ(normal_players_.size(), 1u);
  normal_players_.begin()->first.observer->OnExitPictureInPicture(
      normal_players_.begin()->first.player_id);
}

void MediaSessionImpl::GetMediaImageBitmap(
    const media_session::MediaImage& image,
    int minimum_size_px,
    int desired_size_px,
    GetMediaImageBitmapCallback callback) {
  // We should make sure |image| is in |images_|.
  bool found = false;
  for (auto& image_type : images_)
    found = found || base::Contains(image_type.second, image);

  // Check that |image.sizes| contains a size that is above the minimum size.
  bool check_size = false;
  for (auto& size : image.sizes)
    check_size = check_size || IsSizeAtLeast(size, minimum_size_px);

  if (!found || !check_size) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  web_contents()->DownloadImage(
      image.src, false /* is_favicon */, desired_size_px /* preferred_size */,
      desired_size_px /* max_bitmap_size */, false /* bypass_cache */,
      base::BindOnce(&MediaSessionImpl::OnImageDownloadComplete,
                     base::Unretained(this),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(callback), SkBitmap()),
                     minimum_size_px, desired_size_px));
}

void MediaSessionImpl::AbandonSystemAudioFocusIfNeeded() {
  if (audio_focus_state_ == State::INACTIVE || !normal_players_.empty() ||
      !pepper_players_.empty() || !one_shot_players_.empty()) {
    return;
  }
  delegate_->AbandonAudioFocus();
  is_ducking_ = false;

  SetAudioFocusState(State::INACTIVE);
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::SetAudioFocusState(State audio_focus_state) {
  if (audio_focus_state == audio_focus_state_)
    return;

  audio_focus_state_ = audio_focus_state;
  switch (audio_focus_state_) {
    case State::ACTIVE:
      uma_helper_.OnSessionActive();
      break;
    case State::SUSPENDED:
      uma_helper_.OnSessionSuspended();
      break;
    case State::INACTIVE:
      uma_helper_.OnSessionInactive();
      break;
  }

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::FlushForTesting() {
  observers_.FlushForTesting();
}

void MediaSessionImpl::RebuildAndNotifyMediaSessionInfoChanged() {
  media_session::mojom::MediaSessionInfoPtr current_info =
      GetMediaSessionInfoSync();

  if (current_info == session_info_)
    return;

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(current_info.Clone());

  delegate_->MediaSessionInfoChanged(current_info.Clone());

  session_info_ = std::move(current_info);
}

bool MediaSessionImpl::AddPepperPlayer(MediaSessionPlayerObserver* observer,
                                       int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);

  if (result == AudioFocusDelegate::AudioFocusResult::kFailed)
    return false;

  pepper_players_.insert(PlayerIdentifier(observer, player_id));

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();

  return result != AudioFocusDelegate::AudioFocusResult::kFailed;
}

bool MediaSessionImpl::AddOneShotPlayer(MediaSessionPlayerObserver* observer,
                                        int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);

  if (result == AudioFocusDelegate::AudioFocusResult::kFailed)
    return false;

  one_shot_players_.insert(PlayerIdentifier(observer, player_id));

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();

  return true;
}

// MediaSessionService-related methods

void MediaSessionImpl::OnServiceCreated(MediaSessionServiceImpl* service) {
  RenderFrameHost* rfh = service->GetRenderFrameHost();
  if (!rfh)
    return;

  content::BackForwardCache::DisableForRenderFrameHost(
      rfh, "MediaSessionImpl::OnServiceCreated");

  services_[rfh] = service;
  UpdateRoutedService();
}

void MediaSessionImpl::OnServiceDestroyed(MediaSessionServiceImpl* service) {
  services_.erase(service->GetRenderFrameHost());

  if (routed_service_ == service)
    UpdateRoutedService();
}

void MediaSessionImpl::OnMediaSessionPlaybackStateChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::OnMediaSessionMetadataChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::OnMediaSessionActionsChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::DidReceiveAction(
    media_session::mojom::MediaSessionAction action) {
  DidReceiveAction(action, nullptr /* details */);
}

void MediaSessionImpl::DidReceiveAction(
    media_session::mojom::MediaSessionAction action,
    blink::mojom::MediaSessionActionDetailsPtr details) {
  MediaSessionUmaHelper::RecordMediaSessionUserAction(
      MediaSessionActionToUserAction(action), focused_);

  // Pause all players in non-routed frames if the action is PAUSE.
  //
  // This is the default PAUSE action handler per Media Session API spec. The
  // reason for pausing all players in all other sessions is to avoid the
  // players in other frames keep the session active so that the UI will always
  // show the pause button but it does not pause anything (as the routed frame
  // already pauses when responding to the PAUSE action while other frames does
  // not).
  //
  // TODO(zqzhang): Currently, this might not work well on desktop as Pepper and
  // OneShot players are not really suspended, so that the session is still
  // active after this. See https://crbug.com/619084 and
  // https://crbug.com/596516.
  if (media_session::mojom::MediaSessionAction::kPause == action) {
    RenderFrameHost* rfh_of_routed_service =
        routed_service_ ? routed_service_->GetRenderFrameHost() : nullptr;
    for (const auto& player : normal_players_) {
      if (player.first.observer->render_frame_host() != rfh_of_routed_service)
        player.first.observer->OnSuspend(player.first.player_id);
    }
    for (const auto& player : pepper_players_) {
      if (player.observer->render_frame_host() != rfh_of_routed_service) {
        player.observer->OnSetVolumeMultiplier(player.player_id,
                                               ducking_volume_multiplier_);
      }
    }
    for (const auto& player : one_shot_players_) {
      if (player.observer->render_frame_host() != rfh_of_routed_service)
        player.observer->OnSuspend(player.player_id);
    }
  }

  if (!routed_service_)
    return;

  routed_service_->GetClient()->DidReceiveAction(action, std::move(details));
}

bool MediaSessionImpl::IsServiceActiveForRenderFrameHost(RenderFrameHost* rfh) {
  return services_.find(rfh) != services_.end();
}

void MediaSessionImpl::UpdateRoutedService() {
  MediaSessionServiceImpl* new_service = ComputeServiceForRouting();

  if (new_service == routed_service_)
    return;

  routed_service_ = new_service;

  RebuildAndNotifyMetadataChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();
}

MediaSessionServiceImpl* MediaSessionImpl::ComputeServiceForRouting() {
  // The service selection strategy is: select a frame that has a playing/paused
  // player and has a corresponding MediaSessionService and return the
  // corresponding MediaSessionService. If multiple frames satisfy the criteria,
  // prefer the top-most frame.
  std::set<RenderFrameHost*> frames;
  for (const auto& player : normal_players_) {
    RenderFrameHost* frame = player.first.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  for (const auto& player : one_shot_players_) {
    RenderFrameHost* frame = player.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  for (const auto& player : pepper_players_) {
    RenderFrameHost* frame = player.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  RenderFrameHost* best_frame = nullptr;
  size_t min_depth = std::numeric_limits<size_t>::max();
  std::map<RenderFrameHost*, size_t> map_rfh_to_depth;

  for (RenderFrameHost* frame : frames) {
    size_t depth = ComputeFrameDepth(frame, &map_rfh_to_depth);
    if (depth >= min_depth)
      continue;
    if (!IsServiceActiveForRenderFrameHost(frame))
      continue;
    best_frame = frame;
    min_depth = depth;
  }

  return best_frame ? services_[best_frame] : nullptr;
}

void MediaSessionImpl::OnPictureInPictureAvailabilityChanged() {
  if (normal_players_.size() != 1)
    return;

  RebuildAndNotifyActionsChanged();
}

bool MediaSessionImpl::ShouldRouteAction(
    media_session::mojom::MediaSessionAction action) const {
  return routed_service_ && base::Contains(routed_service_->actions(), action);
}

const base::UnguessableToken& MediaSessionImpl::GetSourceId() const {
  return MediaSessionData::GetOrCreate(web_contents()->GetBrowserContext())
      ->source_id();
}

const base::UnguessableToken& MediaSessionImpl::GetRequestId() const {
  return delegate_->request_id();
}

void MediaSessionImpl::RebuildAndNotifyActionsChanged() {
  std::set<media_session::mojom::MediaSessionAction> actions =
      routed_service_ ? routed_service_->actions()
                      : std::set<media_session::mojom::MediaSessionAction>();

  // Picture-in-Picture window controller needs to know only actions that are
  // handled by the website.
  if (auto* pip_window_controller_ =
          PictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->MediaSessionActionsChanged(actions);
  }

  // If we are controllable then we should always add these actions as we can
  // support them by directly interacting with the players underneath.
  if (IsControllable()) {
    actions.insert(media_session::mojom::MediaSessionAction::kPlay);
    actions.insert(media_session::mojom::MediaSessionAction::kPause);
    actions.insert(media_session::mojom::MediaSessionAction::kStop);
  }

  if (base::FeatureList::IsEnabled(
          media::kGlobalMediaControlsPictureInPicture) &&
      IsPictureInPictureAvailable()) {
    actions.insert(
        media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
    actions.insert(
        media_session::mojom::MediaSessionAction::kExitPictureInPicture);
  }

  // If we support kSeekTo then we support kScrubTo as well.
  if (base::Contains(actions,
                     media_session::mojom::MediaSessionAction::kSeekTo)) {
    actions.insert(media_session::mojom::MediaSessionAction::kScrubTo);
  }

  if (actions_ == actions)
    return;

  actions_ = actions;

  std::vector<media_session::mojom::MediaSessionAction> actions_vec(
      actions.begin(), actions.end());
  for (auto& observer : observers_)
    observer->MediaSessionActionsChanged(actions_vec);
}

void MediaSessionImpl::RebuildAndNotifyMetadataChanged() {
  std::vector<media_session::MediaImage> artwork;
  media_session::MediaMetadata metadata;

  if (routed_service_ && routed_service_->metadata()) {
    metadata.title = routed_service_->metadata()->title;
    metadata.artist = routed_service_->metadata()->artist;
    metadata.album = routed_service_->metadata()->album;
    artwork = routed_service_->metadata()->artwork;
  }

  if (metadata.title.empty())
    metadata.title = SanitizeMediaTitle(web_contents()->GetTitle());

  ContentClient* content_client = content::GetContentClient();
  const GURL& url = web_contents()->GetLastCommittedURL();

  // If the url is a file then we should display a placeholder.
  base::string16 formatted_origin =
      url.SchemeIsFile()
          ? content_client->GetLocalizedString(IDS_MEDIA_SESSION_FILE_SOURCE)
          : url_formatter::FormatUrl(
                url::Origin::Create(url).GetURL(),
                url_formatter::kFormatUrlOmitDefaults |
                    url_formatter::kFormatUrlOmitHTTPS |
                    url_formatter::kFormatUrlOmitTrivialSubdomains,
                net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  metadata.source_title = formatted_origin;

  // If we have no artwork in |images_| or the arwork has changed then we should
  // update it with the latest artwork from the routed service.
  auto it = images_.find(MediaSessionImageType::kArtwork);
  bool images_changed = it == images_.end() || it->second != artwork;
  if (images_changed)
    images_.insert_or_assign(MediaSessionImageType::kArtwork, artwork);

  bool metadata_changed = metadata_ != metadata;
  if (metadata_changed)
    metadata_ = metadata;

  if (!images_changed && !metadata_changed)
    return;
  for (auto& observer : observers_) {
    if (metadata_changed)
      observer->MediaSessionMetadataChanged(this->metadata_);

    if (images_changed)
      observer->MediaSessionImagesChanged(this->images_);
  }
}

bool MediaSessionImpl::IsPictureInPictureAvailable() const {
  if (normal_players_.size() != 1)
    return false;

  auto& first = normal_players_.begin()->first;
  return first.observer->IsPictureInPictureAvailable(first.player_id);
}

MediaAudioVideoState MediaSessionImpl::GetMediaAudioVideoState() {
  RenderFrameHost* routed_rfh =
      routed_service_ ? routed_service_->GetRenderFrameHost() : nullptr;
  MediaAudioVideoState state = MediaAudioVideoState::kUnknown;

  ForAllPlayers(base::BindRepeating(
      [](RenderFrameHost* routed_rfh, MediaAudioVideoState* state,
         const PlayerIdentifier& player) {
        // If we have a routed frame then we should limit the players to the
        // frame so it is aligned with the media metadata.
        if (routed_rfh && player.observer->render_frame_host() != routed_rfh)
          return;

        if (player.observer->HasVideo(player.player_id))
          *state = MediaAudioVideoState::kAudioVideo;

        if (*state != MediaAudioVideoState::kAudioVideo)
          *state = MediaAudioVideoState::kAudioOnly;
      },
      routed_rfh, &state));

  return state;
}

void MediaSessionImpl::ForAllPlayers(
    base::RepeatingCallback<void(const PlayerIdentifier&)> callback) {
  for (const auto& player : normal_players_)
    callback.Run(player.first);

  for (const auto& player : one_shot_players_)
    callback.Run(player);

  for (const auto& player : pepper_players_)
    callback.Run(player);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaSessionImpl)

}  // namespace content
