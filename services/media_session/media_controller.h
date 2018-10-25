// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {

// MediaController provides a control surface over Mojo for controlling a
// specific MediaSession. If |session_| is nullptr then all commands will be
// dropped.
class MediaController : public mojom::MediaController {
 public:
  MediaController();
  ~MediaController() override;

  // mojom::MediaController overrides.
  void Suspend() override;
  void Resume() override;
  void ToggleSuspendResume() override;

  void SetMediaSession(mojom::MediaSession*, mojom::MediaPlaybackState);
  void ClearMediaSession();

  void BindToInterface(mojom::MediaControllerRequest);
  void FlushForTesting();

 private:
  // Holds mojo bindings for mojom::MediaController.
  mojo::BindingSet<mojom::MediaController> bindings_;

  // The current playback state of the |session_|.
  mojom::MediaPlaybackState playback_state_ =
      mojom::MediaPlaybackState::kPaused;

  // Raw pointer to the local proxy. This is used for sending control events to
  // the underlying MediaSession.
  mojom::MediaSession* session_ = nullptr;

  // Protects |session_| as it is not thread safe.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
