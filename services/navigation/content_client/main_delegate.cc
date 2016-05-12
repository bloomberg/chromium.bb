// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/content_client/main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "services/navigation/content_client/content_browser_client.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"

namespace navigation {

MainDelegate::MainDelegate() {}
MainDelegate::~MainDelegate() {}

bool MainDelegate::BasicStartupComplete(int* exit_code) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  content::SetContentClient(&content_client_);

  return false;
}

void MainDelegate::PreSandboxStartup() {
  ui::InitializeInputMethodForTesting();
}

content::ContentBrowserClient* MainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new ContentBrowserClient);
  return browser_client_.get();
}

}  // namespace navigation
