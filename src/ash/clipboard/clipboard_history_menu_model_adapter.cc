// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
bool IsDataReadAllowed(const ui::DataTransferEndpoint* source,
                       const ui::DataTransferEndpoint* destination) {
  ui::DataTransferPolicyController* policy_controller =
      ui::DataTransferPolicyController::Get();
  if (!policy_controller)
    return true;
  return policy_controller->IsDataReadAllowed(source, destination);
}
}  // namespace

// static
std::unique_ptr<ClipboardHistoryMenuModelAdapter>
ClipboardHistoryMenuModelAdapter::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager) {
  return base::WrapUnique(new ClipboardHistoryMenuModelAdapter(
      std::make_unique<ui::SimpleMenuModel>(delegate),
      std::move(menu_closed_callback), clipboard_history, resource_manager));
}

ClipboardHistoryMenuModelAdapter::~ClipboardHistoryMenuModelAdapter() = default;

void ClipboardHistoryMenuModelAdapter::Run(
    const gfx::Rect& anchor_rect,
    views::MenuAnchorPosition menu_anchor_position,
    ui::MenuSourceType source_type) {
  DCHECK(!root_view_);
  DCHECK(model_);
  DCHECK(item_snapshots_.empty());
  DCHECK(item_views_by_command_id_.empty());

  menu_open_time_ = base::TimeTicks::Now();

  int command_id = ClipboardHistoryUtil::kFirstItemCommandId;
  const auto& items = clipboard_history_->GetItems();
  // Do not include the final kDeleteCommandId item in histograms, because it is
  // not shown.
  UMA_HISTOGRAM_COUNTS_100(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", items.size());

  const ui::DataTransferEndpoint data_dst(ui::EndpointType::kDefault,
                                          /*notify_if_restricted=*/false);
  for (const auto& item : items) {
    model_->AddItem(command_id, base::string16());

    // Enable or disable the command depending on whether its corresponding
    // clipboard history item is allowed to read or not.
    // This clipboard read isn't initiated by the user, that's why it shouldn't
    // notify if the clipboard is restricted.
    model_->SetEnabledAt(model_->GetIndexOfCommandId(command_id),
                         IsDataReadAllowed(item.data().source(), &data_dst));

    item_snapshots_.emplace(command_id, item);
    ++command_id;
  }

  // Start async rendering of HTML, if any exists.
  ClipboardImageModelFactory::Get()->Activate();

  root_view_ = CreateMenu();
  root_view_->SetTitle(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_TITLE));
  menu_runner_ = std::make_unique<views::MenuRunner>(
      root_view_, views::MenuRunner::CONTEXT_MENU |
                      views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                      views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(
      /*widget_owner=*/nullptr, /*menu_button_controller=*/nullptr, anchor_rect,
      menu_anchor_position, source_type);
}

bool ClipboardHistoryMenuModelAdapter::IsRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ClipboardHistoryMenuModelAdapter::Cancel() {
  DCHECK(menu_runner_);
  menu_runner_->Cancel();
}

base::Optional<int>
ClipboardHistoryMenuModelAdapter::GetSelectedMenuItemCommand() const {
  DCHECK(root_view_);

  // `root_view_` may be selected if no menu item is under selection.
  auto* menu_item = root_view_->GetMenuController()->GetSelectedMenuItem();
  return menu_item && menu_item != root_view_
             ? base::make_optional(menu_item->GetCommand())
             : base::nullopt;
}

const ClipboardHistoryItem&
ClipboardHistoryMenuModelAdapter::GetItemFromCommandId(int command_id) const {
  auto iter = item_snapshots_.find(command_id);
  DCHECK(iter != item_snapshots_.cend());
  return iter->second;
}

int ClipboardHistoryMenuModelAdapter::GetMenuItemsCount() const {
  // We should not use `root_view_` to retrieve the item count. Because the
  // menu item view is removed from `root_view_` asynchronously.
  return item_views_by_command_id_.size();
}

void ClipboardHistoryMenuModelAdapter::SelectMenuItemWithCommandId(
    int command_id) {
  views::MenuItemView* selected_menu_item =
      root_view_->GetMenuItemByID(command_id);
  DCHECK(IsRunning());
  views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
      selected_menu_item);
}

