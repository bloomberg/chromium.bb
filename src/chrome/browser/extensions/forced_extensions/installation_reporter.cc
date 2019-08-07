// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"

#include <map>

#include "base/logging.h"
#include "base/no_destructor.h"

namespace {

using InstallationDataMap =
    std::map<extensions::ExtensionId,
             extensions::InstallationReporter::InstallationData>;

InstallationDataMap& GetInstallationDataMap(const Profile* profile) {
  static base::NoDestructor<std::map<const Profile*, InstallationDataMap>>
      failure_maps;
  return (*failure_maps)[profile];
}

}  // namespace

namespace extensions {

InstallationReporter::InstallationData::InstallationData() = default;

InstallationReporter::InstallationData::InstallationData(
    const InstallationData&) = default;

std::string InstallationReporter::GetFormattedInstallationData(
    const InstallationData& data) {
  std::ostringstream str;
  str << "failure_reason: "
      << static_cast<int>(data.failure_reason.value_or(FailureReason::UNKNOWN));
  if (data.install_error_detail) {
    str << "; install_error_detail: "
        << static_cast<int>(data.install_error_detail.value());
  }
  if (data.install_stage) {
    str << "; install_stage: " << static_cast<int>(data.install_stage.value());
  }
  if (data.downloading_stage) {
    str << "; downloading_stage: "
        << static_cast<int>(data.downloading_stage.value());
  }
  return str.str();
}

// static
void InstallationReporter::ReportInstallationStage(const Profile* profile,
                                                   const ExtensionId& id,
                                                   Stage stage) {
  GetInstallationDataMap(profile)[id].install_stage = stage;
}

// static
void InstallationReporter::ReportDownloadingStage(
    const Profile* profile,
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  GetInstallationDataMap(profile)[id].downloading_stage = stage;
}

// static
void InstallationReporter::ReportFailure(const Profile* profile,
                                         const ExtensionId& id,
                                         FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  GetInstallationDataMap(profile)[id].failure_reason = reason;
}

// static
void InstallationReporter::ReportCrxInstallError(
    const Profile* profile,
    const ExtensionId& id,
    FailureReason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == FailureReason::CRX_INSTALL_ERROR_DECLINED ||
         reason ==
             FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE ||
         reason == FailureReason::CRX_INSTALL_ERROR_OTHER);
  InstallationData& data = GetInstallationDataMap(profile)[id];
  data.failure_reason = reason;
  data.install_error_detail = crx_install_error;
}

// static
InstallationReporter::InstallationData InstallationReporter::Get(
    const Profile* profile,
    const ExtensionId& id) {
  InstallationDataMap& map = GetInstallationDataMap(profile);
  auto it = map.find(id);
  return it == map.end() ? InstallationData() : it->second;
}

// static
void InstallationReporter::Clear(const Profile* profile) {
  GetInstallationDataMap(profile).clear();
}

}  //  namespace extensions
