// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_MAPPING_H_
#define UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_MAPPING_H_

#include "ui/events/ozone/gamepad/webgamepad_constants.h"

namespace ui {

// The following HATX and HATY is not part of web gamepad definition, but we
// need to specially treat them cause HAT_Y can be mapped to DPAD_UP or
// DPAD_DOWN, and HAT_X can be mapped to DAPD_LEFT or DPAD_RIGHT.
constexpr int kHAT_X = 4;
constexpr int kHAT_Y = 5;

typedef bool (*GamepadMapper)(uint16_t key,
                              uint16_t code,
                              GamepadEventType* mapped_type,
                              uint16_t* mapped_code);

// This function gets the best mapper for the gamepad vendor_id and product_id.
GamepadMapper GetGamepadMapper(uint16_t vendor_id, uint16_t product_id);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_MAPPING_H_
