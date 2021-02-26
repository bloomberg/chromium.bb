// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller_impl.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_controller.h"

namespace ash {

namespace {

ui::ClipboardNonBacked* GetClipboard() {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  return clipboard;
}

bool IsRectContainedByAnyDisplay(const gfx::Rect& rect) {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    if (display.bounds().Contains(rect))
      return true;
  }
  return false;
}

void SendSyntheticKeyEvent(ui::KeyboardCode key_code, int flags) {
  ui::KeyEvent synthetic_key_event(/*type=*/ui::ET_KEY_PRESSED, key_code,
                                   /*code=*/static_cast<ui::DomCode>(0), flags);
  auto* host = GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);
  host->DeliverEventToSink(&synthetic_key_event);
}

}  // namespace

// ClipboardHistoryControllerImpl::AcceleratorTarget ---------------------------

class ClipboardHistoryControllerImpl::AcceleratorTarget
    : public ui::AcceleratorTarget {
 public:
  explicit AcceleratorTarget(ClipboardHistoryControllerImpl* controller)
      : controller_(controller),
        show_menu_combo_(
            ui::Accelerator(/*key_code=*/ui::VKEY_V,
                            /*modifiers=*/ui::EF_COMMAND_DOWN,
                            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        delete_selected_(ui::Accelerator(
            /*key_code=*/ui::VKEY_BACK,
            /*modifiers=*/ui::EF_NONE,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        tab_navigation_(ui::Accelerator(
            /*key_code=*/ui::VKEY_TAB,
            /*modifiers=*/ui::EF_NONE,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        shift_tab_navigation_(ui::Accelerator(
            /*key_code=*/ui::VKEY_TAB,
            /*modifiers=*/ui::EF_SHIFT_DOWN,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)) {}
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

  void Init() {
    // Register, but no need to unregister because this outlives
    // AcceleratorController.
    Shell::Get()->accelerator_controller()->Register(
        {show_menu_combo_}, /*accelerator_target=*/this);
  }

  void OnMenuShown() {
    Shell::Get()->accelerator_controller()->Register(
        {delete_selected_, tab_navigation_, shift_tab_navigation_},
        /*accelerator_target=*/this);
  }

  void OnMenuClosed() {
    Shell::Get()->accelerator_controller()->Unregister(
        delete_selected_, /*accelerator_target=*/this);
    Shell::Get()->accelerator_controller()->Unregister(
        tab_navigation_, /*accelerator_target=*/this);
    Shell::Get()->accelerator_controller()->Unregister(
        shift_tab_navigation_, /*accelerator_target=*/this);
  }

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == show_menu_combo_) {
      HandleShowMenuCombo();
    } else if (accelerator == delete_selected_) {
      HandleDeleteSelected(accelerator.modifiers());
    } else if (accelerator == tab_navigation_) {
      HandleTab();
    } else if (accelerator == shift_tab_navigation_) {
      HandleShiftTab();
    } else {
      NOTREACHED();
      return false;
    }

    return true;
  }

  bool CanHandleAccelerators() const override {
    return controller_->IsMenuShowing() || controller_->CanShowMenu();
  }

  void HandleDeleteSelected(int event_flags) {
    DCHECK(controller_->IsMenuShowing());
    controller_->DeleteSelectedMenuItemIfAny();
  }

  void HandleShowMenuCombo() {
    if (controller_->IsMenuShowing())
      controller_->ExecuteSelectedMenuItem(show_menu_combo_.modifiers());
    else
      controller_->ShowMenuByAccelerator();
  }

  void HandleTab() {
    DCHECK(controller_->IsMenuShowing());
    controller_->AdvancePseudoFocus(/*reverse=*/false);
  }

  void HandleShiftTab() {
    DCHECK(controller_->IsMenuShowing());
    controller_->AdvancePseudoFocus(/*reverse=*/true);
  }

  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryControllerImpl* const controller_;

  // The accelerator to show the menu or execute the selected menu item.
  const ui::Accelerator show_menu_combo_;

  // The accelerator to delete the selected menu item. It is only registered
  // while the menu is showing.
  const ui::Accelerator delete_selected_;

  // Move the pseudo focus forward.
  const ui::Accelerator tab_navigation_;

  // Moves the pseudo focus backward.
  const ui::Accelerator shift_tab_navigation_;
};

// ClipboardHistoryControllerImpl::MenuDelegate --------------------------------

class ClipboardHistoryControllerImpl::MenuDelegate
    : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(ClipboardHistoryControllerImpl* controller)
      : controller_(controller) {}
  MenuDelegate(const MenuDelegate&) = delete;
  MenuDelegate& operator=(const MenuDelegate&) = delete;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    controller_->ExecuteCommand(command_id, event_flags);
  }

 private:
  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryControllerImpl* const controller_;
};

// ClipboardHistoryControllerImpl ----------------------------------------------

ClipboardHistoryControllerImpl::ClipboardHistoryControllerImpl()
    : clipboard_history_(std::make_unique<ClipboardHistory>()),
      resource_manager_(std::make_unique<ClipboardHistoryResourceManager>(
          clipboard_history_.get())),
      accelerator_target_(std::make_unique<AcceleratorTarget>(this)),
      menu_delegate_(std::make_unique<MenuDelegate>(this)),
      nudge_controller_(
          std::make_unique<ClipboardNudgeController>(clipboard_history_.get(),
                                                     this)) {
  clipboard_history_->AddObserver(this);
}

ClipboardHistoryControllerImpl::~ClipboardHistoryControllerImpl() {
  clipboard_history_->RemoveObserver(this);
}

void ClipboardHistoryControllerImpl::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void ClipboardHistoryControllerImpl::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

void ClipboardHistoryControllerImpl::Init() {
  accelerator_target_->Init();
}

bool ClipboardHistoryControllerImpl::IsMenuShowing() const {
  return context_menu_ && context_menu_->IsRunning();
}

gfx::Rect ClipboardHistoryControllerImpl::GetMenuBoundsInScreenForTest() const {
  return context_menu_->GetMenuBoundsInScreenForTest();
}

void ClipboardHistoryControllerImpl::ShowMenu(
    const gfx::Rect& anchor_rect,
    views::MenuAnchorPosition menu_anchor_position,
    ui::MenuSourceType source_type) {
  if (IsMenuShowing() || !CanShowMenu())
    return;

  // Close the running context menu if any before showing the clipboard history
  // menu. Because the clipboard history menu should not be nested.
  auto* active_menu_instance = views::MenuController::GetActiveInstance();
  if (active_menu_instance)
    active_menu_instance->Cancel(views::MenuController::ExitType::kAll);

  context_menu_ = ClipboardHistoryMenuModelAdapter::Create(
      menu_delegate_.get(),
      base::BindRepeating(&ClipboardHistoryControllerImpl::OnMenuClosed,
                          base::Unretained(this)),
      clipboard_history_.get(), resource_manager_.get());
  context_menu_->Run(anchor_rect, menu_anchor_position, source_type);

  DCHECK(IsMenuShowing());
  accelerator_target_->OnMenuShown();

  // The first menu item should be selected as default after the clipboard
  // history menu shows.
  context_menu_->SelectMenuItemWithCommandId(
      ClipboardHistoryUtil::kFirstItemCommandId);
  for (auto& observer : observers_)
    observer.OnClipboardHistoryMenuShown();
}

bool ClipboardHistoryControllerImpl::CanShowMenu() const {
  return !clipboard_history_->IsEmpty() &&
         ClipboardHistoryUtil::IsEnabledInCurrentMode();
}

std::unique_ptr<ScopedClipboardHistoryPause>
ClipboardHistoryControllerImpl::CreateScopedPause() {
  return std::make_unique<ScopedClipboardHistoryPauseImpl>(
      clipboard_history_.get());
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryCleared() {
  // Prevent clipboard contents getting restored if the Clipboard is cleared
  // soon after a `PasteMenuItemData()`.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!IsMenuShowing())
    return;
  context_menu_->Cancel();
  context_menu_.reset();
}

void ClipboardHistoryControllerImpl::ExecuteSelectedMenuItem(int event_flags) {
  DCHECK(IsMenuShowing());
  // Deactivate ClipboardImageModelFactory prior to pasting to ensure that any
  // modifications to the clipboard for HTML rendering purposes are reversed.
  ClipboardImageModelFactory::Get()->Deactivate();

  auto command = context_menu_->GetSelectedMenuItemCommand();

  // If no menu item is currently selected, we'll fallback to the first item.
  PasteMenuItemData(command.value_or(ClipboardHistoryUtil::kFirstItemCommandId),
                    event_flags);
}

void ClipboardHistoryControllerImpl::ExecuteCommand(int command_id,
                                                    int event_flags) {
  DCHECK(context_menu_);

  DCHECK_GE(command_id, ClipboardHistoryUtil::kFirstItemCommandId);
  DCHECK_LE(command_id, ClipboardHistoryUtil::kMaxItemCommandId);

  using Action = ClipboardHistoryUtil::Action;
  Action action = context_menu_->GetActionForCommandId(command_id);
  switch (action) {
    case Action::kPaste:
      PasteMenuItemData(command_id, event_flags & ui::EF_SHIFT_DOWN);
      return;
    case Action::kDelete:
      DeleteItemWithCommandId(command_id);
      return;
    case Action::kSelect:
      context_menu_->SelectMenuItemWithCommandId(command_id);
      return;
    case Action::kEmpty:
      NOTREACHED();
      return;
  }
}

void ClipboardHistoryControllerImpl::ShowMenuByAccelerator() {
  ShowMenu(CalculateAnchorRect(), views::MenuAnchorPosition::kBubbleRight,
           ui::MENU_SOURCE_KEYBOARD);
}

void ClipboardHistoryControllerImpl::PasteMenuItemData(int command_id,
                                                       bool paste_plain_text) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.MenuOptionSelected", command_id,
      ClipboardHistoryUtil::kMaxCommandId);

  // Deactivate ClipboardImageModelFactory prior to pasting to ensure that any
  // modifications to the clipboard for HTML rendering purposes are reversed.
  ClipboardImageModelFactory::Get()->Deactivate();

  // Force close the context menu. Failure to do so before dispatching our
  // synthetic key event will result in the context menu consuming the event.
  DCHECK(context_menu_);
  context_menu_->Cancel();

  const ClipboardHistoryItem& selected_item =
      context_menu_->GetItemFromCommandId(command_id);

  auto* clipboard = GetClipboard();
  std::unique_ptr<ui::ClipboardData> original_data;

  // If necessary, replace the clipboard's |original_data| temporarily so that
  // we can paste the selected history item.
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  if (paste_plain_text ||
      selected_item.data() != *clipboard->GetClipboardData(&data_dst)) {
    std::unique_ptr<ui::ClipboardData> temp_data;
    if (paste_plain_text) {
      // When the shift key is pressed, we only paste plain text.
      temp_data = std::make_unique<ui::ClipboardData>();
      temp_data->set_text(selected_item.data().text());
      ui::DataTransferEndpoint* data_src = selected_item.data().source();
      if (data_src) {
        temp_data->set_source(
            std::make_unique<ui::DataTransferEndpoint>(*data_src));
      }
    } else {
      temp_data = std::make_unique<ui::ClipboardData>(selected_item.data());
    }

    // Pause clipboard history when manipulating the clipboard for a paste.
    ScopedClipboardHistoryPauseImpl scoped_pause(clipboard_history_.get());
    original_data = clipboard->WriteClipboardData(std::move(temp_data));
  }

  SendSyntheticKeyEvent(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  for (auto& observer : observers_)
    observer.OnClipboardHistoryPasted();

  if (!original_data)
    return;

  // Replace the original item back on top of the clipboard. Some apps take a
  // long time to receive the paste event, also some apps will read from the
  // clipboard multiple times per paste. Wait a bit before replacing the item
  // back onto the clipboard.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>& weak_ptr,
             std::unique_ptr<ui::ClipboardData> original_data) {
            // When restoring the original item back on top of the clipboard we
            // need to pause clipboard history. Failure to do so will result in
            // the original item being re-recorded when this restoration step
            // should actually be opaque to the user.
            std::unique_ptr<ScopedClipboardHistoryPauseImpl> scoped_pause;
            if (weak_ptr) {
              scoped_pause = std::make_unique<ScopedClipboardHistoryPauseImpl>(
                  weak_ptr->clipboard_history_.get());
            }
            GetClipboard()->WriteClipboardData(std::move(original_data));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(original_data)),
      base::TimeDelta::FromMilliseconds(200));
}

