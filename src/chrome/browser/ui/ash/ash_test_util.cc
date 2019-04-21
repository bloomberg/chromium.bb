// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_test_util.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.test-mojom.h"
#include "base/run_loop.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/test/mus/change_completion_waiter.h"

namespace test {

ash::mojom::ShellTestApiPtr GetShellTestApi() {
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);

  return shell_test_api;
}

void WaitForNoPointerHoldLock(bool wait_for_changes) {
  ash::mojom::ShellTestApiPtr shell_test_api = GetShellTestApi();

  // Allow nestable tasks because this is called within a move loop.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  shell_test_api->WaitForNoPointerHoldLock(run_loop.QuitClosure());
  run_loop.Run();

  if (wait_for_changes)
    aura::test::WaitForAllChangesToComplete();
}

void WaitForOverviewAnimationState(ash::mojom::OverviewAnimationState state) {
  ash::mojom::ShellTestApiPtr shell_test_api = GetShellTestApi();

  base::RunLoop run_loop;
  shell_test_api->WaitForOverviewAnimationState(state, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace test
