// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"

#include <string>

#include "base/notreached.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {
namespace {

using MojomRmaState = ash::shimless_rma::mojom::State;
using ProtoRmadState = rmad::RmadState::StateCase;

using MojomRmadErrorCode = ash::shimless_rma::mojom::RmadErrorCode;
using ProtoRmadErrorCode = rmad::RmadErrorCode;

using MojomOsUpdateOperation = ash::shimless_rma::mojom::OsUpdateOperation;
using ProtoOsUpdateOperation = update_engine::Operation;

using MojomComponentType = ash::shimless_rma::mojom::ComponentType;
using ProtoComponentType = rmad::RmadComponent;

using MojomComponentRepairState =
    ash::shimless_rma::mojom::ComponentRepairStatus;
using ProtoComponentRepairState =
    rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus;

using MojomWpDisableAction =
    ash::shimless_rma::mojom::WriteProtectDisableCompleteAction;
using ProtoWpDisableAction = rmad::WriteProtectDisableCompleteState::Action;

using MojomProvisioningStatus = ash::shimless_rma::mojom::ProvisioningStatus;
using ProtoProvisioningStatus = rmad::ProvisionStatus::Status;

using MojomCalibrationInstruction =
    ash::shimless_rma::mojom::CalibrationSetupInstruction;
using ProtoCalibrationInstruction = rmad::CalibrationSetupInstruction;

using MojomCalibrationOverallStatus =
    ash::shimless_rma::mojom::CalibrationOverallStatus;
using ProtoCalibrationOverallStatus = rmad::CalibrationOverallStatus;

using MojomCalibrationStatus = ash::shimless_rma::mojom::CalibrationStatus;
using ProtoCalibrationStatus =
    rmad::CalibrationComponentStatus_CalibrationStatus;

using MojomFinalizationStatus = ash::shimless_rma::mojom::FinalizationStatus;
using ProtoFinalizationStatus = rmad::FinalizeStatus_Status;

using MojomUpdateRoFirmwareStatus =
    ash::shimless_rma::mojom::UpdateRoFirmwareStatus;
using ProtoUpdateRoFirmwaretatus = rmad::UpdateRoFirmwareStatus;
}  // namespace

// The rmad state does not map 1:1 with UI app state, the UI handles more states
// such as selecting wifi network and updating Chrome OS.
// Because some states do not exist in rmad a type mapping build rule cannot be
// added to automate conversion and ToMojom(rmad::RmadState::StateCase) must be
// used directly.
// FromMojom is not needed as state is not passed back from javascript.
MojomRmaState EnumTraits<MojomRmaState, ProtoRmadState>::ToMojom(
    ProtoRmadState state) {
  switch (state) {
    case ProtoRmadState::kWelcome:
      return MojomRmaState::kWelcomeScreen;
    case ProtoRmadState::kComponentsRepair:
      return MojomRmaState::kSelectComponents;
    case ProtoRmadState::kDeviceDestination:
      return MojomRmaState::kChooseDestination;
    case ProtoRmadState::kWpDisableMethod:
      return MojomRmaState::kChooseWriteProtectDisableMethod;
    case ProtoRmadState::kWpDisableRsu:
      return MojomRmaState::kEnterRSUWPDisableCode;
    case ProtoRmadState::kWpDisablePhysical:
      return MojomRmaState::kWaitForManualWPDisable;
    case ProtoRmadState::kWpDisableComplete:
      return MojomRmaState::kWPDisableComplete;
    case ProtoRmadState::kUpdateRoFirmware:
      return MojomRmaState::kUpdateRoFirmware;
    case ProtoRmadState::kRestock:
      return MojomRmaState::kRestock;
    case ProtoRmadState::kUpdateDeviceInfo:
      return MojomRmaState::kUpdateDeviceInformation;
    case ProtoRmadState::kCheckCalibration:
      return MojomRmaState::kCheckCalibration;
    case ProtoRmadState::kSetupCalibration:
      return MojomRmaState::kSetupCalibration;
    case ProtoRmadState::kRunCalibration:
      return MojomRmaState::kRunCalibration;
    case ProtoRmadState::kProvisionDevice:
      return MojomRmaState::kProvisionDevice;
    case ProtoRmadState::kWpEnablePhysical:
      return MojomRmaState::kWaitForManualWPEnable;
    case ProtoRmadState::kFinalize:
      return MojomRmaState::kFinalize;
    case ProtoRmadState::kRepairComplete:
      return MojomRmaState::kRepairComplete;

    case ProtoRmadState::STATE_NOT_SET:
    default:
      return MojomRmaState::kUnknown;
  }
}

