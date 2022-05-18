// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/fake_cros_healthd.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/ash/components/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/ash/components/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace ash::cros_healthd {

namespace {

// TODO(https://crbug.com/1164001): remove after migration to namespace ash.
namespace mojom = ::chromeos::cros_healthd::mojom;

// Will destroy `handle` if it's not a valid platform handle.
mojo::ScopedHandle CloneScopedHandle(mojo::ScopedHandle* handle) {
  DCHECK(handle);
  if (!handle->is_valid()) {
    return mojo::ScopedHandle();
  }
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(*handle));
  DCHECK(platform_handle.is_valid());
  *handle = mojo::WrapPlatformHandle(platform_handle.Clone());
  return mojo::WrapPlatformHandle(std::move(platform_handle));
}

// Used to track the fake instance, mirrors the instance in the base class.
FakeCrosHealthd* g_instance = nullptr;

}  // namespace

FakeCrosHealthd::FakeCrosHealthd() = default;

FakeCrosHealthd::~FakeCrosHealthd() = default;

// static
void FakeCrosHealthd::Initialize() {
  CHECK(!g_instance);
  g_instance = new FakeCrosHealthd();

  if (!FakeCrosHealthdClient::Get()) {
    CHECK(!CrosHealthdClient::Get())
        << "A real dbus client has already been initialized. Cannot initialize "
           "FakeCrosHealthd.";
    CrosHealthdClient::InitializeFake();
  }
  // FakeCrosHealthd will shutdown the fake dbus client when shutdowning so it
  // is safe to use `Unretained` here.
  FakeCrosHealthdClient::Get()->SetBootstrapCallback(base::BindRepeating(
      &FakeCrosHealthd::BindNewRemote, base::Unretained(g_instance)));
}

// static
void FakeCrosHealthd::Shutdown() {
  // Make sure that the ServiceConnection is created, so it always use fake
  // to bootstrap. Without this, ServiceConnection could be initialized
  // after FakeCrosHealthd is shutdowned in unit tests and causes weird
  // behavior.
  ServiceConnection::GetInstance();

  CHECK(FakeCrosHealthdClient::Get())
      << "The fake dbus client has been shutdowned by others. Cannot shutdown "
         "the FakeCrosHealthd";
  CrosHealthdClient::Shutdown();

  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;

  // After all the receivers in this class are destructed, flush all the mojo
  // remote in ServiceConnection so it will be disconnected and reset. Without
  // this, the mojo object remain in a unstable state and cause errors.
  ServiceConnection::GetInstance()->FlushForTesting();
}

// static
FakeCrosHealthd* FakeCrosHealthd::Get() {
  return g_instance;
}

void FakeCrosHealthd::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  available_routines_ = available_routines;
}

void FakeCrosHealthd::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  run_routine_response_.Swap(&response);
}

void FakeCrosHealthd::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  routine_update_response_.Swap(&response);
}

void FakeCrosHealthd::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& response_info) {
  telemetry_response_info_.Swap(&response_info);
}

void FakeCrosHealthd::SetProbeProcessInfoResponseForTesting(
    mojom::ProcessResultPtr& result) {
  process_response_.Swap(&result);
}

void FakeCrosHealthd::SetCallbackDelay(base::TimeDelta delay) {
  callback_delay_ = delay;
}

void FakeCrosHealthd::EmitAcInsertedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : power_observers_)
    observer->OnAcInserted();
}

void FakeCrosHealthd::EmitAcRemovedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : power_observers_)
    observer->OnAcRemoved();
}

void FakeCrosHealthd::EmitOsSuspendEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : power_observers_)
    observer->OnOsSuspend();
}

void FakeCrosHealthd::EmitOsResumeEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : power_observers_)
    observer->OnOsResume();
}

void FakeCrosHealthd::EmitAdapterAddedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterAdded();
}

void FakeCrosHealthd::EmitAdapterRemovedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterRemoved();
}

