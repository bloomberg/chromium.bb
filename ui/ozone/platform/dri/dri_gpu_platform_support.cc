// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_gpu_platform_support.h"

#include "ipc/ipc_message_macros.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/native_display_delegate_dri.h"

namespace ui {

namespace {

class FindDisplayById {
 public:
  FindDisplayById(int64_t display_id) : display_id_(display_id) {}

  bool operator()(const DisplaySnapshot_Params& display) const {
    return display.display_id == display_id_;
  }

 private:
  int64_t display_id_;
};

}  // namespace

DriGpuPlatformSupport::DriGpuPlatformSupport(
    DriSurfaceFactory* dri,
    DriWindowDelegateManager* window_manager,
    ScreenManager* screen_manager,
    scoped_ptr<NativeDisplayDelegateDri> ndd)
    : sender_(NULL),
      dri_(dri),
      window_manager_(window_manager),
      screen_manager_(screen_manager),
      ndd_(ndd.Pass()) {
}

DriGpuPlatformSupport::~DriGpuPlatformSupport() {
}

void DriGpuPlatformSupport::AddHandler(scoped_ptr<GpuPlatformSupport> handler) {
  handlers_.push_back(handler.release());
}

void DriGpuPlatformSupport::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(sender);
}

bool DriGpuPlatformSupport::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DriGpuPlatformSupport, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CreateWindowDelegate, OnCreateWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DestroyWindowDelegate,
                      OnDestroyWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_WindowBoundsChanged, OnWindowBoundsChanged)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ForceDPMSOn, OnForceDPMSOn)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RefreshNativeDisplays,
                      OnRefreshNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ConfigureNativeDisplay,
                      OnConfigureNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DisableNativeDisplay, OnDisableNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_TakeDisplayControl, OnTakeDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RelinquishDisplayControl,
                      OnRelinquishDisplayControl)
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  if (!handled)
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i]->OnMessageReceived(message))
        return true;

  return false;
}

void DriGpuPlatformSupport::OnCreateWindowDelegate(
    gfx::AcceleratedWidget widget) {
  // Due to how the GPU process starts up this IPC call may happen after the IPC
  // to create a surface. Since a surface wants to know the window associated
  // with it, we create it ahead of time. So when this call happens we do not
  // create a delegate if it already exists.
  if (!window_manager_->HasWindowDelegate(widget)) {
    scoped_ptr<DriWindowDelegate> delegate(
        new DriWindowDelegateImpl(widget, screen_manager_));
    delegate->Initialize();
    window_manager_->AddWindowDelegate(widget, delegate.Pass());
  }
}

void DriGpuPlatformSupport::OnDestroyWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DriWindowDelegate> delegate =
      window_manager_->RemoveWindowDelegate(widget);
  delegate->Shutdown();
}

void DriGpuPlatformSupport::OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                                  const gfx::Rect& bounds) {
  window_manager_->GetWindowDelegate(widget)->OnBoundsChanged(bounds);
}

void DriGpuPlatformSupport::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location,
                                        int frame_delay_ms) {
  dri_->SetHardwareCursor(widget, bitmaps, location, frame_delay_ms);
}

void DriGpuPlatformSupport::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  dri_->MoveHardwareCursor(widget, location);
}

void DriGpuPlatformSupport::OnForceDPMSOn() {
  ndd_->ForceDPMSOn();
}

void DriGpuPlatformSupport::OnRefreshNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& cached_displays) {
  std::vector<DisplaySnapshot_Params> displays;
  std::vector<DisplaySnapshot*> native_displays = ndd_->GetDisplays();

  // If any of the cached displays are in the list of new displays then apply
  // their configuration immediately.
  for (size_t i = 0; i < native_displays.size(); ++i) {
    std::vector<DisplaySnapshot_Params>::const_iterator it =
        std::find_if(cached_displays.begin(), cached_displays.end(),
                     FindDisplayById(native_displays[i]->display_id()));

    if (it == cached_displays.end())
      continue;

    if (it->has_current_mode)
      OnConfigureNativeDisplay(it->display_id, it->current_mode, it->origin);
    else
      OnDisableNativeDisplay(it->display_id);
  }

  for (size_t i = 0; i < native_displays.size(); ++i)
    displays.push_back(GetDisplaySnapshotParams(*native_displays[i]));

  sender_->Send(new OzoneHostMsg_UpdateNativeDisplays(displays));
}

void DriGpuPlatformSupport::OnConfigureNativeDisplay(
    int64_t id,
    const DisplayMode_Params& mode_param,
    const gfx::Point& origin) {
  DisplaySnapshot* display = ndd_->FindDisplaySnapshot(id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << id;
    return;
  }

  const DisplayMode* mode = NULL;
  for (size_t i = 0; i < display->modes().size(); ++i) {
    if (mode_param.size == display->modes()[i]->size() &&
        mode_param.is_interlaced == display->modes()[i]->is_interlaced() &&
        mode_param.refresh_rate == display->modes()[i]->refresh_rate()) {
      mode = display->modes()[i];
      break;
    }
  }

  // If the display doesn't have the mode natively, then lookup the mode from
  // other displays and try using it on the current display (some displays
  // support panel fitting and they can use different modes even if the mode
  // isn't explicitly declared).
  if (!mode)
    mode = ndd_->FindDisplayMode(mode_param.size, mode_param.is_interlaced,
                                 mode_param.refresh_rate);

  if (!mode) {
    LOG(ERROR) << "Failed to find mode: size=" << mode_param.size.ToString()
               << " is_interlaced=" << mode_param.is_interlaced
               << " refresh_rate=" << mode_param.refresh_rate;
    return;
  }

  ndd_->Configure(*display, mode, origin);
}

void DriGpuPlatformSupport::OnDisableNativeDisplay(int64_t id) {
  DisplaySnapshot* display = ndd_->FindDisplaySnapshot(id);
  if (display)
    ndd_->Configure(*display, NULL, gfx::Point());
  else
    LOG(ERROR) << "There is no display with ID " << id;
}

void DriGpuPlatformSupport::OnTakeDisplayControl() {
  ndd_->TakeDisplayControl();
}

void DriGpuPlatformSupport::OnRelinquishDisplayControl() {
  ndd_->RelinquishDisplayControl();
}

}  // namespace ui
