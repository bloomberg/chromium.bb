// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"

#include <lib/fdio/spawn.h>
#include <stdio.h>
#include <zircon/processargs.h>

#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/sandbox/switches.h"

namespace service_manager {
namespace {

enum SandboxFeature {
  // Clones the job. This is required to start new processes (to make it useful
  // the process will also need access to the fuchsia.process.Launcher service).
  kCloneJob = 1 << 0,

  // Provides access to resources required by Vulkan.
  kProvideVulkanResources = 1 << 1,

  // Read only access to /config/ssl, which contains root certs info.
  kProvideSslConfig = 1 << 2,
};

struct SandboxConfig {
  SandboxType type;
  base::span<const char* const> services;
  uint32_t features;
};

constexpr SandboxConfig kSandboxConfigs[] = {
    {
        SANDBOX_TYPE_WEB_CONTEXT,

        // Services directory is passed by calling SetServiceDirectory().
        base::span<const char* const>(),

        // kCloneJob: context process needs to be able to spawn child processes.
        // kProvideVulkanResources: Vulkan access is required to delegate to the
        // GPU process. kProvideSslConfig: Context process is responsible for
        // cert verification.
        kCloneJob | kProvideVulkanResources | kProvideSslConfig,
    },
    {
        SANDBOX_TYPE_RENDERER,
        base::make_span(
            (const char* const[]){fuchsia::fonts::Provider::Name_,
                                  fuchsia::mediacodec::CodecFactory::Name_}),
        0,
    },
    {
        SANDBOX_TYPE_GPU,
        base::make_span(
            (const char* const[]){fuchsia::ui::scenic::Scenic::Name_,
                                  "fuchsia.vulkan.loader.Loader"}),
        kProvideVulkanResources,
    },
};

constexpr SandboxConfig kDefaultConfig = {
    SANDBOX_TYPE_INVALID,
    base::span<const char* const>(),
    0,
};

const SandboxConfig& GetConfigForSandboxType(SandboxType type) {
  for (auto& config : kSandboxConfigs) {
    if (config.type == type)
      return config;
  }
  return kDefaultConfig;
}

}  // namespace

SandboxPolicyFuchsia::SandboxPolicyFuchsia() = default;

SandboxPolicyFuchsia::~SandboxPolicyFuchsia() {
  if (service_directory_) {
    service_directory_task_runner_->DeleteSoon(FROM_HERE,
                                               std::move(service_directory_));
  }
}

void SandboxPolicyFuchsia::Initialize(service_manager::SandboxType type) {
  DCHECK_NE(type, service_manager::SANDBOX_TYPE_INVALID);
  DCHECK_EQ(type_, service_manager::SANDBOX_TYPE_INVALID);

  type_ = type;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoSandbox)) {
    type_ = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  }

  // If we need to pass some services for the given sandbox type then create
  // |sandbox_directory_| and initialize it with the corresponding list of
  // services. FilteredServiceDirectory must be initialized on a thread that has
  // async_dispatcher.
  const SandboxConfig& config = GetConfigForSandboxType(type_);
  if (!config.services.empty()) {
    service_directory_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    service_directory_ =
        std::make_unique<base::fuchsia::FilteredServiceDirectory>(
            base::fuchsia::ServiceDirectoryClient::ForCurrentProcess());
    for (const char* service_name : config.services)
      service_directory_->AddService(service_name);

    // Bind the service directory and store the client channel for
    // UpdateLaunchOptionsForSandbox()'s use.
    service_directory_client_ = service_directory_->ConnectClient();
    CHECK(service_directory_client_);
  }
}

void SandboxPolicyFuchsia::SetServiceDirectory(
    fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory_client) {
  DCHECK_EQ(type_, SANDBOX_TYPE_WEB_CONTEXT);
  DCHECK(!service_directory_client_);

  service_directory_client_ = std::move(service_directory_client);
}

void SandboxPolicyFuchsia::UpdateLaunchOptionsForSandbox(
    base::LaunchOptions* options) {
  DCHECK_NE(type_, service_manager::SANDBOX_TYPE_INVALID);

  // Always clone stderr to get logs output.
  options->fds_to_remap.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));
  options->fds_to_remap.push_back(std::make_pair(STDOUT_FILENO, STDOUT_FILENO));

  if (type_ == service_manager::SANDBOX_TYPE_NO_SANDBOX) {
    options->spawn_flags = FDIO_SPAWN_CLONE_NAMESPACE | FDIO_SPAWN_CLONE_JOB;
    options->clear_environment = false;
    return;
  }

  // Map /pkg (read-only files deployed from the package) into the child's
  // namespace.
  base::FilePath package_root;
  base::PathService::Get(base::DIR_ASSETS, &package_root);
  options->paths_to_clone.push_back(package_root);

  // Clear environmental variables to better isolate the child from
  // this process.
  options->clear_environment = true;

  // Don't clone anything by default.
  options->spawn_flags = 0;

  const SandboxConfig& config = GetConfigForSandboxType(type_);

  if (config.features & kCloneJob)
    options->spawn_flags |= FDIO_SPAWN_CLONE_JOB;

  if (config.features & kProvideSslConfig)
    options->paths_to_clone.push_back(base::FilePath("/config/ssl"));

  if (config.features & kProvideVulkanResources) {
    // /dev/class/gpu and /config/vulkan/icd.d are to used configure and
    // access the GPU.
    options->paths_to_clone.push_back(base::FilePath("/dev/class/gpu"));
    const auto vulkan_icd_path = base::FilePath("/config/vulkan/icd.d");
    if (base::PathExists(vulkan_icd_path))
      options->paths_to_clone.push_back(vulkan_icd_path);
  }

  // If the process needs access to any services then transfer the
  // |service_directory_client_| handle for it to mount at "/svc".
  if (service_directory_client_) {
    options->paths_to_transfer.push_back(base::PathToTransfer{
        base::FilePath("/svc"),
        service_directory_client_.TakeChannel().release()});
  }
}

}  // namespace service_manager
