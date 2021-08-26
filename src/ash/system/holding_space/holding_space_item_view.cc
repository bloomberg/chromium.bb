// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/bind.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A UI class property used to identify if a view is an instance of
// `HoldingSpaceItemView`. Class name is not an adequate identifier as it may be
// overridden by subclasses.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsHoldingSpaceItemViewProperty, false)

// Appearance.
constexpr size_t kCheckmarkBackgroundSize = 18;

// Helpers ---------------------------------------------------------------------

// Schedules repaint of `layer`.
void InvalidateLayer(ui::Layer* layer) {
  layer->SchedulePaint(gfx::Rect(layer->size()));
}

// CallbackPainter -------------------------------------------------------------

// A painter which delegates painting to a callback.
class CallbackPainter : public views::Painter {
 public:
  using Callback = base::RepeatingCallback<void(gfx::Canvas*, gfx::Size)>;

  CallbackPainter(const CallbackPainter&) = delete;
  CallbackPainter& operator=(const CallbackPainter&) = delete;
  ~CallbackPainter() override = default;

  // Creates a painted layer which delegates painting to `callback`.
  static std::unique_ptr<ui::LayerOwner> CreatePaintedLayer(Callback callback) {
    auto owner = views::Painter::CreatePaintedLayer(
        base::WrapUnique(new CallbackPainter(callback)));
    owner->layer()->SetFillsBoundsOpaquely(false);
    return owner;
  }

 private:
  explicit CallbackPainter(Callback callback) : callback_(callback) {}

  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    callback_.Run(canvas, size);
  }

  Callback callback_;
};

// MinimumSizableView ---------------------------------------------------------

// A view which respects a minimum size restriction.
class MinimumSizableView : public views::View {
 public:
  explicit MinimumSizableView(const gfx::Size& min_size)
      : min_size_(min_size) {}

  MinimumSizableView(const MinimumSizableView&) = delete;
  MinimumSizableView& operator=(const MinimumSizableView&) = delete;
  ~MinimumSizableView() override = default;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size preferred_size(views::View::CalculatePreferredSize());
    preferred_size.SetToMax(min_size_);
    return preferred_size;
  }

  int GetHeightForWidth(int width) const override {
    return std::max(views::View::GetHeightForWidth(width), min_size_.height());
  }

  const gfx::Size min_size_;
};

}  // namespace

// HoldingSpaceItemView --------------------------------------------------------

HoldingSpaceItemView::HoldingSpaceItemView(HoldingSpaceViewDelegate* delegate,
                                           const HoldingSpaceItem* item)
    : delegate_(delegate), item_(item), item_id_(item->id()) {
  // Subscribe to be notified of `item_` deletion. Note that it is safe to use a
  // raw pointer here since `this` owns the callback.
  item_deletion_subscription_ = item_->AddDeletionCallback(base::BindRepeating(
      [](HoldingSpaceItemView* view) { view->item_ = nullptr; },
      base::Unretained(this)));

  model_observer_.Observe(HoldingSpaceController::Get()->model());

  SetProperty(kIsHoldingSpaceItemViewProperty, true);

  set_context_menu_controller(delegate_);
  set_drag_controller(delegate_);

  SetNotifyEnterExitOnChild(true);

  // Accessibility.
  GetViewAccessibility().OverrideName(item->GetText());
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kListItem);

  // Layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Focus.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  focused_layer_owner_ =
      CallbackPainter::CreatePaintedLayer(base::BindRepeating(
          &HoldingSpaceItemView::OnPaintFocus, base::Unretained(this)));
  layer()->Add(focused_layer_owner_->layer());

  // Selection.
  selected_layer_owner_ =
      CallbackPainter::CreatePaintedLayer(base::BindRepeating(
          &HoldingSpaceItemView::OnPaintSelect, base::Unretained(this)));
  layer()->Add(selected_layer_owner_->layer());

  // This view's `selected_` state is represented differently depending on
  // `delegate_`'s selection UI. Register to be notified of changes.
  selection_ui_changed_subscription_ =
      delegate_->AddSelectionUiChangedCallback(base::BindRepeating(
          &HoldingSpaceItemView::OnSelectionUiChanged, base::Unretained(this)));

  delegate_->OnHoldingSpaceItemViewCreated(this);
}

HoldingSpaceItemView::~HoldingSpaceItemView() {
  if (delegate_)
    delegate_->OnHoldingSpaceItemViewDestroying(this);
}

// static
HoldingSpaceItemView* HoldingSpaceItemView::Cast(views::View* view) {
  return const_cast<HoldingSpaceItemView*>(
      Cast(const_cast<const views::View*>(view)));
}

// static
const HoldingSpaceItemView* HoldingSpaceItemView::Cast(
    const views::View* view) {
  DCHECK(HoldingSpaceItemView::IsInstance(view));
  return static_cast<const HoldingSpaceItemView*>(view);
}

