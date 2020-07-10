// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
#define CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_

#include "base/macros.h"
#include "components/bubble/bubble_reference.h"
#include "content/public/browser/serial_chooser.h"

// Owns a serial port chooser dialog and closes it when destroyed.
class SerialChooser : public content::SerialChooser {
 public:
  explicit SerialChooser(BubbleReference bubble);
  ~SerialChooser() override;

 private:
  BubbleReference bubble_;

  DISALLOW_COPY_AND_ASSIGN(SerialChooser);
};

#endif  // CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
