/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/gamepad/Gamepad.h"

namespace blink {

Gamepad::Gamepad() : index_(0), timestamp_(0), display_id_(0) {}

Gamepad::~Gamepad() {}

void Gamepad::SetAxes(unsigned count, const double* data) {
  axes_.Resize(count);
  if (count)
    std::copy(data, data + count, axes_.begin());
}

void Gamepad::SetButtons(unsigned count, const device::GamepadButton* data) {
  if (buttons_.size() != count) {
    buttons_.Resize(count);
    for (unsigned i = 0; i < count; ++i)
      buttons_[i] = GamepadButton::Create();
  }
  for (unsigned i = 0; i < count; ++i) {
    buttons_[i]->SetValue(data[i].value);
    buttons_[i]->SetPressed(data[i].pressed);
    buttons_[i]->SetTouched(data[i].touched || data[i].pressed ||
                            (data[i].value > 0.0f));
  }
}

void Gamepad::SetPose(const device::GamepadPose& pose) {
  if (!pose.not_null) {
    if (pose_)
      pose_ = nullptr;
    return;
  }

  if (!pose_)
    pose_ = GamepadPose::Create();

  pose_->SetPose(pose);
}

void Gamepad::SetHand(const device::GamepadHand& hand) {
  switch (hand) {
    case device::GamepadHand::kNone:
      hand_ = "";
      break;
    case device::GamepadHand::kLeft:
      hand_ = "left";
      break;
    case device::GamepadHand::kRight:
      hand_ = "right";
      break;
    default:
      NOTREACHED();
  }
}

DEFINE_TRACE(Gamepad) {
  visitor->Trace(buttons_);
  visitor->Trace(pose_);
}

}  // namespace blink
