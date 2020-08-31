// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/unlock_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/metrics.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_monitor_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace proximity_auth {
namespace {

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class FindAndConnectToHostResult {
  kFoundAndConnectedToHost = 0,
  kCanceledBluetoothDisabled = 1,
  kCanceledUserEnteredPassword = 2,
  kSecureChannelConnectionAttemptFailure = 3,
  kTimedOut = 4,
  kMaxValue = kTimedOut
};

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other). Entries should be never modified
// or deleted. Only additions possible.
enum class GetRemoteStatusResultFailureReason {
  kCanceledBluetoothDisabled = 0,
  kDeprecatedTimedOutCouldNotEstablishAuthenticatedChannel = 1,
  kTimedOutDidNotReceiveRemoteStatusUpdate = 2,
  kDeprecatedUserEnteredPasswordWhileBluetoothDisabled = 3,
  kCanceledUserEnteredPassword = 4,
  kAuthenticatedChannelDropped = 5,
  kMaxValue = kAuthenticatedChannelDropped
};

// The maximum amount of time that the unlock manager can stay in the 'waking
// up' state after resuming from sleep.
constexpr base::TimeDelta kWakingUpDuration = base::TimeDelta::FromSeconds(15);

// The maximum amount of time that we wait for the BluetoothAdapter to be
// fully initialized after resuming from sleep.
// TODO(crbug.com/986896): This is necessary because the BluetoothAdapter
// returns incorrect presence and power values directly after resume, and does
// not return correct values until about 1-2 seconds later. Remove this once
// the bug is fixed.
constexpr base::TimeDelta kBluetoothAdapterResumeMaxDuration =
    base::TimeDelta::FromSeconds(3);

// The limit on the elapsed time for an auth attempt. If an auth attempt exceeds
// this limit, it will time out and be rejected. This is provided as a failsafe,
// in case something goes wrong.
constexpr base::TimeDelta kAuthAttemptTimeout = base::TimeDelta::FromSeconds(5);

constexpr base::TimeDelta kMinExtendedDuration =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kMaxExtendedDuration =
    base::TimeDelta::FromSeconds(15);
const int kNumDurationMetricBuckets = 100;

// Returns the remote device's security settings state, for metrics,
// corresponding to a remote status update.
metrics::RemoteSecuritySettingsState GetRemoteSecuritySettingsState(
    const RemoteStatusUpdate& status_update) {
  switch (status_update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return metrics::RemoteSecuritySettingsState::UNKNOWN;

    case SECURE_SCREEN_LOCK_DISABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_DISABLED_TRUST_AGENT_ENABLED;
      }

    case SECURE_SCREEN_LOCK_ENABLED:
      switch (status_update.trust_agent_state) {
        case TRUST_AGENT_UNSUPPORTED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_UNSUPPORTED;
        case TRUST_AGENT_DISABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_DISABLED;
        case TRUST_AGENT_ENABLED:
          return metrics::RemoteSecuritySettingsState::
              SCREEN_LOCK_ENABLED_TRUST_AGENT_ENABLED;
      }
  }

  NOTREACHED();
  return metrics::RemoteSecuritySettingsState::UNKNOWN;
}

std::string GetHistogramStatusSuffix(bool unlockable) {
  return unlockable ? "Unlockable" : "Other";
}

std::string GetHistogramScreenLockTypeName(
    ProximityAuthSystem::ScreenlockType screenlock_type) {
  return screenlock_type == ProximityAuthSystem::SESSION_LOCK ? "Unlock"
                                                              : "SignIn";
}

void RecordFindAndConnectToHostResult(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    FindAndConnectToHostResult result) {
  base::UmaHistogramEnumeration(
      "SmartLock.FindAndConnectToHostResult." +
          GetHistogramScreenLockTypeName(screenlock_type),
      result);
}

void RecordGetRemoteStatusResultSuccess(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    bool success = true) {
  base::UmaHistogramBoolean("SmartLock.GetRemoteStatus." +
                                GetHistogramScreenLockTypeName(screenlock_type),
                            success);
}

void RecordGetRemoteStatusResultFailure(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    GetRemoteStatusResultFailureReason failure_reason) {
  RecordGetRemoteStatusResultSuccess(screenlock_type, false /* success */);
  base::UmaHistogramEnumeration(
      "SmartLock.GetRemoteStatus." +
          GetHistogramScreenLockTypeName(screenlock_type) + ".Failure",
      failure_reason);
}

