// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"

#include <set>
#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

using device::BluetoothUUID;
using UUIDSet = device::BluetoothDevice::UUIDSet;
using ManufacturerDataMap = device::BluetoothDevice::ManufacturerDataMap;
using blink::mojom::WebBluetoothResult;

namespace {

// Signal Strength Display Notes:
//
// RSSI values are displayed by the chooser to empower a user to differentiate
// between multiple devices with the same name, comparing devices with different
// names is a secondary goal. It is important that a user is able to move closer
// and farther away from a device and have it transition between two different
// signal strength levels, thus we want to spread RSSI values out evenly accross
// displayed levels.
//
// RSSI values from UMA in RecordRSSISignalStrength are charted here:
// https://photos.app.goo.gl/6R0ksxWzBsfvrbXH2 (2017-10-18)
// with a copy-paste of table data at every 5dBm:
//  dBm   CDF* Histogram Bucket Quantity (hand drawn estimate)
// -100 00.26%
//  -95 01.22% ---
//  -90 04.14% -----------
//  -85 11.24% ----------------------------
//  -80 18.29% ----------------------------
//  -75 27.13% -----------------------------------
//  -70 37.72% ------------------------------------------
//  -65 49.42% ----------------------------------------------
//  -60 62.91% -----------------------------------------------------
//  -55 74.35% ---------------------------------------------
//  -50 83.07% ----------------------------------
//  -45 88.43% ---------------------
//  -40 92.41% ---------------
//  -35 94.84% ---------
//  -30 95.96% ----
//  -25 96.60% --
//  -20 96.90% -
//  -15 97.07%
//  -10 97.21%
//   -5 97.34%
//    0 97.47%
//
// CDF: Cumulative Distribution Function:
// https://en.wikipedia.org/wiki/Cumulative_distribution_function
//
// Conversion to signal strengths is done by selecting 4 threshold points
// equally spaced through the CDF.
const int k20thPercentileRSSI = -79;
const int k40thPercentileRSSI = -69;
const int k60thPercentileRSSI = -61;
const int k80thPercentileRSSI = -52;

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "Web Bluetooth Device Chooser";

}  // namespace

namespace content {

// Sets the default duration for a Bluetooth scan to 60 seconds.
int64_t BluetoothDeviceChooserController::scan_duration_ = 60;

namespace {

#if DCHECK_IS_ON()
void LogRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {}
#else
void LogRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  DVLOG(1) << "requestDevice called with the following filters: ";
  DVLOG(1) << "acceptAllDevices: " << options->accept_all_devices;

  if (!options->filters)
    return;

