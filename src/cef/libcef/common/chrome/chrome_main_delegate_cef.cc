// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_main_delegate_cef.h"

#include "libcef/browser/chrome/chrome_browser_context.h"
#include "libcef/browser/chrome/chrome_content_browser_client_cef.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/resource_util.h"
#include "libcef/renderer/chrome/chrome_content_renderer_client_cef.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/common/chrome_switches.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_MAC)
#include "libcef/common/util_mac.h"
#endif

namespace {

base::LazyInstance<ChromeContentRendererClientCef>::DestructorAtExit
    g_chrome_content_renderer_client = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeMainDelegateCef::ChromeMainDelegateCef(CefMainRunnerHandler* runner,
                                             CefSettings* settings,
                                             CefRefPtr<CefApp> application)
    : ChromeMainDelegate(base::TimeTicks::Now()),
      runner_(runner),
      settings_(settings),
      application_(application) {
#if defined(OS_LINUX)
  resource_util::OverrideAssetPath();
#endif
}

ChromeMainDelegateCef::~ChromeMainDelegateCef() = default;

bool ChromeMainDelegateCef::BasicStartupComplete(int* exit_code) {
  // Returns false if startup should proceed.
  bool result = ChromeMainDelegate::BasicStartupComplete(exit_code);
  if (result)
    return true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if defined(OS_POSIX)
  // Read the crash configuration file. Platforms using Breakpad also add a
  // command-line switch. On Windows this is done from chrome_elf.
  crash_reporting::BasicStartupComplete(command_line);
#endif

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // In the browser process. Populate the global command-line object.
    // TODO(chrome-runtime): Copy more settings from AlloyMainDelegate and test.
    if (settings_->command_line_args_disabled) {
      // Remove any existing command-line arguments.
      base::CommandLine::StringVector argv;
      argv.push_back(command_line->GetProgram().value());
      command_line->InitFromArgv(argv);

      const base::CommandLine::SwitchMap& map = command_line->GetSwitches();
      const_cast<base::CommandLine::SwitchMap*>(&map)->clear();
    }

    bool no_sandbox = settings_->no_sandbox ? true : false;

    if (no_sandbox) {
      command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
    }

    if (settings_->user_agent.length > 0) {
      command_line->AppendSwitchASCII(embedder_support::kUserAgent,
                                      CefString(&settings_->user_agent));
    } else if (settings_->user_agent_product.length > 0) {
      command_line->AppendSwitchASCII(
          switches::kUserAgentProductAndVersion,
          CefString(&settings_->user_agent_product));
    }

    if (settings_->locale.length > 0) {
      command_line->AppendSwitchASCII(switches::kLang,
                                      CefString(&settings_->locale));
    } else if (!command_line->HasSwitch(switches::kLang)) {
      command_line->AppendSwitchASCII(switches::kLang, "en-US");
    }

    if (settings_->javascript_flags.length > 0) {
      command_line->AppendSwitchASCII(switches::kJavaScriptFlags,
                                      CefString(&settings_->javascript_flags));
    }

    if (settings_->remote_debugging_port >= 1024 &&
        settings_->remote_debugging_port <= 65535) {
      command_line->AppendSwitchASCII(
          switches::kRemoteDebuggingPort,
          base::NumberToString(settings_->remote_debugging_port));
    }

    if (settings_->uncaught_exception_stack_size > 0) {
      command_line->AppendSwitchASCII(
          switches::kUncaughtExceptionStackSize,
          base::NumberToString(settings_->uncaught_exception_stack_size));
    }
  }

  if (application_) {
    // Give the application a chance to view/modify the command line.
    CefRefPtr<CefCommandLineImpl> commandLinePtr(
        new CefCommandLineImpl(command_line, false, false));
    application_->OnBeforeCommandLineProcessing(process_type,
                                                commandLinePtr.get());
    commandLinePtr->Detach(nullptr);
  }

#if defined(OS_MAC)
  util_mac::BasicStartupComplete();
#endif

  return false;
}

void ChromeMainDelegateCef::PreSandboxStartup() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_MAC)
  if (process_type.empty()) {
    util_mac::PreSandboxStartup();
  }
#endif  // defined(OS_MAC)

  // Since this may be configured via CefSettings we override the value on
  // all platforms. We can't use the default implementation on macOS because
  // chrome::GetDefaultUserDataDirectory expects to find the Chromium version
  // number in the app bundle path.
  resource_util::OverrideUserDataDir(settings_, command_line);

  ChromeMainDelegate::PreSandboxStartup();

  // Initialize crash reporting state for this process/module.
  // chrome::DIR_CRASH_DUMPS must be configured before calling this function.
  crash_reporting::PreSandboxStartup(*command_line, process_type);
}

void ChromeMainDelegateCef::PreCreateMainMessageLoop() {
  // The parent ChromeMainDelegate implementation creates the NSApplication
  // instance on macOS, and we intentionally don't want to do that here.
  // TODO(macos): Do we need l10n_util::OverrideLocaleWithCocoaLocale()?
  runner_->PreCreateMainMessageLoop();
}

int ChromeMainDelegateCef::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    return runner_->RunMainProcess(main_function_params);
  }

  return ChromeMainDelegate::RunProcess(process_type, main_function_params);
}

#if defined(OS_LINUX)
void ChromeMainDelegateCef::ZygoteForked() {
  ChromeMainDelegate::ZygoteForked();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif  // defined(OS_LINUX)

content::ContentClient* ChromeMainDelegateCef::CreateContentClient() {
  return &chrome_content_client_cef_;
}

content::ContentBrowserClient*
ChromeMainDelegateCef::CreateContentBrowserClient() {
  // Match the logic in the parent ChromeMainDelegate implementation, but create
  // our own object type.
  chrome_content_browser_client_ =
      std::make_unique<ChromeContentBrowserClientCef>();
  return chrome_content_browser_client_.get();
}

content::ContentRendererClient*
ChromeMainDelegateCef::CreateContentRendererClient() {
  return g_chrome_content_renderer_client.Pointer();
}

CefRefPtr<CefRequestContext> ChromeMainDelegateCef::GetGlobalRequestContext() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->request_context();
  return nullptr;
}

CefBrowserContext* ChromeMainDelegateCef::CreateNewBrowserContext(
    const CefRequestContextSettings& settings,
    base::OnceClosure initialized_cb) {
  auto context = new ChromeBrowserContext(settings);
  context->InitializeAsync(std::move(initialized_cb));
  return context;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetBackgroundTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->background_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserVisibleTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_visible_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserBlockingTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_blocking_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetRenderTaskRunner() {
  auto renderer_client = content_renderer_client();
  if (renderer_client)
    return renderer_client->render_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetWebWorkerTaskRunner() {
  auto renderer_client = content_renderer_client();
  if (renderer_client)
    return renderer_client->GetCurrentTaskRunner();
  return nullptr;
}

ChromeContentBrowserClientCef* ChromeMainDelegateCef::content_browser_client()
    const {
  return static_cast<ChromeContentBrowserClientCef*>(
      chrome_content_browser_client_.get());
}

ChromeContentRendererClientCef* ChromeMainDelegateCef::content_renderer_client()
    const {
  if (!g_chrome_content_renderer_client.IsCreated())
    return nullptr;
  return g_chrome_content_renderer_client.Pointer();
}