void RecordAuthResultFailure(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    SmartLockMetricsRecorder::SmartLockAuthResultFailureReason failure_reason) {
  if (screenlock_type == ProximityAuthSystem::SESSION_LOCK) {
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(failure_reason);
  } else if (screenlock_type == ProximityAuthSystem::SIGN_IN) {
    SmartLockMetricsRecorder::RecordAuthResultSignInFailure(failure_reason);
  }
}

void RecordExtendedDurationTimerMetric(const std::string& histogram_name,
                                       base::TimeDelta duration) {
  // Use a custom |max| to account for Smart Lock's timeout (larger than the
  // default 10 seconds).
  base::UmaHistogramCustomTimes(
      histogram_name, duration, kMinExtendedDuration /* min */,
      kMaxExtendedDuration /* max */, kNumDurationMetricBuckets /* buckets */);
}

}  // namespace

UnlockManagerImpl::UnlockManagerImpl(
    ProximityAuthSystem::ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client)
    : screenlock_type_(screenlock_type),
      proximity_auth_client_(proximity_auth_client),
      bluetooth_suspension_recovery_timer_(
          std::make_unique<base::OneShotTimer>()) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindOnce(&UnlockManagerImpl::OnBluetoothAdapterInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

UnlockManagerImpl::~UnlockManagerImpl() {
  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
  if (proximity_monitor_)
    proximity_monitor_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

bool UnlockManagerImpl::IsUnlockAllowed() {
  return (remote_screenlock_state_ &&
          *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED &&
          is_bluetooth_connection_to_phone_active_ && proximity_monitor_ &&
          proximity_monitor_->IsUnlockAllowed() &&
          (screenlock_type_ != ProximityAuthSystem::SIGN_IN || GetMessenger()));
}

void UnlockManagerImpl::SetRemoteDeviceLifeCycle(
    RemoteDeviceLifeCycle* life_cycle) {
  PA_LOG(VERBOSE) << "Request received to change scan state to: "
                  << (life_cycle == nullptr ? "inactive" : "active") << ".";

  if (life_cycle_)
    life_cycle_->RemoveObserver(this);
  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);

  life_cycle_ = life_cycle;
  if (life_cycle_) {
    life_cycle_->AddObserver(this);

    is_bluetooth_connection_to_phone_active_ = false;
    show_lock_screen_time_ = base::DefaultClock::GetInstance()->Now();
    has_user_been_shown_first_status_ = false;

    if (IsBluetoothPresentAndPowered()) {
      SetIsPerformingInitialScan(true /* is_performing_initial_scan */);
      AttemptToStartRemoteDeviceLifecycle();
    } else {
      RecordFindAndConnectToHostResult(
          screenlock_type_,
          FindAndConnectToHostResult::kCanceledBluetoothDisabled);
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  } else {
    ResetPerformanceMetricsTimestamps();

    if (proximity_monitor_)
      proximity_monitor_->RemoveObserver(this);
    proximity_monitor_.reset();

    UpdateLockScreen();
  }
}

void UnlockManagerImpl::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  remote_screenlock_state_.reset();
  if (new_state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    DCHECK(life_cycle_->GetChannel());
    DCHECK(GetMessenger());
    if (!proximity_monitor_) {
      proximity_monitor_ = CreateProximityMonitor(life_cycle_);
      proximity_monitor_->AddObserver(this);
      proximity_monitor_->Start();
    }
    GetMessenger()->AddObserver(this);

    is_bluetooth_connection_to_phone_active_ = true;
    attempt_get_remote_status_start_time_ =
        base::DefaultClock::GetInstance()->Now();

    PA_LOG(VERBOSE) << "Successfully connected to host; waiting for remote "
                       "status update.";

    if (is_performing_initial_scan_) {
      RecordFindAndConnectToHostResult(
          screenlock_type_,
          FindAndConnectToHostResult::kFoundAndConnectedToHost);
    }
  } else {
    is_bluetooth_connection_to_phone_active_ = false;

    if (proximity_monitor_) {
      proximity_monitor_->RemoveObserver(this);
      proximity_monitor_->Stop();
      proximity_monitor_.reset();
    }
  }

  // Note: though the name is AUTHENTICATION_FAILED, this state actually
  // encompasses any connection failure in
  // |secure_channel::mojom::ConnectionAttemptFailureReason| beside Bluetooth
  // becoming disabled. See https://crbug.com/991644 for more.
  if (new_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED) {
    PA_LOG(ERROR) << "Connection attempt to host failed.";

    if (is_performing_initial_scan_) {
      RecordFindAndConnectToHostResult(
          screenlock_type_,
          FindAndConnectToHostResult::kSecureChannelConnectionAttemptFailure);
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  }

  if (new_state == RemoteDeviceLifeCycle::State::FINDING_CONNECTION &&
      old_state == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    PA_LOG(ERROR) << "Secure channel dropped for unknown reason; potentially "
                     "due to Bluetooth being disabled.";

    if (is_performing_initial_scan_) {
      OnDisconnected();
      SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    }
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnUnlockEventSent(bool success) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Sent easy_unlock event, but no auth attempted.";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockEventSentButNotAttemptingAuth);
  } else if (success) {
    FinalizeAuthAttempt(base::nullopt /* failure_reason */);
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedtoNotifyHostDeviceThatSmartLockWasUsed);
  }
}

void UnlockManagerImpl::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(VERBOSE) << "Status Update: ("
                  << "user_present=" << status_update.user_presence << ", "
                  << "secure_screen_lock="
                  << status_update.secure_screen_lock_state << ", "
                  << "trust_agent=" << status_update.trust_agent_state << ")";
  metrics::RecordRemoteSecuritySettingsState(
      GetRemoteSecuritySettingsState(status_update));

  remote_screenlock_state_.reset(new RemoteScreenlockState(
      GetScreenlockStateFromRemoteUpdate(status_update)));

  // Only record these metrics within the initial period of opening the laptop
  // displaying the lock screen.
  if (is_performing_initial_scan_) {
    RecordFirstRemoteStatusReceived(
        *remote_screenlock_state_ ==
        RemoteScreenlockState::UNLOCKED /* unlockable */);
  }

  // This also calls |UpdateLockScreen()|
  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::OnDecryptResponse(const std::string& decrypted_bytes) {
  if (!is_attempting_auth_) {
    PA_LOG(ERROR) << "Decrypt response received but not attempting auth.";
    return;
  }

  if (decrypted_bytes.empty()) {
    PA_LOG(WARNING) << "Failed to decrypt sign-in challenge.";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedToDecryptSignInChallenge);
  } else {
    sign_in_secret_.reset(new std::string(decrypted_bytes));
    if (GetMessenger())
      GetMessenger()->DispatchUnlockEvent();
  }
}

