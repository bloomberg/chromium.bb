// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_CAPTURE_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_CAPTURE_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/capture_client.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopCaptureClient : public aura::client::CaptureClient {
 public:
  DesktopCaptureClient();
  virtual ~DesktopCaptureClient();

 private:
  // Overridden from aura::client::CaptureClient:
  virtual void SetCapture(aura::Window* window) OVERRIDE;
  virtual void ReleaseCapture(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetCaptureWindow() OVERRIDE;

  aura::Window* capture_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_CAPTURE_CLIENT_H_
