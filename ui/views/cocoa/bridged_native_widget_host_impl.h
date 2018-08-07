// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/cocoa/bridged_native_widget_host.h"
#include "ui/views/views_export.h"

namespace views {

class BridgedNativeWidget;
class NativeWidgetMac;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the BridgedNativeWidget, which interacts with the Cocoa
// APIs, and which may live in an app shim process.
class VIEWS_EXPORT BridgedNativeWidgetHostImpl
    : public BridgedNativeWidgetHost {
 public:
  // Creates one side of the bridge. |parent| must not be NULL.
  explicit BridgedNativeWidgetHostImpl(NativeWidgetMac* parent);
  ~BridgedNativeWidgetHostImpl() override;

  // Provide direct access to the BridgedNativeWidget that this is hosting.
  // TODO(ccameron): Remove all accesses to this member, and replace them
  // with methods that may be sent across processes.
  BridgedNativeWidget* bridge() const { return bridge_.get(); }

 private:
  // TODO(ccameron): Rather than instantiate a BridgedNativeWidget here,
  // we will instantiate a mojo BridgedNativeWidget interface to a Cocoa
  // instance that may be in another process.
  std::unique_ptr<BridgedNativeWidget> bridge_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetHostImpl);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
