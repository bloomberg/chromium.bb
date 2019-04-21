// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller.h"

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_view.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"

namespace ash {

namespace {

std::unique_ptr<message_center::MessageView> CreateCustomMediaNotificationView(
    const message_center::Notification& notification) {
  DCHECK_EQ(kMediaSessionNotificationCustomViewType,
            notification.custom_view_type());
  return std::make_unique<MediaNotificationView>(notification);
}

// The maximum number of media notifications to count when recording the
// Media.Notification.Count histogram. 20 was chosen because it would be very
// unlikely to see a user with 20+ things playing at once.
const int kMediaNotificationCountHistogramMax = 20;

}  // namespace

// static
const char MediaNotificationController::kCountHistogramName[] =
    "Media.Notification.Count";

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
                           mojo::MakeRequest(&controller_manager_ptr_));

  media_session::mojom::AudioFocusObserverPtr audio_focus_observer;
  audio_focus_observer_binding_.Bind(mojo::MakeRequest(&audio_focus_observer));
  audio_focus_ptr->AddObserver(std::move(audio_focus_observer));
}

MediaNotificationController::~MediaNotificationController() = default;

void MediaNotificationController::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  if (base::ContainsKey(notifications_, id))
    return;

  media_session::mojom::MediaControllerPtr controller;

  // |controller_manager_ptr_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_ptr_) {
    controller_manager_ptr_->CreateMediaControllerForSession(
        mojo::MakeRequest(&controller), *session->request_id);
  }

  notifications_.emplace(
      std::piecewise_construct, std::forward_as_tuple(id),
      std::forward_as_tuple(id, session->source_name.value_or(std::string()),
                            std::move(controller),
                            std::move(session->session_info)));
}

void MediaNotificationController::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  notifications_.erase(session->request_id->ToString());
}

void MediaNotificationController::SetView(const std::string& id,
                                          MediaNotificationView* view) {
  auto it = notifications_.find(id);
  if (it == notifications_.end())
    return;
  it->second.SetView(view);
}

void MediaNotificationController::RecordConcurrentNotificationCount() {
  UMA_HISTOGRAM_EXACT_LINEAR(
      kCountHistogramName,
      message_center::MessageCenter::Get()
          ->FindNotificationsByAppId(kMediaSessionNotifierId)
          .size(),
      kMediaNotificationCountHistogramMax);
}

}  // namespace ash
