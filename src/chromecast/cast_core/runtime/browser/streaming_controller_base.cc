// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_controller_base.h"

#include "base/bind.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

StreamingControllerBase::StreamingControllerBase(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    CastWebContents* cast_web_contents)
    : message_port_(std::move(message_port)) {
  DCHECK(message_port_);

  CastWebContents::Observer::Observe(cast_web_contents);
}

StreamingControllerBase::~StreamingControllerBase() = default;

void StreamingControllerBase::ProcessAVConstraints(
    cast_streaming::ReceiverSession::AVConstraints* constraints) {}

void StreamingControllerBase::MainFrameReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);

  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&demuxer_connector_);
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&renderer_connection_);

  DCHECK(demuxer_connector_);
  DCHECK(renderer_connection_);

  TryStartPlayback();
}

void StreamingControllerBase::InitializeReceiverSession(
    std::unique_ptr<cast_streaming::ReceiverSession::AVConstraints> constraints,
    cast_streaming::ReceiverSession::Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(constraints);

  ProcessAVConstraints(constraints.get());
  constraints_ = std::move(constraints);
  client_ = client;

  TryStartPlayback();
}

void StreamingControllerBase::StartPlaybackAsync(PlaybackStartedCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!playback_started_cb_);
  DCHECK(cb);

  playback_started_cb_ = std::move(cb);

  TryStartPlayback();
}

void StreamingControllerBase::TryStartPlayback() {
  if (playback_started_cb_ && constraints_ && demuxer_connector_) {
    cast_streaming::ReceiverSession::MessagePortProvider message_port_provider =
        base::BindOnce(
            [](std::unique_ptr<cast_api_bindings::MessagePort> port) {
              return port;
            },
            std::move(message_port_));
    receiver_session_ = cast_streaming::ReceiverSession::Create(
        std::move(constraints_), std::move(message_port_provider), client_);
    DCHECK(receiver_session_);

    StartPlayback(receiver_session_.get(), std::move(demuxer_connector_),
                  std::move(renderer_connection_));
    std::move(playback_started_cb_).Run();
  }
}

}  // namespace chromecast