void ClipboardHistoryMenuModelAdapter::RemoveMenuItemWithCommandId(
    int command_id) {
  // Calculate `new_selected_command_id` before removing
  // the item specified by `command_id` from data structures because the item to
  // be removed is needed in calculation.
  base::Optional<int> new_selected_command_id =
      CalculateSelectedCommandIdAfterDeletion(command_id);

  // Update the menu item selection.
  if (new_selected_command_id.has_value()) {
    SelectMenuItemWithCommandId(*new_selected_command_id);
  } else {
    views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
        root_view_);
  }

  auto item_view_to_delete = item_views_by_command_id_.find(command_id);
  DCHECK(item_view_to_delete != item_views_by_command_id_.cend());

  // Disable views to be removed in order to prevent them from handling events.
  root_view_->GetMenuItemByID(command_id)->SetEnabled(false);
  item_view_to_delete->second->SetEnabled(false);

  item_views_by_command_id_.erase(item_view_to_delete);

  auto item_to_delete = item_snapshots_.find(command_id);
  DCHECK(item_to_delete != item_snapshots_.end());
  item_snapshots_.erase(item_to_delete);

  // The current selected menu item may be accessed after item deletion. So
  // postpone the menu item deletion.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistoryMenuModelAdapter::RemoveItemView,
                     weak_ptr_factory_.GetWeakPtr(), command_id));
}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocus(bool reverse) {
  base::Optional<int> selected_command = GetSelectedMenuItemCommand();

  // If no item is selected, select the topmost or bottom menu item depending
  // on the focus move direction.
  if (!selected_command.has_value()) {
    SelectMenuItemWithCommandId(
        reverse ? item_views_by_command_id_.rbegin()->first
                : ClipboardHistoryUtil::kFirstItemCommandId);
    return;
  }

  AdvancePseudoFocusFromSelectedItem(reverse);
}

ClipboardHistoryUtil::Action
ClipboardHistoryMenuModelAdapter::GetActionForCommandId(int command_id) const {
  auto selected_item_iter = item_views_by_command_id_.find(command_id);
  DCHECK(selected_item_iter != item_views_by_command_id_.cend());

  return selected_item_iter->second->action();
}

gfx::Rect ClipboardHistoryMenuModelAdapter::GetMenuBoundsInScreenForTest()
    const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetBoundsInScreen();
}

const views::MenuItemView*
ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(int index) const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetMenuItemAt(index);
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(
    int index) {
  return const_cast<views::MenuItemView*>(
      const_cast<const ClipboardHistoryMenuModelAdapter*>(this)
          ->GetMenuItemViewAtForTest(index));
}

ClipboardHistoryMenuModelAdapter::ClipboardHistoryMenuModelAdapter(
    std::unique_ptr<ui::SimpleMenuModel> model,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager)
    : views::MenuModelAdapter(model.get(), std::move(menu_closed_callback)),
      model_(std::move(model)),
      clipboard_history_(clipboard_history),
      resource_manager_(resource_manager) {}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocusFromSelectedItem(
    bool reverse) {
  base::Optional<int> selected_item_command = GetSelectedMenuItemCommand();
  DCHECK(selected_item_command.has_value());
  auto selected_item_iter =
      item_views_by_command_id_.find(*selected_item_command);
  DCHECK(selected_item_iter != item_views_by_command_id_.end());
  ClipboardHistoryItemView* selected_item_view = selected_item_iter->second;

  // Move the pseudo focus on the selected item view. Return early if the
  // focused view does not change.
  const bool selected_item_has_focus =
      selected_item_view->AdvancePseudoFocus(reverse);
  if (selected_item_has_focus)
    return;

  int next_selected_item_command = -1;
  ClipboardHistoryItemView* next_focused_view = nullptr;

  if (reverse) {
    auto next_focused_item_iter =
        selected_item_iter == item_views_by_command_id_.begin()
            ? item_views_by_command_id_.rbegin()
            : std::make_reverse_iterator(selected_item_iter);
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  } else {
    auto next_focused_item_iter = std::next(selected_item_iter, 1);
    if (next_focused_item_iter == item_views_by_command_id_.end())
      next_focused_item_iter = item_views_by_command_id_.begin();
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  }

  // Advancing pseudo focus should precede the item selection. Because when an
  // item view is selected, the selected view does not overwrite its pseudo
  // focus if its pseudo focus is non-empty. It can ensure that the pseudo focus
  // and the corresponding UI appearance update only once.
  next_focused_view->AdvancePseudoFocus(reverse);
  SelectMenuItemWithCommandId(next_selected_item_command);
}

