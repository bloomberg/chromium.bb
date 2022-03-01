// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

class PasswordGenerationPopupController;

// Interface for creating and controlling a platform dependent view.
class PasswordGenerationPopupView {
 public:
  // Display the popup. Returns |true| if popup is shown, |false| otherwise.
  // Warning: both the controller and view could be destroyed, when the function
  // returns |false|, and they must not be used after that.
  virtual bool Show() WARN_UNUSED_RESULT = 0;

  // This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // The state of the popup has changed from editing to offering a new password.
  // The layout should be recreated.
  virtual void UpdateState() = 0;

  // The password was edited, the popup should show the new value.
  virtual void UpdatePasswordValue() {}

  // Updates layout information from the controller and performs the layout.
  // Returns |true| in case of success popup redraw, |false| otherwise.
  // Warning: both the controller and view could be destroyed, when the function
  // returns |false|, and they must not be used after that.
  virtual bool UpdateBoundsAndRedrawPopup() WARN_UNUSED_RESULT = 0;

  // Called when the password selection state has changed.
  virtual void PasswordSelectionUpdated() = 0;

  // Note that PasswordGenerationPopupView owns itself, and will only be deleted
  // when Hide() is called.
  static PasswordGenerationPopupView* Create(
      base::WeakPtr<PasswordGenerationPopupController> controller);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_