// static
MojomRmadErrorCode EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::ToMojom(
    ProtoRmadErrorCode error) {
  switch (error) {
    case ProtoRmadErrorCode::RMAD_ERROR_OK:
      return MojomRmadErrorCode::kOk;
    case ProtoRmadErrorCode::RMAD_ERROR_WAIT:
      return MojomRmadErrorCode::kWait;
    case ProtoRmadErrorCode::RMAD_ERROR_EXPECT_REBOOT:
      return MojomRmadErrorCode::kExpectReboot;
    case ProtoRmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN:
      return MojomRmadErrorCode::kExpectShutdown;
    case ProtoRmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED:
      return MojomRmadErrorCode::kRmaNotRequired;
    case ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING:
      return MojomRmadErrorCode::kStateHandlerMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED:
      return MojomRmadErrorCode::kStateHandlerInitializationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_INVALID:
      return MojomRmadErrorCode::kRequestInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING:
      return MojomRmadErrorCode::kRequestArgsMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION:
      return MojomRmadErrorCode::kRequestArgsViolation;
    case ProtoRmadErrorCode::RMAD_ERROR_TRANSITION_FAILED:
      return MojomRmadErrorCode::kTransitionFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_ABORT_FAILED:
      return MojomRmadErrorCode::kAbortFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_MISSING_COMPONENT:
      return MojomRmadErrorCode::kMissingComponent;
    case ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE:
      return MojomRmadErrorCode::kWriteProtectDisableRsuNoChallenge;
    case ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID:
      return MojomRmadErrorCode::kWriteProtectDisableRsuCodeInvalid;
    case ProtoRmadErrorCode::
        RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED:
      return MojomRmadErrorCode::kWriteProtectDisableBatteryNotDisconnected;
    case ProtoRmadErrorCode::
        RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED:
      return MojomRmadErrorCode::kWriteProtectSignalNotDetected;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK:
      return MojomRmadErrorCode::kReimagingDownloadNoNetwork;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR:
      return MojomRmadErrorCode::kReimagingDownloadNetworkError;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED:
      return MojomRmadErrorCode::kReimagingDownloadCancelled;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND:
      return MojomRmadErrorCode::kReimagingUsbNotFound;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND:
      return MojomRmadErrorCode::kReimagingUsbTooManyFound;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE:
      return MojomRmadErrorCode::kReimagingUsbInvalidImage;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED:
      return MojomRmadErrorCode::kReimagingImagingFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE:
      return MojomRmadErrorCode::kReimagingUnknownFailure;
    case ProtoRmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID:
      return MojomRmadErrorCode::kDeviceInfoInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_MISSING:
      return MojomRmadErrorCode::kCalibrationComponentMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_STATUS_MISSING:
      return MojomRmadErrorCode::kCalibrationStatusMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_INVALID:
      return MojomRmadErrorCode::kCalibrationComponentInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED:
      return MojomRmadErrorCode::kCalibrationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED:
      return MojomRmadErrorCode::kProvisioningFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_POWERWASH_FAILED:
      return MojomRmadErrorCode::kPowerwashFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED:
      return MojomRmadErrorCode::kFinalizationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT:
      return MojomRmadErrorCode::kLogUploadFtpServerCannotConnect;
    case ProtoRmadErrorCode::
        RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED:
      return MojomRmadErrorCode::kLogUploadFtpServerConnectionRejected;
    case ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED:
      return MojomRmadErrorCode::kLogUploadFtpServerTransferFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA:
      return MojomRmadErrorCode::kCannotCancelRma;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_GET_LOG:
      return MojomRmadErrorCode::kCannotGetLog;
    case ProtoRmadErrorCode::RMAD_ERROR_DAEMON_INITIALIZATION_FAILED:
      return MojomRmadErrorCode::kDaemonInitializationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_UPDATE_RO_FIRMWARE_FAILED:
      return MojomRmadErrorCode::kUpdateRoFirmwareFailed;

    case ProtoRmadErrorCode::RMAD_ERROR_NOT_SET:
    default:
      NOTREACHED();
      return MojomRmadErrorCode::kNotSet;
  }
  NOTREACHED();
  return MojomRmadErrorCode::kNotSet;
}