base::Optional<int>
ClipboardHistoryMenuModelAdapter::CalculateSelectedCommandIdAfterDeletion(
    int command_id) const {
  // If the menu item view to be deleted is the last one, Cancel()
  // should be called so this function should not be hit.
  DCHECK_GT(item_snapshots_.size(), 1u);

  auto start_item = item_snapshots_.find(command_id);
  DCHECK(start_item != item_snapshots_.cend());

  // Search in the forward direction.
  auto check_function = [this](const auto& key_value) -> bool {
    return model_->IsEnabledAt(model_->GetIndexOfCommandId(key_value.first));
  };
  auto selectable_iter_forward = std::find_if(
      std::next(start_item, 1), item_snapshots_.cend(), check_function);
  if (selectable_iter_forward != item_snapshots_.cend())
    return selectable_iter_forward->first;

  // If no selectable item is found, then search in the reverse direction.
  auto selectable_iter_reverse =
      std::find_if(std::make_reverse_iterator(start_item),
                   item_snapshots_.crend(), check_function);
  if (selectable_iter_reverse != item_snapshots_.crend())
    return selectable_iter_reverse->first;

  // No other selectable item, then returns the invalid id.
  return base::nullopt;
}

void ClipboardHistoryMenuModelAdapter::RemoveItemView(int command_id) {
  base::Optional<int> original_selected_command_id =
      GetSelectedMenuItemCommand();

  // The menu item view and its corresponding command should be removed at the
  // same time. Otherwise, it may run into check errors.
  model_->RemoveItemAt(model_->GetIndexOfCommandId(command_id));
  root_view_->RemoveMenuItem(root_view_->GetMenuItemByID(command_id));
  root_view_->ChildrenChanged();

  // `ChildrenChanged()` clears the selection. So restore the selection.
  if (original_selected_command_id.has_value())
    SelectMenuItemWithCommandId(*original_selected_command_id);

  if (item_removal_callback_for_test_)
    item_removal_callback_for_test_.Run();
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    int index) {
  const int command_id = model->GetCommandIdAt(index);

  views::MenuItemView* container = menu->AppendMenuItem(command_id);

  // Ignore `container` in accessibility events handling. Let `item_view`
  // handle.
  container->GetViewAccessibility().OverrideIsIgnored(true);

  // Margins are managed by `ClipboardHistoryItemView`.
  container->SetMargins(/*top_margin=*/0, /*bottom_margin=*/0);

  std::unique_ptr<ClipboardHistoryItemView> item_view =
      ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
          GetItemFromCommandId(command_id), resource_manager_, container);
  item_view->Init();
  item_views_by_command_id_.insert(std::make_pair(command_id, item_view.get()));
  container->AddChildView(std::move(item_view));

  return container;
}

void ClipboardHistoryMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  ClipboardImageModelFactory::Get()->Deactivate();
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - menu_open_time_;
  UMA_HISTOGRAM_TIMES("Ash.ClipboardHistory.ContextMenu.UserJourneyTime",
                      user_journey_time);
  views::MenuModelAdapter::OnMenuClosed(menu);
  item_views_by_command_id_.clear();

  // This implementation of MenuModelAdapter does not have a widget so we need
  // to manually notify the accessibility side of the closed menu.
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* active_widget =
      views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(active_widget);
  views::View* focused_view =
      active_widget->GetFocusManager()->GetFocusedView();
  if (focused_view) {
    focused_view->NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd,
                                           /*send_native_event=*/true);
  }
}

}  // namespace ash
