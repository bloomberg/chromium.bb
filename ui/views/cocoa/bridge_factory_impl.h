// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGE_FACTORY_IMPL_H_
#define UI_VIEWS_COCOA_BRIDGE_FACTORY_IMPL_H_

#include "ui/views/views_export.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget.mojom.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"

// TODO(ccameron): This file is to be moved to /ui/views_bridge_mac when
// possible. For now, put it in the namespace of that path.
namespace views_bridge_mac {

// The factory that creates BridgedNativeWidget instances. This object is to
// be instantiated in app shim processes.
class VIEWS_EXPORT BridgeFactoryImpl : public mojom::BridgeFactory {
 public:
  static BridgeFactoryImpl* Get();
  void CreateBridge(uint64_t bridge_id,
                    mojom::BridgedNativeWidgetRequest bridge_request,
                    mojom::BridgedNativeWidgetHostPtr host) override;
  void DestroyBridge(uint64_t bridge_id) override;
};

}  // namespace views_bridge_mac

#endif  // UI_VIEWS_COCOA_BRIDGE_FACTORY_IMPL_H_
