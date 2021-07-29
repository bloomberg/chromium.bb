// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/shell/app/shell_main_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(erikchen): Move #include to .cc file and forward declare
// chromeos::LacrosService to resolve crbug.com/1195401.
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {
class ContentUtilityClient;
}

namespace extensions {

class TestShellMainDelegate : public extensions::ShellMainDelegate {
 public:
  TestShellMainDelegate();
  ~TestShellMainDelegate() override;

  // ContentMainDelegate implementation:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void PostEarlyInitialization(bool is_running_tests) override;
#endif

 protected:
  // content::ContentMainDelegate implementation:
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  std::unique_ptr<content::ContentUtilityClient> utility_client_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::LacrosService> lacros_service_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestShellMainDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
