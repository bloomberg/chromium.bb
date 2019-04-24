// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_engine_main_delegate.h"

#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/browser/web_engine_browser_main.h"
#include "fuchsia/engine/browser/web_engine_content_browser_client.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia/engine/web_engine_content_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

WebEngineMainDelegate* g_current_web_engine_main_delegate = nullptr;

void InitLoggingFromCommandLine(const base::CommandLine& command_line) {
  base::FilePath log_filename;
  std::string filename = command_line.GetSwitchValueASCII(switches::kLogFile);
  if (filename.empty()) {
    base::PathService::Get(base::DIR_EXE, &log_filename);
    log_filename = log_filename.AppendASCII("webrunner.log");
  } else {
    log_filename = base::FilePath::FromUTF8Unsafe(filename);
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

void InitializeResourceBundle() {
  base::FilePath pak_file;
  bool result = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(result);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("webrunner.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

}  // namespace

// static
WebEngineMainDelegate* WebEngineMainDelegate::GetInstanceForTest() {
  return g_current_web_engine_main_delegate;
}

WebEngineMainDelegate::WebEngineMainDelegate(zx::channel context_channel)
    : context_channel_(std::move(context_channel)) {
  g_current_web_engine_main_delegate = this;
}

WebEngineMainDelegate::~WebEngineMainDelegate() = default;

bool WebEngineMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  InitLoggingFromCommandLine(*command_line);
  content_client_ = std::make_unique<WebEngineContentClient>();
  SetContentClient(content_client_.get());
  return false;
}

void WebEngineMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

int WebEngineMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

  return WebEngineBrowserMain(main_function_params);
}

content::ContentBrowserClient*
WebEngineMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_ = std::make_unique<WebEngineContentBrowserClient>(
      std::move(context_channel_));
  return browser_client_.get();
}

content::ContentRendererClient*
WebEngineMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WebEngineContentRendererClient>();
  return renderer_client_.get();
}
