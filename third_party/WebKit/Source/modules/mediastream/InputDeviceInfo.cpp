// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/InputDeviceInfo.h"

#include "modules/mediastream/MediaTrackCapabilities.h"

namespace blink {

InputDeviceInfo* InputDeviceInfo::Create(const String& device_id,
                                         const String& label,
                                         const String& group_id,
                                         MediaDeviceType device_type) {
  return new InputDeviceInfo(device_id, label, group_id, device_type);
}

InputDeviceInfo::InputDeviceInfo(const String& device_id,
                                 const String& label,
                                 const String& group_id,
                                 MediaDeviceType device_type)
    : MediaDeviceInfo(device_id, label, group_id, device_type) {}

void InputDeviceInfo::getCapabilities(MediaTrackCapabilities& capabilities) {}

}  // namespace blink
