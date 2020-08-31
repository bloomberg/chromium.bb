// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_disk.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/base/text/bytes_formatting.h"

namespace {
chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

std::string FormatBytes(const int64_t value) {
  return base::UTF16ToUTF8(ui::FormatBytes(value));
}

void EmitResizeResultMetric(vm_tools::concierge::DiskImageStatus status) {
  base::UmaHistogramEnumeration(
      "Crostini.DiskResize.Result", status,
      static_cast<vm_tools::concierge::DiskImageStatus>(
          vm_tools::concierge::DiskImageStatus_MAX + 1));
}
}  // namespace

namespace crostini {
CrostiniDiskInfo::CrostiniDiskInfo() = default;
CrostiniDiskInfo::CrostiniDiskInfo(CrostiniDiskInfo&&) = default;
CrostiniDiskInfo& CrostiniDiskInfo::operator=(CrostiniDiskInfo&&) = default;
CrostiniDiskInfo::~CrostiniDiskInfo() = default;

namespace disk {

void GetDiskInfo(OnceDiskInfoCallback callback,
                 Profile* profile,
                 std::string vm_name,
                 bool full_info) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    std::move(callback).Run(nullptr);
    VLOG(1) << "Crostini not enabled. Nothing to do.";
    return;
  }
  if (full_info) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                       base::FilePath(crostini::kHomeDirectory)),
        base::BindOnce(&OnAmountOfFreeDiskSpace, std::move(callback), profile,
                       std::move(vm_name)));
  } else {
    VLOG(1) << "Starting concierge";
    // Since we only care about the disk's current size and whether it's a
    // sparse disk, we claim there's plenty of free space available to prevent
    // error conditions in |OnCrostiniSufficientlyRunning|.
    constexpr int64_t kFakeAvailableDiskBytes =
        kDiskHeadroomBytes + kRecommendedDiskSizeBytes;

    CrostiniManager::GetForProfile(profile)->StartConcierge(base::BindOnce(
        [](OnceDiskInfoCallback callback, Profile* profile, std::string vm_name,
           bool success) {
          if (!success) {
            LOG(ERROR) << "Failed to start concierge";
            std::move(callback).Run(nullptr);
            return;
          }
          OnCrostiniSufficientlyRunning(
              std::move(callback), profile, std::move(vm_name),
              kFakeAvailableDiskBytes, CrostiniResult::SUCCESS);
        },
        std::move(callback), profile, std::move(vm_name)));
  }
}

void OnAmountOfFreeDiskSpace(OnceDiskInfoCallback callback,
                             Profile* profile,
                             std::string vm_name,
                             int64_t free_space) {
  if (free_space == 0) {
    LOG(ERROR) << "Failed to get amount of free disk space";
    std::move(callback).Run(nullptr);
  } else {
    VLOG(1) << "Starting vm " << vm_name;
    auto container_id = ContainerId(vm_name, kCrostiniDefaultContainerName);
    CrostiniManager::GetForProfile(profile)->EnsureVmRunning(
        std::move(container_id),
        base::BindOnce(&OnCrostiniSufficientlyRunning, std::move(callback),
                       profile, std::move(vm_name), free_space));
  }
}

void OnCrostiniSufficientlyRunning(OnceDiskInfoCallback callback,
                                   Profile* profile,
                                   std::string vm_name,
                                   int64_t free_space,
                                   CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Start VM: error " << static_cast<int>(result);
    std::move(callback).Run(nullptr);
  } else {
    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(CryptohomeIdForProfile(profile));
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    request.set_vm_name(vm_name);
    GetConciergeClient()->ListVmDisks(
        std::move(request), base::BindOnce(&OnListVmDisks, std::move(callback),
                                           std::move(vm_name), free_space));
  }
}

void OnListVmDisks(
    OnceDiskInfoCallback callback,
    std::string vm_name,
    int64_t free_space,
    base::Optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get response from concierge";
    std::move(callback).Run(nullptr);
    return;
  }
  if (!response->success()) {
    LOG(ERROR) << "Failed to get successful response from concierge "
               << response->failure_reason();
    std::move(callback).Run(nullptr);
    return;
  }
  auto disk_info = std::make_unique<CrostiniDiskInfo>();
  auto image =
      std::find_if(response->images().begin(), response->images().end(),
                   [&vm_name](const auto& a) { return a.name() == vm_name; });
  if (image == response->images().end()) {
    // No match found for the VM:
    LOG(ERROR) << "No VM found with name " << vm_name;
    std::move(callback).Run(nullptr);
    return;
  }
  VLOG(1) << "name: " << image->name();
  VLOG(1) << "image_type: " << image->image_type();
  VLOG(1) << "size: " << image->size();
  VLOG(1) << "user_chosen_size: " << image->user_chosen_size();

  if (image->image_type() !=
      vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW) {
    // Can't resize qcow2 images and don't know how to handle auto or pluginvm
    // images.
    disk_info->can_resize = false;
    std::move(callback).Run(std::move(disk_info));
    return;
  }
  if (image->min_size() == 0) {
    VLOG(1) << "Unable to get minimum disk size. VM not running yet?";
  }
  // User has to leave at least kDiskHeadroomBytes for the host system.
  // In some cases we can be over-provisioned (e.g. we increased the headroom
  // required), when that happens the user can still go up to their currently
  // allocated size.
  int64_t max_size =
      std::max(free_space - kDiskHeadroomBytes + image->size(), image->size());
  disk_info->is_user_chosen_size = image->user_chosen_size();
  disk_info->can_resize =
      image->image_type() == vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW;
  disk_info->is_low_space_available = max_size < kRecommendedDiskSizeBytes;

  const int64_t min_size =
      std::max(static_cast<int64_t>(image->min_size()), kMinimumDiskSizeBytes);
  std::vector<crostini::mojom::DiskSliderTickPtr> ticks =
      GetTicks(min_size, image->size(), max_size, &(disk_info->default_index));
  if (ticks.size() == 0) {
    LOG(ERROR) << "Unable to calculate the number of ticks for min: "
               << min_size << " current: " << image->size()
               << " max: " << max_size;
    std::move(callback).Run(nullptr);
    return;
  }
  disk_info->ticks = std::move(ticks);

  std::move(callback).Run(std::move(disk_info));
}

