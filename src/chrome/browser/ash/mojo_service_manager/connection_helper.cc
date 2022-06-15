// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mojo_service_manager/connection_helper.h"

#include <memory>

#include "chromeos/components/mojo_service_manager/connection.h"
#include "chromeos/features.h"

#if !BUILDFLAG(USE_REAL_CHROMEOS_SERVICES)
#include "base/system/sys_info.h"
#include "chromeos/components/mojo_service_manager/fake_mojo_service_manager.h"
#endif

namespace {

namespace service_manager = chromeos::mojo_service_manager;

base::ScopedClosureRunner CreateRealConnectionAndPassCloser() {
  CHECK(service_manager::BootstrapServiceManagerConnection())
      << "Cannot connect to ChromeOS mojo service manager after retries. "
         "This result in the ash don't have a mojo broker and will not be "
         "able to bootstrap any mojo connection to other processes.";
  return base::ScopedClosureRunner{
      base::BindOnce(&service_manager::ResetServiceManagerConnection)};
}

#if !BUILDFLAG(USE_REAL_CHROMEOS_SERVICES)
void ResetFakeConnection(
    std::unique_ptr<service_manager::FakeMojoServiceManager>
        fake_service_manager) {
  // Reset the connection before the fake service manager so the disconnect
  // handler won't be triggered.
  service_manager::ResetServiceManagerConnection();
  fake_service_manager.reset();
}

base::ScopedClosureRunner CreateFakeConnectionAndPassCloser() {
  auto fake_service_manager =
      std::make_unique<service_manager::FakeMojoServiceManager>();
  service_manager::SetServiceManagerRemoteForTesting(
      fake_service_manager->BindNewPipeAndPassRemote());

  return base::ScopedClosureRunner{
      base::BindOnce(&ResetFakeConnection, std::move(fake_service_manager))};
}
#endif

}  // namespace

namespace ash {
namespace mojo_service_manager {

base::ScopedClosureRunner CreateConnectionAndPassCloser() {
#if BUILDFLAG(USE_REAL_CHROMEOS_SERVICES)
  return CreateRealConnectionAndPassCloser();
#else
  // TODO(crbug.com/952745): Always use fakes after adding
  // use_real_chromeos_services=true to where needed.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    return CreateRealConnectionAndPassCloser();
  } else {
    return CreateFakeConnectionAndPassCloser();
  }
#endif
}

}  // namespace mojo_service_manager
}  // namespace ash
