// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/platform.h"

#include <shlobj.h>

#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/implementation/windows/atomic_boolean.h"
#include "internal/platform/implementation/windows/atomic_reference.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"
#include "internal/platform/implementation/windows/cancelable.h"
#include "internal/platform/implementation/windows/condition_variable.h"
#include "internal/platform/implementation/windows/executor.h"
#include "internal/platform/implementation/windows/future.h"
#include "internal/platform/implementation/windows/listenable_future.h"
#include "internal/platform/implementation/windows/log_message.h"
#include "internal/platform/implementation/windows/mutex.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/server_sync.h"
#include "internal/platform/implementation/windows/settable_future.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/implementation/windows/webrtc.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/wifi_lan.h"

namespace location {
namespace nearby {
namespace api {
namespace {

std::string GetPayloadPath(PayloadId payload_id) {
  PWSTR basePath;

  // Retrieves the full path of a known folder identified by the folder's
  // KNOWNFOLDERID.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
  SHGetKnownFolderPath(
      FOLDERID_Downloads,  //  rfid: A reference to the KNOWNFOLDERID that
                           //  identifies the folder.
      0,           // dwFlags: Flags that specify special retrieval options.
      NULL,        // hToken: An access token that represents a particular user.
      &basePath);  // ppszPath: When this method returns, contains the address
                   // of a pointer to a null-terminated Unicode string that
                   // specifies the path of the known folder. The calling
                   // process is responsible for freeing this resource once it
                   // is no longer needed by calling CoTaskMemFree, whether
                   // SHGetKnownFolderPath succeeds or not.
  size_t bufferSize;
  wcstombs_s(&bufferSize, NULL, 0, basePath, 0);
  char* fullpathUTF8 = new char[bufferSize + 1];
  memset(fullpathUTF8, 0, bufferSize);
  wcstombs_s(&bufferSize, fullpathUTF8, bufferSize, basePath, bufferSize - 1);
  std::string fullPath = std::string(fullpathUTF8);
  auto retval = absl::StrCat(fullPath += "\\", payload_id);
  delete[] fullpathUTF8;
  return retval;
}
}  // namespace

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return absl::make_unique<windows::AtomicBoolean>();
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return absl::make_unique<windows::AtomicUint32>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return absl::make_unique<windows::Mutex>(mode);
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return absl::make_unique<windows::ConditionVariable>(mutex);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id, std::int64_t total_size) {
  return shared::IOFile::CreateInputFile(GetPayloadPath(payload_id),
                                         total_size);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  return shared::IOFile::CreateOutputFile(GetPayloadPath(payload_id));
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<windows::LogMessage>(file, line, severity);
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return absl::make_unique<windows::SubmittableExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return absl::make_unique<windows::SubmittableExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return absl::make_unique<windows::ScheduledExecutor>();
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return absl::make_unique<windows::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    nearby::api::BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BluetoothClassicMedium>(adapter);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BleMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    BluetoothAdapter&) {
  return nullptr;
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<windows::ServerSyncMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::unique_ptr<WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return absl::make_unique<windows::WifiLanMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  return absl::make_unique<windows::WebRtcMedium>();
}

}  // namespace api
}  // namespace nearby
}  // namespace location
