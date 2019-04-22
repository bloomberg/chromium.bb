// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVER_TEST_IMPL_H_
#define SERVICES_WS_WINDOW_SERVER_TEST_IMPL_H_

#include <memory>

#include "services/ws/public/mojom/window_server_test.mojom.h"

namespace viz {
class CopyOutputResult;
}

namespace ws {

class WindowService;
class WindowTree;

// Used to detect when a client (as identified by a name) has drawn at least
// once to screen.
class WindowServerTestImpl : public mojom::WindowServerTest {
 public:
  explicit WindowServerTestImpl(WindowService* server);
  ~WindowServerTestImpl() override;

 private:
  WindowTree* GetWindowTreeWithClientName(const std::string& name);

  // Callback when a surface is activated. |desired_client_name| is the name of
  // the client that was requested, |actual_client_name| is the name of the
  // client that actually painted.
  void OnSurfaceActivated(const std::string& desired_client_name,
                          EnsureClientHasDrawnWindowCallback cb,
                          const std::string& actual_client_name);

  // Installs a callback that calls OnSurfaceActivated() the next time a client
  // creates a compositor frame.
  void InstallCallback(const std::string& name,
                       EnsureClientHasDrawnWindowCallback cb);

  // Request to capture the window contents of the client and invoke the
  // callback with sanity check result of the captured pixels.
  void RequestWindowContents(const std::string& client_name,
                             int retry_count,
                             EnsureClientHasDrawnWindowCallback cb);
  void OnWindowContentsCaptured(const std::string& client_name,
                                int retry_count,
                                EnsureClientHasDrawnWindowCallback cb,
                                std::unique_ptr<viz::CopyOutputResult> result);

  // mojom::WindowServerTest:
  void EnsureClientHasDrawnWindow(
      const std::string& client_name,
      EnsureClientHasDrawnWindowCallback callback) override;

  WindowService* window_service_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerTestImpl);
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_SERVER_TEST_IMPL_H_