// static
bool HoldingSpaceItemView::IsInstance(const views::View* view) {
  return view->GetProperty(kIsHoldingSpaceItemViewProperty);
}

void HoldingSpaceItemView::Reset() {
  delegate_ = nullptr;
}

bool HoldingSpaceItemView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return (delegate_ && delegate_->OnHoldingSpaceItemViewAccessibleAction(
                           this, action_data)) ||
         views::View::HandleAccessibleAction(action_data);
}

void HoldingSpaceItemView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = GetLocalBounds();

  // Selection ring.
  selected_layer_owner_->layer()->SetBounds(bounds);
  InvalidateLayer(selected_layer_owner_->layer());

  // Focus ring.
  // NOTE: The focus ring is painted just outside the bounds for this view.
  bounds.Inset(gfx::Insets(kHoldingSpaceFocusInsets));
  focused_layer_owner_->layer()->SetBounds(bounds);
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnFocus() {
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnBlur() {
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (delegate_ && delegate_->OnHoldingSpaceItemViewGestureEvent(this, *event))
    event->SetHandled();
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  return delegate_ && delegate_->OnHoldingSpaceItemViewKeyPressed(this, event);
}

void HoldingSpaceItemView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdatePrimaryAction();
      break;
    default:
      break;
  }
  views::View::OnMouseEvent(event);
}

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  return delegate_ &&
         delegate_->OnHoldingSpaceItemViewMousePressed(this, event);
}

void HoldingSpaceItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (delegate_)
    delegate_->OnHoldingSpaceItemViewMouseReleased(this, event);
}

void HoldingSpaceItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  AshColorProvider* const ash_color_provider = AshColorProvider::Get();

  // Background.
  SetBackground(views::CreateRoundedRectBackground(
      ash_color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kHoldingSpaceCornerRadius));

  // Checkmark.
  checkmark_->SetBackground(holding_space_util::CreateCircleBackground(
      ash_color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor),
      kCheckmarkBackgroundSize));
  checkmark_->SetImage(gfx::CreateVectorIcon(
      kCheckIcon, kHoldingSpaceIconSize,
      ash_color_provider->IsDarkModeEnabled() ? gfx::kGoogleGrey900
                                              : SK_ColorWHITE));

  // Focused/selected layers.
  InvalidateLayer(focused_layer_owner_->layer());
  InvalidateLayer(selected_layer_owner_->layer());

  if (!primary_action_container_)
    return;

  // Cancel.
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  primary_action_cancel_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kCancelIcon, kHoldingSpaceIconSize, icon_color));

  // Pin.
  const gfx::ImageSkia unpinned_icon = gfx::CreateVectorIcon(
      views::kUnpinIcon, kHoldingSpaceIconSize, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, kHoldingSpaceIconSize, icon_color);
  primary_action_pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  primary_action_pin_->SetToggledImage(views::Button::STATE_NORMAL,
                                       &pinned_icon);
}

void HoldingSpaceItemView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    uint32_t updated_fields) {
  if (item_ == item) {
    GetViewAccessibility().OverrideName(item->GetText());
    UpdatePrimaryAction();
  }
}

void HoldingSpaceItemView::StartDrag(const ui::LocatedEvent& event,
                                     ui::mojom::DragEventSource source) {
  int drag_operations = GetDragOperations(event.location());
  if (drag_operations == ui::DragDropTypes::DRAG_NONE)
    return;

  views::Widget* widget = GetWidget();
  DCHECK(widget);

  if (widget->dragged_view())
    return;

  auto data = std::make_unique<ui::OSExchangeData>();
  WriteDragData(event.location(), data.get());

  gfx::Point widget_location(event.location());
  views::View::ConvertPointToWidget(this, &widget_location);
  widget->RunShellDrag(this, std::move(data), widget_location, drag_operations,
                       source);
}

void HoldingSpaceItemView::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;
  InvalidateLayer(selected_layer_owner_->layer());

  if (delegate_)
    delegate_->OnHoldingSpaceItemViewSelectedChanged(this);

  OnSelectionUiChanged();
}

views::Builder<views::ImageView>
HoldingSpaceItemView::CreateCheckmarkBuilder() {
  DCHECK(!checkmark_);
  auto checkmark = views::Builder<views::ImageView>();
  checkmark.CopyAddressTo(&checkmark_)
      .SetID(kHoldingSpaceItemCheckmarkId)
      .SetVisible(selected());
  return checkmark;
}

