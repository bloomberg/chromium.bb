// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBatteryListener_h
#define WebBatteryListener_h

#include "WebBatteryStatus.h"

namespace blink {

class WebBatteryStatus;

class WebBatteryListener {
public:
    // This method is called when a new battery status is available.
    virtual void updateBatteryStatus(const WebBatteryStatus&) = 0;

    virtual ~WebBatteryListener() { }
};

} // namespace blink

#endif // WebBatteryListener_h
