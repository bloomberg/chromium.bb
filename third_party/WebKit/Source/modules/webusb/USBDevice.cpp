// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webusb/USBDevice.h"

#include "modules/webusb/USBConfiguration.h"

namespace blink {

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const
{
    HeapVector<Member<USBConfiguration>> configurations;
    for (size_t i = 0; i < info().configurations.size(); ++i)
        configurations.append(USBConfiguration::create(this, i));
    return configurations;
}

} // namespace blink