void UnlockManagerImpl::OnUnlockResponse(bool success) {
  if (!is_attempting_auth_) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockRequestSentButNotAttemptingAuth);
    PA_LOG(ERROR) << "Unlock request sent but not attempting auth.";
    return;
  }

  PA_LOG(INFO) << "Received unlock response from device: "
               << (success ? "yes" : "no") << ".";

  if (success && GetMessenger()) {
    GetMessenger()->DispatchUnlockEvent();
  } else {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kFailedToSendUnlockRequest);
  }
}

void UnlockManagerImpl::OnDisconnected() {
  if (is_attempting_auth_) {
    RecordAuthResultFailure(
        screenlock_type_,
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kAuthenticatedChannelDropped);
  } else if (is_performing_initial_scan_) {
    RecordGetRemoteStatusResultFailure(
        screenlock_type_,
        GetRemoteStatusResultFailureReason::kAuthenticatedChannelDropped);
  }

  if (GetMessenger())
    GetMessenger()->RemoveObserver(this);
}

void UnlockManagerImpl::OnProximityStateChanged() {
  PA_LOG(VERBOSE) << "Proximity state changed.";
  UpdateLockScreen();
}

void UnlockManagerImpl::OnBluetoothAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void UnlockManagerImpl::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                              bool present) {
  if (!IsBluetoothAdapterRecoveringFromSuspend())
    OnBluetoothAdapterPresentAndPoweredChanged();
}

void UnlockManagerImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                              bool powered) {
  if (!IsBluetoothAdapterRecoveringFromSuspend())
    OnBluetoothAdapterPresentAndPoweredChanged();
}

void UnlockManagerImpl::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // TODO(crbug.com/986896): For a short time window after resuming from
  // suspension, BluetoothAdapter returns incorrect presence and power values.
  // Cache the correct values now, in case we need to check those values during
  // that time window when the device resumes.
  was_bluetooth_present_and_powered_before_last_suspend_ =
      IsBluetoothPresentAndPowered();
  bluetooth_suspension_recovery_timer_->Stop();
}