void ClipboardHistoryControllerImpl::DeleteSelectedMenuItemIfAny() {
  DCHECK(context_menu_);
  auto selected_command = context_menu_->GetSelectedMenuItemCommand();

  // Return early if no item is selected.
  if (!selected_command.has_value())
    return;

  DeleteItemWithCommandId(*selected_command);
}

void ClipboardHistoryControllerImpl::DeleteItemWithCommandId(int command_id) {
  DCHECK(context_menu_);

  // Pressing VKEY_DELETE is handled here via AcceleratorTarget because the
  // contextual menu consumes the key event. Record the "pressing the delete
  // button" histogram here because this action does the same thing as
  // activating the button directly via click/tap. There is no special handling
  // for pasting an item via VKEY_RETURN because in that case the menu does not
  // process the key event.
  const auto& to_be_deleted_item =
      context_menu_->GetItemFromCommandId(command_id);
  ClipboardHistoryUtil::RecordClipboardHistoryItemDeleted(to_be_deleted_item);
  clipboard_history_->RemoveItemForId(to_be_deleted_item.id());

  // If the item to be deleted is the last one, close the whole menu.
  if (context_menu_->GetMenuItemsCount() == 1) {
    context_menu_->Cancel();
    context_menu_.reset();
    return;
  }

  context_menu_->RemoveMenuItemWithCommandId(command_id);
}

