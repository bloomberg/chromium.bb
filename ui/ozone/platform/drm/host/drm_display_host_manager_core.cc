// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host_manager_core.h"

#include <fcntl.h>
#include <stddef.h>
#include <xf86drm.h>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_device_handle.h"
#include "ui/ozone/platform/drm/host/drm_display_host.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

namespace ui {

namespace {

typedef base::Callback<void(const base::FilePath&,
                            const base::FilePath&,
                            scoped_ptr<DrmDeviceHandle>)>
    OnOpenDeviceReplyCallback;

const char kDefaultGraphicsCardPattern[] = "/dev/dri/card%d";
const char kVgemDevDriCardPath[] = "/dev/dri/";
const char kVgemSysCardPath[] = "/sys/bus/platform/devices/vgem/drm/";

const char* kDisplayActionString[] = {
    "ADD", "REMOVE", "CHANGE",
};

// Find sysfs device path for the given device path.
base::FilePath MapDevPathToSysPath(const base::FilePath& device_path) {
  // |device_path| looks something like /dev/dri/card0. We take the basename of
  // that (card0) and append it to /sys/class/drm. /sys/class/drm/card0 is a
  // symlink that points to something like
  // /sys/devices/pci0000:00/0000:00:02.0/0000:05:00.0/drm/card0, which exposes
  // some metadata about the attached device.
  return base::MakeAbsoluteFilePath(
      base::FilePath("/sys/class/drm").Append(device_path.BaseName()));
}

void OpenDeviceOnWorkerThread(
    const base::FilePath& device_path,
    const scoped_refptr<base::TaskRunner>& reply_runner,
    const OnOpenDeviceReplyCallback& callback) {
  base::FilePath sys_path = MapDevPathToSysPath(device_path);

  scoped_ptr<DrmDeviceHandle> handle(new DrmDeviceHandle());
  handle->Initialize(device_path, sys_path);
  reply_runner->PostTask(FROM_HERE,
                         base::Bind(callback, device_path, sys_path,
                                    base::Passed(std::move(handle))));
}

base::FilePath GetPrimaryDisplayCardPath() {
  struct drm_mode_card_res res;
  for (int i = 0; /* end on first card# that does not exist */; i++) {
    std::string card_path = base::StringPrintf(kDefaultGraphicsCardPattern, i);

    if (access(card_path.c_str(), F_OK) != 0)
      break;

    int fd = open(card_path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      VPLOG(1) << "Failed to open '" << card_path << "'";
      continue;
    }

    memset(&res, 0, sizeof(struct drm_mode_card_res));
    int ret = drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
    close(fd);
    if (ret == 0 && res.count_crtcs > 0) {
      return base::FilePath(card_path);
    }

    VPLOG_IF(1, ret) << "Failed to get DRM resources for '" << card_path << "'";
  }

  LOG(FATAL) << "Failed to open primary graphics device.";
  return base::FilePath();  // Not reached.
}

base::FilePath GetVgemCardPath() {
  base::FileEnumerator file_iter(base::FilePath(kVgemSysCardPath), false,
                                 base::FileEnumerator::DIRECTORIES,
                                 FILE_PATH_LITERAL("card*"));

  while (!file_iter.Next().empty()) {
    // Inspect the card%d directories in the directory and extract the filename.
    std::string vgem_card_path =
        kVgemDevDriCardPath + file_iter.GetInfo().GetName().BaseName().value();
    DVLOG(1) << "VGEM card path is " << vgem_card_path;
    return base::FilePath(vgem_card_path);
  }
  DVLOG(1) << "Don't support VGEM";
  return base::FilePath();
}

class FindDrmDisplayHostById {
 public:
  explicit FindDrmDisplayHostById(int64_t display_id)
      : display_id_(display_id) {}

  bool operator()(const scoped_ptr<DrmDisplayHost>& display) const {
    return display->snapshot()->display_id() == display_id_;
  }