  int i = 0;
  for (const auto& filter : options->filters.value()) {
    DVLOG(1) << "Filter #" << ++i;
    if (filter->name)
      DVLOG(1) << "Name: " << filter->name.value();

    if (filter->name_prefix)
      DVLOG(1) << "Name Prefix: " << filter->name_prefix.value();

    if (filter->services) {
      base::Value services_list(base::Value::Type::LIST);
      for (const auto& service : filter->services.value())
        services_list.Append(service.canonical_value());
      DVLOG(1) << "Services: " << services_list;
    }

    if (filter->manufacturer_data) {
      base::Value manufacturer_data_list(base::Value::Type::LIST);
      for (const auto& manufacturer_data : filter->manufacturer_data.value()) {
        base::Value filter_data_list(base::Value::Type::LIST);
        base::Value filter_mask_list(base::Value::Type::LIST);
        for (const auto& data_filter : manufacturer_data.second) {
          filter_data_list.Append(base::Value(data_filter->data));
          filter_mask_list.Append(base::Value(data_filter->mask));
        }
        base::Value data_filter_dict(base::Value::Type::DICTIONARY);
        data_filter_dict.SetKey("Company Identifier",
                                base::Value(manufacturer_data.first->id));
        data_filter_dict.SetKey("Data", std::move(filter_data_list));
        data_filter_dict.SetKey("Mask", std::move(filter_mask_list));
        manufacturer_data_list.Append(std::move(data_filter_dict));
      }
      DVLOG(1) << "Manufacturer Data: " << manufacturer_data_list;
    }
  }
}
#endif

bool MatchesFilter(const std::string* device_name,
                   const UUIDSet& device_uuids,
                   const ManufacturerDataMap& device_manufacturer_data,
                   const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  if (filter->name) {
    if (device_name == nullptr)
      return false;
    if (filter->name.value() != *device_name)
      return false;
  }

  if (filter->name_prefix && filter->name_prefix->size()) {
    if (device_name == nullptr)
      return false;
    if (!base::StartsWith(*device_name, filter->name_prefix.value(),
                          base::CompareCase::SENSITIVE))
      return false;
  }

  if (filter->services) {
    for (const auto& service : filter->services.value()) {
      if (!base::Contains(device_uuids, service)) {
        return false;
      }
    }
  }

  if (filter->manufacturer_data) {
    for (const auto& filter_data : filter->manufacturer_data.value()) {
      // Check the company identifier.
      auto it = device_manufacturer_data.find(filter_data.first->id);
      if (it == device_manufacturer_data.end())
        return false;
      // Check data filter size is less than device manufacturer data size.
      const auto& device_data = it->second;
      if (filter_data.second.size() > device_data.size())
        return false;
      // For each bit in mask, check the corresponding bit in device
      // manufacturer data is equal to the corresponding bit in expected data.
      size_t i = 0;
      for (const auto& filter_byte : filter_data.second) {
        if ((filter_byte->mask & filter_byte->data) !=
            (filter_byte->mask & device_data.at(i++))) {
          return false;
        }
      }
    }
  }

  return true;
}

bool MatchesFilters(
    const std::string* device_name,
    const UUIDSet& device_uuids,
    const ManufacturerDataMap& device_manufacturer_data,
    const absl::optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  DCHECK(HasValidFilter(filters));
  for (const auto& filter : filters.value()) {
    if (MatchesFilter(device_name, device_uuids, device_manufacturer_data,
                      filter)) {
      return true;
    }
  }
  return false;
}

void StopDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // Nothing goes wrong if the discovery session fails to stop, and we don't
  // need to wait for it before letting the user's script proceed, so we ignore
  // the results here.
  discovery_session->Stop();
}

}  // namespace

BluetoothDeviceChooserController::BluetoothDeviceChooserController(
    WebBluetoothServiceImpl* web_bluetooth_service,
    RenderFrameHost* render_frame_host,
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)),
      web_bluetooth_service_(web_bluetooth_service),
      render_frame_host_(render_frame_host),
      discovery_session_timer_(
          FROM_HERE,
          base::Seconds(scan_duration_),
          base::BindRepeating(
              &BluetoothDeviceChooserController::StopDeviceDiscovery,
              // base::Timer guarantees it won't call back after its
              // destructor starts.
              base::Unretained(this))) {
  CHECK(adapter_);
}

BluetoothDeviceChooserController::~BluetoothDeviceChooserController() {
  if (callback_) {
    std::move(callback_).Run(WebBluetoothResult::CHOOSER_CANCELLED,
                             /*options=*/nullptr,
                             /*device_id=*/std::string());
  }
}

