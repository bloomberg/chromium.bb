// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridge_factory_impl.h"

#include "base/no_destructor.h"
#include "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/cocoa/bridged_native_widget_host.h"

namespace views_bridge_mac {

using views::BridgedNativeWidgetImpl;
using views::BridgedNativeWidgetHostHelper;

namespace {

class Bridge : public BridgedNativeWidgetHostHelper {
 public:
  Bridge(uint64_t bridge_id,
         mojom::BridgedNativeWidgetHostPtr host_ptr,
         mojom::BridgedNativeWidgetRequest bridge_request) {
    host_ptr_ = std::move(host_ptr);
    bridge_impl_ = std::make_unique<BridgedNativeWidgetImpl>(
        bridge_id, host_ptr_.get(), this);
    bridge_impl_->BindRequest(std::move(bridge_request));
  }
  ~Bridge() override {}

  // BridgedNativeWidgetHostHelper:
  NSView* GetNativeViewAccessible() override { return nil; }
  void DispatchKeyEvent(ui::KeyEvent* event) override {}
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override {
    return false;
  }
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override {
    *found_word = false;
  }
  double SheetPositionY() override { return 0; }

  mojom::BridgedNativeWidgetHostPtr host_ptr_;
  std::unique_ptr<BridgedNativeWidgetImpl> bridge_impl_;
};

std::map<uint64_t, std::unique_ptr<Bridge>>& GetBridgeMap() {
  static base::NoDestructor<std::map<uint64_t, std::unique_ptr<Bridge>>> map;
  return *map;
}

}  // namespace

// static
BridgeFactoryImpl* BridgeFactoryImpl::Get() {
  static base::NoDestructor<BridgeFactoryImpl> factory;
  return factory.get();
}

void BridgeFactoryImpl::CreateBridge(
    uint64_t bridge_id,
    mojom::BridgedNativeWidgetRequest bridge_request,
    mojom::BridgedNativeWidgetHostPtr host_ptr) {
  GetBridgeMap()[bridge_id] = std::make_unique<Bridge>(
      bridge_id, std::move(host_ptr), std::move(bridge_request));
}

void BridgeFactoryImpl::DestroyBridge(uint64_t bridge_id) {
  GetBridgeMap().erase(bridge_id);
}

}  // namespace views_bridge_mac
