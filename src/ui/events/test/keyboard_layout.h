// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_
#define UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_

#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <Carbon/Carbon.h>
#include "base/mac/scoped_cftyperef.h"
#elif defined(USE_OZONE)
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"  // nogncheck
#endif

namespace ui {

enum KeyboardLayout {
  KEYBOARD_LAYOUT_ENGLISH_US,
#if defined(OS_WIN)
  KEYBOARD_LAYOUT_FRENCH,
  KEYBOARD_LAYOUT_GERMAN,
  KEYBOARD_LAYOUT_GREEK,
  KEYBOARD_LAYOUT_JAPANESE,
  KEYBOARD_LAYOUT_KOREAN,
  KEYBOARD_LAYOUT_RUSSIAN,
#endif
};

#if defined(OS_WIN)
using PlatformKeyboardLayout = HKL;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
using PlatformKeyboardLayout = base::ScopedCFTypeRef<TISInputSourceRef>;
#endif

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
PlatformKeyboardLayout GetPlatformKeyboardLayout(KeyboardLayout layout);
#endif

// Changes the active keyboard layout for the scope of this object.
class ScopedKeyboardLayout {
 public:
  explicit ScopedKeyboardLayout(KeyboardLayout layout);
  ~ScopedKeyboardLayout();

 private:
#if defined(USE_OZONE)
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_keyboard_layout_engine_;
#endif
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  static PlatformKeyboardLayout GetActiveLayout();
  static void ActivateLayout(PlatformKeyboardLayout layout);

  PlatformKeyboardLayout original_layout_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedKeyboardLayout);
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_KEYBOARD_LAYOUT_H_
