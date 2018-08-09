// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/cocoa/bridged_native_widget_host.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

namespace ui {
class RecyclableCompositorMac;
}

namespace views {

class BridgedNativeWidget;
class NativeWidgetMac;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the BridgedNativeWidget, which interacts with the Cocoa
// APIs, and which may live in an app shim process.
class VIEWS_EXPORT BridgedNativeWidgetHostImpl
    : public BridgedNativeWidgetHost,
      public ui::LayerDelegate,
      public ui::LayerOwner,
      public ui::AcceleratedWidgetMacNSView {
 public:
  // Creates one side of the bridge. |parent| must not be NULL.
  explicit BridgedNativeWidgetHostImpl(NativeWidgetMac* parent);
  ~BridgedNativeWidgetHostImpl() override;

  // Provide direct access to the BridgedNativeWidget that this is hosting.
  // TODO(ccameron): Remove all accesses to this member, and replace them
  // with methods that may be sent across processes.
  BridgedNativeWidget* bridge() const { return bridge_.get(); }

  // Set the root view (set during initialization and un-set during teardown).
  void SetRootView(views::View* root_view);

  // Initialize the ui::Compositor and ui::Layer.
  void CreateCompositor(const Widget::InitParams& params);

 private:
  void DestroyCompositor();

  // views::BridgedNativeWidgetHost:
  void SetCompositorSize(const gfx::Size& size_in_dip,
                         float scale_factor) override;
  void SetCompositorVisibility(bool visible) override;
  void SetSize(const gfx::Size& new_size) override;
  void SetKeyboardAccessible(bool enabled) override;
  void SetIsFirstResponder(bool is_first_responder) override;
  void OnScrollEvent(const ui::ScrollEvent& const_event) override;
  void OnMouseEvent(const ui::MouseEvent& const_event) override;
  void OnGestureEvent(const ui::GestureEvent& const_event) override;
  void GetIsDraggableBackgroundAt(const gfx::Point& location_in_content,
                                  bool* is_draggable_background) override;
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        base::string16* new_tooltip_text) override;
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::AcceleratedWidgetMacNSView:
  void AcceleratedWidgetCALayerParamsUpdated() override;

  views::NativeWidgetMac* const native_widget_mac_;  // Weak. Owns |this_|.

  views::View* root_view_ = nullptr;  // Weak. Owned by |native_widget_mac_|.

  // TODO(ccameron): Rather than instantiate a BridgedNativeWidget here,
  // we will instantiate a mojo BridgedNativeWidget interface to a Cocoa
  // instance that may be in another process.
  std::unique_ptr<BridgedNativeWidget> bridge_;

  std::unique_ptr<ui::RecyclableCompositorMac> compositor_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetHostImpl);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
