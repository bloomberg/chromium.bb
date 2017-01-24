// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/public/cpp/standalone_service/switches.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/runner/init.h"

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#endif

namespace {

// Cross-platform helper to override the necessary hard-coded path location
// used to load ICU data.
//
// TODO(rockot): We should just parameterize InitializeICU so hacks like
// this are unnecessary.
class ScopedIcuDataDirOverride {
 public:
  ScopedIcuDataDirOverride(const base::FilePath& path) {
#if defined(OS_WIN)
    base::PathService::Get(base::DIR_MODULE, &original_path_);
#elif defined(OS_MACOSX)
    original_path_= base::mac::FrameworkBundlePath();
#else
    base::PathService::Get(base::DIR_EXE, &original_path_);
#endif

    if (!path.empty())
      SetPathOverride(path);
  }

  ~ScopedIcuDataDirOverride() { SetPathOverride(original_path_); }

 private:
  static void SetPathOverride(const base::FilePath& path) {
#if defined(OS_WIN)
    base::PathService::Override(base::DIR_MODULE, path);
#elif defined(OS_MACOSX)
    base::mac::SetOverrideFrameworkBundlePath(path);
#else
    base::PathService::Override(base::DIR_EXE, path);
#endif
  }

  base::FilePath original_path_;
};

// TODO(rockot): We should consider removing ServiceMain and instead allowing
// service sources to define a CreateService method which returns a new instance
// of service_manager::Service. This would reduce boilerplate in service sources
// since they all effectively do the same thing via
// service_manager::ServiceRunner.
void RunServiceMain(service_manager::mojom::ServiceRequest request) {
  MojoResult result = ServiceMain(request.PassMessagePipe().release().value());
  DCHECK_EQ(result, MOJO_RESULT_OK);
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

#if !defined(OFFICIAL_BIULD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif

  service_manager::InitializeLogging();

  {
    base::FilePath icu_data_dir = base::CommandLine::ForCurrentProcess()
        ->GetSwitchValuePath(service_manager::switches::kIcuDataDir);
    ScopedIcuDataDirOverride path_override(icu_data_dir);
    base::i18n::InitializeICU();
  }

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping before initializing sandbox to make sure symbol
  // names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif

  service_manager::WaitForDebuggerIfNecessary();

  service_manager::RunStandaloneService(base::Bind(&RunServiceMain));
  return 0;
}