void BluetoothDeviceChooserController::GetDevice(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    Callback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // GetDevice should only be called once.
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);
  options_ = std::move(options);
  LogRequestDeviceOptions(options_);

  // Check blocklist to reject invalid filters and adjust optional_services.
  if (options_->filters &&
      BluetoothBlocklist::Get().IsExcluded(options_->filters.value())) {
    PostErrorCallback(WebBluetoothResult::REQUEST_DEVICE_WITH_BLOCKLISTED_UUID);
    return;
  }
  BluetoothBlocklist::Get().RemoveExcludedUUIDs(options_.get());

  WebBluetoothResult allow_result =
      web_bluetooth_service_->GetBluetoothAllowed();
  if (allow_result != WebBluetoothResult::SUCCESS) {
    switch (allow_result) {
      case WebBluetoothResult::CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED:
        break;
      case WebBluetoothResult::CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED: {
        // Log to the developer console.
        render_frame_host_->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kInfo,
            "Bluetooth permission has been blocked.");
        // Block requests.
        break;
      }
      default:
        break;
    }
    PostErrorCallback(allow_result);
    return;
  }

  if (!adapter_->IsPresent()) {
    DVLOG(1) << "Bluetooth Adapter not present. Can't serve requestDevice.";
    PostErrorCallback(WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
    return;
  }

  BluetoothChooser::EventHandler chooser_event_handler = base::BindRepeating(
      &BluetoothDeviceChooserController::OnBluetoothChooserEvent,
      base::Unretained(this));

  if (auto* delegate = GetContentClient()->browser()->GetBluetoothDelegate()) {
    chooser_ = delegate->RunBluetoothChooser(render_frame_host_,
                                             std::move(chooser_event_handler));
  }

  if (!chooser_) {
    PostErrorCallback(WebBluetoothResult::WEB_BLUETOOTH_NOT_SUPPORTED);
    return;
  }

  if (adapter_->GetOsPermissionStatus() ==
      device::BluetoothAdapter::PermissionStatus::kDenied) {
    chooser_->SetAdapterPresence(
        BluetoothChooser::AdapterPresence::UNAUTHORIZED);
    return;
  }

  if (!chooser_->CanAskForScanningPermission()) {
    DVLOG(1) << "Closing immediately because Chooser cannot obtain permission.";
    OnBluetoothChooserEvent(BluetoothChooserEvent::DENIED_PERMISSION,
                            "" /* device_address */);
    return;
  }

  device_ids_.clear();
  PopulateConnectedDevices();
  if (!chooser_) {
    // If the dialog's closing, no need to do any of the rest of this.
    return;
  }

  if (!adapter_->IsPowered()) {
    chooser_->SetAdapterPresence(
        BluetoothChooser::AdapterPresence::POWERED_OFF);
    return;
  }

  StartDeviceDiscovery();
}

void BluetoothDeviceChooserController::AddFilteredDevice(
    const device::BluetoothDevice& device) {
  absl::optional<std::string> device_name = device.GetName();
  if (chooser_.get()) {
    if (options_->accept_all_devices ||
        MatchesFilters(device_name ? &device_name.value() : nullptr,
                       device.GetUUIDs(), device.GetManufacturerData(),
                       options_->filters)) {
      absl::optional<int8_t> rssi = device.GetInquiryRSSI();
      std::string device_id = device.GetAddress();
      device_ids_.insert(device_id);
      chooser_->AddOrUpdateDevice(
          device_id, !!device.GetName() /* should_update_name */,
          device.GetNameForDisplay(), device.IsGattConnected(),
          web_bluetooth_service_->IsDevicePaired(device.GetAddress()),
          rssi ? CalculateSignalStrengthLevel(rssi.value()) : -1);
    }
  }
}

void BluetoothDeviceChooserController::AdapterPoweredChanged(bool powered) {
  if (!powered && discovery_session_.get()) {
    StopDiscoverySession(std::move(discovery_session_));
  }

  if (chooser_.get()) {
    chooser_->SetAdapterPresence(
        powered ? BluetoothChooser::AdapterPresence::POWERED_ON
                : BluetoothChooser::AdapterPresence::POWERED_OFF);
    if (powered) {
      OnBluetoothChooserEvent(BluetoothChooserEvent::RESCAN,
                              "" /* device_address */);
    }
  }

  if (!powered) {
    discovery_session_timer_.Stop();
  }
}

int BluetoothDeviceChooserController::CalculateSignalStrengthLevel(
    int8_t rssi) {
  RecordRSSISignalStrength(rssi);

  if (rssi < k20thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_0);
    return 0;
  } else if (rssi < k40thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_1);
    return 1;
  } else if (rssi < k60thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_2);
    return 2;
  } else if (rssi < k80thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_3);
    return 3;
  } else {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_4);
    return 4;
  }
}

void BluetoothDeviceChooserController::SetTestScanDurationForTesting(
    TestScanDurationSetting setting) {
  switch (setting) {
    case TestScanDurationSetting::IMMEDIATE_TIMEOUT:
      scan_duration_ = 0;
      break;
    case TestScanDurationSetting::NEVER_TIMEOUT:
      scan_duration_ = base::TimeDelta::Max().InSeconds();
      break;
  }
}