std::vector<crostini::mojom::DiskSliderTickPtr> GetTicks(
    int64_t min,
    int64_t current,
    int64_t max,
    int* out_default_index) {
  if (current < min) {
    // btrfs is conservative, sometimes it won't let us resize to what the user
    // currently has. In those cases act like the current size is the same as
    // the minimum.
    VLOG(1) << "Minimum size is larger than the current, setting current = min";
    current = min;
  }
  if (current > max) {
    LOG(ERROR) << "current (" << current << ") > max (" << max << ")";
    return {};
  }
  std::vector<int64_t> values = GetTicksForDiskSize(min, max);

  // Find the first value which is >= the current size, round that value down to
  // the current size, and then record that as the default. This means that the
  // ticks won't be evenly spaced, but GetTicksForDiskSize uses ~100 ticks so
  // close enough.
  auto it = std::lower_bound(begin(values), end(values), current);
  if (it != end(values)) {
    *it = current;
    *out_default_index = std::distance(begin(values), it);
  } else {
    DCHECK(values.empty());
    return {};
  }

  std::vector<crostini::mojom::DiskSliderTickPtr> ticks;
  ticks.reserve(values.size());
  for (const auto& val : values) {
    std::string formatted_val = FormatBytes(val);
    ticks.emplace_back(crostini::mojom::DiskSliderTick::New(val, formatted_val,
                                                            formatted_val));
  }
  return ticks;
}

class Observer : public chromeos::ConciergeClient::DiskImageObserver {
 public:
  Observer(std::string uuid, base::OnceCallback<void(bool)> callback)
      : uuid_(std::move(uuid)), callback_(std::move(callback)) {}
  ~Observer() override { GetConciergeClient()->RemoveDiskImageObserver(this); }
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override {
    if (signal.command_uuid() != uuid_) {
      return;
    }
    switch (signal.status()) {
      case vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS:
        break;
      case vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED:
        EmitResizeResultMetric(signal.status());
        std::move(callback_).Run(true);
        break;
      default:
        LOG(ERROR) << "Failed or unrecognised status when resizing: "
                   << signal.status() << " " << signal.failure_reason();
        EmitResizeResultMetric(signal.status());
        std::move(callback_).Run(false);
        delete this;
    }
  }

 private:
  std::string uuid_;
  base::OnceCallback<void(bool)> callback_;
};

void ResizeCrostiniDisk(Profile* profile,
                        std::string vm_name,
                        uint64_t size_bytes,
                        base::OnceCallback<void(bool)> callback) {
  ContainerId container_id(vm_name, kCrostiniDefaultContainerName);
  CrostiniManager::GetForProfile(profile)->EnsureVmRunning(
      std::move(container_id),
      base::BindOnce(&OnVMRunning, std::move(callback), profile,
                     std::move(vm_name), size_bytes));
}

void OnVMRunning(base::OnceCallback<void(bool)> callback,
                 Profile* profile,
                 std::string vm_name,
                 int64_t size_bytes,
                 CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to launch VM: error " << static_cast<int>(result);
    std::move(callback).Run(false);
  } else {
    vm_tools::concierge::ResizeDiskImageRequest request;
    request.set_cryptohome_id(CryptohomeIdForProfile(profile));
    request.set_vm_name(std::move(vm_name));
    request.set_disk_size(size_bytes);

    base::UmaHistogramBoolean("Crostini.DiskResize.Started", true);
    GetConciergeClient()->ResizeDiskImage(
        request, base::BindOnce(&OnResize, std::move(callback)));
  }
}

void OnResize(
    base::OnceCallback<void(bool)> callback,
    base::Optional<vm_tools::concierge::ResizeDiskImageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Got null response from concierge";
    EmitResizeResultMetric(
        vm_tools::concierge::DiskImageStatus::DISK_STATUS_UNKNOWN);
    std::move(callback).Run(false);
  } else if (response->status() ==
             vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED) {
    EmitResizeResultMetric(response->status());
    std::move(callback).Run(true);
  } else if (response->status() ==
             vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS) {
    GetConciergeClient()->AddDiskImageObserver(
        new Observer(response->command_uuid(), std::move(callback)));
  } else {
    LOG(ERROR) << "Got unexpected or error status from concierge: "
               << response->status();
    EmitResizeResultMetric(response->status());
    std::move(callback).Run(false);
  }
}
}  // namespace disk
}  // namespace crostini
