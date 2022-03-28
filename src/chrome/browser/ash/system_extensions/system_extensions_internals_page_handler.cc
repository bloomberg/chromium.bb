// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_internals_page_handler.h"

#include "base/debug/stack_trace.h"

SystemExtensionsInternalsPageHandler::SystemExtensionsInternalsPageHandler(
    Profile* profile)
    : profile_(profile) {}

SystemExtensionsInternalsPageHandler::~SystemExtensionsInternalsPageHandler() =
    default;

void SystemExtensionsInternalsPageHandler::
    InstallSystemExtensionFromDownloadsDir(
        const base::SafeBaseName& system_extension_dir_name,
        InstallSystemExtensionFromDownloadsDirCallback callback) {
  base::FilePath downloads_path;
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    std::move(callback).Run(false);
    return;
  }

  auto& install_manager =
      SystemExtensionsProvider::Get(profile_)->install_manager();
  base::FilePath system_extension_dir =
      downloads_path.Append(system_extension_dir_name);

  install_manager.InstallUnpackedExtensionFromDir(
      system_extension_dir,
      base::BindOnce(&SystemExtensionsInternalsPageHandler::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemExtensionsInternalsPageHandler::OnInstallFinished(
    InstallSystemExtensionFromDownloadsDirCallback callback,
    InstallStatusOrSystemExtensionId result) {
  if (!result.ok()) {
    LOG(ERROR) << "failed with: " << static_cast<int32_t>(result.status());
  }

  std::move(callback).Run(result.ok());
}
