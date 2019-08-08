// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_DIALOG_VIEW_INTERFACE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_DIALOG_VIEW_INTERFACE_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
class PasswordGenerationController;

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

class PasswordGenerationDialogViewInterface {
 public:
  virtual ~PasswordGenerationDialogViewInterface() = default;

  // Called to show the dialog. |password| is the generated password.
  virtual void Show(base::string16& password,
                    base::WeakPtr<password_manager::PasswordManagerDriver>
                        target_frame_driver) = 0;

 private:
  friend class PasswordGenerationControllerImpl;

  // Factory function used to create a concrete instance of this view.
  static std::unique_ptr<PasswordGenerationDialogViewInterface> Create(
      PasswordGenerationController* controller);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_DIALOG_VIEW_INTERFACE_H_
