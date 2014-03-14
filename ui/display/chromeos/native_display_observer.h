// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_OBSERVER_H_
#define UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_OBSERVER_H_

namespace ui {

// Observer class used by NativeDisplayDelegate to announce when the display
// configuration changes.
class NativeDisplayObserver {
 public:
  virtual ~NativeDisplayObserver() {}

  virtual void OnConfigurationChanged() = 0;
};

}  //  namespace ui

#endif  // UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_OBSERVER_H_