 private:
  int64_t display_id_;
};

}  // namespace

DrmDisplayHostManagerProxy::~DrmDisplayHostManagerProxy() {}

DrmDisplayHostManagerCore::DrmDisplayHostManagerCore(
    DrmDisplayHostManagerProxy* proxy,
    DeviceManager* device_manager,
    InputControllerEvdev* input_controller)
    : proxy_(proxy),
      device_manager_(device_manager),
      input_controller_(input_controller),
      primary_graphics_card_path_(GetPrimaryDisplayCardPath()),
      weak_ptr_factory_(this) {
  {
    // First device needs to be treated specially. We need to open this
    // synchronously since the GPU process will need it to initialize the
    // graphics state.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::FilePath primary_graphics_card_path_sysfs =
        MapDevPathToSysPath(primary_graphics_card_path_);

    primary_drm_device_handle_.reset(new DrmDeviceHandle());
    if (!primary_drm_device_handle_->Initialize(
            primary_graphics_card_path_, primary_graphics_card_path_sysfs)) {
      LOG(FATAL) << "Failed to open primary graphics card";
      return;
    }
    drm_devices_[primary_graphics_card_path_] =
        primary_graphics_card_path_sysfs;

    vgem_card_path_ = GetVgemCardPath();
  }

  device_manager_->AddObserver(this);
  proxy_->RegisterHandler();

  ScopedVector<HardwareDisplayControllerInfo> display_infos =
      GetAvailableDisplayControllerInfos(primary_drm_device_handle_->fd());
  has_dummy_display_ = !display_infos.empty();
  for (size_t i = 0; i < display_infos.size(); ++i) {
    displays_.push_back(make_scoped_ptr(new DrmDisplayHost(
        proxy_->GetGpuPlatformSupportHost(),
        CreateDisplaySnapshotParams(
            display_infos[i], primary_drm_device_handle_->fd(),
            primary_drm_device_handle_->sys_path(), 0, gfx::Point()),
        true /* is_dummy */)));
  }
}

DrmDisplayHostManagerCore::~DrmDisplayHostManagerCore() {
  device_manager_->RemoveObserver(this);
}

DrmDisplayHost* DrmDisplayHostManagerCore::GetDisplay(int64_t display_id) {
  auto it = std::find_if(displays_.begin(), displays_.end(),
                         FindDrmDisplayHostById(display_id));
  if (it == displays_.end())
    return nullptr;

  return it->get();
}

void DrmDisplayHostManagerCore::AddDelegate(
    DrmNativeDisplayDelegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void DrmDisplayHostManagerCore::RemoveDelegate(
    DrmNativeDisplayDelegate* delegate) {
  DCHECK_EQ(delegate_, delegate);
  delegate_ = nullptr;
}

void DrmDisplayHostManagerCore::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  if (display_control_change_pending_) {
    LOG(ERROR) << "TakeDisplayControl called while change already pending";
    callback.Run(false);
    return;
  }

  if (!display_externally_controlled_) {
    LOG(ERROR) << "TakeDisplayControl called while display already owned";
    callback.Run(true);
    return;
  }

  take_display_control_callback_ = callback;
  display_control_change_pending_ = true;

  if (!proxy_->TakeDisplayControl())
    GpuTookDisplayControl(false);
}

void DrmDisplayHostManagerCore::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  if (display_control_change_pending_) {
    LOG(ERROR)
        << "RelinquishDisplayControl called while change already pending";
    callback.Run(false);
    return;
  }

  if (display_externally_controlled_) {
    LOG(ERROR) << "RelinquishDisplayControl called while display not owned";
    callback.Run(true);
    return;
  }

  relinquish_display_control_callback_ = callback;
  display_control_change_pending_ = true;

  if (!proxy_->RelinquishDisplayControl())
    GpuRelinquishedDisplayControl(false);
}

void DrmDisplayHostManagerCore::UpdateDisplays(
    const GetDisplaysCallback& callback) {
  get_displays_callback_ = callback;
  if (!proxy_->RefreshNativeDisplays()) {
    get_displays_callback_.Reset();
    RunUpdateDisplaysCallback(callback);
  }
}

