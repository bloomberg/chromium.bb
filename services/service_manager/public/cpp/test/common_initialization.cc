// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/common_initialization.h"

#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/catalog/catalog.h"
#include "services/service_manager/public/cpp/test/service_test_catalog.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/core/embedder/default_mach_broker.h"
#endif

namespace service_manager {

int InitializeAndLaunchUnitTests(int argc,
                                 char** argv,
                                 base::RunTestSuiteCallback run_test_suite) {
  catalog::Catalog::SetDefaultCatalogManifest(
      service_manager::test::CreateTestCatalog());

  mojo::core::Init();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  mojo::core::SetMachPortProvider(
      mojo::core::DefaultMachBroker::Get()->port_provider());
#endif

  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(argc, argv, std::move(run_test_suite));
}

}  // namespace service_manager
