// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/main_delegate.h"

namespace service_manager {

MainDelegate::MainDelegate() = default;

MainDelegate::~MainDelegate() = default;

int MainDelegate::RunEmbedderProcess() {
  return 0;
}

void MainDelegate::ShutDownEmbedderProcess() {}

ProcessType MainDelegate::OverrideProcessType() {
  return ProcessType::kDefault;
}

std::unique_ptr<base::Value> MainDelegate::CreateServiceCatalog() {
  return nullptr;
}

bool MainDelegate::ShouldLaunchAsServiceProcess(const Identity& identity) {
  return true;
}

void MainDelegate::AdjustServiceProcessCommandLine(
    const Identity& identity,
    base::CommandLine* command_line) {}

bool MainDelegate::ShouldTerminateServiceManagerOnInstanceQuit(
    const Identity& identity,
    int* exit_code) {
  return false;
}

void MainDelegate::OnServiceManagerInitialized(
    const base::Closure& quit_closure,
    BackgroundServiceManager* service_manager) {}

std::unique_ptr<Service> MainDelegate::CreateEmbeddedService(
    const std::string& service_name) {
  return nullptr;
}

}  // namespace service_manager
