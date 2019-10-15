// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"

#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"

namespace blink {

void FakeLocalFrameHost::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::LocalFrameHost::Name_,
      base::BindRepeating(&FakeLocalFrameHost::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrameHost::EnterFullscreen(
    mojom::blink::FullscreenOptionsPtr options) {}

void FakeLocalFrameHost::ExitFullscreen() {}

void FakeLocalFrameHost::FullscreenStateChanged(bool is_fullscreen) {}

void FakeLocalFrameHost::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::blink::LocalFrameHost>(
      std::move(handle)));
}

}  // namespace blink
