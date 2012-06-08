// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_ROOT_WINDOW_CAPTURE_CLIENT_H_
#define UI_AURA_SHARED_ROOT_WINDOW_CAPTURE_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/aura_export.h"

namespace aura {
class RootWindow;

namespace shared {

class AURA_EXPORT RootWindowCaptureClient : public client::CaptureClient {
 public:
  explicit RootWindowCaptureClient(RootWindow* root_window);
  virtual ~RootWindowCaptureClient();

  // Overridden from client::CaptureClient:
  virtual void SetCapture(Window* window) OVERRIDE;
  virtual void ReleaseCapture(Window* window) OVERRIDE;
  virtual Window* GetCaptureWindow() OVERRIDE;

 private:
  RootWindow* root_window_;
  Window* capture_window_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowCaptureClient);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_SHARED_ROOT_WINDOW_CAPTURE_CLIENT_H_
