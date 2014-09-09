// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MESSAGE_HANDLER_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MESSAGE_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/ozone/public/gpu_platform_support.h"

namespace gfx {
class Point;
}

namespace ui {

class NativeDisplayDelegateDri;

struct DisplayMode_Params;
struct DisplaySnapshot_Params;

class DisplayMessageHandler : public GpuPlatformSupport {
 public:
  DisplayMessageHandler(scoped_ptr<NativeDisplayDelegateDri> ndd);
  virtual ~DisplayMessageHandler();

  // GpuPlatformSupport:
  virtual void OnChannelEstablished(IPC::Sender* sender) OVERRIDE;

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnForceDPMSOn();
  void OnRefreshNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& cached_displays);
  void OnConfigureNativeDisplay(int64_t id,
                                const DisplayMode_Params& mode,
                                const gfx::Point& origin);
  void OnDisableNativeDisplay(int64_t id);

  IPC::Sender* sender_;
  scoped_ptr<NativeDisplayDelegateDri> ndd_;

  DISALLOW_COPY_AND_ASSIGN(DisplayMessageHandler);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MESSAGE_HANDLER_H_
