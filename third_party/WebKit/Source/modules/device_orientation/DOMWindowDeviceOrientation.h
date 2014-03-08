// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowDeviceOrientation_h
#define DOMWindowDeviceOrientation_h

#include "core/events/EventTarget.h"

namespace WebCore {

class DOMWindowDeviceOrientation {
public:
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(deviceorientation);
};

} // namespace WebCore

#endif // DOMWindowDeviceOrientation_h
