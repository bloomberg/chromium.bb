// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller.h"

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_view.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view_factory.h"
#include "url/gurl.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

std::unique_ptr<message_center::MessageView> CreateCustomMediaNotificationView(
    const message_center::Notification& notification) {
  DCHECK_EQ(kMediaSessionNotificationCustomViewType,
            notification.custom_view_type());
  return std::make_unique<MediaNotificationView>(notification);
}

bool IsMediaSessionNotificationVisible() {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
             kMediaSessionNotificationId) != nullptr;
}

}  // namespace

MediaNotificationController::MediaNotificationController(
    service_manager::Connector* connector) {
  if (!message_center::MessageViewFactory::HasCustomNotificationViewFactory(
          kMediaSessionNotificationCustomViewType)) {
    message_center::MessageViewFactory::SetCustomNotificationViewFactory(
        kMediaSessionNotificationCustomViewType,
        base::BindRepeating(&CreateCustomMediaNotificationView));
  }

  // |connector| can be null in tests.
  if (!connector)
    return;

  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr;
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&audio_focus_ptr));
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&media_controller_ptr_));

  media_session::mojom::AudioFocusObserverPtr audio_focus_observer;
  audio_focus_observer_binding_.Bind(mojo::MakeRequest(&audio_focus_observer));
  audio_focus_ptr->AddObserver(std::move(audio_focus_observer));

  media_session::mojom::MediaSessionObserverPtr media_session_observer;
  media_session_observer_binding_.Bind(
      mojo::MakeRequest(&media_session_observer));
  media_controller_ptr_->AddObserver(std::move(media_session_observer));
}

MediaNotificationController::~MediaNotificationController() = default;

void MediaNotificationController::OnActiveSessionChanged(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  // Hide the notification if the active session is null.
  if (session.is_null()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kMediaSessionNotificationId, false);
    return;
  }

  if (IsMediaSessionNotificationVisible())
    return;

  session_info_ = std::move(session->session_info);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NotificationType::NOTIFICATION_TYPE_CUSTOM,
          kMediaSessionNotificationId, base::string16(), base::string16(),
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kMediaSessionNotifierId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &MediaNotificationController::OnNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr())),
          gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  // Set the priority to low to prevent the notification showing as a popup and
  // keep it at the bottom of the list.
  notification->set_priority(message_center::LOW_PRIORITY);

  notification->set_custom_view_type(kMediaSessionNotificationCustomViewType);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void MediaNotificationController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  session_info_ = std::move(session_info);

  if (view_)
    view_->UpdateWithMediaSessionInfo(session_info_);
}

void MediaNotificationController::FlushForTesting() {
  media_controller_ptr_.FlushForTesting();
}

void MediaNotificationController::SetView(MediaNotificationView* view) {
  DCHECK(view_ || view);

  view_ = view;

  if (view) {
    DCHECK(!session_info_.is_null());
    view_->UpdateWithMediaSessionInfo(session_info_);
  }
}

void MediaNotificationController::OnNotificationClicked(
    base::Optional<int> button_id) {
  DCHECK(button_id.has_value());

  switch (static_cast<MediaSessionAction>(*button_id)) {
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
  }
}

}  // namespace ash
