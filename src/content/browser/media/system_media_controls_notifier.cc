// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/win/system_media_controls/system_media_controls_service.h"

namespace content {

using ABI::Windows::Media::MediaPlaybackStatus;

SystemMediaControlsNotifier::SystemMediaControlsNotifier(
    service_manager::Connector* connector)
    : connector_(connector) {}

SystemMediaControlsNotifier::~SystemMediaControlsNotifier() = default;

void SystemMediaControlsNotifier::Initialize() {
  // |service_| can be set in tests.
  if (!service_)
    service_ = system_media_controls::SystemMediaControlsService::GetInstance();

  // |service_| can still be null if the current system does not support System
  // Media Transport Controls.
  if (!service_)
    return;

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&controller_manager_ptr));
  controller_manager_ptr->CreateActiveMediaController(
      mojo::MakeRequest(&media_controller_ptr_));

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_session::mojom::MediaControllerObserverPtr media_controller_observer;
  media_controller_observer_binding_.Bind(
      mojo::MakeRequest(&media_controller_observer));
  media_controller_ptr_->AddObserver(std::move(media_controller_observer));
}

void SystemMediaControlsNotifier::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  DCHECK(service_);

  session_info_ = std::move(session_info);
  if (session_info_) {
    if (session_info_->playback_state ==
        media_session::mojom::MediaPlaybackState::kPlaying) {
      service_->SetPlaybackStatus(
          MediaPlaybackStatus::MediaPlaybackStatus_Playing);
    } else {
      service_->SetPlaybackStatus(
          MediaPlaybackStatus::MediaPlaybackStatus_Paused);
    }
  } else {
    service_->SetPlaybackStatus(
        MediaPlaybackStatus::MediaPlaybackStatus_Stopped);
  }
}

}  // namespace content
