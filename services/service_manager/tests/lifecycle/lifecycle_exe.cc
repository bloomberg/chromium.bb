// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/runner/child/test_native_main.h"
#include "services/service_manager/runner/init.h"
#include "services/service_manager/tests/lifecycle/app_client.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  service_manager::InitializeLogging();
  return service_manager::TestNativeMain(
      base::MakeUnique<service_manager::test::AppClient>());
}
