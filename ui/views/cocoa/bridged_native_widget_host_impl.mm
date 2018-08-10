// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridged_native_widget_host_impl.h"

#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/word_lookup_client.h"

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
  SetFocusManager(nullptr);
  DestroyCompositor();
}

void BridgedNativeWidgetHostImpl::SetRootView(views::View* root_view) {
  root_view_ = root_view;
  // TODO(ccameron): The BridgedNativeWidget should not need to know its root
  // view.
  bridge_->SetRootView(root_view);
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

void BridgedNativeWidgetHostImpl::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_) {
    // Only the destructor can replace the focus manager (and it passes null).
    DCHECK(!focus_manager);
    if (View* old_focus = focus_manager_->GetFocusedView())
      OnDidChangeFocus(old_focus, nullptr);
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
    return;
  }

  focus_manager_ = focus_manager;
  focus_manager_->AddFocusChangeListener(this);
  if (View* new_focus = focus_manager_->GetFocusedView())
    OnDidChangeFocus(nullptr, new_focus);
}

ui::InputMethod* BridgedNativeWidgetHostImpl::GetInputMethod() {
  if (!input_method_) {
    input_method_ = ui::CreateInputMethod(this, gfx::kNullAcceleratedWidget);
    // For now, use always-focused mode on Mac for the input method.
    // TODO(tapted): Move this to OnWindowKeyStatusChangedTo() and balance.
    input_method_->OnFocus();
  }
  return input_method_.get();
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

void BridgedNativeWidgetHostImpl::OnScrollEvent(
    const ui::ScrollEvent& const_event) {
  ui::ScrollEvent event = const_event;
  root_view_->GetWidget()->OnScrollEvent(&event);
}

void BridgedNativeWidgetHostImpl::OnMouseEvent(
    const ui::MouseEvent& const_event) {
  ui::MouseEvent event = const_event;
  root_view_->GetWidget()->OnMouseEvent(&event);
}

void BridgedNativeWidgetHostImpl::OnGestureEvent(
    const ui::GestureEvent& const_event) {
  ui::GestureEvent event = const_event;
  root_view_->GetWidget()->OnGestureEvent(&event);
}

void BridgedNativeWidgetHostImpl::SetSize(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

void BridgedNativeWidgetHostImpl::SetKeyboardAccessible(bool enabled) {
  views::FocusManager* focus_manager =
      root_view_->GetWidget()->GetFocusManager();
  if (focus_manager)
    focus_manager->SetKeyboardAccessible(enabled);
}

void BridgedNativeWidgetHostImpl::SetIsFirstResponder(bool is_first_responder) {
  if (is_first_responder)
    root_view_->GetWidget()->GetFocusManager()->RestoreFocusedView();
  else
    root_view_->GetWidget()->GetFocusManager()->StoreFocusedView(true);
}

void BridgedNativeWidgetHostImpl::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    bool* is_draggable_background) {
  int component =
      root_view_->GetWidget()->GetNonClientComponent(location_in_content);
  *is_draggable_background = component == HTCAPTION;
}

void BridgedNativeWidgetHostImpl::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    base::string16* new_tooltip_text) {
  views::View* view =
      root_view_->GetTooltipHandlerForPoint(location_in_content);
  if (view) {
    gfx::Point view_point = location_in_content;
    views::View::ConvertPointToScreen(root_view_, &view_point);
    views::View::ConvertPointFromScreen(view, &view_point);
    if (!view->GetTooltipText(view_point, new_tooltip_text))
      DCHECK(new_tooltip_text->empty());
  }
}

void BridgedNativeWidgetHostImpl::GetWordAt(
    const gfx::Point& location_in_content,
    bool* found_word,
    gfx::DecoratedText* decorated_word,
    gfx::Point* baseline_point) {
  *found_word = false;

  views::View* target =
      root_view_->GetEventHandlerForPoint(location_in_content);
  if (!target)
    return;

  views::WordLookupClient* word_lookup_client = target->GetWordLookupClient();
  if (!word_lookup_client)
    return;

  gfx::Point location_in_target = location_in_content;
  views::View::ConvertPointToTarget(root_view_, target, &location_in_target);
  if (!word_lookup_client->GetWordLookupDataAtPoint(
          location_in_target, decorated_word, baseline_point)) {
    return;
  }

  // Convert |baselinePoint| to the coordinate system of |root_view_|.
  views::View::ConvertPointToTarget(target, root_view_, baseline_point);
  *found_word = true;
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, FocusChangeListener:

void BridgedNativeWidgetHostImpl::OnWillChangeFocus(View* focused_before,
                                                    View* focused_now) {}

void BridgedNativeWidgetHostImpl::OnDidChangeFocus(View* focused_before,
                                                   View* focused_now) {
  ui::InputMethod* input_method =
      native_widget_mac_->GetWidget()->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* input_client = input_method->GetTextInputClient();
    // Sanity check: When focus moves away from the widget (i.e. |focused_now|
    // is nil), then the textInputClient will be cleared.
    DCHECK(!!focused_now || !input_client);
    bridge_->SetTextInputClient(input_client);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, internal::InputMethodDelegate:

ui::EventDispatchDetails BridgedNativeWidgetHostImpl::DispatchKeyEventPostIME(
    ui::KeyEvent* key) {
  DCHECK(focus_manager_);
  if (!focus_manager_->OnKeyEvent(*key))
    key->StopPropagation();
  else
    native_widget_mac_->GetWidget()->OnKeyEvent(key);
  return ui::EventDispatchDetails();
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
