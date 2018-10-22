// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDER_LOOP_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_RENDER_LOOP_BROWSER_INTERFACE_H_

#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

// RenderLoop talks to the browser main thread through this interface.
class RenderLoopBrowserInterface {
 public:
  virtual ~RenderLoopBrowserInterface() = default;

  virtual void ForceExitVr() = 0;
  virtual void ReportUiActivityResultForTesting(
      const VrUiTestActivityResult& result) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDER_LOOP_BROWSER_INTERFACE_H_
