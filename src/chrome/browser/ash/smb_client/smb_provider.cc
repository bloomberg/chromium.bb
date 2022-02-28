// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_provider.h"

#include <utility>

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/smb_client/smb_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_share_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {
namespace smb_client {

SmbProvider::SmbProvider()
    : provider_id_(file_system_provider::ProviderId::CreateFromNativeId("smb")),
      capabilities_(false /* configurable */,
                    false /* watchable */,
                    true /* multiple_mounts */,
                    extensions::SOURCE_NETWORK),
      name_(l10n_util::GetStringUTF8(IDS_SMB_SHARES_ADD_SERVICE_MENU_OPTION)) {
  icon_set_.SetIcon(file_system_provider::IconSet::IconSize::SIZE_16x16,
                    GURL("chrome://theme/IDR_SMB_ICON"));
  icon_set_.SetIcon(file_system_provider::IconSet::IconSize::SIZE_32x32,
                    GURL("chrome://theme/IDR_SMB_ICON@2x"));
}

SmbProvider::~SmbProvider() = default;

std::unique_ptr<file_system_provider::ProvidedFileSystemInterface>
SmbProvider::CreateProvidedFileSystem(
    Profile* profile,
    const file_system_provider::ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<SmbFileSystem>(file_system_info);
}

const file_system_provider::Capabilities& SmbProvider::GetCapabilities() const {
  return capabilities_;
}

const file_system_provider::ProviderId& SmbProvider::GetId() const {
  return provider_id_;
}

const std::string& SmbProvider::GetName() const {
  return name_;
}

const file_system_provider::IconSet& SmbProvider::GetIconSet() const {
  return icon_set_;
}

bool SmbProvider::RequestMount(Profile* profile) {
  smb_dialog::SmbShareDialog::Show();
  return true;
}

}  // namespace smb_client
}  // namespace ash
