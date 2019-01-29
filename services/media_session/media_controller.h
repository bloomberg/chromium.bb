// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_

#include <memory>

#include "base/optional.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {

// MediaController provides a control surface over Mojo for controlling a
// specific MediaSession. If |session_| is nullptr then all commands will be
// dropped. MediaController is also a MediaSessionObserver and will forward
// events to added observers.
class MediaController : public mojom::MediaController,
                        public mojom::MediaSessionObserver {
 public:
  MediaController();
  ~MediaController() override;

  // mojom::MediaController overrides.
  void Suspend() override;
  void Resume() override;
  void Stop() override;
  void ToggleSuspendResume() override;
  void AddObserver(mojom::MediaControllerObserverPtr observer) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void Seek(base::TimeDelta seek_time) override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(
      mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<MediaMetadata>&) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& action) override;

  // Sets the media session that the controller should be bound to. If the
  // session is already bound to the same session then we will return false.
  bool SetMediaSession(mojom::MediaSession* session);

  void BindToInterface(mojom::MediaControllerRequest request);
  void FlushForTesting();

 private:
  // Holds mojo bindings for mojom::MediaController.
  mojo::BindingSet<mojom::MediaController> bindings_;

  // The current info for the |session_|.
  mojom::MediaSessionInfoPtr session_info_;

  // The current metadata for |session_|.
  base::Optional<MediaMetadata> session_metadata_;

  // The current actions for |session_|.
  std::vector<mojom::MediaSessionAction> session_actions_;

  // Raw pointer to the local proxy. This is used for sending control events to
  // the underlying MediaSession.
  mojom::MediaSession* session_ = nullptr;

  // Observers that are observing |this|.
  mojo::InterfacePtrSet<mojom::MediaControllerObserver> observers_;

  // Binding for |this| to act as an observer to |session_|.
  mojo::Binding<mojom::MediaSessionObserver> session_binding_{this};

  // Protects |session_| as it is not thread safe.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
