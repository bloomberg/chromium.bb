// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_output.h"

namespace ui {

namespace {
const uint32_t kXdgVersion5 = 5;
}

class OutputObserver : public WaylandOutput::Observer {
 public:
  explicit OutputObserver(const base::Closure& closure) : closure_(closure) {}

  void OnOutputReadyForUse() override {
    if (!closure_.is_null())
      closure_.Run();
  }

 private:
  const base::Closure closure_;
};

TEST(WaylandConnectionTest, UseUnstableVersion) {
  base::MessageLoopForUI message_loop;
  wl::FakeServer server;
  EXPECT_CALL(*server.xdg_shell(),
              UseUnstableVersion(XDG_SHELL_VERSION_CURRENT));
  ASSERT_TRUE(server.Start(kXdgVersion5));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();
}

TEST(WaylandConnectionTest, Ping) {
  base::MessageLoopForUI message_loop;
  wl::FakeServer server;
  ASSERT_TRUE(server.Start(kXdgVersion5));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();

  xdg_shell_send_ping(server.xdg_shell()->resource(), 1234);
  EXPECT_CALL(*server.xdg_shell(), Pong(1234));

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();
}

TEST(WaylandConnectionTest, Output) {
  base::MessageLoopForUI message_loop;
  wl::FakeServer server;
  ASSERT_TRUE(server.Start(kXdgVersion5));
  server.output()->SetRect(gfx::Rect(0, 0, 800, 600));
  WaylandConnection connection;
  ASSERT_TRUE(connection.Initialize());
  connection.StartProcessingEvents();

  base::RunLoop run_loop;
  OutputObserver observer(run_loop.QuitClosure());
  connection.PrimaryOutput()->SetObserver(&observer);
  run_loop.Run();

  ASSERT_TRUE(connection.GetOutputList().size() == 1);
  WaylandOutput* output = connection.PrimaryOutput();
  ASSERT_TRUE(output->Geometry().width() == 800);
  ASSERT_TRUE(output->Geometry().height() == 600);

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();
}

}  // namespace ui
