// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HID_HID_CHOOSER_H_
#define CHROME_BROWSER_UI_HID_HID_CHOOSER_H_

#include "base/macros.h"
#include "components/bubble/bubble_reference.h"

// Owns a HID device chooser dialog and closes it when destroyed.
class HidChooser {
 public:
  explicit HidChooser(BubbleReference bubble);
  ~HidChooser();

 private:
  BubbleReference bubble_;

  DISALLOW_COPY_AND_ASSIGN(HidChooser);
};

#endif  // CHROME_BROWSER_UI_HID_HID_CHOOSER_H_
