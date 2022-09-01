// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

namespace ash {

namespace {

RgbKeyboardManager* g_instance = nullptr;

}  // namespace

RgbKeyboardManager::RgbKeyboardManager(ImeControllerImpl* ime_controller)
    : ime_controller_ptr_(ime_controller) {
  DCHECK(ime_controller_ptr_);
  DCHECK(!g_instance);
  g_instance = this;

  ime_controller_ptr_->AddObserver(this);

  VLOG(1) << "Initializing RGB Keyboard support";
  FetchRgbKeyboardSupport();
}

RgbKeyboardManager::~RgbKeyboardManager() {
  ime_controller_ptr_->RemoveObserver(this);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void RgbKeyboardManager::FetchRgbKeyboardSupport() {
  DCHECK(RgbkbdClient::Get());
  RgbkbdClient::Get()->GetRgbKeyboardCapabilities(
      base::BindOnce(&RgbKeyboardManager::OnGetRgbKeyboardCapabilities,
                     weak_ptr_factory_.GetWeakPtr()));
}

rgbkbd::RgbKeyboardCapabilities RgbKeyboardManager::GetRgbKeyboardCapabilities()
    const {
  return capabilities_;
}

void RgbKeyboardManager::SetStaticBackgroundColor(uint8_t r,
                                                  uint8_t g,
                                                  uint8_t b) {
  DCHECK(RgbkbdClient::Get());
  if (!IsRgbKeyboardSupported()) {
    LOG(ERROR) << "Attempted to set RGB keyboard color, but flag is disabled.";
    return;
  }

  VLOG(1) << "Setting RGB keyboard color to R:" << static_cast<int>(r)
          << " G:" << static_cast<int>(g) << " B:" << static_cast<int>(b);
  RgbkbdClient::Get()->SetStaticBackgroundColor(r, g, b);
}

void RgbKeyboardManager::SetRainbowMode() {
  DCHECK(RgbkbdClient::Get());
  if (!IsRgbKeyboardSupported()) {
    LOG(ERROR) << "Attempted to set RGB rainbow mode, but flag is disabled.";
    return;
  }

  VLOG(1) << "Setting RGB keyboard to rainbow mode";
  RgbkbdClient::Get()->SetRainbowMode();
}

void RgbKeyboardManager::SetAnimationMode(rgbkbd::RgbAnimationMode mode) {
  if (!features::IsExperimentalRgbKeyboardPatternsEnabled()) {
    LOG(ERROR) << "Attempted to set RGB animation mode, but flag is disabled.";
    return;
  }

  DCHECK(RgbkbdClient::Get());
  VLOG(1) << "Setting RGB keyboard animation mode to "
          << static_cast<uint32_t>(mode);
  RgbkbdClient::Get()->SetAnimationMode(mode);
}

void RgbKeyboardManager::OnCapsLockChanged(bool enabled) {
  if (IsRgbKeyboardSupported()) {
    VLOG(1) << "Setting RGB keyboard caps lock state to " << enabled;
    RgbkbdClient::Get()->SetCapsLockState(enabled);
  }
}

// static
RgbKeyboardManager* RgbKeyboardManager::Get() {
  return g_instance;
}

void RgbKeyboardManager::OnGetRgbKeyboardCapabilities(
    absl::optional<rgbkbd::RgbKeyboardCapabilities> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "No response received for GetRgbKeyboardCapabilities";
    return;
  }

  capabilities_ = reply.value();
  VLOG(1) << "RGB Keyboard capabilities="
          << static_cast<uint32_t>(capabilities_);

  // Upon login, CapsLock may already be enabled.
  if (IsRgbKeyboardSupported()) {
    VLOG(1) << "Setting initial RGB keyboard caps lock state to "
            << ime_controller_ptr_->IsCapsLockEnabled();
    RgbkbdClient::Get()->SetCapsLockState(
        ime_controller_ptr_->IsCapsLockEnabled());
  }
}

}  // namespace ash
