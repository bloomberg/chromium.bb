// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_external_begin_frame_controller_client.h"

namespace viz {

FakeExternalBeginFrameControllerClient::FakeExternalBeginFrameControllerClient()
    : binding_(this) {}

FakeExternalBeginFrameControllerClient::
    ~FakeExternalBeginFrameControllerClient() = default;

mojom::ExternalBeginFrameControllerClientPtr
FakeExternalBeginFrameControllerClient::BindInterfacePtr() {
  mojom::ExternalBeginFrameControllerClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void FakeExternalBeginFrameControllerClient::OnNeedsBeginFrames(
    bool needs_begin_frames) {}

void FakeExternalBeginFrameControllerClient::OnDisplayDidFinishFrame(
    const BeginFrameAck& ack) {}

}  // namespace viz
