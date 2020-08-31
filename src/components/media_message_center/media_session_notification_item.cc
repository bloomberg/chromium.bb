// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_session_notification_item.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/media_message_center/media_notification_constants.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_view.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

MediaSessionNotificationItem::Source GetSource(const std::string& name) {
  if (name == "web")
    return MediaSessionNotificationItem::Source::kWeb;

  if (name == "arc")
    return MediaSessionNotificationItem::Source::kArc;

  if (name == "assistant")
    return MediaSessionNotificationItem::Source::kAssistant;

  return MediaSessionNotificationItem::Source::kUnknown;
}

// How long to wait (in milliseconds) for a new media session to begin.
constexpr base::TimeDelta kFreezeTimerDelay =
    base::TimeDelta::FromMilliseconds(2500);

}  // namespace

MediaSessionNotificationItem::MediaSessionNotificationItem(
    MediaNotificationController* notification_controller,
    const std::string& request_id,
    const std::string& source_name,
    mojo::Remote<media_session::mojom::MediaController> controller,
    media_session::mojom::MediaSessionInfoPtr session_info)
    : controller_(notification_controller),
      request_id_(request_id),
      source_(GetSource(source_name)) {
  DCHECK(controller_);

  if (auto task_runner = notification_controller->GetTaskRunner())
    freeze_timer_.SetTaskRunner(task_runner);

  SetController(std::move(controller), std::move(session_info));
}

MediaSessionNotificationItem::~MediaSessionNotificationItem() {
  controller_->HideNotification(request_id_);
}

void MediaSessionNotificationItem::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  session_info_ = std::move(session_info);

  MaybeUnfreeze();
  MaybeHideOrShowNotification();

  if (view_ && !frozen_)
    view_->UpdateWithMediaSessionInfo(session_info_);
}

void MediaSessionNotificationItem::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  session_metadata_ = metadata.value_or(media_session::MediaMetadata());

  view_needs_metadata_update_ = true;

  MaybeUnfreeze();
  MaybeHideOrShowNotification();

  // |MaybeHideOrShowNotification()| can synchronously create a
  // MediaNotificationView that calls |SetView()|. If that happens, then we
  // don't want to call |view_->UpdateWithMediaMetadata()| below since |view_|
  // will have already received the metadata when calling |SetView()|.
  // |view_needs_metadata_update_| is set to false in |SetView()|. The reason we
  // want to avoid sending the metadata twice is that metrics are recorded when
  // metadata is set and we don't want to double-count metrics.
  if (view_ && view_needs_metadata_update_ && !frozen_)
    view_->UpdateWithMediaMetadata(session_metadata_);

  view_needs_metadata_update_ = false;
}

void MediaSessionNotificationItem::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  session_actions_ = base::flat_set<media_session::mojom::MediaSessionAction>(
      actions.begin(), actions.end());

  if (view_ && !frozen_) {
    DCHECK(view_);
    view_->UpdateWithMediaActions(session_actions_);
  } else if (waiting_for_actions_) {
    MaybeUnfreeze();
  }
}

void MediaSessionNotificationItem::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (type == media_session::mojom::MediaSessionImageType::kSourceIcon) {
    session_favicon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    if (view_ && !frozen_)
      view_->UpdateWithFavicon(*session_favicon_);

    return;
  }

  DCHECK_EQ(media_session::mojom::MediaSessionImageType::kArtwork, type);

  session_artwork_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  if (view_ && !frozen_)
    view_->UpdateWithMediaArtwork(*session_artwork_);
  else if (waiting_for_artwork_)
    MaybeUnfreeze();
}

void MediaSessionNotificationItem::SetView(MediaNotificationView* view) {
  DCHECK(view_ || view);

  view_ = view;

  if (view_) {
    view_needs_metadata_update_ = false;
    view_->UpdateWithMediaSessionInfo(session_info_);
    view_->UpdateWithMediaMetadata(session_metadata_);
    view_->UpdateWithMediaActions(session_actions_);

    if (session_artwork_.has_value())
      view_->UpdateWithMediaArtwork(*session_artwork_);
    if (session_favicon_.has_value())
      view_->UpdateWithFavicon(*session_favicon_);
  }
}

void MediaSessionNotificationItem::OnMediaSessionActionButtonPressed(
    MediaSessionAction action) {
  UMA_HISTOGRAM_ENUMERATION(kUserActionHistogramName, action);

  if (frozen_)
    return;

  controller_->LogMediaSessionActionButtonPressed(request_id_, action);
  media_session::PerformMediaSessionAction(action, media_controller_remote_);
}

void MediaSessionNotificationItem::Dismiss() {
  if (media_controller_remote_.is_bound())
    media_controller_remote_->Stop();
  controller_->RemoveItem(request_id_);
}

