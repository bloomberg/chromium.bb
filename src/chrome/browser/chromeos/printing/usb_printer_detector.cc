// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/usb_printer_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "content/public/browser/browser_thread.h"
#include "device/base/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"

namespace chromeos {
namespace {

// Given a usb device, guesses the make and model for a driver lookup.
//
// TODO(https://crbug.com/895037): Possibly go deeper and query the IEEE1284
// fields for make and model if we determine those are more likely to contain
// what we want.  Strings currently come from udev.
// TODO(https://crbug.com/895037): When above is added, parse out document
// formats and add to DetectedPrinter
std::string GuessEffectiveMakeAndModel(const device::UsbDevice& device) {
  return base::UTF16ToUTF8(device.manufacturer_string()) + " " +
         base::UTF16ToUTF8(device.product_string());
}

// The PrinterDetector that drives the flow for setting up a USB printer to use
// CUPS backend.
class UsbPrinterDetectorImpl : public UsbPrinterDetector,
                               public device::UsbService::Observer {
 public:
  explicit UsbPrinterDetectorImpl(device::UsbService* usb_service)
      : usb_observer_(this),
        weak_ptr_factory_(this) {
    if (usb_service) {
      usb_observer_.Add(usb_service);
      usb_service->GetDevices(base::Bind(&UsbPrinterDetectorImpl::OnGetDevices,
                                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  ~UsbPrinterDetectorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  }

  // PrinterDetector override.
  void RegisterPrintersFoundCallback(OnPrintersFoundCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(!on_printers_found_callback_);
    on_printers_found_callback_ = std::move(cb);
  }

  // PrinterDetector override.
  std::vector<DetectedPrinter> GetPrinters() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    base::AutoLock auto_lock(printers_lock_);
    return GetPrintersLocked();
  }

 private:
  std::vector<DetectedPrinter> GetPrintersLocked() {
    printers_lock_.AssertAcquired();
    std::vector<DetectedPrinter> printers_list;
    printers_list.reserve(printers_.size());
    for (const auto& entry : printers_) {
      printers_list.push_back(entry.second);
    }
    return printers_list;
  }

  // Callback for initial enumeration of usb devices.
  void OnGetDevices(
      const std::vector<scoped_refptr<device::UsbDevice>>& devices) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (const auto& device : devices) {
      OnDeviceAdded(device);
    }
  }

  // UsbService::observer override.
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!UsbDeviceIsPrinter(*device)) {
      return;
    }
    std::unique_ptr<Printer> converted = UsbDeviceToPrinter(*device);
    if (!converted.get()) {
      // An error will already have been logged if we failed to convert.
      return;
    }
    DetectedPrinter entry;
    entry.printer = *converted;
    entry.ppd_search_data.usb_vendor_id = device->vendor_id();
    entry.ppd_search_data.usb_product_id = device->product_id();
    entry.ppd_search_data.make_and_model.push_back(
        GuessEffectiveMakeAndModel(*device));
    entry.ppd_search_data.discovery_type =
        PrinterSearchData::PrinterDiscoveryType::kUsb;
    // TODO(https://crbug.com/895037): Add in command set from IEEE1284

    base::AutoLock auto_lock(printers_lock_);
    printers_[device->guid()] = entry;
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrintersLocked());
    }
  }

  // UsbService::observer override.
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!UsbDeviceIsPrinter(*device)) {
      return;
    }
    base::AutoLock auto_lock(printers_lock_);
    printers_.erase(device->guid());
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrintersLocked());
    }
  }

  SEQUENCE_CHECKER(sequence_);

  // Map from USB GUID to DetectedPrinter for all detected printers, and
  // associated lock, since we don't require all access to be from the same
  // sequence.
  std::map<std::string, DetectedPrinter> printers_;
  base::Lock printers_lock_;

  OnPrintersFoundCallback on_printers_found_callback_;

  ScopedObserver<device::UsbService, device::UsbService::Observer>
      usb_observer_;
  base::WeakPtrFactory<UsbPrinterDetectorImpl> weak_ptr_factory_;
};

}  // namespace

// static
std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::Create() {
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  return std::make_unique<UsbPrinterDetectorImpl>(usb_service);
}

std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::CreateForTesting(
    device::UsbService* usb_service) {
  return std::make_unique<UsbPrinterDetectorImpl>(usb_service);
}

}  // namespace chromeos
