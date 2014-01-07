// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
#define UI_AURA_TEST_TEST_FOCUS_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace test {

class TestFocusClient : public client::FocusClient,
                        public WindowObserver {
 public:
  TestFocusClient();
  virtual ~TestFocusClient();

 private:
  // Overridden from client::FocusClient:
  virtual void AddObserver(client::FocusChangeObserver* observer) OVERRIDE;
  virtual void RemoveObserver(client::FocusChangeObserver* observer) OVERRIDE;
  virtual void FocusWindow(Window* window) OVERRIDE;
  virtual void ResetFocusWithinActiveWindow(Window* window) OVERRIDE;
  virtual Window* GetFocusedWindow() OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroying(Window* window) OVERRIDE;

  Window* focused_window_;
  ScopedObserver<Window, WindowObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