views::Builder<views::View> HoldingSpaceItemView::CreatePrimaryActionBuilder(
    const gfx::Size& min_size) {
  DCHECK(!primary_action_container_);
  DCHECK(!primary_action_cancel_);
  DCHECK(!primary_action_pin_);

  using HorizontalAlignment = views::ImageButton::HorizontalAlignment;
  using VerticalAlignment = views::ImageButton::VerticalAlignment;

  gfx::Size preferred_size(kHoldingSpaceIconSize, kHoldingSpaceIconSize);
  preferred_size.SetToMax(min_size);

  auto primary_action = views::Builder<views::View>();
  primary_action.CopyAddressTo(&primary_action_container_)
      .SetID(kHoldingSpaceItemPrimaryActionContainerId)
      .SetUseDefaultFillLayout(true)
      .SetVisible(false)
      .AddChild(
          views::Builder<views::ImageButton>()
              .CopyAddressTo(&primary_action_cancel_)
              .SetID(kHoldingSpaceItemCancelButtonId)
              .SetCallback(base::BindRepeating(
                  &HoldingSpaceItemView::OnPrimaryActionPressed,
                  base::Unretained(this)))
              .SetFocusBehavior(views::View::FocusBehavior::NEVER)
              .SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER)
              .SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE)
              .SetPreferredSize(preferred_size)
              .SetVisible(false))
      .AddChild(
          views::Builder<views::ToggleImageButton>()
              .CopyAddressTo(&primary_action_pin_)
              .SetID(kHoldingSpaceItemPinButtonId)
              .SetCallback(base::BindRepeating(
                  &HoldingSpaceItemView::OnPrimaryActionPressed,
                  base::Unretained(this)))
              .SetFocusBehavior(views::View::FocusBehavior::NEVER)
              .SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER)
              .SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE)
              .SetPreferredSize(preferred_size)
              .SetVisible(false));
  return primary_action;
}

void HoldingSpaceItemView::OnSelectionUiChanged() {
  const bool multiselect =
      delegate_ && delegate_->selection_ui() ==
                       HoldingSpaceViewDelegate::SelectionUi::kMultiSelect;

  checkmark_->SetVisible(selected() && multiselect);
}

void HoldingSpaceItemView::OnPaintFocus(gfx::Canvas* canvas, gfx::Size size) {
  if (!HasFocus())
    return;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  flags.setStrokeWidth(views::FocusRing::kHaloThickness);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  gfx::Rect bounds = gfx::Rect(size);
  bounds.Inset(gfx::Insets(flags.getStrokeWidth() / 2));
  canvas->DrawRoundRect(bounds, kHoldingSpaceFocusCornerRadius, flags);
}

void HoldingSpaceItemView::OnPaintSelect(gfx::Canvas* canvas, gfx::Size size) {
  if (!selected_)
    return;

  const SkColor color =
      SkColorSetA(AshColorProvider::Get()->GetControlsLayerColor(
                      AshColorProvider::ControlsLayerType::kFocusRingColor),
                  kHoldingSpaceSelectedOverlayOpacity * 0xFF);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);

  canvas->DrawRoundRect(gfx::Rect(size), kHoldingSpaceCornerRadius, flags);
}

void HoldingSpaceItemView::OnPrimaryActionPressed() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  DCHECK_NE(primary_action_cancel_->GetVisible(),
            primary_action_pin_->GetVisible());

  if (delegate())
    delegate()->OnHoldingSpaceItemViewPrimaryActionPressed(this);

  // Cancel.
  if (primary_action_cancel_->GetVisible()) {
    HoldingSpaceController::Get()->client()->CancelItems({item()});
    return;
  }

  // Pin.
  const bool is_item_pinned =
      HoldingSpaceController::Get()->model()->ContainsItem(
          HoldingSpaceItem::Type::kPinnedFile, item()->file_path());

  // Unpinning `item()` may result in the destruction of this view.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  if (is_item_pinned)
    HoldingSpaceController::Get()->client()->UnpinItems({item()});
  else
    HoldingSpaceController::Get()->client()->PinItems({item()});

  if (weak_ptr)
    UpdatePrimaryAction();
}

void HoldingSpaceItemView::UpdatePrimaryAction() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  if (!IsMouseHovered()) {
    primary_action_container_->SetVisible(false);
    OnPrimaryActionVisibilityChanged(false);
    return;
  }

  // Cancel.
  const bool is_item_in_progress = !item()->progress().IsComplete();
  primary_action_cancel_->SetVisible(is_item_in_progress);

  // Pin.
  const bool is_item_pinned =
      HoldingSpaceController::Get()->model()->ContainsItem(
          HoldingSpaceItem::Type::kPinnedFile, item()->file_path());
  primary_action_pin_->SetToggled(!is_item_pinned);
  primary_action_pin_->SetVisible(!is_item_in_progress);

  primary_action_container_->SetVisible(true);
  OnPrimaryActionVisibilityChanged(true);
}

BEGIN_METADATA(HoldingSpaceItemView, views::View)
END_METADATA

}  // namespace ash
