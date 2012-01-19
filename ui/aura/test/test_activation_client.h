// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_ACTIVATION_CLIENT_H_
#define UI_AURA_TEST_TEST_ACTIVATION_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace test {

class TestActivationClient : public client::ActivationClient,
                             public WindowObserver {
 public:
  TestActivationClient();
  virtual ~TestActivationClient();

  // Overridden from client::ActivationClient:
  virtual void ActivateWindow(Window* window) OVERRIDE;
  virtual void DeactivateWindow(Window* window) OVERRIDE;
  virtual Window* GetActiveWindow() OVERRIDE;
  virtual bool CanFocusWindow(Window* window) const OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroyed(Window* window) OVERRIDE;

 private:
  // This class explicitly does NOT store the active window in a window property
  // to make sure that storing the active window in a property is not treated as
  // part of the aura API. Assumptions to that end will cause tests that use
  // this client to fail.
  Window* active_window_;

  DISALLOW_COPY_AND_ASSIGN(TestActivationClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_ACTIVATION_CLIENT_H_
