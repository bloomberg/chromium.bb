// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/display_message_handler.h"

#include "ui/display/types/chromeos/display_mode.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#include "ui/ozone/common/chromeos/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"

namespace ui {

DisplayMessageHandler::DisplayMessageHandler(
    scoped_ptr<NativeDisplayDelegateDri> ndd)
    : sender_(NULL),
      ndd_(ndd.Pass()) {}

DisplayMessageHandler::~DisplayMessageHandler() {}

void DisplayMessageHandler::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;
}

bool DisplayMessageHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DisplayMessageHandler, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ForceDPMSOn, OnForceDPMSOn)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RefreshNativeDisplays,
                      OnRefreshNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ConfigureNativeDisplay,
                      OnConfigureNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DisableNativeDisplay,
                      OnDisableNativeDisplay)
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DisplayMessageHandler::OnForceDPMSOn() {
  ndd_->ForceDPMSOn();
}

void DisplayMessageHandler::OnRefreshNativeDisplays() {
  std::vector<DisplaySnapshot_Params> displays;
  std::vector<DisplaySnapshot*> native_displays = ndd_->GetDisplays();
  for (size_t i = 0; i < native_displays.size(); ++i)
    displays.push_back(GetDisplaySnapshotParams(*native_displays[i]));

  sender_->Send(new OzoneHostMsg_UpdateNativeDisplays(displays));
}

void DisplayMessageHandler::OnConfigureNativeDisplay(
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
    mode = ndd_->FindDisplayMode(mode_param.size,
                                 mode_param.is_interlaced,
                                 mode_param.refresh_rate);

  if (!mode) {
    LOG(ERROR) << "Failed to find mode: size=" << mode_param.size.ToString()
               << " is_interlaced=" << mode_param.is_interlaced
               << " refresh_rate=" << mode_param.refresh_rate;
    return;
  }

  ndd_->Configure(*display, mode, origin);
}

void DisplayMessageHandler::OnDisableNativeDisplay(int64_t id) {
  DisplaySnapshot* display = ndd_->FindDisplaySnapshot(id);
  if (display)
    ndd_->Configure(*display, NULL, gfx::Point());
  else
    LOG(ERROR) << "There is no display with ID " << id;
}

}  // namespace ui