void UnlockManagerImpl::SuspendDone(const base::TimeDelta& sleep_duration) {
  bluetooth_suspension_recovery_timer_->Start(
      FROM_HERE, kBluetoothAdapterResumeMaxDuration,
      base::BindOnce(
          &UnlockManagerImpl::OnBluetoothAdapterPresentAndPoweredChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // The next scan after resuming is expected to be triggered by calling
  // SetRemoteDeviceLifeCycle().
}

bool UnlockManagerImpl::IsBluetoothPresentAndPowered() const {
  // TODO(crbug.com/986896): If the BluetoothAdapter is still "resuming after
  // suspension" at this time, it's prone to this bug, meaning we cannot trust
  // its returned presence and power values. If this is the case, depend on
  // the cached |was_bluetooth_present_and_powered_before_last_suspend_| to
  // signal if Bluetooth is enabled; otherwise, directly check request values
  // from BluetoothAdapter. Remove this check once the bug is fixed.
  if (IsBluetoothAdapterRecoveringFromSuspend())
    return was_bluetooth_present_and_powered_before_last_suspend_;

  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent() &&
         bluetooth_adapter_->IsPowered();
}

void UnlockManagerImpl::OnBluetoothAdapterPresentAndPoweredChanged() {
  DCHECK(!IsBluetoothAdapterRecoveringFromSuspend());

  if (IsBluetoothPresentAndPowered()) {
    if (!is_performing_initial_scan_)
      SetIsPerformingInitialScan(true /* is_performing_initial_scan */);

    return;
  }

  if (is_performing_initial_scan_) {
    if (is_bluetooth_connection_to_phone_active_ &&
        !has_received_first_remote_status_) {
      RecordGetRemoteStatusResultFailure(
          screenlock_type_,
          GetRemoteStatusResultFailureReason::kCanceledBluetoothDisabled);
    } else {
      RecordFindAndConnectToHostResult(
          screenlock_type_,
          FindAndConnectToHostResult::kCanceledBluetoothDisabled);
    }

    SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
    return;
  }

  // If Bluetooth is off but no initial scan is active, still ensure that the
  // lock screen UI reflects that Bluetooth is off.
  UpdateLockScreen();
}

bool UnlockManagerImpl::IsBluetoothAdapterRecoveringFromSuspend() const {
  return bluetooth_suspension_recovery_timer_->IsRunning();
}

void UnlockManagerImpl::AttemptToStartRemoteDeviceLifecycle() {
  if (IsBluetoothPresentAndPowered() && life_cycle_ &&
      life_cycle_->GetState() == RemoteDeviceLifeCycle::State::STOPPED) {
    // If Bluetooth is disabled after this, |life_cycle_| will be notified by
    // SecureChannel that the connection attempt failed. From that point on,
    // |life_cycle_| will wait to be started again by UnlockManager.
    life_cycle_->Start();
  }
}

void UnlockManagerImpl::OnAuthAttempted(mojom::AuthType auth_type) {
  if (is_attempting_auth_) {
    PA_LOG(VERBOSE) << "Already attempting auth.";
    return;
  }

  if (auth_type != mojom::AuthType::USER_CLICK)
    return;

  is_attempting_auth_ = true;

  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "No life_cycle active when auth was attempted";
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kNoPendingOrActiveHost);
    UpdateLockScreen();
    return;
  }

  if (!IsUnlockAllowed()) {
    FinalizeAuthAttempt(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kUnlockNotAllowed);
    UpdateLockScreen();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &UnlockManagerImpl::FinalizeAuthAttempt,
          reject_auth_attempt_weak_ptr_factory_.GetWeakPtr(),
          SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
              kAuthAttemptTimedOut),
      kAuthAttemptTimeout);

  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    SendSignInChallenge();
  } else {
    GetMessenger()->RequestUnlock();
  }
}

