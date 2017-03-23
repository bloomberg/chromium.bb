// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationDispatcher.h"

#include "public/platform/Connector.h"
#include "public/platform/Platform.h"
#include "services/device/public/interfaces/constants.mojom-blink.h"

namespace blink {

ScreenOrientationDispatcher& ScreenOrientationDispatcher::instance() {
  DEFINE_STATIC_LOCAL(ScreenOrientationDispatcher, screenOrientationDispatcher,
                      (new ScreenOrientationDispatcher));
  return screenOrientationDispatcher;
}

ScreenOrientationDispatcher::ScreenOrientationDispatcher() {}

ScreenOrientationDispatcher::~ScreenOrientationDispatcher() {
  DCHECK(!m_listener);
}

DEFINE_TRACE(ScreenOrientationDispatcher) {
  PlatformEventDispatcher::trace(visitor);
}

void ScreenOrientationDispatcher::startListening() {
  DCHECK(!m_listener);

  Platform::current()->connector()->bindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&m_listener));
  m_listener->Start();
}

void ScreenOrientationDispatcher::stopListening() {
  DCHECK(m_listener);

  m_listener->Stop();
  m_listener.reset();
}

}  // namespace blink