void ClipboardHistoryControllerImpl::AdvancePseudoFocus(bool reverse) {
  DCHECK(context_menu_);
  context_menu_->AdvancePseudoFocus(reverse);
}

gfx::Rect ClipboardHistoryControllerImpl::CalculateAnchorRect() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());

  // Some web apps render the caret in an IFrame, and we will not get the
  // bounds in that case.
  // TODO(https://crbug.com/1099930): Show the menu in the middle of the
  // webview if the bounds are empty.
  ui::TextInputClient* text_input_client =
      host->GetInputMethod()->GetTextInputClient();

  // `text_input_client` may be null. For example, in clamshell mode and without
  // any window open.
  const gfx::Rect textfield_bounds =
      text_input_client ? text_input_client->GetCaretBounds() : gfx::Rect();

  // Note that the width of caret's bounds may be zero in some views (such as
  // the search bar of Google search web page). So we cannot use
  // gfx::Size::IsEmpty() here. In addition, the applications using IFrame may
  // provide unreliable `textfield_bounds` which are not fully contained by the
  // display bounds.
  const bool textfield_bounds_are_valid =
      textfield_bounds.size() != gfx::Size() &&
      IsRectContainedByAnyDisplay(textfield_bounds);

  if (textfield_bounds_are_valid)
    return textfield_bounds;

  return gfx::Rect(display::Screen::GetScreen()->GetCursorScreenPoint(),
                   gfx::Size());
}

void ClipboardHistoryControllerImpl::OnMenuClosed() {
  accelerator_target_->OnMenuClosed();
}

}  // namespace ash
