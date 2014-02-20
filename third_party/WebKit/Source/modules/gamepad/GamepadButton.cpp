// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/Gamepad.h"

namespace WebCore {

PassRefPtr<GamepadButton> GamepadButton::create()
{
    RefPtr<GamepadButton> input = adoptRef(new GamepadButton());
    return input.release();
}

GamepadButton::GamepadButton()
    : m_value(0.f)
    , m_pressed(false)
{
    ScriptWrappable::init(this);
}

GamepadButton::~GamepadButton()
{
}

} // namespace WebCore
