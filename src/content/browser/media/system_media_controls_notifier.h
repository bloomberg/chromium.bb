// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
#define CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace system_media_controls {
class SystemMediaControlsService;
}  // namespace system_media_controls

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// The SystemMediaControlsNotifier connects to Window's "System Media Transport
// Controls" and keeps the OS informed of the current media playstate and
// metadata. It observes changes to the active Media Session and updates the
// SMTC accordingly.
class CONTENT_EXPORT SystemMediaControlsNotifier
    : public media_session::mojom::MediaControllerObserver {
 public:
  explicit SystemMediaControlsNotifier(service_manager::Connector* connector);
  ~SystemMediaControlsNotifier() override;

  void Initialize();

  // media_session::mojom::MediaControllerObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}

  void SetSystemMediaControlsServiceForTesting(
      system_media_controls::SystemMediaControlsService* service) {
    service_ = service;
  }

 private:
  // Our connection to Window's System Media Transport Controls.
  system_media_controls::SystemMediaControlsService* service_ = nullptr;

  // used to connect to the Media Session service.
  service_manager::Connector* connector_;

  // Tracks current media session state/metadata.
  media_session::mojom::MediaControllerPtr media_controller_ptr_;
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Used to receive updates to the active media controller.
  mojo::Binding<media_session::mojom::MediaControllerObserver>
      media_controller_observer_binding_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsNotifier);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
