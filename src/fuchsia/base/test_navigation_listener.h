// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_
#define FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace cr_fuchsia {

// Observes navigation events and enables test code to block until a desired
// navigational state is observed.
class TestNavigationListener : public fuchsia::web::NavigationEventListener {
 public:
  using BeforeAckCallback =
      base::RepeatingCallback<void(const fuchsia::web::NavigationState& change,
                                   OnNavigationStateChangedCallback)>;

  TestNavigationListener();
  ~TestNavigationListener() final;

  // Spins a RunLoop until the page navigates to |expected_url| and the page's
  // title is |expected_title| (if set).
  void RunUntilNavigationEquals(
      const GURL& expected_url,
      const base::Optional<std::string>& expected_title);

  // Register a callback which intercepts the execution of the event
  // acknowledgement callback. |before_ack| takes ownership of the
  // acknowledgement callback and the responsibility for executing it.
  void SetBeforeAckHook(BeforeAckCallback before_ack);

 private:
  // fuchsia::web::NavigationEventListener implementation.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) final;

  GURL current_url_;
  std::string current_title_;

  BeforeAckCallback before_ack_;

  // Returns |true| if the most recently received URL and title match the
  // expectations set by WaitForNavigation.
  bool IsFulfilled();

  DISALLOW_COPY_AND_ASSIGN(TestNavigationListener);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_
