// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/file_system_app.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#elif defined(OS_ANDROID)
#include "base/base_paths_android.h"
#include "base/path_service.h"
#elif defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#include "base/path_service.h"
#endif

namespace filesystem {

namespace {

const char kUserDataDir[] = "user-data-dir";

}  // namespace

FileSystemApp::FileSystemApp(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_binding_(this, std::move(receiver)),
      lock_table_(base::MakeRefCounted<LockTable>()) {}

FileSystemApp::~FileSystemApp() = default;

void FileSystemApp::OnConnect(
    const service_manager::ConnectSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  if (interface_name == mojom::FileSystem::Name_) {
    file_systems_.Add(
        std::make_unique<FileSystemImpl>(source_info.identity, GetUserDataDir(),
                                         lock_table_),
        mojo::PendingReceiver<mojom::FileSystem>(std::move(receiver_pipe)));
  }
}

// static
base::FilePath FileSystemApp::GetUserDataDir() {
  base::FilePath path;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserDataDir)) {
    path = command_line->GetSwitchValuePath(kUserDataDir);
  } else {
#if defined(OS_WIN)
    CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
#elif defined(OS_MACOSX)
    CHECK(base::PathService::Get(base::DIR_APP_DATA, &path));
#elif defined(OS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
#elif defined(OS_LINUX)
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    path = base::nix::GetXDGDirectory(
        env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
#else
    NOTIMPLEMENTED();
#endif
    path = path.Append(FILE_PATH_LITERAL("filesystem"));
  }

  if (!base::PathExists(path))
    base::CreateDirectory(path);

  return path;
}

}  // namespace filesystem
