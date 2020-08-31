// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
#define CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "content/public/browser/serial_chooser.h"

// Owns a serial port chooser dialog and closes it when destroyed.
class SerialChooser : public content::SerialChooser {
 public:
  explicit SerialChooser(base::OnceClosure close_closure);
  ~SerialChooser() override = default;

 private:
  base::ScopedClosureRunner closure_runner_;

  DISALLOW_COPY_AND_ASSIGN(SerialChooser);
};

#endif  // CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
