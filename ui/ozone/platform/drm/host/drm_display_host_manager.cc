// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"

#include <fcntl.h>
#include <stdio.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_device_handle.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

namespace ui {

namespace {

typedef base::Callback<void(const base::FilePath&, scoped_ptr<DrmDeviceHandle>)>
    OnOpenDeviceReplyCallback;

const char kDefaultGraphicsCardPattern[] = "/dev/dri/card%d";

const char* kDisplayActionString[] = {
    "ADD",
    "REMOVE",
    "CHANGE",
};

void OpenDeviceOnWorkerThread(
    const base::FilePath& path,
    const scoped_refptr<base::TaskRunner>& reply_runner,
    const OnOpenDeviceReplyCallback& callback) {
  scoped_ptr<DrmDeviceHandle> handle(new DrmDeviceHandle());
  handle->Initialize(path);
  reply_runner->PostTask(
      FROM_HERE, base::Bind(callback, path, base::Passed(handle.Pass())));
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

  return base::FilePath();
}

class FindDisplaySnapshotById {
 public:
  FindDisplaySnapshotById(int64_t display_id) : display_id_(display_id) {}

  bool operator()(const DisplaySnapshot* display) {
    return display->display_id() == display_id_;
  }

 private:
  int64_t display_id_;
};

}  // namespace

DrmDisplayHostManager::DrmDisplayHostManager(DrmGpuPlatformSupportHost* proxy,
                                             DeviceManager* device_manager)
    : proxy_(proxy),
      device_manager_(device_manager),
      delegate_(nullptr),
      primary_graphics_card_path_(GetPrimaryDisplayCardPath()),
      has_dummy_display_(false),
      task_pending_(false),
      weak_ptr_factory_(this) {
  {
    // First device needs to be treated specially. We need to open this
    // synchronously since the GPU process will need it to initialize the
    // graphics state.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    primary_drm_device_handle_.reset(new DrmDeviceHandle());
    if (!primary_drm_device_handle_->Initialize(primary_graphics_card_path_)) {
      LOG(FATAL) << "Failed to open primary graphics card";
      return;
    }
    drm_devices_.insert(primary_graphics_card_path_);
  }

  device_manager_->AddObserver(this);
  proxy_->RegisterHandler(this);

  ScopedVector<HardwareDisplayControllerInfo> display_infos =
      GetAvailableDisplayControllerInfos(primary_drm_device_handle_->fd());
  has_dummy_display_ = !display_infos.empty();
  for (size_t i = 0; i < display_infos.size(); ++i) {
    displays_.push_back(new DisplaySnapshotProxy(CreateDisplaySnapshotParams(
        display_infos[i], primary_drm_device_handle_->fd(), i, gfx::Point())));
  }
}

DrmDisplayHostManager::~DrmDisplayHostManager() {
  device_manager_->RemoveObserver(this);
  proxy_->UnregisterHandler(this);
}

DisplaySnapshot* DrmDisplayHostManager::GetDisplay(int64_t display_id) {
  auto it = std::find_if(displays_.begin(), displays_.end(),
                         FindDisplaySnapshotById(display_id));
  if (it == displays_.end())
    return nullptr;

  return *it;
}

void DrmDisplayHostManager::AddDelegate(DrmNativeDisplayDelegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void DrmDisplayHostManager::RemoveDelegate(DrmNativeDisplayDelegate* delegate) {
  DCHECK_EQ(delegate_, delegate);
  delegate_ = nullptr;
}

bool DrmDisplayHostManager::TakeDisplayControl() {
  proxy_->Send(new OzoneGpuMsg_TakeDisplayControl());
  return true;
}

bool DrmDisplayHostManager::RelinquishDisplayControl() {
  proxy_->Send(new OzoneGpuMsg_RelinquishDisplayControl());
  return true;
}

void DrmDisplayHostManager::UpdateDisplays(
    const GetDisplaysCallback& callback) {
  get_displays_callback_ = callback;
  if (!proxy_->Send(new OzoneGpuMsg_RefreshNativeDisplays())) {
    get_displays_callback_.Reset();
    callback.Run(displays_.get());
  }
}

void DrmDisplayHostManager::Configure(int64_t display_id,
                                      const DisplayMode* mode,
                                      const gfx::Point& origin,
                                      const ConfigureCallback& callback) {
  // The dummy display is used on the first run only. Note: cannot post a task
  // here since there is no task runner.
  if (has_dummy_display_) {
    callback.Run(true);
    return;
  }

  configure_callback_map_[display_id] = callback;

  bool status = false;
  if (mode) {
    status = proxy_->Send(new OzoneGpuMsg_ConfigureNativeDisplay(
        display_id, GetDisplayModeParams(*mode), origin));
  } else {
    status = proxy_->Send(new OzoneGpuMsg_DisableNativeDisplay(display_id));
  }

  if (!status)
    OnDisplayConfigured(display_id, false);
}

void DrmDisplayHostManager::GetHDCPState(int64_t display_id,
                                         const GetHDCPStateCallback& callback) {
  get_hdcp_state_callback_map_[display_id] = callback;
  if (!proxy_->Send(new OzoneGpuMsg_GetHDCPState(display_id)))
    OnHDCPStateReceived(display_id, false, HDCP_STATE_UNDESIRED);
}

void DrmDisplayHostManager::SetHDCPState(int64_t display_id,
                                         HDCPState state,
                                         const SetHDCPStateCallback& callback) {
  set_hdcp_state_callback_map_[display_id] = callback;
  if (!proxy_->Send(new OzoneGpuMsg_SetHDCPState(display_id, state)))
    OnHDCPStateUpdated(display_id, false);
}

bool DrmDisplayHostManager::SetGammaRamp(
    int64_t display_id,
    const std::vector<GammaRampRGBEntry>& lut) {
  proxy_->Send(new OzoneGpuMsg_SetGammaRamp(display_id, lut));
  return true;
}

void DrmDisplayHostManager::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::DISPLAY)
    return;

