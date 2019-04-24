// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/io.h>
#include <lib/fdio/util.h>
#include <lib/zx/job.h>
#include <stdio.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "fuchsia/engine/common.h"
#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"

namespace {

// Returns the underlying channel if |directory| is a client endpoint for a
// |fuchsia::io::Directory| protocol. Otherwise, returns an empty channel.
zx::channel ValidateDirectoryAndTakeChannel(
    fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle) {
  fidl::SynchronousInterfacePtr<fuchsia::io::Directory> directory =
      directory_handle.BindSync();
  zx_status_t status = ZX_ERR_INTERNAL;
  std::vector<uint8_t> entries;

  directory->ReadDirents(0, &status, &entries);
  if (status == ZX_OK) {
    return directory.Unbind().TakeChannel();
  }

  // Not a directory.
  return zx::channel();
}

}  // namespace

ContextProviderImpl::ContextProviderImpl() = default;

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    chromium::web::CreateContextParams params,
    ::fidl::InterfaceRequest<chromium::web::Context> context_request) {
  chromium::web::CreateContextParams2 converted_params;

  converted_params.set_service_directory(
      fidl::InterfaceHandle<fuchsia::io::Directory>(
          std::move(params.service_directory)));

  if (params.data_directory) {
    converted_params.set_data_directory(
        fidl::InterfaceHandle<fuchsia::io::Directory>(
            std::move(params.data_directory)));
  }
  Create2(std::move(converted_params), std::move(context_request));
}

void ContextProviderImpl::Create2(
    chromium::web::CreateContextParams2 params,
    fidl::InterfaceRequest<chromium::web::Context> context_request) {
  if (!context_request.is_valid()) {
    // TODO(crbug.com/934539): Add type epitaph.
    DLOG(WARNING) << "Invalid |context_request|.";
    return;
  }
  if (!params.has_service_directory()) {
    // TODO(crbug.com/934539): Add type epitaph.
    DLOG(WARNING)
        << "Missing argument |service_directory| in CreateContextParams2.";
    return;
  }

  base::LaunchOptions launch_options;
  service_manager::SandboxPolicyFuchsia sandbox_policy;
  sandbox_policy.Initialize(service_manager::SANDBOX_TYPE_WEB_CONTEXT);
  sandbox_policy.SetServiceDirectory(
      std::move(*params.mutable_service_directory()));
  sandbox_policy.UpdateLaunchOptionsForSandbox(&launch_options);

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  zx::channel context_handle(context_request.TakeChannel());
  launch_options.handles_to_transfer.push_back(
      {kContextRequestHandleId, context_handle.get()});

  // Bind |data_directory| to /data directory, if provided.
  if (params.has_data_directory()) {
    zx::channel data_directory_channel = ValidateDirectoryAndTakeChannel(
        std::move(*params.mutable_data_directory()));
    if (data_directory_channel.get() == ZX_HANDLE_INVALID) {
      // TODO(crbug.com/934539): Add type epitaph.
      DLOG(WARNING)
          << "Invalid argument |data_directory| in CreateContextParams2.";
      return;
    }

    base::FilePath data_path;
    if (!base::PathService::Get(base::DIR_APP_DATA, &data_path)) {
      // TODO(crbug.com/934539): Add type epitaph.
      DLOG(WARNING) << "Failed to get data directory service path.";
      return;
    }
    launch_options.paths_to_transfer.push_back(
        base::PathToTransfer{data_path, data_directory_channel.release()});
  }

  // Isolate the child Context processes by containing them within their own
  // respective jobs.
  zx::job job;
  zx_status_t status = zx::job::create(*base::GetDefaultJob(), 0, &job);
  if (status != ZX_OK) {
    ZX_LOG(FATAL, status) << "zx_job_create";
    return;
  }
  launch_options.job_handle = job.get();

  const base::CommandLine* launch_command =
      base::CommandLine::ForCurrentProcess();
  if (launch_for_test_)
    launch_for_test_.Run(*launch_command, launch_options);
  else
    base::LaunchProcess(*launch_command, launch_options);

  // |context_handle| was transferred (not copied) to the Context process.
  ignore_result(context_handle.release());
}

void ContextProviderImpl::Bind(
    fidl::InterfaceRequest<chromium::web::ContextProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void ContextProviderImpl::SetLaunchCallbackForTest(
    LaunchCallbackForTest launch) {
  launch_for_test_ = std::move(launch);
}
