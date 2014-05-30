// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_ACCELERATOR_DELEGATE_H_
#define UI_WM_CORE_ACCELERATOR_DELEGATE_H_

namespace ui {
class Accelerator;
class KeyEvent;
}

namespace wm {

class AcceleratorDelegate {
 public:
  virtual ~AcceleratorDelegate() {}

  // TODO(oshima): Move the repeat detection to AcceleratorFilter.
  virtual void PreProcessAccelerator(const ui::Accelerator& accelerator) = 0;

  // Returns true if the window should be allowed a chance to handle
  // system keys.
  virtual bool CanConsumeSystemKeys(const ui::KeyEvent& event) = 0;

  // Returns true if the |accelerator| should be processed.
  virtual bool ShouldProcessAcceleratorNow(
      const ui::KeyEvent& key_event,
      const ui::Accelerator& accelerator) = 0;

  // Return true if the |accelerator| has been processed.
  virtual bool ProcessAccelerator(const ui::Accelerator& accelerator) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_ACCELERATOR_DELEGATE_H_
