// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_SERVER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_SERVER_H_

#include "ash/test/ash_test_suite.h"
#include "base/callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"

namespace exo {

class WaylandClientTestSuiteServer : public ash::AshTestSuite {
 public:
  using CreateServerCallback =
      base::OnceCallback<std::unique_ptr<WaylandClientTestSuiteServer>(int,
                                                                       char**)>;

  WaylandClientTestSuiteServer(int argc, char** argv);

  WaylandClientTestSuiteServer(const WaylandClientTestSuiteServer&) = delete;
  WaylandClientTestSuiteServer& operator=(const WaylandClientTestSuiteServer&) =
      delete;

  ~WaylandClientTestSuiteServer() override;

  int Run();

  static int TestMain(int argc,
                      char** argv,
                      CreateServerCallback create_server_callback);

 protected:
  // Overridden from ash::AshTestSuite:
  void Initialize() override;
  void Shutdown() override;

  // Implemented by the derived class for a particular set of client tests.
  virtual void SetClientTestUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) = 0;

 private:
  void RunTestsOnClientThread(base::OnceClosure finished_closure);

  // Do not run the wayland server within the test.
  const bool run_with_external_wayland_server_ = false;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  // Result of RunTestsOnClientThread().
  int result_ = 1;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_SERVER_H_
