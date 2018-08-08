// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_

#include "ui/views/views_export.h"

namespace views {

// The interface through which the app shim (BridgedNativeWidgetImpl)
// communicates with the browser process (BridgedNativeWidgetHostImpl).
class VIEWS_EXPORT BridgedNativeWidgetHost {
 public:
  virtual ~BridgedNativeWidgetHost() = default;

  // Update the ui::Compositor and ui::Layer's size.
  virtual void SetCompositorSize(const gfx::Size& size_in_dip,
                                 float scale_factor) = 0;

  // Update the ui::Compositor and ui::Layer's visibility.
  virtual void SetCompositorVisibility(bool visible) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_