  event_queue_.push(DisplayEvent(event.action_type(), event.path()));
  ProcessEvent();
}

void DrmDisplayHostManager::ProcessEvent() {
  while (!event_queue_.empty() && !task_pending_) {
    DisplayEvent event = event_queue_.front();
    event_queue_.pop();
    VLOG(1) << "Got display event " << kDisplayActionString[event.action_type]
            << " for " << event.path.value();
    switch (event.action_type) {
      case DeviceEvent::ADD:
        if (drm_devices_.find(event.path) == drm_devices_.end()) {
          task_pending_ = base::WorkerPool::PostTask(
              FROM_HERE,
              base::Bind(&OpenDeviceOnWorkerThread, event.path,
                         base::ThreadTaskRunnerHandle::Get(),
                         base::Bind(&DrmDisplayHostManager::OnAddGraphicsDevice,
                                    weak_ptr_factory_.GetWeakPtr())),
              false /* task_is_slow */);
        }
        break;
      case DeviceEvent::CHANGE:
        task_pending_ = base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(&DrmDisplayHostManager::OnUpdateGraphicsDevice,
                       weak_ptr_factory_.GetWeakPtr()));
        break;
      case DeviceEvent::REMOVE:
        DCHECK(event.path != primary_graphics_card_path_)
            << "Removing primary graphics card";
        auto it = drm_devices_.find(event.path);
        if (it != drm_devices_.end()) {
          task_pending_ = base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&DrmDisplayHostManager::OnRemoveGraphicsDevice,
                         weak_ptr_factory_.GetWeakPtr(), event.path));
          drm_devices_.erase(it);
        }
        break;
    }
  }
}

