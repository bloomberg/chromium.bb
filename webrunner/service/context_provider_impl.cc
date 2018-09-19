// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/io.h>
#include <lib/fdio/util.h>
#include <lib/zx/job.h>
#include <stdio.h>
#include <zircon/processargs.h>

#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "webrunner/service/common.h"

namespace webrunner {
namespace {

// Relaunches the current executable as a Context process.
base::Process LaunchContextProcess(const base::CommandLine& launch_command,
                                   const base::LaunchOptions& launch_options) {
  return base::LaunchProcess(launch_command, launch_options);
}

// Returns true if |handle| is connected to a directory.
bool IsValidDirectory(zx_handle_t handle) {
  base::File directory =
      base::fuchsia::GetFileFromHandle(zx::handle(fdio_service_clone(handle)));
  if (!directory.IsValid())
    return false;

  base::File::Info info;
  if (!directory.GetInfo(&info)) {
    LOG(ERROR) << "Could not query FileInfo for handle.";
    directory.Close();
    return false;
  }

  if (!info.is_directory) {
    LOG(ERROR) << "Handle is not a directory.";
    return false;
  }

  return true;
}

}  // namespace

ContextProviderImpl::ContextProviderImpl() : ContextProviderImpl(false) {}

ContextProviderImpl::ContextProviderImpl(bool use_shared_tmp)
    : launch_(base::BindRepeating(&LaunchContextProcess)),
      use_shared_tmp_(use_shared_tmp) {}

ContextProviderImpl::~ContextProviderImpl() = default;

// static
std::unique_ptr<ContextProviderImpl> ContextProviderImpl::CreateForTest() {
  // Bind the unique_ptr in a two step process.
  // std::make_unique<> doesn't work well with private constructors,
  // and the unique_ptr(raw_ptr*) constructor format isn't permitted as per
  // PRESUBMIT.py policy.
  std::unique_ptr<ContextProviderImpl> provider;
  provider.reset(new ContextProviderImpl(true));
  return provider;
}

void ContextProviderImpl::SetLaunchCallbackForTests(
    const LaunchContextProcessCallback& launch) {
  launch_ = launch;
}

void ContextProviderImpl::Create(
    chromium::web::CreateContextParams params,
    ::fidl::InterfaceRequest<chromium::web::Context> context_request) {
  DCHECK(context_request.is_valid());

  base::CommandLine launch_command = *base::CommandLine::ForCurrentProcess();

  base::LaunchOptions launch_options;

  // Clone job because the context needs to be able to spawn child processes.
  launch_options.spawn_flags = FDIO_SPAWN_CLONE_JOB;

  // Clone stdout/stderr to get logs in system debug log.
  launch_options.fds_to_remap.push_back(
      std::make_pair(STDERR_FILENO, STDERR_FILENO));
  launch_options.fds_to_remap.push_back(
      std::make_pair(STDOUT_FILENO, STDOUT_FILENO));

  // Context and child processes need access to the read-only package files.
  launch_options.paths_to_clone.push_back(base::FilePath("/pkg"));

  // Context needs access to the read-only SSL root certificates list.
  launch_options.paths_to_clone.push_back(base::FilePath("/config/ssl"));

  if (use_shared_tmp_)
    launch_options.paths_to_clone.push_back(base::FilePath("/tmp"));

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  zx::channel context_handle(context_request.TakeChannel());
  launch_options.handles_to_transfer.push_back(
      {kContextRequestHandleId, context_handle.get()});

  // Pass /svc directory specified by the caller.
  launch_options.paths_to_transfer.push_back(base::PathToTransfer{
      base::FilePath("/svc"), std::move(params.service_directory.release())});

  // Bind |data_directory| to /data directory, if provided.
  if (params.data_directory) {
    if (!IsValidDirectory(params.data_directory.get()))
      return;

    base::FilePath data_path;
    CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_path));
    launch_options.paths_to_transfer.push_back(
        base::PathToTransfer{data_path, params.data_directory.release()});
  }

  // Isolate the child Context processes by containing them within their own
  // respective jobs.
  zx::job job;
  zx_status_t status = zx::job::create(*base::GetDefaultJob(), 0, &job);
  ZX_CHECK(status == ZX_OK, status) << "zx_job_create";
  launch_options.job_handle = job.get();

  ignore_result(launch_.Run(std::move(launch_command), launch_options));

  ignore_result(context_handle.release());
  ignore_result(job.release());
}

void ContextProviderImpl::Bind(
    fidl::InterfaceRequest<chromium::web::ContextProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace webrunner
