// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationDispatcher.h"

#include "public/platform/Platform.h"
#include "services/device/public/interfaces/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"

namespace blink {

ScreenOrientationDispatcher& ScreenOrientationDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(ScreenOrientationDispatcher,
                      screen_orientation_dispatcher,
                      (new ScreenOrientationDispatcher));
  return screen_orientation_dispatcher;
}

ScreenOrientationDispatcher::ScreenOrientationDispatcher() {}

ScreenOrientationDispatcher::~ScreenOrientationDispatcher() {
  DCHECK(!listener_);
}

void ScreenOrientationDispatcher::Trace(blink::Visitor* visitor) {
  PlatformEventDispatcher::Trace(visitor);
}

void ScreenOrientationDispatcher::StartListening() {
  DCHECK(!listener_);

  Platform::Current()->GetConnector()->BindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&listener_));
  listener_->Start();
}

void ScreenOrientationDispatcher::StopListening() {
  DCHECK(listener_);

  listener_->Stop();
  listener_.reset();
}

}  // namespace blink