// static
bool EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::FromMojom(
    MojomRmadErrorCode error,
    ProtoRmadErrorCode* out) {
  switch (error) {
    case MojomRmadErrorCode::kOk:
      *out = ProtoRmadErrorCode::RMAD_ERROR_OK;
      return true;
    case MojomRmadErrorCode::kWait:
      *out = ProtoRmadErrorCode::RMAD_ERROR_WAIT;
      return true;
    case MojomRmadErrorCode::kExpectReboot:
      *out = ProtoRmadErrorCode::RMAD_ERROR_EXPECT_REBOOT;
      return true;
    case MojomRmadErrorCode::kExpectShutdown:
      *out = ProtoRmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN;
      return true;
    case MojomRmadErrorCode::kRmaNotRequired:
      *out = ProtoRmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED;
      return true;
    case MojomRmadErrorCode::kStateHandlerMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING;
      return true;
    case MojomRmadErrorCode::kStateHandlerInitializationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED;
      return true;
    case MojomRmadErrorCode::kRequestInvalid:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_INVALID;
      return true;
    case MojomRmadErrorCode::kRequestArgsMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING;
      return true;
    case MojomRmadErrorCode::kRequestArgsViolation:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION;
      return true;
    case MojomRmadErrorCode::kTransitionFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_TRANSITION_FAILED;
      return true;
    case MojomRmadErrorCode::kAbortFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_ABORT_FAILED;
      return true;
    case MojomRmadErrorCode::kMissingComponent:
      *out = ProtoRmadErrorCode::RMAD_ERROR_MISSING_COMPONENT;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableRsuNoChallenge:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableRsuCodeInvalid:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableBatteryNotDisconnected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED;
      return true;
    case MojomRmadErrorCode::kWriteProtectSignalNotDetected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadNoNetwork:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadNetworkError:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadCancelled:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED;
      return true;
    case MojomRmadErrorCode::kReimagingUsbNotFound:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND;
      return true;
    case MojomRmadErrorCode::kReimagingUsbTooManyFound:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND;
      return true;
    case MojomRmadErrorCode::kReimagingUsbInvalidImage:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE;
      return true;
    case MojomRmadErrorCode::kReimagingImagingFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED;
      return true;
    case MojomRmadErrorCode::kReimagingUnknownFailure:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE;
      return true;
    case MojomRmadErrorCode::kDeviceInfoInvalid:
      *out = ProtoRmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID;
      return true;
    case MojomRmadErrorCode::kCalibrationComponentMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_MISSING;
      return true;
    case MojomRmadErrorCode::kCalibrationStatusMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_STATUS_MISSING;
      return true;
    case MojomRmadErrorCode::kCalibrationComponentInvalid:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_INVALID;
      return true;
    case MojomRmadErrorCode::kCalibrationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED;
      return true;
    case MojomRmadErrorCode::kProvisioningFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED;
      return true;
    case MojomRmadErrorCode::kPowerwashFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_POWERWASH_FAILED;
      return true;
    case MojomRmadErrorCode::kFinalizationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerCannotConnect:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerConnectionRejected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerTransferFailed:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED;
      return true;
    case MojomRmadErrorCode::kCannotCancelRma:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA;
      return true;
    case MojomRmadErrorCode::kCannotGetLog:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CANNOT_GET_LOG;
      return true;
    case MojomRmadErrorCode::kDaemonInitializationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_DAEMON_INITIALIZATION_FAILED;
      return true;
    case MojomRmadErrorCode::kUpdateRoFirmwareFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_UPDATE_RO_FIRMWARE_FAILED;
      return true;

    case MojomRmadErrorCode::kNotSet:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

MojomOsUpdateOperation
EnumTraits<MojomOsUpdateOperation, ProtoOsUpdateOperation>::ToMojom(
    ProtoOsUpdateOperation operation) {
  switch (operation) {
    case update_engine::IDLE:
      return MojomOsUpdateOperation::kIdle;
    case update_engine::CHECKING_FOR_UPDATE:
      return MojomOsUpdateOperation::kCheckingForUpdate;
    case update_engine::UPDATE_AVAILABLE:
      return MojomOsUpdateOperation::kUpdateAvailable;
    case update_engine::DOWNLOADING:
      return MojomOsUpdateOperation::kDownloading;
    case update_engine::VERIFYING:
      return MojomOsUpdateOperation::kVerifying;
    case update_engine::FINALIZING:
      return MojomOsUpdateOperation::kFinalizing;
    case update_engine::UPDATED_NEED_REBOOT:
      return MojomOsUpdateOperation::kUpdatedNeedReboot;
    case update_engine::REPORTING_ERROR_EVENT:
      return MojomOsUpdateOperation::kReportingErrorEvent;
    case update_engine::ATTEMPTING_ROLLBACK:
      return MojomOsUpdateOperation::kAttemptingRollback;
    case update_engine::DISABLED:
      return MojomOsUpdateOperation::kDisabled;
    case update_engine::NEED_PERMISSION_TO_UPDATE:
      return MojomOsUpdateOperation::kNeedPermissionToUpdate;
    case update_engine::ERROR:
    case update_engine::Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
    case update_engine::Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return MojomOsUpdateOperation::kIdle;
  }
  NOTREACHED();
  return MojomOsUpdateOperation::kIdle;
}

// static
bool EnumTraits<MojomOsUpdateOperation, ProtoOsUpdateOperation>::FromMojom(
    MojomOsUpdateOperation input,
    ProtoOsUpdateOperation* out) {
  switch (input) {
    case MojomOsUpdateOperation::kIdle:
      *out = update_engine::IDLE;
      return true;
    case MojomOsUpdateOperation::kCheckingForUpdate:
      *out = update_engine::CHECKING_FOR_UPDATE;
      return true;
    case MojomOsUpdateOperation::kUpdateAvailable:
      *out = update_engine::UPDATE_AVAILABLE;
      return true;
    case MojomOsUpdateOperation::kDownloading:
      *out = update_engine::DOWNLOADING;
      return true;
    case MojomOsUpdateOperation::kVerifying:
      *out = update_engine::VERIFYING;
      return true;
    case MojomOsUpdateOperation::kFinalizing:
      *out = update_engine::FINALIZING;
      return true;
    case MojomOsUpdateOperation::kUpdatedNeedReboot:
      *out = update_engine::UPDATED_NEED_REBOOT;
      return true;
    case MojomOsUpdateOperation::kReportingErrorEvent:
      *out = update_engine::REPORTING_ERROR_EVENT;
      return true;
    case MojomOsUpdateOperation::kAttemptingRollback:
      *out = update_engine::ATTEMPTING_ROLLBACK;
      return true;
    case MojomOsUpdateOperation::kDisabled:
      *out = update_engine::DISABLED;
      return true;
    case MojomOsUpdateOperation::kNeedPermissionToUpdate:
      *out = update_engine::NEED_PERMISSION_TO_UPDATE;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
MojomComponentType EnumTraits<MojomComponentType, ProtoComponentType>::ToMojom(
    ProtoComponentType component) {
  switch (component) {
    case rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC:
      return MojomComponentType::kAudioCodec;
    case rmad::RmadComponent::RMAD_COMPONENT_BATTERY:
      return MojomComponentType::kBattery;
    case rmad::RmadComponent::RMAD_COMPONENT_STORAGE:
      return MojomComponentType::kStorage;
    case rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED:
      return MojomComponentType::kVpdCached;
    case rmad::RmadComponent::RMAD_COMPONENT_NETWORK:  // Obsolete in M91.
      return MojomComponentType::kNetwork;
    case rmad::RmadComponent::RMAD_COMPONENT_CAMERA:
      return MojomComponentType::kCamera;
    case rmad::RmadComponent::RMAD_COMPONENT_STYLUS:
      return MojomComponentType::kStylus;
    case rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD:
      return MojomComponentType::kTouchpad;
    case rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN:
      return MojomComponentType::kTouchsreen;
    case rmad::RmadComponent::RMAD_COMPONENT_DRAM:
      return MojomComponentType::kDram;
    case rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL:
      return MojomComponentType::kDisplayPanel;
    case rmad::RmadComponent::RMAD_COMPONENT_CELLULAR:
      return MojomComponentType::kCellular;
    case rmad::RmadComponent::RMAD_COMPONENT_ETHERNET:
      return MojomComponentType::kEthernet;
    case rmad::RmadComponent::RMAD_COMPONENT_WIRELESS:
      return MojomComponentType::kWireless;

    // Additional rmad components.
    case rmad::RmadComponent::RMAD_COMPONENT_SCREEN:
      return MojomComponentType::kScreen;
    case rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER:
      return MojomComponentType::kBaseAccelerometer;
    case rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER:
      return MojomComponentType::kLidAccelerometer;
    case rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE:
      return MojomComponentType::kBaseGyroscope;
    case rmad::RmadComponent::RMAD_COMPONENT_LID_GYROSCOPE:
      return MojomComponentType::kLidGyroscope;

    case rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD:
      return MojomComponentType::kKeyboard;
    case rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON:
      return MojomComponentType::kPowerButton;

    case rmad::RmadComponent::RMAD_COMPONENT_UNKNOWN:
    default:
      NOTREACHED();
      return MojomComponentType::kComponentUnknown;
  }
  NOTREACHED();
  return MojomComponentType::kComponentUnknown;
}

// static
bool EnumTraits<MojomComponentType, ProtoComponentType>::FromMojom(
    MojomComponentType component,
    ProtoComponentType* out) {
  switch (component) {
    case MojomComponentType::kAudioCodec:
      *out = rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC;
      return true;
    case MojomComponentType::kBattery:
      *out = rmad::RmadComponent::RMAD_COMPONENT_BATTERY;
      return true;
    case MojomComponentType::kStorage:
      *out = rmad::RmadComponent::RMAD_COMPONENT_STORAGE;
      return true;
    case MojomComponentType::kVpdCached:
      *out = rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED;
      return true;
    case MojomComponentType::kNetwork:
      *out = rmad::RmadComponent::RMAD_COMPONENT_NETWORK;
      return true;  // Obsolete in M91.
    case MojomComponentType::kCamera:
      *out = rmad::RmadComponent::RMAD_COMPONENT_CAMERA;
      return true;
    case MojomComponentType::kStylus:
      *out = rmad::RmadComponent::RMAD_COMPONENT_STYLUS;
      return true;
    case MojomComponentType::kTouchpad:
      *out = rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD;
      return true;
    case MojomComponentType::kTouchsreen:
      *out = rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN;
      return true;
    case MojomComponentType::kDram:
      *out = rmad::RmadComponent::RMAD_COMPONENT_DRAM;
      return true;
    case MojomComponentType::kDisplayPanel:
      *out = rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL;
      return true;
    case MojomComponentType::kCellular:
      *out = rmad::RmadComponent::RMAD_COMPONENT_CELLULAR;
      return true;
    case MojomComponentType::kEthernet:
      *out = rmad::RmadComponent::RMAD_COMPONENT_ETHERNET;
      return true;
    case MojomComponentType::kWireless:
      *out = rmad::RmadComponent::RMAD_COMPONENT_WIRELESS;
      return true;

      // Additional rmad components.
    case MojomComponentType::kScreen:
      *out = rmad::RmadComponent::RMAD_COMPONENT_SCREEN;
      return true;
    case MojomComponentType::kBaseAccelerometer:
      *out = rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER;
      return true;
    case MojomComponentType::kLidAccelerometer:
      *out = rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER;
      return true;
    case MojomComponentType::kBaseGyroscope:
      *out = rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE;
      return true;
    case MojomComponentType::kLidGyroscope:
      *out = rmad::RmadComponent::RMAD_COMPONENT_LID_GYROSCOPE;
      return true;

    case MojomComponentType::kKeyboard:
      *out = rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD;
      return true;
    case MojomComponentType::kPowerButton:
      *out = rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON;
      return true;

    case MojomComponentType::kComponentUnknown:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

// static
MojomComponentRepairState
EnumTraits<MojomComponentRepairState, ProtoComponentRepairState>::ToMojom(
    ProtoComponentRepairState state) {
  switch (state) {
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_ORIGINAL:
      return MojomComponentRepairState::kOriginal;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_REPLACED:
      return MojomComponentRepairState::kReplaced;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_MISSING:
      return MojomComponentRepairState::kMissing;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_UNKNOWN:
      return MojomComponentRepairState::kRepairUnknown;

    default:
      NOTREACHED();
      return MojomComponentRepairState::kRepairUnknown;
  }
  NOTREACHED();
  return MojomComponentRepairState::kRepairUnknown;
}

// static
bool EnumTraits<MojomComponentRepairState, ProtoComponentRepairState>::
    FromMojom(MojomComponentRepairState state, ProtoComponentRepairState* out) {
  switch (state) {
    case MojomComponentRepairState::kOriginal:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL;
      return true;
    case MojomComponentRepairState::kReplaced:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED;
      return true;
    case MojomComponentRepairState::kMissing:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_MISSING;
      return true;
    case MojomComponentRepairState::kRepairUnknown:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_UNKNOWN;
      return true;

    default:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

// static
const std::string&
StructTraits<ash::shimless_rma::mojom::ComponentDataView,
             rmad::ComponentsRepairState_ComponentRepairStatus>::
    identifier(
        const rmad::ComponentsRepairState_ComponentRepairStatus& component) {
  return component.identifier();
}

// static
MojomWpDisableAction
EnumTraits<MojomWpDisableAction, ProtoWpDisableAction>::ToMojom(
    ProtoWpDisableAction action) {
  switch (action) {
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_SKIPPED_ASSEMBLE_DEVICE:
      return MojomWpDisableAction::kSkippedAssembleDevice;
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE:
      return MojomWpDisableAction::kCompleteAssembleDevice;
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_COMPLETE_KEEP_DEVICE_OPEN:
      return MojomWpDisableAction::kCompleteKeepDeviceOpen;
    case rmad::WriteProtectDisableCompleteState::RMAD_WP_DISABLE_UNKNOWN:
      return MojomWpDisableAction::kUnknown;

    default:
      NOTREACHED();
      return MojomWpDisableAction::kUnknown;
  }
  NOTREACHED();
  return MojomWpDisableAction::kUnknown;
}

// static
bool EnumTraits<MojomWpDisableAction, ProtoWpDisableAction>::FromMojom(
    MojomWpDisableAction input,
    ProtoWpDisableAction* out) {
  switch (input) {
    case MojomWpDisableAction::kSkippedAssembleDevice:
      *out = rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_SKIPPED_ASSEMBLE_DEVICE;
      return true;
    case MojomWpDisableAction::kCompleteAssembleDevice:
      *out = rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE;
      return true;
    case MojomWpDisableAction::kCompleteKeepDeviceOpen:
      *out = rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_COMPLETE_KEEP_DEVICE_OPEN;
      return true;

    case MojomWpDisableAction::kUnknown:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

// static
MojomProvisioningStatus
EnumTraits<MojomProvisioningStatus, ProtoProvisioningStatus>::ToMojom(
    ProtoProvisioningStatus status) {
  switch (status) {
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS:
      return MojomProvisioningStatus::kInProgress;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_COMPLETE:
      return MojomProvisioningStatus::kComplete;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING:
      return MojomProvisioningStatus::kFailedBlocking;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING:
      return MojomProvisioningStatus::kFailedNonBlocking;

    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_UNKNOWN:
    default:
      NOTREACHED();
      return MojomProvisioningStatus::kInProgress;
  }
  NOTREACHED();
  return MojomProvisioningStatus::kInProgress;
}

// static
bool EnumTraits<MojomProvisioningStatus, ProtoProvisioningStatus>::FromMojom(
    MojomProvisioningStatus status,
    ProtoProvisioningStatus* out) {
  switch (status) {
    case MojomProvisioningStatus::kInProgress:
      *out = rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS;
      return true;
    case MojomProvisioningStatus::kComplete:
      *out = rmad::ProvisionStatus::RMAD_PROVISION_STATUS_COMPLETE;
      return true;
    case MojomProvisioningStatus::kFailedBlocking:
      *out = rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING;
      return true;
    case MojomProvisioningStatus::kFailedNonBlocking:
      *out = rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING;
      return true;
  }
  NOTREACHED();
  return false;
}

bool StructTraits<ash::shimless_rma::mojom::ComponentDataView,
                  rmad::ComponentsRepairState_ComponentRepairStatus>::
    Read(ash::shimless_rma::mojom::ComponentDataView data,
         rmad::ComponentsRepairState_ComponentRepairStatus* out) {
  rmad::RmadComponent component;
  rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus repair_status;
  std::string identifier;
  if (data.ReadComponent(&component) && data.ReadState(&repair_status) &&
      data.ReadIdentifier(&identifier)) {
    out->set_component(component);
    out->set_repair_status(repair_status);
    out->set_identifier(identifier);
    return true;
  }
  return false;
}

// static
MojomCalibrationInstruction
EnumTraits<MojomCalibrationInstruction, ProtoCalibrationInstruction>::ToMojom(
    ProtoCalibrationInstruction step) {
  switch (step) {
    case ProtoCalibrationInstruction::RMAD_CALIBRATION_INSTRUCTION_UNKNOWN:
      return MojomCalibrationInstruction::kCalibrationInstructionUnknown;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE:
      return MojomCalibrationInstruction::
          kCalibrationInstructionPlaceBaseOnFlatSurface;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_PLACE_LID_ON_FLAT_SURFACE:
      return MojomCalibrationInstruction::
          kCalibrationInstructionPlaceLidOnFlatSurface;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_NO_NEED_CALIBRATION:
    default:
      NOTREACHED();
      return MojomCalibrationInstruction::
          kCalibrationInstructionPlaceBaseOnFlatSurface;
  }
  NOTREACHED();
  return MojomCalibrationInstruction::
      kCalibrationInstructionPlaceBaseOnFlatSurface;
}

// static
bool EnumTraits<MojomCalibrationInstruction, ProtoCalibrationInstruction>::
    FromMojom(MojomCalibrationInstruction step,
              ProtoCalibrationInstruction* out) {
  switch (step) {
    case MojomCalibrationInstruction::kCalibrationInstructionUnknown:
      *out = ProtoCalibrationInstruction::RMAD_CALIBRATION_INSTRUCTION_UNKNOWN;
      return true;
    case MojomCalibrationInstruction::
        kCalibrationInstructionPlaceBaseOnFlatSurface:
      *out = ProtoCalibrationInstruction::
          RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE;
      return true;
    case MojomCalibrationInstruction::
        kCalibrationInstructionPlaceLidOnFlatSurface:
      *out = ProtoCalibrationInstruction::
          RMAD_CALIBRATION_INSTRUCTION_PLACE_LID_ON_FLAT_SURFACE;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
MojomCalibrationOverallStatus
EnumTraits<MojomCalibrationOverallStatus, ProtoCalibrationOverallStatus>::
    ToMojom(ProtoCalibrationOverallStatus step) {
  switch (step) {
    case ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_COMPLETE:
      return MojomCalibrationOverallStatus::kCalibrationOverallComplete;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallCurrentRoundComplete;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_FAILED:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallCurrentRoundFailed;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_INITIALIZATION_FAILED:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallInitializationFailed;

    case ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_UNKNOWN:
    default:
      NOTREACHED();
      return MojomCalibrationOverallStatus::kCalibrationOverallComplete;
  }
  NOTREACHED();
  return MojomCalibrationOverallStatus::kCalibrationOverallComplete;
}

// static
bool EnumTraits<MojomCalibrationOverallStatus, ProtoCalibrationOverallStatus>::
    FromMojom(MojomCalibrationOverallStatus step,
              ProtoCalibrationOverallStatus* out) {
  switch (step) {
    case MojomCalibrationOverallStatus::kCalibrationOverallComplete:
      *out = ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_COMPLETE;
      return true;
    case MojomCalibrationOverallStatus::kCalibrationOverallCurrentRoundComplete:
      *out = ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE;
      return true;
    case MojomCalibrationOverallStatus::kCalibrationOverallCurrentRoundFailed:
      *out = ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_FAILED;
      return true;
    case MojomCalibrationOverallStatus::kCalibrationOverallInitializationFailed:
      *out = ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_INITIALIZATION_FAILED;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
MojomCalibrationStatus
EnumTraits<MojomCalibrationStatus, ProtoCalibrationStatus>::ToMojom(
    ProtoCalibrationStatus step) {
  switch (step) {
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING:
      return MojomCalibrationStatus::kCalibrationWaiting;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS:
      return MojomCalibrationStatus::kCalibrationInProgress;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE:
      return MojomCalibrationStatus::kCalibrationComplete;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_FAILED:
      return MojomCalibrationStatus::kCalibrationFailed;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP:
      return MojomCalibrationStatus::kCalibrationSkip;

    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_UNKNOWN:
    default:
      NOTREACHED();
      return MojomCalibrationStatus::kCalibrationWaiting;
  }
  NOTREACHED();
  return MojomCalibrationStatus::kCalibrationWaiting;
}

// static
bool EnumTraits<MojomCalibrationStatus, ProtoCalibrationStatus>::FromMojom(
    MojomCalibrationStatus step,
    ProtoCalibrationStatus* out) {
  switch (step) {
    case MojomCalibrationStatus::kCalibrationWaiting:
      *out = rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING;
      return true;
    case MojomCalibrationStatus::kCalibrationInProgress:
      *out = rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS;
      return true;
    case MojomCalibrationStatus::kCalibrationComplete:
      *out = rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE;
      return true;
    case MojomCalibrationStatus::kCalibrationFailed:
      *out = rmad::CalibrationComponentStatus::RMAD_CALIBRATION_FAILED;
      return true;
    case MojomCalibrationStatus::kCalibrationSkip:
      *out = rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP;
      return true;
  }
  NOTREACHED();
  return false;
}

// static// static
MojomFinalizationStatus
EnumTraits<MojomFinalizationStatus, ProtoFinalizationStatus>::ToMojom(
    ProtoFinalizationStatus step) {
  switch (step) {
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS:
      return MojomFinalizationStatus::kInProgress;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE:
      return MojomFinalizationStatus::kComplete;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING:
      return MojomFinalizationStatus::kFailedBlocking;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING:
      return MojomFinalizationStatus::kFailedNonBlocking;

    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_UNKNOWN:
    default:
      NOTREACHED();
      return MojomFinalizationStatus::kInProgress;
  }
  NOTREACHED();
  return MojomFinalizationStatus::kInProgress;
}

bool EnumTraits<MojomFinalizationStatus, ProtoFinalizationStatus>::FromMojom(
    MojomFinalizationStatus step,
    ProtoFinalizationStatus* out) {
  switch (step) {
    case MojomFinalizationStatus::kInProgress:
      *out = rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS;
      return true;
    case MojomFinalizationStatus::kComplete:
      *out = rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE;
      return true;
    case MojomFinalizationStatus::kFailedBlocking:
      *out = rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING;
      return true;
    case MojomFinalizationStatus::kFailedNonBlocking:
      *out = rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING;
      return true;
  }
  NOTREACHED();
  return false;
}

bool StructTraits<ash::shimless_rma::mojom::CalibrationComponentStatusDataView,
                  rmad::CalibrationComponentStatus>::
    Read(ash::shimless_rma::mojom::CalibrationComponentStatusDataView data,
         rmad::CalibrationComponentStatus* out) {
  rmad::RmadComponent component;
  rmad::CalibrationComponentStatus::CalibrationStatus status;
  if (data.ReadComponent(&component) && data.ReadStatus(&status)) {
    out->set_component(component);
    out->set_status(status);
    out->set_progress(data.progress());
    return true;
  }
  return false;
}

// static// static
MojomUpdateRoFirmwareStatus
EnumTraits<MojomUpdateRoFirmwareStatus, ProtoUpdateRoFirmwaretatus>::ToMojom(
    ProtoUpdateRoFirmwaretatus step) {
  switch (step) {
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_WAIT_USB:
      return MojomUpdateRoFirmwareStatus::kWaitUsb;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_FILE_NOT_FOUND:
      return MojomUpdateRoFirmwareStatus::kFileNotFound;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING:
      return MojomUpdateRoFirmwareStatus::kDownloading;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING:
      return MojomUpdateRoFirmwareStatus::kUpdating;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING:
      return MojomUpdateRoFirmwareStatus::kRebooting;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_COMPLETE:
      return MojomUpdateRoFirmwareStatus::kComplete;

    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UNKNOWN:
    default:
      NOTREACHED();
      return MojomUpdateRoFirmwareStatus::kUnknown;
  }
  NOTREACHED();
  return MojomUpdateRoFirmwareStatus::kUnknown;
}

bool EnumTraits<MojomUpdateRoFirmwareStatus, ProtoUpdateRoFirmwaretatus>::
    FromMojom(MojomUpdateRoFirmwareStatus step,
              ProtoUpdateRoFirmwaretatus* out) {
  switch (step) {
    case MojomUpdateRoFirmwareStatus::kWaitUsb:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_WAIT_USB;
      return true;
    case MojomUpdateRoFirmwareStatus::kFileNotFound:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_FILE_NOT_FOUND;
      return true;
    case MojomUpdateRoFirmwareStatus::kDownloading:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING;
      return true;
    case MojomUpdateRoFirmwareStatus::kUpdating:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING;
      return true;
    case MojomUpdateRoFirmwareStatus::kRebooting:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING;
      return true;
    case MojomUpdateRoFirmwareStatus::kComplete:
      *out = ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_COMPLETE;
      return true;
    case MojomUpdateRoFirmwareStatus::kUnknown:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}
}  // namespace mojo