void DrmDisplayHostManager::OnAddGraphicsDevice(
    const base::FilePath& path,
    scoped_ptr<DrmDeviceHandle> handle) {
  if (handle->IsValid()) {
    drm_devices_.insert(path);
    proxy_->Send(new OzoneGpuMsg_AddGraphicsDevice(
        path, base::FileDescriptor(handle->PassFD())));
    NotifyDisplayDelegate();
  }

  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManager::OnUpdateGraphicsDevice() {
  NotifyDisplayDelegate();
  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManager::OnRemoveGraphicsDevice(const base::FilePath& path) {
  proxy_->Send(new OzoneGpuMsg_RemoveGraphicsDevice(path));
  NotifyDisplayDelegate();
  task_pending_ = false;
  ProcessEvent();
}

void DrmDisplayHostManager::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
  drm_devices_.clear();
  drm_devices_.insert(primary_graphics_card_path_);
  scoped_ptr<DrmDeviceHandle> handle = primary_drm_device_handle_.Pass();
  if (!handle) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    handle.reset(new DrmDeviceHandle());
    if (!handle->Initialize(primary_graphics_card_path_))
      LOG(FATAL) << "Failed to open primary graphics card";
  }

  // Send the primary device first since this is used to initialize graphics
  // state.
  proxy_->Send(new OzoneGpuMsg_AddGraphicsDevice(
      primary_graphics_card_path_, base::FileDescriptor(handle->PassFD())));

  device_manager_->ScanDevices(this);
  NotifyDisplayDelegate();
}

void DrmDisplayHostManager::OnChannelDestroyed(int host_id) {
  // If the channel got destroyed in the middle of a configuration then just
  // respond with failure.
  if (!get_displays_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DrmDisplayHostManager::RunUpdateDisplaysCallback,
                   weak_ptr_factory_.GetWeakPtr(), get_displays_callback_));
    get_displays_callback_.Reset();
  }

  for (const auto& pair : configure_callback_map_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(pair.second, false));
  }
  configure_callback_map_.clear();
}

bool DrmDisplayHostManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmDisplayHostManager, message)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays, OnUpdateNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayConfigured, OnDisplayConfigured)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateReceived, OnHDCPStateReceived)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateUpdated, OnHDCPStateUpdated)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmDisplayHostManager::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& displays) {
  has_dummy_display_ = false;
  displays_.clear();
  for (size_t i = 0; i < displays.size(); ++i)
    displays_.push_back(new DisplaySnapshotProxy(displays[i]));

  if (!get_displays_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DrmDisplayHostManager::RunUpdateDisplaysCallback,
                   weak_ptr_factory_.GetWeakPtr(), get_displays_callback_));
    get_displays_callback_.Reset();
  }
}

void DrmDisplayHostManager::OnDisplayConfigured(int64_t display_id,
                                                bool status) {
  auto it = configure_callback_map_.find(display_id);
  if (it != configure_callback_map_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(it->second, status));
    configure_callback_map_.erase(it);
  }
}

void DrmDisplayHostManager::OnHDCPStateReceived(int64_t display_id,
                                                bool status,
                                                HDCPState state) {
  auto it = get_hdcp_state_callback_map_.find(display_id);
  if (it != get_hdcp_state_callback_map_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(it->second, status, state));
    get_hdcp_state_callback_map_.erase(it);
  }
}

void DrmDisplayHostManager::OnHDCPStateUpdated(int64_t display_id,
                                               bool status) {
  auto it = set_hdcp_state_callback_map_.find(display_id);
  if (it != set_hdcp_state_callback_map_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(it->second, status));
    set_hdcp_state_callback_map_.erase(it);
  }
}

void DrmDisplayHostManager::RunUpdateDisplaysCallback(
    const GetDisplaysCallback& callback) const {
  callback.Run(displays_.get());
}

void DrmDisplayHostManager::NotifyDisplayDelegate() const {
  if (delegate_)
    delegate_->OnConfigurationChanged();
}

}  // namespace ui
