// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridged_native_widget_host_impl.h"

#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"

namespace views {

BridgedNativeWidgetHostImpl::BridgedNativeWidgetHostImpl(
    NativeWidgetMac* parent)
    : native_widget_mac_(parent),
      bridge_(new BridgedNativeWidget(this, parent)) {}

BridgedNativeWidgetHostImpl::~BridgedNativeWidgetHostImpl() {
  // Destroy the bridge first to prevent any calls back into this during
  // destruction.
  // TODO(ccameron): When all communication from |bridge_| to this goes through
  // the BridgedNativeWidgetHost, this can be replaced with closing that pipe.
  bridge_.reset();
  DestroyCompositor();
}

void BridgedNativeWidgetHostImpl::CreateCompositor(
    const Widget::InitParams& params) {
  DCHECK(!compositor_);
  DCHECK(!layer());
  DCHECK(ViewsDelegate::GetInstance());

  // "Infer" must be handled by ViewsDelegate::OnBeforeWidgetInit().
  DCHECK_NE(Widget::InitParams::INFER_OPACITY, params.opacity);
  bool translucent = params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;

  // Create the layer.
  SetLayer(std::make_unique<ui::Layer>(params.layer_type));
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(!translucent);

  // Create the compositor and attach the layer to it.
  ui::ContextFactory* context_factory =
      ViewsDelegate::GetInstance()->GetContextFactory();
  DCHECK(context_factory);
  ui::ContextFactoryPrivate* context_factory_private =
      ViewsDelegate::GetInstance()->GetContextFactoryPrivate();
  compositor_ = ui::RecyclableCompositorMacFactory::Get()->CreateCompositor(
      context_factory, context_factory_private);
  compositor_->widget()->SetNSView(this);
  compositor_->compositor()->SetBackgroundColor(
      translucent ? SK_ColorTRANSPARENT : SK_ColorWHITE);
  compositor_->compositor()->SetRootLayer(layer());

  // The compositor is initially locked (prevented from producing frames), and
  // is only unlocked when the BridgedNativeWidget calls back via
  // SetCompositorVisibility.
  bridge_->InitCompositorView();
}

void BridgedNativeWidgetHostImpl::DestroyCompositor() {
  if (layer()) {
    // LayerOwner supports a change in ownership, e.g., to animate a closing
    // window, but that won't work as expected for the root layer in
    // BridgedNativeWidget.
    DCHECK_EQ(this, layer()->owner());
    layer()->CompleteAllAnimations();
    layer()->SuppressPaint();
    layer()->set_delegate(nullptr);
  }
  DestroyLayer();
  if (!compositor_)
    return;
  compositor_->widget()->ResetNSView();
  compositor_->compositor()->SetRootLayer(nullptr);
  ui::RecyclableCompositorMacFactory::Get()->RecycleCompositor(
      std::move(compositor_));
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, views::BridgedNativeWidgetHost:

void BridgedNativeWidgetHostImpl::SetCompositorSize(
    const gfx::Size& size_in_dip,
    float scale_factor) {
  layer()->SetBounds(gfx::Rect(size_in_dip));
  compositor_->UpdateSurface(ConvertSizeToPixel(scale_factor, size_in_dip),
                             scale_factor);
}

void BridgedNativeWidgetHostImpl::SetCompositorVisibility(bool window_visible) {
  layer()->SetVisible(window_visible);
  if (window_visible) {
    compositor_->Unsuspend();
    layer()->SchedulePaint(layer()->bounds());
  } else {
    compositor_->Suspend();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, LayerDelegate:

void BridgedNativeWidgetHostImpl::OnPaintLayer(
    const ui::PaintContext& context) {
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(context);
}

void BridgedNativeWidgetHostImpl::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  native_widget_mac_->GetWidget()->DeviceScaleFactorChanged(
      old_device_scale_factor, new_device_scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, AcceleratedWidgetMac:

void BridgedNativeWidgetHostImpl::AcceleratedWidgetCALayerParamsUpdated() {
  const gfx::CALayerParams* ca_layer_params =
      compositor_->widget()->GetCALayerParams();
  if (ca_layer_params)
    bridge_->SetCALayerParams(*ca_layer_params);
}

}  // namespace views