void DrmDisplayHostManagerCore::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::DISPLAY)
    return;

  event_queue_.push(DisplayEvent(event.action_type(), event.path()));
  ProcessEvent();
}

void DrmDisplayHostManagerCore::ProcessEvent() {
  while (!event_queue_.empty() && !task_pending_) {
    DisplayEvent event = event_queue_.front();
    event_queue_.pop();
    VLOG(1) << "Got display event " << kDisplayActionString[event.action_type]
            << " for " << event.path.value();
    switch (event.action_type) {
      case DeviceEvent::ADD:
        if (event.path == vgem_card_path_)
          continue;
        if (drm_devices_.find(event.path) == drm_devices_.end()) {
          task_pending_ = base::WorkerPool::PostTask(
              FROM_HERE,
              base::Bind(
                  &OpenDeviceOnWorkerThread, event.path,
                  base::ThreadTaskRunnerHandle::Get(),
                  base::Bind(&DrmDisplayHostManagerCore::OnAddGraphicsDevice,
                             weak_ptr_factory_.GetWeakPtr())),
              false /* task_is_slow */);
        }
        break;
      case DeviceEvent::CHANGE:
        task_pending_ = base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(&DrmDisplayHostManagerCore::OnUpdateGraphicsDevice,
                       weak_ptr_factory_.GetWeakPtr()));
        break;
      case DeviceEvent::REMOVE:
        DCHECK(event.path != primary_graphics_card_path_)
            << "Removing primary graphics card";
        DCHECK(event.path != vgem_card_path_) << "Removing VGEM device";
        auto it = drm_devices_.find(event.path);
        if (it != drm_devices_.end()) {
          task_pending_ = base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&DrmDisplayHostManagerCore::OnRemoveGraphicsDevice,
                         weak_ptr_factory_.GetWeakPtr(), it->second));
          drm_devices_.erase(it);
        }
        break;
    }
  }
}

void DrmDisplayHostManagerCore::OnAddGraphicsDevice(
    const base::FilePath& dev_path,
    const base::FilePath& sys_path,
    scoped_ptr<DrmDeviceHandle> handle) {
  if (handle->IsValid()) {
    drm_devices_[dev_path] = sys_path;
    proxy_->AddGraphicsDevice(sys_path, base::FileDescriptor(handle->PassFD()));
    NotifyDisplayDelegate();
  }

  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManagerCore::OnUpdateGraphicsDevice() {
  NotifyDisplayDelegate();
  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManagerCore::OnRemoveGraphicsDevice(
    const base::FilePath& sys_path) {
  proxy_->RemoveGraphicsDevice(sys_path);
  NotifyDisplayDelegate();
  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManagerCore::GpuThreadStarted() {
  // If in the middle of a configuration, just respond with the old list of
  // displays. This is fine, since after the DRM resources are initialized and
  // IPC-ed to the GPU NotifyDisplayDelegate() is called to let the display
  // delegate know that the display configuration changed and it needs to
  // update it again.
  if (!get_displays_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DrmDisplayHostManagerCore::RunUpdateDisplaysCallback,
                   weak_ptr_factory_.GetWeakPtr(), get_displays_callback_));
    get_displays_callback_.Reset();
  }

  // Signal that we're taking DRM master since we're going through the
  // initialization process again and we'll take all the available resources.
  if (!take_display_control_callback_.is_null())
    GpuTookDisplayControl(true);

  if (!relinquish_display_control_callback_.is_null())
    GpuRelinquishedDisplayControl(false);

  scoped_ptr<DrmDeviceHandle> handle = std::move(primary_drm_device_handle_);
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    drm_devices_.clear();
    drm_devices_[primary_graphics_card_path_] =
        MapDevPathToSysPath(primary_graphics_card_path_);

    if (!handle) {
      handle.reset(new DrmDeviceHandle());
      if (!handle->Initialize(primary_graphics_card_path_,
                              drm_devices_[primary_graphics_card_path_]))
        LOG(FATAL) << "Failed to open primary graphics card";
    }
  }

  // Send the primary device first since this is used to initialize graphics
  // state.
  proxy_->AddGraphicsDevice(drm_devices_[primary_graphics_card_path_],
                            base::FileDescriptor(handle->PassFD()));

  device_manager_->ScanDevices(this);
  NotifyDisplayDelegate();
}