void BluetoothDeviceChooserController::PopulateConnectedDevices() {
  // TODO(crbug.com/728897): Use RetrieveGattConnectedDevices once implemented.
  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    if (device->IsGattConnected()) {
      AddFilteredDevice(*device);
    }
  }
}

void BluetoothDeviceChooserController::StartDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (discovery_session_.get() && discovery_session_->IsActive()) {
    // Already running; just increase the timeout.
    discovery_session_timer_.Reset();
    return;
  }

  chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::DISCOVERING);
  adapter_->StartDiscoverySessionWithFilter(
      ComputeScanFilter(options_->filters), kScanClientName,
      base::BindOnce(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionFailed,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceChooserController::StopDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StopDiscoverySession(std::move(discovery_session_));
  if (chooser_) {
    chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::IDLE);
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Started discovery session.";
  if (chooser_) {
    discovery_session_ = std::move(discovery_session);
    discovery_session_timer_.Reset();
  } else {
    StopDiscoverySession(std::move(discovery_session));
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionFailed() {
  if (chooser_) {
    chooser_->ShowDiscoveryState(
        BluetoothChooser::DiscoveryState::FAILED_TO_START);
  }
}

void BluetoothDeviceChooserController::OnBluetoothChooserEvent(
    BluetoothChooserEvent event,
    const std::string& device_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  switch (event) {
    case BluetoothChooserEvent::RESCAN:
      device_ids_.clear();
      PopulateConnectedDevices();
      DCHECK(chooser_);
      StartDeviceDiscovery();
      // No need to close the chooser so we return.
      return;
    case BluetoothChooserEvent::DENIED_PERMISSION:
      PostErrorCallback(
          WebBluetoothResult::CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN);
      break;
    case BluetoothChooserEvent::CANCELLED:
      PostErrorCallback(WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooserEvent::SHOW_OVERVIEW_HELP:
      DVLOG(1) << "Overview Help link pressed.";
      PostErrorCallback(WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooserEvent::SHOW_ADAPTER_OFF_HELP:
      DVLOG(1) << "Adapter Off Help link pressed.";
      PostErrorCallback(WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooserEvent::SHOW_NEED_LOCATION_HELP:
      DVLOG(1) << "Need Location Help link pressed.";
      PostErrorCallback(WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooserEvent::SELECTED:
      PostSuccessCallback(device_address);
      break;
  }
  // Close chooser.
  chooser_.reset();
}

void BluetoothDeviceChooserController::PostSuccessCallback(
    const std::string& device_address) {
  DCHECK(callback_);

  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), WebBluetoothResult::SUCCESS,
                         std::move(options_), device_address))) {
    DLOG(WARNING) << "No TaskRunner.";
  }
}

void BluetoothDeviceChooserController::PostErrorCallback(
    WebBluetoothResult error) {
  DCHECK(callback_);

  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), error, /*options=*/nullptr,
                         /*device_id=*/std::string()))) {
    DLOG(WARNING) << "No TaskRunner.";
  }
}

// static
std::unique_ptr<device::BluetoothDiscoveryFilter>
BluetoothDeviceChooserController::ComputeScanFilter(
    const absl::optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  // There isn't much support for GATT over BR/EDR from neither platforms nor
  // devices so performing a Dual scan will find devices that the API is not
  // able to interact with. To avoid wasting power and confusing users with
  // devices they are not able to interact with, we only perform an LE Scan.
  auto discovery_filter = std::make_unique<device::BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);

  if (filters) {
    for (const auto& filter : filters.value()) {
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      // Keep track of whether this filter can be converted accurately.
      bool has_supported_fields = false;
      if (filter->services) {
        device_filter.uuids =
            base::flat_set<device::BluetoothUUID>(filter->services.value());
        has_supported_fields = true;
      }
      if (filter->name) {
        device_filter.name = filter->name.value();
        has_supported_fields = true;
      }

      // If we don't have any supported fields in this filter then we cannot
      // filter any devices as we don't want to filter out devices which would
      // have passed these unsupported filter criteria.
      if (!has_supported_fields) {
        discovery_filter->ClearDeviceFilters();
        return discovery_filter;
      }
      discovery_filter->AddDeviceFilter(device_filter);
    }
  }

  return discovery_filter;
}

}  // namespace content
