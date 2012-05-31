// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_TEST_SUPPORT_H_
#define UI_BASE_IME_TEXT_INPUT_TEST_SUPPORT_H_

#include "base/basictypes.h"

namespace ui {

class TextInputTestSupport {
 public:
  TextInputTestSupport();
  virtual ~TextInputTestSupport();

  // Initialize DBusThreadManager for text input testing.
  static void Initilaize();

  // Shutdown DBusThreadManager.
  static void Shutdown();

 private:
  DISALLOW_COPY_AND_ASSIGN(TextInputTestSupport);
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_TEST_SUPPORT_H_