void DrmDisplayHostManagerCore::GpuHasUpdatedNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& params) {
  std::vector<scoped_ptr<DrmDisplayHost>> old_displays;
  displays_.swap(old_displays);
  for (size_t i = 0; i < params.size(); ++i) {
    auto it = std::find_if(old_displays.begin(), old_displays.end(),
                           FindDrmDisplayHostById(params[i].display_id));
    if (it == old_displays.end()) {
      displays_.push_back(make_scoped_ptr(
          new DrmDisplayHost(proxy_->GetGpuPlatformSupportHost(), params[i],
                             false /* is_dummy */)));
    } else {
      (*it)->UpdateDisplaySnapshot(params[i]);
      displays_.push_back(std::move(*it));
      old_displays.erase(it);
    }
  }

  if (!get_displays_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DrmDisplayHostManagerCore::RunUpdateDisplaysCallback,
                   weak_ptr_factory_.GetWeakPtr(), get_displays_callback_));
    get_displays_callback_.Reset();
  }
}

void DrmDisplayHostManagerCore::GpuConfiguredDisplay(int64_t display_id,
                                                     bool status) {
  DrmDisplayHost* display = GetDisplay(display_id);
  if (display)
    display->OnDisplayConfigured(status);
  else
    LOG(ERROR) << "Couldn't find display with id=" << display_id;
}

void DrmDisplayHostManagerCore::GpuReceivedHDCPState(int64_t display_id,
                                                     bool status,
                                                     HDCPState state) {
  DrmDisplayHost* display = GetDisplay(display_id);
  if (display)
    display->OnHDCPStateReceived(status, state);
  else
    LOG(ERROR) << "Couldn't find display with id=" << display_id;
}

void DrmDisplayHostManagerCore::GpuUpdatedHDCPState(int64_t display_id,
                                                    bool status) {
  DrmDisplayHost* display = GetDisplay(display_id);
  if (display)
    display->OnHDCPStateUpdated(status);
  else
    LOG(ERROR) << "Couldn't find display with id=" << display_id;
}

void DrmDisplayHostManagerCore::GpuTookDisplayControl(bool status) {
  if (take_display_control_callback_.is_null()) {
    LOG(ERROR) << "No callback for take display control";
    return;
  }

  DCHECK(display_externally_controlled_);
  DCHECK(display_control_change_pending_);

  if (status) {
    input_controller_->SetInputDevicesEnabled(true);
    display_externally_controlled_ = false;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(take_display_control_callback_, status));
  take_display_control_callback_.Reset();
  display_control_change_pending_ = false;
}

void DrmDisplayHostManagerCore::GpuRelinquishedDisplayControl(bool status) {
  if (relinquish_display_control_callback_.is_null()) {
    LOG(ERROR) << "No callback for relinquish display control";
    return;
  }

  DCHECK(!display_externally_controlled_);
  DCHECK(display_control_change_pending_);

  if (status) {
    input_controller_->SetInputDevicesEnabled(false);
    display_externally_controlled_ = true;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(relinquish_display_control_callback_, status));
  relinquish_display_control_callback_.Reset();
  display_control_change_pending_ = false;
}

void DrmDisplayHostManagerCore::RunUpdateDisplaysCallback(
    const GetDisplaysCallback& callback) const {
  std::vector<DisplaySnapshot*> snapshots;
  for (const auto& display : displays_)
    snapshots.push_back(display->snapshot());

  callback.Run(snapshots);
}

void DrmDisplayHostManagerCore::NotifyDisplayDelegate() const {
  if (delegate_)
    delegate_->OnConfigurationChanged();
}

}  // namespace ui
