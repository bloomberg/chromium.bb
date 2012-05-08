// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/content_client/examples_main_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_content_plugin_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_content_utility_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/examples/content_client/examples_content_browser_client.h"

#if defined(OS_WIN)
#include "base/logging_win.h"
#endif

namespace views {
namespace examples {
namespace {

#if defined(OS_WIN)
// {83FAC8EE-7A0E-4dbb-A3F6-6F500D7CAB1A}
const GUID kViewsExamplesProviderName =
    { 0x83fac8ee, 0x7a0e, 0x4dbb,
        { 0xa3, 0xf6, 0x6f, 0x50, 0xd, 0x7c, 0xab, 0x1a } };
#endif

int ExamplesBrowserMain(
    const content::MainFunctionParams& main_function_params) {
  scoped_ptr<content::BrowserMainRunner> main_runner(
      content::BrowserMainRunner::Create());
  int exit_code = main_runner->Initialize(main_function_params);
  if (exit_code >= 0)
    return exit_code;
  exit_code = main_runner->Run();
  main_runner->Shutdown();
  return exit_code;
}

}  // namespace

ExamplesMainDelegate::ExamplesMainDelegate() {
}

ExamplesMainDelegate::~ExamplesMainDelegate() {
}

bool ExamplesMainDelegate::BasicStartupComplete(int* exit_code) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  content::SetContentClient(&content_client_);
  InitializeShellContentClient(process_type);

  bool success = logging::InitLogging(NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  CHECK(success);
#if defined(OS_WIN)
  logging::LogEventProvider::Initialize(kViewsExamplesProviderName);
#endif

  return false;
}

void ExamplesMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

void ExamplesMainDelegate::SandboxInitialized(const std::string& process_type) {
}

int ExamplesMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type != "")
    return -1;

  return ExamplesBrowserMain(main_function_params);
}

void ExamplesMainDelegate::ProcessExiting(const std::string& process_type) {
}

#if defined(OS_POSIX)
content::ZygoteForkDelegate* ExamplesMainDelegate::ZygoteStarting() {
  return NULL;
}

void ExamplesMainDelegate::ZygoteForked() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  InitializeShellContentClient(process_type);
}
#endif

void ExamplesMainDelegate::InitializeShellContentClient(
    const std::string& process_type) {
  if (process_type.empty()) {
    browser_client_.reset(new ExamplesContentBrowserClient);
    content::GetContentClient()->set_browser(browser_client_.get());
  } else if (process_type == switches::kRendererProcess) {
    renderer_client_.reset(new content::ShellContentRendererClient);
    content::GetContentClient()->set_renderer(renderer_client_.get());
  } else if (process_type == switches::kPluginProcess) {
    plugin_client_.reset(new content::ShellContentPluginClient);
    content::GetContentClient()->set_plugin(plugin_client_.get());
  } else if (process_type == switches::kUtilityProcess) {
    utility_client_.reset(new content::ShellContentUtilityClient);
    content::GetContentClient()->set_utility(utility_client_.get());
  }
}

void ExamplesMainDelegate::InitializeResourceBundle() {
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US");
}

}  // namespace examples
}  // namespace views
