// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sessions/core/session_id.h"

class SessionService;

namespace base {
class Location;
}

namespace sessions {
class SerializedNavigationEntry;
struct SerializedUserAgentOverride;
struct SessionTab;
struct SessionWindow;
}

// A simple class that makes writing SessionService related tests easier.

class SessionServiceTestHelper {
 public:
  SessionServiceTestHelper();
  explicit SessionServiceTestHelper(SessionService* service);
  ~SessionServiceTestHelper();

  void PrepareTabInWindow(const SessionID& window_id,
                          const SessionID& tab_id,
                          int visual_index,
                          bool select);

  void SetTabExtensionAppID(const SessionID& window_id,
                            const SessionID& tab_id,
                            const std::string& extension_app_id);

  void SetTabUserAgentOverride(
      const SessionID& window_id,
      const SessionID& tab_id,
      const sessions::SerializedUserAgentOverride& user_agent_override);

  void SetForceBrowserNotAliveWithNoWindows(
      bool force_browser_not_alive_with_no_windows);

  // Reads the contents of the last session.
  void ReadWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID* active_window_id);

  void AssertTabEquals(const SessionID& window_id,
                       const SessionID& tab_id,
                       int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const sessions::SessionTab& session_tab);

  void AssertTabEquals(int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const sessions::SessionTab& session_tab);

  void AssertNavigationEquals(
      const sessions::SerializedNavigationEntry& expected,
      const sessions::SerializedNavigationEntry& actual);

  void AssertSingleWindowWithSingleTab(
      const std::vector<std::unique_ptr<sessions::SessionWindow>>& windows,
      size_t nav_count);

  void SetService(SessionService* service);
  SessionService* ReleaseService();
  SessionService* service() { return service_.get(); }

  void RunTaskOnBackendThread(const base::Location& from_here,
                              base::OnceClosure task);

  void SetAvailableRange(const SessionID& tab_id,
                         const std::pair<int, int>& range);
  bool GetAvailableRange(const SessionID& tab_id, std::pair<int, int>* range);

  void SetHasOpenTrackableBrowsers(bool has_open_trackable_browsers);
  bool GetHasOpenTrackableBrowsers();

  void SetIsOnlyOneTabLeft(bool is_only_one_tab_left);

 private:
  std::unique_ptr<SessionService> service_;

  DISALLOW_COPY_AND_ASSIGN(SessionServiceTestHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