void MediaSessionNotificationItem::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller,
    media_session::mojom::MediaSessionInfoPtr session_info) {
  observer_receiver_.reset();
  artwork_observer_receiver_.reset();
  favicon_observer_receiver_.reset();

  is_bound_ = true;
  media_controller_remote_ = std::move(controller);
  session_info_ = std::move(session_info);

  if (media_controller_remote_.is_bound()) {
    // Bind an observer to the associated media controller.
    media_controller_remote_->AddObserver(
        observer_receiver_.BindNewPipeAndPassRemote());

    // TODO(https://crbug.com/931397): Use dip to calculate the size.
    // Bind an observer to be notified when the artwork changes.
    media_controller_remote_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kArtwork,
        kMediaSessionNotificationArtworkMinSize,
        kMediaSessionNotificationArtworkDesiredSize,
        artwork_observer_receiver_.BindNewPipeAndPassRemote());

    media_controller_remote_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        gfx::kFaviconSize, kMediaSessionNotificationArtworkDesiredSize,
        favicon_observer_receiver_.BindNewPipeAndPassRemote());
  }

  MaybeHideOrShowNotification();
}

void MediaSessionNotificationItem::Freeze(base::OnceClosure unfrozen_callback) {
  is_bound_ = false;
  unfrozen_callback_ = std::move(unfrozen_callback);

  if (frozen_)
    return;

  frozen_ = true;
  frozen_with_actions_ = HasActions();
  frozen_with_artwork_ = HasArtwork();

  freeze_timer_.Start(
      FROM_HERE, kFreezeTimerDelay,
      base::BindOnce(&MediaSessionNotificationItem::OnFreezeTimerFired,
                     base::Unretained(this)));
}

void MediaSessionNotificationItem::FlushForTesting() {
  media_controller_remote_.FlushForTesting();
}

bool MediaSessionNotificationItem::ShouldShowNotification() const {
  // If the |is_controllable| bit is set in MediaSessionInfo then we should show
  // a media notification.
  if (!session_info_ || !session_info_->is_controllable)
    return false;

  // If we do not have a title then we should hide the notification.
  if (session_metadata_.title.empty())
    return false;

  return true;
}

void MediaSessionNotificationItem::MaybeUnfreeze() {
  if (!frozen_)
    return;

  if (waiting_for_actions_ && !HasActions())
    return;

  if (waiting_for_artwork_ && !HasArtwork())
    return;

  if (!ShouldShowNotification() || !is_bound_)
    return;

  // If the currently frozen view has actions and the new session currently has
  // no actions, then wait until either the freeze timer ends or the new actions
  // are received.
  if (frozen_with_actions_ && !HasActions()) {
    waiting_for_actions_ = true;
    return;
  }

  // If the currently frozen view has artwork and the new session currently has
  // no artwork, then wait until either the freeze timer ends or the new artwork
  // is downloaded.
  if (frozen_with_artwork_ && !HasArtwork()) {
    waiting_for_artwork_ = true;
    return;
  }

  Unfreeze();
}

void MediaSessionNotificationItem::Unfreeze() {
  frozen_ = false;
  waiting_for_actions_ = false;
  frozen_with_actions_ = false;
  waiting_for_artwork_ = false;
  frozen_with_artwork_ = false;
  freeze_timer_.Stop();

  // When we unfreeze, we want to fully update |view_| with any changes that
  // we've avoided sending during the freeze.
  if (view_) {
    view_needs_metadata_update_ = false;
    view_->UpdateWithMediaSessionInfo(session_info_);
    view_->UpdateWithMediaMetadata(session_metadata_);
    view_->UpdateWithMediaActions(session_actions_);

    if (session_artwork_.has_value())
      view_->UpdateWithMediaArtwork(*session_artwork_);
    if (session_favicon_.has_value())
      view_->UpdateWithFavicon(*session_favicon_);
  }

  std::move(unfrozen_callback_).Run();
}

bool MediaSessionNotificationItem::HasActions() const {
  return !session_actions_.empty();
}

bool MediaSessionNotificationItem::HasArtwork() const {
  return session_artwork_.has_value() && !session_artwork_->isNull();
}

void MediaSessionNotificationItem::OnFreezeTimerFired() {
  DCHECK(frozen_);

  // If we've just been waiting for actions or artwork, stop waiting and just
  // show what we have.
  if ((waiting_for_actions_ || waiting_for_artwork_) &&
      ShouldShowNotification() && is_bound_) {
    Unfreeze();
    return;
  }

  if (is_bound_) {
    controller_->HideNotification(request_id_);
  } else {
    controller_->RemoveItem(request_id_);
  }
}

void MediaSessionNotificationItem::MaybeHideOrShowNotification() {
  if (frozen_)
    return;

  if (!ShouldShowNotification()) {
    controller_->HideNotification(request_id_);
    return;
  }

  // If we have an existing view, then we don't need to create a new one.
  if (view_)
    return;

  controller_->ShowNotification(request_id_);

  UMA_HISTOGRAM_ENUMERATION(kSourceHistogramName, source_);
}

}  // namespace media_message_center
