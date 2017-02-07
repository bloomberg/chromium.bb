// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/InputDeviceCapabilities.h"

namespace blink {

InputDeviceCapabilities::InputDeviceCapabilities(bool firesTouchEvents) {
  m_firesTouchEvents = firesTouchEvents;
}

InputDeviceCapabilities::InputDeviceCapabilities(
    const InputDeviceCapabilitiesInit& initializer) {
  m_firesTouchEvents = initializer.firesTouchEvents();
}

InputDeviceCapabilities* InputDeviceCapabilitiesConstants::firesTouchEvents(
    bool firesTouch) {
  if (firesTouch) {
    if (!m_firesTouchEvents)
      m_firesTouchEvents = InputDeviceCapabilities::create(true);
    return m_firesTouchEvents;
  }
  if (!m_doesntFireTouchEvents)
    m_doesntFireTouchEvents = InputDeviceCapabilities::create(false);
  return m_doesntFireTouchEvents;
}

}  // namespace blink
