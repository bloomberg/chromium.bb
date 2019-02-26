// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/control_image_button.h"

namespace views {

ControlImageButton::ControlImageButton(ButtonListener* listener)
    : ImageButton(listener), id_() {}

ControlImageButton::~ControlImageButton() = default;

}  // namespace views
