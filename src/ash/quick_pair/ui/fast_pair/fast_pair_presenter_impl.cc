// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"

#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center.h"

namespace {

const char kDiscoveryLearnMoreLink[] =
    "https://support.google.com/chromebook?p=fast_pair_m101";
const char kAssociateAccountLearnMoreLink[] =
    "https://support.google.com/chromebook?p=bluetooth_pairing_m101";

bool ShouldShowUserEmail(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairPresenterImpl::Factory*
    FastPairPresenterImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairPresenter> FastPairPresenterImpl::Factory::Create(
    message_center::MessageCenter* message_center) {
  if (g_test_factory_)
    return g_test_factory_->CreateInstance(message_center);

  return base::WrapUnique(new FastPairPresenterImpl(message_center));
}

// static
void FastPairPresenterImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPresenterImpl::Factory::~Factory() = default;

FastPairPresenterImpl::FastPairPresenterImpl(
    message_center::MessageCenter* message_center)
    : notification_controller_(
          std::make_unique<FastPairNotificationController>(message_center)) {}

FastPairPresenterImpl::~FastPairPresenterImpl() = default;

void FastPairPresenterImpl::ShowDiscovery(scoped_refptr<Device> device,
                                          DiscoveryCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryMetadataRetrieved,
                          weak_pointer_factory_.GetWeakPtr(), std::move(device),
                          std::move(callback)));
}

void FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata)
    return;

  // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
  // available then the device is v1.
  if (device_metadata->GetDetails()
          .anti_spoofing_key_pair()
          .public_key()
          .empty()) {
    device->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion,
                              {1});
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion1);
  } else {
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion2);
  }

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();

  if (!ShouldShowUserEmail(
          Shell::Get()->session_controller()->login_status()) ||
      !identity_manager) {
    notification_controller_->ShowGuestDiscoveryNotification(
        base::ASCIIToUTF16(device_metadata->GetDetails().name()),
        device_metadata->image(),
        base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                            weak_pointer_factory_.GetWeakPtr(), callback),
        base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                            weak_pointer_factory_.GetWeakPtr(), callback),
        base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                       weak_pointer_factory_.GetWeakPtr(), callback));
    return;
  }

  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  notification_controller_->ShowUserDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnDiscoveryClicked(DiscoveryCallback callback) {
  callback.Run(DiscoveryAction::kPairToDevice);
}

void FastPairPresenterImpl::OnDiscoveryDismissed(DiscoveryCallback callback,
                                                 bool user_dismissed) {
  callback.Run(user_dismissed ? DiscoveryAction::kDismissedByUser
                              : DiscoveryAction::kDismissed);
}

void FastPairPresenterImpl::OnDiscoveryLearnMoreClicked(
    DiscoveryCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kDiscoveryLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction);
  callback.Run(DiscoveryAction::kLearnMore);
}

void FastPairPresenterImpl::ShowPairing(scoped_refptr<Device> device) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device)));
}

void FastPairPresenterImpl::OnPairingMetadataRetrieved(
    scoped_refptr<Device> device,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowPairingNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(), base::DoNothing());
}

void FastPairPresenterImpl::ShowPairingFailed(scoped_refptr<Device> device,
                                              PairingFailedCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(callback)));
}

void FastPairPresenterImpl::OnPairingFailedMetadataRetrieved(
    scoped_refptr<Device> device,
    PairingFailedCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowErrorNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnNavigateToSettings,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnNavigateToSettings(
    PairingFailedCallback callback) {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
    RecordNavigateToSettingsResult(/*success=*/true);
  } else {
    QP_LOG(WARNING) << "Cannot open Bluetooth Settings since it's not possible "
                       "to opening WebUI settings";
    RecordNavigateToSettingsResult(/*success=*/false);
  }

  callback.Run(PairingFailedAction::kNavigateToSettings);
}

void FastPairPresenterImpl::OnPairingFailedDismissed(
    PairingFailedCallback callback,
    bool user_dismissed) {
  callback.Run(user_dismissed ? PairingFailedAction::kDismissedByUser
                              : PairingFailedAction::kDismissed);
}

void FastPairPresenterImpl::ShowAssociateAccount(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), std::move(device),
          std::move(callback)));
}

void FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  QP_LOG(VERBOSE) << __func__ << ": " << device;
  if (!device_metadata) {
    return;
  }

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager) {
    QP_LOG(ERROR) << __func__
                  << ": IdentityManager is not available for Associate Account "
                     "notification.";
    return;
  }

  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;

  notification_controller_->ShowAssociateAccount(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountActionClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnAssociateAccountDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnAssociateAccountActionClicked(
    AssociateAccountCallback callback) {
  callback.Run(AssociateAccountAction::kAssoicateAccount);
}

void FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked(
    AssociateAccountCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kAssociateAccountLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction);
  callback.Run(AssociateAccountAction::kLearnMore);
}

void FastPairPresenterImpl::OnAssociateAccountDismissed(
    AssociateAccountCallback callback,
    bool user_dismissed) {
  callback.Run(user_dismissed ? AssociateAccountAction::kDismissedByUser
                              : AssociateAccountAction::kDismissed);
}

void FastPairPresenterImpl::ShowCompanionApp(scoped_refptr<Device> device,
                                             CompanionAppCallback callback) {}

void FastPairPresenterImpl::RemoveNotifications(scoped_refptr<Device> device) {
  notification_controller_->RemoveNotifications();
}

}  // namespace quick_pair
}  // namespace ash
