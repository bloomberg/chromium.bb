// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_

namespace chromeos {

// Interface that handles notifications received from any of login wizard
// screens.
class BaseScreenDelegate {
 public:
  // Forces current screen showing.
  virtual void ShowCurrentScreen() = 0;

 protected:
  virtual ~BaseScreenDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_DELEGATE_H_