void FakeCrosHealthd::EmitAdapterPropertyChangedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterPropertyChanged();
}

void FakeCrosHealthd::EmitDeviceAddedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceAdded();
}

void FakeCrosHealthd::EmitDeviceRemovedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnDeviceRemoved();
}

void FakeCrosHealthd::EmitDevicePropertyChangedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : bluetooth_observers_)
    observer->OnDevicePropertyChanged();
}

void FakeCrosHealthd::EmitLidClosedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : lid_observers_)
    observer->OnLidClosed();
}

void FakeCrosHealthd::EmitLidOpenedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : lid_observers_)
    observer->OnLidOpened();
}

void FakeCrosHealthd::EmitAudioUnderrunEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : audio_observers_)
    observer->OnUnderrun();
}

void FakeCrosHealthd::EmitAudioSevereUnderrunEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : audio_observers_)
    observer->OnSevereUnderrun();
}

void FakeCrosHealthd::EmitThunderboltAddEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : thunderbolt_observers_)
    observer->OnAdd();
}

void FakeCrosHealthd::EmitUsbAddEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  mojom::UsbEventInfo info;
  for (auto& observer : usb_observers_)
    observer->OnAdd(info.Clone());
}

void FakeCrosHealthd::EmitConnectionStateChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::NetworkState state) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : network_observers_) {
    observer->OnConnectionStateChanged(network_guid, state);
  }
}

void FakeCrosHealthd::EmitSignalStrengthChangedEventForTesting(
    const std::string& network_guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  healthd_receiver_.FlushForTesting();
  event_receiver_set_.FlushForTesting();

  for (auto& observer : network_observers_) {
    observer->OnSignalStrengthChanged(
        network_guid, chromeos::network_health::mojom::UInt32Value::New(
                          signal_strength->value));
  }
}

void FakeCrosHealthd::RequestNetworkHealthForTesting(
    chromeos::network_health::mojom::NetworkHealthService::
        GetHealthSnapshotCallback callback) {
  // Flush the receiver, so pending network interface are registered before it
  // is used.
  healthd_receiver_.FlushForTesting();

  network_health_remote_->GetHealthSnapshot(std::move(callback));
}

void FakeCrosHealthd::RunLanConnectivityRoutineForTesting(
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
        RunLanConnectivityCallback callback) {
  // Flush the receiver, so pending network interface are registered before it
  // is used.
  healthd_receiver_.FlushForTesting();

  network_diagnostics_routines_->RunLanConnectivity(std::move(callback));
}

absl::optional<mojom::DiagnosticRoutineEnum>
FakeCrosHealthd::GetLastRunRoutine() const {
  return last_run_routine_;
}

absl::optional<FakeCrosHealthd::RoutineUpdateParams>
FakeCrosHealthd::GetRoutineUpdateParams() const {
  return routine_update_params_;
}

mojo::Remote<mojom::CrosHealthdServiceFactory>
FakeCrosHealthd::BindNewRemote() {
  healthd_receiver_.reset();
  return mojo::Remote<mojom::CrosHealthdServiceFactory>(
      healthd_receiver_.BindNewPipeAndPassRemote());
}

FakeCrosHealthd::RoutineUpdateParams::RoutineUpdateParams(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output)
    : id(id), command(command), include_output(include_output) {}

void FakeCrosHealthd::GetProbeService(
    mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) {
  probe_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthd::GetDiagnosticsService(
    mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) {
  diagnostics_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthd::GetEventService(
    mojo::PendingReceiver<mojom::CrosHealthdEventService> service) {
  event_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthd::SendNetworkHealthService(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
        remote) {
  network_health_remote_.Bind(std::move(remote));
}

void FakeCrosHealthd::SendNetworkDiagnosticsRoutines(
    mojo::PendingRemote<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        network_diagnostics_routines) {
  network_diagnostics_routines_.Bind(std::move(network_diagnostics_routines));
}

void FakeCrosHealthd::GetSystemService(
    mojo::PendingReceiver<mojom::CrosHealthdSystemService> service) {
  system_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthd::GetServiceStatus(GetServiceStatusCallback callback) {
  auto response = mojom::ServiceStatus::New();
  response->network_health_bound = network_health_remote_.is_bound();
  response->network_diagnostics_bound = network_health_remote_.is_bound();
  std::move(callback).Run(std::move(response));
}

void FakeCrosHealthd::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), available_routines_),
      callback_delay_);
}

