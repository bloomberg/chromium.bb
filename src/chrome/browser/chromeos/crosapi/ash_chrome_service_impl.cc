// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"
#include "chrome/browser/chromeos/crosapi/browser_manager.h"
#include "chrome/browser/chromeos/crosapi/feedback_ash.h"
#include "chrome/browser/chromeos/crosapi/file_manager_ash.h"
#include "chrome/browser/chromeos/crosapi/keystore_service_ash.h"
#include "chrome/browser/chromeos/crosapi/message_center_ash.h"
#include "chrome/browser/chromeos/crosapi/screen_manager_ash.h"
#include "chrome/browser/chromeos/crosapi/select_file_ash.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"

namespace crosapi {

AshChromeServiceImpl::AshChromeServiceImpl(
    mojo::PendingReceiver<mojom::AshChromeService> pending_receiver)
    : receiver_(this, std::move(pending_receiver)),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()) {
  // TODO(hidehiko): Remove non-critical log from here.
  // Currently this is the signal that the connection is established.
  LOG(WARNING) << "AshChromeService connected.";
}

AshChromeServiceImpl::~AshChromeServiceImpl() = default;

void AshChromeServiceImpl::BindAccountManager(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  DVLOG(1) << "Binding AccountManager receiver";
  // Assumptions:
  // 1. TODO(https://crbug.com/1102768): Multi-Signin / Fast-User-Switching is
  // disabled.
  // 2. ash-chrome has 1 and only 1 "regular" |Profile|.
#if DCHECK_IS_ON()
  int num_regular_profiles = 0;
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (chromeos::ProfileHelper::IsRegularProfile(profile))
      num_regular_profiles++;
  }
  DCHECK_EQ(1, num_regular_profiles);
#endif  // DCHECK_IS_ON()
  // Given these assumptions, there is 1 and only 1 Account Manager that
  // can/should be contacted - the one attached to the regular |Profile| in
  // ash-chrome, for the current |User|.
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  chromeos::AccountManager* const account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(/* profile_path = */ profile->GetPath().value());
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  account_manager_ash_ = std::make_unique<crosapi::AccountManagerAsh>(
      account_manager, std::move(receiver));
}

void AshChromeServiceImpl::BindFileManager(
    mojo::PendingReceiver<crosapi::mojom::FileManager> receiver) {
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  file_manager_ash_ =
      std::make_unique<crosapi::FileManagerAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  keystore_service_ash_ =
      std::make_unique<crosapi::KeystoreServiceAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindMessageCenter(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  message_center_ash_ = std::make_unique<MessageCenterAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  select_file_ash_ = std::make_unique<SelectFileAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void AshChromeServiceImpl::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void AshChromeServiceImpl::BindFeedback(
    mojo::PendingReceiver<mojom::Feedback> receiver) {
  // TODO(https://crbug.com/1148448): Convert this to allow multiple,
  // simultaneous crosapi clients. See BindScreenManager for an example.
  feedback_ash_ = std::make_unique<FeedbackAsh>(std::move(receiver));
}

void AshChromeServiceImpl::OnLacrosStartup(mojom::LacrosInfoPtr lacros_info) {
  BrowserManager::Get()->set_lacros_version(lacros_info->lacros_version);
}

}  // namespace crosapi
