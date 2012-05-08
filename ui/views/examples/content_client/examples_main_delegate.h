// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_CONTENT_CLIENT_EXAMPLES_MAIN_DELEGATE_H_
#define UI_VIEWS_EXAMPLES_CONTENT_CLIENT_EXAMPLES_MAIN_DELEGATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_delegate.h"
#include "content/shell/shell_content_client.h"

namespace content {
class ShellContentRendererClient;
class ShellContentPluginClient;
class ShellContentUtilityClient;
}

namespace views {
namespace examples {

class ExamplesContentBrowserClient;

class ExamplesMainDelegate : public content::ContentMainDelegate {
 public:
  ExamplesMainDelegate();
  virtual ~ExamplesMainDelegate();

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual void SandboxInitialized(const std::string& process_type) OVERRIDE;
  virtual int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) OVERRIDE;
  virtual void ProcessExiting(const std::string& process_type) OVERRIDE;
#if defined(OS_MACOSX)
  virtual bool ProcessRegistersWithSystemProcess(
      const std::string& process_type) OVERRIDE;
  virtual bool ShouldSendMachPort(const std::string& process_type) OVERRIDE;
  virtual bool DelaySandboxInitialization(
      const std::string& process_type) OVERRIDE;
#elif defined(OS_POSIX)
  virtual content::ZygoteForkDelegate* ZygoteStarting() OVERRIDE;
  virtual void ZygoteForked() OVERRIDE;
#endif  // OS_MACOSX

 private:
  void InitializeShellContentClient(const std::string& process_type);
  void InitializeResourceBundle();

  scoped_ptr<ExamplesContentBrowserClient> browser_client_;
  scoped_ptr<content::ShellContentRendererClient> renderer_client_;
  scoped_ptr<content::ShellContentPluginClient> plugin_client_;
  scoped_ptr<content::ShellContentUtilityClient> utility_client_;
  content::ShellContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(ExamplesMainDelegate);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_CONTENT_CLIENT_EXAMPLES_MAIN_DELEGATE_H_