void UnlockManagerImpl::CancelConnectionAttempt() {
  PA_LOG(VERBOSE) << "User entered password.";

  bluetooth_suspension_recovery_timer_->Stop();

  // Note: There is no need to record metrics here if Bluetooth isn't present
  // and powered; that has already been handled at this point in
  // OnBluetoothAdapterPresentAndPoweredChanged().
  if (!IsBluetoothPresentAndPowered())
    return;

  if (is_performing_initial_scan_) {
    if (is_bluetooth_connection_to_phone_active_ &&
        !has_received_first_remote_status_) {
      RecordGetRemoteStatusResultFailure(
          screenlock_type_,
          GetRemoteStatusResultFailureReason::kCanceledUserEnteredPassword);
    } else {
      RecordFindAndConnectToHostResult(
          screenlock_type_,
          FindAndConnectToHostResult::kCanceledUserEnteredPassword);
    }

    SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
  }
}

std::unique_ptr<ProximityMonitor> UnlockManagerImpl::CreateProximityMonitor(
    RemoteDeviceLifeCycle* life_cycle) {
  return std::make_unique<ProximityMonitorImpl>(life_cycle->GetRemoteDevice(),
                                                life_cycle->GetChannel());
}

void UnlockManagerImpl::SendSignInChallenge() {
  if (!life_cycle_ || !GetMessenger()) {
    PA_LOG(ERROR) << "Not ready to send sign-in challenge";
    return;
  }

  if (!GetMessenger()->GetChannel()) {
    PA_LOG(ERROR) << "Channel is not ready to send sign-in challenge.";
    return;
  }

  GetMessenger()->GetChannel()->GetConnectionMetadata(
      base::BindOnce(&UnlockManagerImpl::OnGetConnectionMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnlockManagerImpl::OnGetConnectionMetadata(
    chromeos::secure_channel::mojom::ConnectionMetadataPtr
        connection_metadata_ptr) {
  chromeos::multidevice::RemoteDeviceRef remote_device =
      life_cycle_->GetRemoteDevice();
  proximity_auth_client_->GetChallengeForUserAndDevice(
      remote_device.user_email(), remote_device.public_key(),
      connection_metadata_ptr->channel_binding_data,
      base::Bind(&UnlockManagerImpl::OnGotSignInChallenge,
                 weak_ptr_factory_.GetWeakPtr()));
}

void UnlockManagerImpl::OnGotSignInChallenge(const std::string& challenge) {
  PA_LOG(VERBOSE) << "Got sign-in challenge, sending for decryption...";
  if (GetMessenger())
    GetMessenger()->RequestDecryption(challenge);
}

ScreenlockState UnlockManagerImpl::GetScreenlockState() {
  if (!life_cycle_)
    return ScreenlockState::INACTIVE;

  if (!IsBluetoothPresentAndPowered())
    return ScreenlockState::NO_BLUETOOTH;

  if (IsUnlockAllowed())
    return ScreenlockState::AUTHENTICATED;

  RemoteDeviceLifeCycle::State life_cycle_state = life_cycle_->GetState();
  if (life_cycle_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED)
    return ScreenlockState::PHONE_NOT_AUTHENTICATED;

  if (is_performing_initial_scan_)
    return ScreenlockState::BLUETOOTH_CONNECTING;

  Messenger* messenger = GetMessenger();

  // Show a timeout state if we can not connect to the remote device in a
  // reasonable amount of time.
  if (!is_performing_initial_scan_ && !messenger)
    return ScreenlockState::NO_PHONE;

  // If the RSSI is too low, then the remote device is nowhere near the local
  // device. This message should take priority over messages about screen lock
  // states.
  if (proximity_monitor_ && !proximity_monitor_->IsUnlockAllowed()) {
    if (remote_screenlock_state_ &&
        *remote_screenlock_state_ == RemoteScreenlockState::UNLOCKED) {
      return ScreenlockState::RSSI_TOO_LOW;
    } else {
      return ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW;
    }
  }

  if (remote_screenlock_state_) {
    switch (*remote_screenlock_state_) {
      case RemoteScreenlockState::DISABLED:
        return ScreenlockState::PHONE_NOT_LOCKABLE;

      case RemoteScreenlockState::LOCKED:
        return ScreenlockState::PHONE_LOCKED;

      case RemoteScreenlockState::PRIMARY_USER_ABSENT:
        return ScreenlockState::PRIMARY_USER_ABSENT;

      case RemoteScreenlockState::UNKNOWN:
      case RemoteScreenlockState::UNLOCKED:
        // Handled by the code below.
        break;
    }
  }

  if (messenger) {
    PA_LOG(WARNING) << "Connection to host established, but remote screenlock "
                    << "state was either malformed or not received.";
  }

  return ScreenlockState::NO_PHONE;
}

void UnlockManagerImpl::UpdateLockScreen() {
  AttemptToStartRemoteDeviceLifecycle();

  ScreenlockState new_state = GetScreenlockState();
  if (screenlock_state_ == new_state)
    return;

  PA_LOG(INFO) << "Updating screenlock state from " << screenlock_state_
               << " to " << new_state;

  if (new_state != ScreenlockState::INACTIVE &&
      new_state != ScreenlockState::BLUETOOTH_CONNECTING) {
    RecordFirstStatusShownToUser(
        new_state == ScreenlockState::AUTHENTICATED /* unlockable */);
  }

  proximity_auth_client_->UpdateScreenlockState(new_state);
  screenlock_state_ = new_state;
}

void UnlockManagerImpl::SetIsPerformingInitialScan(
    bool is_performing_initial_scan) {
  PA_LOG(VERBOSE) << "Initial scan state is ["
                  << (is_performing_initial_scan_ ? "active" : "inactive")
                  << "]. Requesting state ["
                  << (is_performing_initial_scan ? "active" : "inactive")
                  << "].";

  is_performing_initial_scan_ = is_performing_initial_scan;

  // Clear the waking up state after a timeout.
  initial_scan_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
  if (is_performing_initial_scan_) {
    initial_scan_start_time_ = base::DefaultClock::GetInstance()->Now();
    has_received_first_remote_status_ = false;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UnlockManagerImpl::OnInitialScanTimeout,
                       initial_scan_timeout_weak_ptr_factory_.GetWeakPtr()),
        kWakingUpDuration);
  }

  UpdateLockScreen();
}

void UnlockManagerImpl::OnInitialScanTimeout() {
  // Note: There is no need to record metrics here if Bluetooth isn't present
  // and powered; that has already been handled at this point in
  // OnBluetoothAdapterPresentAndPoweredChanged().
  if (!IsBluetoothPresentAndPowered())
    return;

  if (is_bluetooth_connection_to_phone_active_) {
    PA_LOG(ERROR) << "Successfully connected to host, but it did not provide "
                     "remote status update.";
    RecordGetRemoteStatusResultFailure(
        screenlock_type_, GetRemoteStatusResultFailureReason::
                              kTimedOutDidNotReceiveRemoteStatusUpdate);
  } else {
    PA_LOG(INFO) << "Initial scan for host returned no result.";
    RecordFindAndConnectToHostResult(screenlock_type_,
                                     FindAndConnectToHostResult::kTimedOut);
  }

  SetIsPerformingInitialScan(false /* is_performing_initial_scan */);
}

void UnlockManagerImpl::FinalizeAuthAttempt(
    const base::Optional<
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>& error) {
  if (error) {
    RecordAuthResultFailure(screenlock_type_, *error);
  }

  if (!is_attempting_auth_)
    return;

  // Cancel the pending task to time out the auth attempt.
  reject_auth_attempt_weak_ptr_factory_.InvalidateWeakPtrs();

  bool should_accept = !error;
  if (should_accept && proximity_monitor_)
    proximity_monitor_->RecordProximityMetricsOnAuthSuccess();

  is_attempting_auth_ = false;
  if (screenlock_type_ == ProximityAuthSystem::SIGN_IN) {
    PA_LOG(VERBOSE) << "Finalizing sign-in...";
    proximity_auth_client_->FinalizeSignin(
        should_accept && sign_in_secret_ ? *sign_in_secret_ : std::string());
  } else {
    PA_LOG(VERBOSE) << "Finalizing unlock...";
    proximity_auth_client_->FinalizeUnlock(should_accept);
  }
}

UnlockManagerImpl::RemoteScreenlockState
UnlockManagerImpl::GetScreenlockStateFromRemoteUpdate(
    RemoteStatusUpdate update) {
  switch (update.secure_screen_lock_state) {
    case SECURE_SCREEN_LOCK_DISABLED:
      return RemoteScreenlockState::DISABLED;

    case SECURE_SCREEN_LOCK_ENABLED:
      if (update.user_presence == USER_PRESENCE_SECONDARY ||
          update.user_presence == USER_PRESENCE_BACKGROUND) {
        return RemoteScreenlockState::PRIMARY_USER_ABSENT;
      }

      if (update.user_presence == USER_PRESENT)
        return RemoteScreenlockState::UNLOCKED;

      return RemoteScreenlockState::LOCKED;

    case SECURE_SCREEN_LOCK_STATE_UNKNOWN:
      return RemoteScreenlockState::UNKNOWN;
  }

  NOTREACHED();
  return RemoteScreenlockState::UNKNOWN;
}

Messenger* UnlockManagerImpl::GetMessenger() {
  // TODO(tengs): We should use a weak pointer to hold the Messenger instance
  // instead.
  if (!life_cycle_)
    return nullptr;
  return life_cycle_->GetMessenger();
}

void UnlockManagerImpl::RecordFirstRemoteStatusReceived(bool unlockable) {
  if (has_received_first_remote_status_)
    return;
  has_received_first_remote_status_ = true;

  RecordGetRemoteStatusResultSuccess(screenlock_type_);

  if (initial_scan_start_time_.is_null() ||
      attempt_get_remote_status_start_time_.is_null()) {
    PA_LOG(WARNING) << "Attempted to RecordFirstRemoteStatusReceived() "
                       "without initial timestamps recorded.";
    NOTREACHED();
    return;
  }

  const std::string histogram_status_suffix =
      GetHistogramStatusSuffix(unlockable);

  base::Time now = base::DefaultClock::GetInstance()->Now();
  base::TimeDelta start_scan_to_receive_first_remote_status_duration =
      now - initial_scan_start_time_;
  base::TimeDelta authentication_to_receive_first_remote_status_duration =
      now - attempt_get_remote_status_start_time_;

  if (screenlock_type_ == ProximityAuthSystem::SESSION_LOCK) {
    RecordExtendedDurationTimerMetric(
        "SmartLock.Performance.StartScanToReceiveFirstRemoteStatusDuration."
        "Unlock",
        start_scan_to_receive_first_remote_status_duration);
    RecordExtendedDurationTimerMetric(
        "SmartLock.Performance.StartScanToReceiveFirstRemoteStatusDuration."
        "Unlock." +
            histogram_status_suffix,
        start_scan_to_receive_first_remote_status_duration);

    // This should be much less than 10 seconds, so use UmaHistogramTimes.
    base::UmaHistogramTimes(
        "SmartLock.Performance."
        "AuthenticationToReceiveFirstRemoteStatusDuration.Unlock",
        authentication_to_receive_first_remote_status_duration);
    base::UmaHistogramTimes(
        "SmartLock.Performance."
        "AuthenticationToReceiveFirstRemoteStatusDuration.Unlock." +
            histogram_status_suffix,
        authentication_to_receive_first_remote_status_duration);
  }

  // TODO(crbug.com/905438): Implement similar SignIn metrics.
}

void UnlockManagerImpl::RecordFirstStatusShownToUser(bool unlockable) {
  if (has_user_been_shown_first_status_)
    return;
  has_user_been_shown_first_status_ = true;

  if (show_lock_screen_time_.is_null()) {
    PA_LOG(WARNING) << "Attempted to RecordFirstStatusShownToUser() "
                       "without initial timestamp recorded.";
    NOTREACHED();
    return;
  }

  const std::string histogram_status_suffix =
      GetHistogramStatusSuffix(unlockable);

  base::Time now = base::DefaultClock::GetInstance()->Now();
  base::TimeDelta show_lock_screen_to_show_first_status_to_user_duration =
      now - show_lock_screen_time_;

  if (screenlock_type_ == ProximityAuthSystem::SESSION_LOCK) {
    RecordExtendedDurationTimerMetric(
        "SmartLock.Performance.ShowLockScreenToShowFirstStatusToUserDuration."
        "Unlock",
        show_lock_screen_to_show_first_status_to_user_duration);
    RecordExtendedDurationTimerMetric(
        "SmartLock.Performance.ShowLockScreenToShowFirstStatusToUserDuration."
        "Unlock." +
            histogram_status_suffix,
        show_lock_screen_to_show_first_status_to_user_duration);
  }

  // TODO(crbug.com/905438): Implement similar SignIn metrics.
}

void UnlockManagerImpl::ResetPerformanceMetricsTimestamps() {
  show_lock_screen_time_ = base::Time();
  initial_scan_start_time_ = base::Time();
  attempt_get_remote_status_start_time_ = base::Time();
}

void UnlockManagerImpl::SetBluetoothSuspensionRecoveryTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  bluetooth_suspension_recovery_timer_ = std::move(timer);
}

}  // namespace proximity_auth
