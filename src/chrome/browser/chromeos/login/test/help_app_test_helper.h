// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HELP_APP_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HELP_APP_TEST_HELPER_H_

#include <set>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace chromeos {

// Provides utilies for launching and interacting with HelpApp dialogs in tests.
class HelpAppTestHelper {
 public:
  // Waits for a HelpApp dialog to open and become visible.
  //
  // Starts listening for window creation events on construction. |Wait| blocks
  // until a HelpApp window is visible. |Wait| returns immediately if a HelpApp
  // window is already visible when |Wait| is called.
  //
  // HelpAppTestHelper::Waiter is single-use. It can only wait for one HelpApp
  // dialog to be opened per lifetime.
  class Waiter : public aura::EnvObserver, public aura::WindowObserver {
   public:
    Waiter();
    ~Waiter() override;

    // Blocks until a HelpApp dialog becomes visible. Once a HelpApp dialog
    // becomes visible during this object's lifetime, all calls to |Wait| return
    // immediately.
    void Wait();

    // aura::EnvObserver
    void OnWindowInitialized(aura::Window* window) override;

    // aura::WindowObserver
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
    void OnWindowDestroyed(aura::Window* window) override;

   private:
    bool IsHelpAppDialog(aura::Window* window);

    base::RunLoop run_loop_;
    std::set<aura::Window*> dialog_windows_;
    ScopedObserver<aura::Window, Waiter> window_observer_{this};
    bool help_app_dialog_opened_ = false;
    DISALLOW_COPY_AND_ASSIGN(Waiter);
  };

  HelpAppTestHelper();
  virtual ~HelpAppTestHelper();

  // Performs setup to allow HelpApp dialogs to open in tests.
  void SetUpHelpAppForTest();

  // TODO(tonydeluna): Add utilities for interacting with the contents of the
  // help dialogs.

  DISALLOW_COPY_AND_ASSIGN(HelpAppTestHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HELP_APP_TEST_HELPER_H_
