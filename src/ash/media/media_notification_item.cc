// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_item.h"

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/media/media_notification_view.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

MediaNotificationItem::Source GetSource(const std::string& name) {
  if (name == "web")
    return MediaNotificationItem::Source::kWeb;

  if (name == "arc")
    return MediaNotificationItem::Source::kArc;

  if (name == "assistant")
    return MediaNotificationItem::Source::kAssistant;

  return MediaNotificationItem::Source::kUnknown;
}

}  // namespace

// static
const char MediaNotificationItem::kSourceHistogramName[] =
    "Media.Notification.Source";

// static
const char MediaNotificationItem::kUserActionHistogramName[] =
    "Media.Notification.UserAction";

MediaNotificationItem::MediaNotificationItem(
    MediaNotificationController* notification_controller,
    const std::string& request_id,
    const std::string& source_name,
    media_session::mojom::MediaControllerPtr controller,
    media_session::mojom::MediaSessionInfoPtr session_info)
    : controller_(notification_controller),
      request_id_(request_id),
      source_(GetSource(source_name)),
      media_controller_ptr_(std::move(controller)),
      session_info_(std::move(session_info)) {
  DCHECK(controller_);

  if (media_controller_ptr_.is_bound()) {
    // Bind an observer to the associated media controller.
    media_session::mojom::MediaControllerObserverPtr media_controller_observer;
    observer_binding_.Bind(mojo::MakeRequest(&media_controller_observer));
    media_controller_ptr_->AddObserver(std::move(media_controller_observer));

    // TODO(https://crbug.com/931397): Use dip to calculate the size.
    // Bind an observer to be notified when the artwork changes.
    media_session::mojom::MediaControllerImageObserverPtr artwork_observer;
    artwork_observer_binding_.Bind(mojo::MakeRequest(&artwork_observer));
    media_controller_ptr_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kArtwork,
        kMediaSessionNotificationArtworkMinSize,
        kMediaSessionNotificationArtworkDesiredSize,
        std::move(artwork_observer));

    media_session::mojom::MediaControllerImageObserverPtr icon_observer;
    icon_observer_binding_.Bind(mojo::MakeRequest(&icon_observer));
    media_controller_ptr_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        message_center::kSmallImageSizeMD, message_center::kSmallImageSizeMD,
        std::move(icon_observer));
  }

  MaybeHideOrShowNotification();
}

MediaNotificationItem::~MediaNotificationItem() {
  controller_->HideNotification(request_id_);
}

void MediaNotificationItem::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  session_info_ = std::move(session_info);

  MaybeHideOrShowNotification();

  if (view_)
    view_->UpdateWithMediaSessionInfo(session_info_);
}

void MediaNotificationItem::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  session_metadata_ = metadata.value_or(media_session::MediaMetadata());

  view_needs_metadata_update_ = true;

  MaybeHideOrShowNotification();

  // |MaybeHideOrShowNotification()| can synchronously create a
  // MediaNotificationView that calls |SetView()|. If that happens, then we
  // don't want to call |view_->UpdateWithMediaMetadata()| below since |view_|
  // will have already received the metadata when calling |SetView()|.
  // |view_needs_metadata_update_| is set to false in |SetView()|.
  if (view_ && view_needs_metadata_update_)
    view_->UpdateWithMediaMetadata(session_metadata_);

  view_needs_metadata_update_ = false;
}

void MediaNotificationItem::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  session_actions_ = std::set<media_session::mojom::MediaSessionAction>(
      actions.begin(), actions.end());

  if (view_)
    view_->UpdateWithMediaActions(session_actions_);
}

void MediaNotificationItem::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  switch (type) {
    case media_session::mojom::MediaSessionImageType::kArtwork:
      session_artwork_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

      if (view_)
        view_->UpdateWithMediaArtwork(*session_artwork_);
      break;
    case media_session::mojom::MediaSessionImageType::kSourceIcon:
      session_icon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

      if (view_)
        view_->UpdateWithMediaIcon(session_icon_);
      break;
  }
}

void MediaNotificationItem::SetView(MediaNotificationView* view) {
  DCHECK(view_ || view);

  view_ = view;

  if (view) {
    view_needs_metadata_update_ = false;
    view_->UpdateWithMediaSessionInfo(session_info_);
    view_->UpdateWithMediaMetadata(session_metadata_);
    view_->UpdateWithMediaActions(session_actions_);

    if (session_artwork_.has_value())
      view_->UpdateWithMediaArtwork(*session_artwork_);
  }
}

void MediaNotificationItem::FlushForTesting() {
  media_controller_ptr_.FlushForTesting();
}

void MediaNotificationItem::MaybeHideOrShowNotification() {
  // If the |is_controllable| bit is set in MediaSessionInfo then we should show
  // a media notification.
  if (!session_info_ || !session_info_->is_controllable) {
    controller_->HideNotification(request_id_);
    return;
  }

  // If we do not have a title and an artist then we should hide the
  // notification.
  if (session_metadata_.title.empty() || session_metadata_.artist.empty()) {
    controller_->HideNotification(request_id_);
    return;
  }

  // If we have an existing view, then we don't need to create a new one.
  if (view_)
    return;

  controller_->ShowNotification(request_id_);

  UMA_HISTOGRAM_ENUMERATION(kSourceHistogramName, source_);
}

void MediaNotificationItem::OnMediaSessionActionButtonPressed(
    MediaSessionAction action) {
  UMA_HISTOGRAM_ENUMERATION(kUserActionHistogramName, action);

  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      media_controller_ptr_->PreviousTrack();
      break;
    case MediaSessionAction::kSeekBackward:
      media_controller_ptr_->Seek(kDefaultSeekTime * -1);
      break;
    case MediaSessionAction::kPlay:
      media_controller_ptr_->Resume();
      break;
    case MediaSessionAction::kPause:
      media_controller_ptr_->Suspend();
      break;
    case MediaSessionAction::kSeekForward:
      media_controller_ptr_->Seek(kDefaultSeekTime);
      break;
    case MediaSessionAction::kNextTrack:
      media_controller_ptr_->NextTrack();
      break;
    case MediaSessionAction::kStop:
      media_controller_ptr_->Stop();
      break;
    case MediaSessionAction::kSkipAd:
      break;
  }
}

}  // namespace ash
