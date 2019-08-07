// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser.h"

#include <utility>

#include "components/bubble/bubble_controller.h"

SerialChooser::SerialChooser(BubbleReference bubble)
    : bubble_(std::move(bubble)) {}

SerialChooser::~SerialChooser() {
  if (bubble_)
    bubble_->CloseBubble(BUBBLE_CLOSE_FORCED);
}