void FakeCrosHealthd::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  routine_update_params_ =
      FakeCrosHealthd::RoutineUpdateParams(id, command, include_output);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          mojom::RoutineUpdate::New(
              routine_update_response_->progress_percent,
              CloneScopedHandle(&routine_update_response_->output),
              routine_update_response_->routine_update_union.Clone())),
      callback_delay_);
}

void FakeCrosHealthd::RunUrandomRoutine(mojom::NullableUint32Ptr length_seconds,
                                        RunUrandomRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kUrandom;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCapacity;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryHealth;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSmartctlCheck;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kAcPower;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunCpuCacheRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuCacheRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuCache;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunCpuStressRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunCpuStressRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCpuStress;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunFloatingPointAccuracyRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kNvmeWearLevel;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kNvmeSelfTest;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunDiskReadRoutine(mojom::DiskReadRoutineTypeEnum type,
                                         uint32_t length_seconds,
                                         uint32_t file_size_mb,
                                         RunDiskReadRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDiskRead;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunPrimeSearchRoutine(
    mojom::NullableUint32Ptr length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kPrimeSearch;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryDischarge;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kBatteryCharge;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunMemoryRoutine(RunMemoryRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kMemory;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kLanConnectivity;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kSignalStrength;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kGatewayCanBePinged;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHasSecureWiFiConnectionRoutine(
    RunHasSecureWiFiConnectionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHasSecureWiFiConnection;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolverPresent;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsLatencyRoutine(
    RunDnsLatencyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunCaptivePortalRoutine(
    RunCaptivePortalRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kCaptivePortal;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpFirewallRoutine(
    RunHttpFirewallRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpsFirewallRoutine(
    RunHttpsFirewallRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsFirewall;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunHttpsLatencyRoutine(
    RunHttpsLatencyRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kHttpsLatency;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunVideoConferencingRoutine(
    const absl::optional<std::string>& stun_server_hostname,
    RunVideoConferencingRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kVideoConferencing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcHttpRoutine(RunArcHttpRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcHttp;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcPingRoutine(RunArcPingRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcPing;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::RunArcDnsResolutionRoutine(
    RunArcDnsResolutionRoutineCallback callback) {
  last_run_routine_ = mojom::DiagnosticRoutineEnum::kArcDnsResolution;
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthd::AddBluetoothObserver(
    mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver> observer) {
  bluetooth_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddLidObserver(
    mojo::PendingRemote<mojom::CrosHealthdLidObserver> observer) {
  lid_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddPowerObserver(
    mojo::PendingRemote<mojom::CrosHealthdPowerObserver> observer) {
  power_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddNetworkObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        observer) {
  network_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddAudioObserver(
    mojo::PendingRemote<mojom::CrosHealthdAudioObserver> observer) {
  audio_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddThunderboltObserver(
    mojo::PendingRemote<mojom::CrosHealthdThunderboltObserver> observer) {
  thunderbolt_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::AddUsbObserver(
    mojo::PendingRemote<mojom::CrosHealthdUsbObserver> observer) {
  usb_observers_.Add(std::move(observer));
}

void FakeCrosHealthd::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), telemetry_response_info_.Clone()),
      callback_delay_);
}

void FakeCrosHealthd::ProbeProcessInfo(const uint32_t process_id,
                                       ProbeProcessInfoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), process_response_.Clone()),
      callback_delay_);
}

}  // namespace ash::cros_healthd
