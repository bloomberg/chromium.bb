// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include "base/utf_string_conversions.h"
#include "grit/app_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_separator.h"
#include "views/controls/menu/submenu_view.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace views {

namespace {

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent)
      : MenuItemView(parent, 0, NORMAL) {
    SetTitle(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU)));
    // Set this so that we're not identified as a normal menu item.
    SetID(kEmptyMenuItemViewID);
    SetEnabled(false);
  }

  virtual bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
    // Empty menu items shouldn't have a tooltip.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuMenuItem);
};

}  // namespace

// Padding between child views.
static const int kChildXPadding = 8;

// MenuItemView ---------------------------------------------------------------

// static
const int MenuItemView::kMenuItemViewID = 1001;

// static
const int MenuItemView::kEmptyMenuItemViewID =
    MenuItemView::kMenuItemViewID + 1;

// static
int MenuItemView::label_start_;

// static
int MenuItemView::item_right_margin_;

// static
int MenuItemView::pref_menu_height_;

// static
const char MenuItemView::kViewClassName[] = "views/MenuItemView";

MenuItemView::MenuItemView(MenuDelegate* delegate)
    : delegate_(delegate),
      controller_(NULL),
      canceled_(false),
      parent_menu_item_(NULL),
      type_(SUBMENU),
      selected_(false),
      command_(0),
      submenu_(NULL),
      has_mnemonics_(false),
      show_mnemonics_(false),
      has_icons_(false) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes supplies a
  // NULL delegate.
  Init(NULL, 0, SUBMENU, delegate);
}

MenuItemView::~MenuItemView() {
  // TODO(sky): ownership is bit wrong now. In particular if a nested message
  // loop is running deletion can't be done, otherwise the stack gets
  // thoroughly screwed. The destructor should be made private, and
  // MenuController should be the only place handling deletion of the menu.
  // (57890).
  delete submenu_;
}

bool MenuItemView::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  *tooltip = UTF16ToWideHack(tooltip_);
  if (!tooltip->empty())
    return true;
  if (GetType() != SEPARATOR) {
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    *tooltip = GetDelegate()->GetTooltipText(command_, location);
    if (!tooltip->empty())
      return true;
  }
  return false;
}

AccessibilityTypes::Role MenuItemView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_MENUITEM;
}

AccessibilityTypes::State MenuItemView::GetAccessibleState() {
  int state = 0;
  switch (GetType()) {
    case SUBMENU:
      state |= AccessibilityTypes::STATE_HASPOPUP;
      break;
    case CHECKBOX:
    case RADIO:
      state |= GetDelegate()->IsItemChecked(GetCommand()) ?
          AccessibilityTypes::STATE_CHECKED : 0;
      break;
    case NORMAL:
    case SEPARATOR:
      // No additional accessibility states currently for these menu states.
      break;
  }
  return state;
}

// static
string16 MenuItemView::GetAccessibleNameForMenuItem(
      const string16& item_text, const string16& accelerator_text) {
  string16 accessible_name = item_text;

  // Filter out the "&" for accessibility clients.
  size_t index = 0;
  const char16 amp = '&';
  while ((index = accessible_name.find(amp, index)) != string16::npos &&
         index + 1 < accessible_name.length()) {
    accessible_name.replace(index, accessible_name.length() - index,
                            accessible_name.substr(index + 1));

    // Special case for "&&" (escaped for "&").
    if (accessible_name[index] == '&')
      ++index;
  }

  // Append accelerator text.
  if (!accelerator_text.empty()) {
    accessible_name.push_back(' ');
    accessible_name.append(accelerator_text);
  }

  return accessible_name;
}

void MenuItemView::RunMenuAt(gfx::NativeWindow parent,
                             MenuButton* button,
                             const gfx::Rect& bounds,
                             AnchorPosition anchor,
                             bool has_mnemonics) {
  // Show mnemonics if the button has focus or alt is pressed.
  bool show_mnemonics = button ? button->HasFocus() : false;
#if defined(OS_WIN)
  // We don't currently need this on gtk as showing the menu gives focus to the
  // button first.
  if (!show_mnemonics)
    show_mnemonics = base::win::IsAltPressed();
#endif
  PrepareForRun(has_mnemonics, show_mnemonics);
  int mouse_event_flags;

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller && !controller->IsBlockingRun()) {
    // A menu is already showing, but it isn't a blocking menu. Cancel it.
    // We can get here during drag and drop if the user right clicks on the
    // menu quickly after the drop.
    controller->Cancel(MenuController::EXIT_ALL);
    controller = NULL;
  }
  bool owns_controller = false;
  if (!controller) {
    // No menus are showing, show one.
    controller = new MenuController(true);
    MenuController::SetActiveInstance(controller);
    owns_controller = true;
  } else {
    // A menu is already showing, use the same controller.

    // Don't support blocking from within non-blocking.
    DCHECK(controller->IsBlockingRun());
  }

  controller_ = controller;

  // Run the loop.
  MenuItemView* result =
      controller->Run(parent, button, this, bounds, anchor, &mouse_event_flags);

  RemoveEmptyMenus();

  controller_ = NULL;

  if (owns_controller) {
    // We created the controller and need to delete it.
    if (MenuController::GetActiveInstance() == controller)
      MenuController::SetActiveInstance(NULL);
    delete controller;
  }
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  DestroyAllMenuHosts();
  if (result && delegate_)
    delegate_->ExecuteCommand(result->GetCommand(), mouse_event_flags);
}

void MenuItemView::RunMenuForDropAt(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    AnchorPosition anchor) {
  PrepareForRun(false, false);

  // If there is a menu, hide it so that only one menu is shown during dnd.
  MenuController* current_controller = MenuController::GetActiveInstance();
  if (current_controller) {
    current_controller->Cancel(MenuController::EXIT_ALL);
  }

  // Always create a new controller for non-blocking.
  controller_ = new MenuController(false);

  // Set the instance, that way it can be canceled by another menu.
  MenuController::SetActiveInstance(controller_);

  controller_->Run(parent, NULL, this, bounds, anchor, NULL);
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(MenuController::EXIT_ALL);
  }
}

MenuItemView* MenuItemView::AppendMenuItemFromModel(ui::MenuModel* model,
                                                    int index,
                                                    int id) {
  SkBitmap icon;
  std::wstring label;
  MenuItemView::Type type;
  ui::MenuModel::ItemType menu_type = model->GetTypeAt(index);
  switch (menu_type) {
    case ui::MenuModel::TYPE_COMMAND:
      model->GetIconAt(index, &icon);
      type = MenuItemView::NORMAL;
      label = UTF16ToWide(model->GetLabelAt(index));
      break;
    case ui::MenuModel::TYPE_CHECK:
      type = MenuItemView::CHECKBOX;
      label = UTF16ToWide(model->GetLabelAt(index));
      break;
    case ui::MenuModel::TYPE_RADIO:
      type = MenuItemView::RADIO;
      label = UTF16ToWide(model->GetLabelAt(index));
      break;
    case ui::MenuModel::TYPE_SEPARATOR:
      type = MenuItemView::SEPARATOR;
      break;
    case ui::MenuModel::TYPE_SUBMENU:
      type = MenuItemView::SUBMENU;
      label = UTF16ToWide(model->GetLabelAt(index));
      break;
    default:
      NOTREACHED();
      type = MenuItemView::NORMAL;
      break;
  }

  return AppendMenuItemImpl(id, label, icon, type);
}

MenuItemView* MenuItemView::AppendMenuItemImpl(int item_id,
                                               const std::wstring& label,
                                               const SkBitmap& icon,
                                               Type type) {
  if (!submenu_)
    CreateSubmenu();
  if (type == SEPARATOR) {
    submenu_->AddChildView(new MenuSeparator());
    return NULL;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetIcon(icon);
  if (type == SUBMENU)
    item->CreateSubmenu();
  submenu_->AddChildView(item);
  return item;
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_)
    submenu_ = new SubmenuView(this);
  return submenu_;
}

void MenuItemView::SetTitle(const std::wstring& title) {
  title_ = WideToUTF16Hack(title);
  SetAccessibleName(GetAccessibleNameForMenuItem(title_, GetAcceleratorText()));
  pref_size_.SetSize(0, 0);  // Triggers preferred size recalculation.
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetTooltip(const std::wstring& tooltip, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->tooltip_ = WideToUTF16Hack(tooltip);
}

void MenuItemView::SetIcon(const SkBitmap& icon, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->SetIcon(icon);
}

void MenuItemView::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
  SchedulePaint();
}

void MenuItemView::Paint(gfx::Canvas* canvas) {
  Paint(canvas, false);
}

gfx::Size MenuItemView::GetPreferredSize() {
  if (pref_size_.IsEmpty())
    pref_size_ = CalculatePreferredSize();
  return pref_size_;
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_;
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  MenuItemView* item = this;
  while (item) {
    MenuItemView* parent = item->GetParentMenuItem();
    if (!parent)
      return item;
    item = parent;
  }
  NOTREACHED();
  return NULL;
}

wchar_t MenuItemView::GetMnemonic() {
  if (!GetRootMenuItem()->has_mnemonics_)
    return 0;

  const std::wstring& title = GetTitle();
  size_t index = 0;
  do {
    index = title.find('&', index);
    if (index != std::wstring::npos) {
      if (index + 1 != title.size() && title[index + 1] != '&') {
        wchar_t char_array[1] = { title[index + 1] };
        return UTF16ToWide(l10n_util::ToLower(WideToUTF16(char_array)))[0];
      }
      index++;
    }
  } while (index != std::wstring::npos);
  return 0;
}

MenuItemView* MenuItemView::GetMenuItemByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return NULL;
  for (int i = 0; i < GetSubmenu()->child_count(); ++i) {
    View* child = GetSubmenu()->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* result = static_cast<MenuItemView*>(child)->
          GetMenuItemByID(id);
      if (result)
        return result;
    }
  }
  return NULL;
}

void MenuItemView::ChildrenChanged() {
  MenuController* controller = GetMenuController();
  if (!controller)
    return;  // We're not showing, nothing to do.

  // Handles the case where we were empty and are no longer empty.
  RemoveEmptyMenus();

  // Handles the case where we were not empty, but now are.
  AddEmptyMenus();

  controller->MenuChildrenChanged(this);

  if (submenu_) {
    // Force a paint and layout. This handles the case of the top level window's
    // size remaining the same, resulting in no change to the submenu's size and
    // no layout.
    submenu_->Layout();
    submenu_->SchedulePaint();
  }
}

void MenuItemView::Layout() {
  if (!has_children())
    return;

  // Child views are laid out right aligned and given the full height. To right
  // align start with the last view and progress to the first.
  for (int i = child_count() - 1, x = width() - item_right_margin_; i >= 0;
       --i) {
    View* child = GetChildViewAt(i);
    int width = child->GetPreferredSize().width();
    child->SetBounds(x - width, 0, width, height());
    x -= width - kChildXPadding;
  }
}

int MenuItemView::GetAcceleratorTextWidth() {
  string16 text = GetAcceleratorText();
  return text.empty() ? 0 : MenuConfig::instance().font.GetStringWidth(text);
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type)
    : delegate_(NULL),
      controller_(NULL),
      canceled_(false),
      parent_menu_item_(parent),
      type_(type),
      selected_(false),
      command_(command),
      submenu_(NULL),
      has_mnemonics_(false),
      show_mnemonics_(false),
      has_icons_(false) {
  Init(parent, command, type, NULL);
}

std::string MenuItemView::GetClassName() const {
  return kViewClassName;
}

// Calculates all sizes that we can from the OS.
//
// This is invoked prior to Running a menu.
void MenuItemView::UpdateMenuPartSizes(bool has_icons) {
  MenuConfig::Reset();
  const MenuConfig& config = MenuConfig::instance();

  item_right_margin_ = config.label_to_arrow_padding + config.arrow_width +
      config.arrow_to_edge_padding;

  if (has_icons) {
    label_start_ = config.item_left_margin + config.check_width +
                   config.icon_to_label_padding;
  } else {
    // If there are no icons don't pad by the icon to label padding. This
    // makes us look close to system menus.
    label_start_ = config.item_left_margin + config.check_width;
  }
  if (config.render_gutter)
    label_start_ += config.gutter_width + config.gutter_to_label;

  MenuItemView menu_item(NULL);
  menu_item.SetTitle(L"blah");  // Text doesn't matter here.
  pref_menu_height_ = menu_item.GetPreferredSize().height();
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type,
                        MenuDelegate* delegate) {
  delegate_ = delegate;
  controller_ = NULL;
  canceled_ = false;
  parent_menu_item_ = parent;
  type_ = type;
  selected_ = false;
  command_ = command;
  submenu_ = NULL;
  show_mnemonics_ = false;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);
  has_icons_ = false;

  MenuDelegate* root_delegate = GetDelegate();
  if (root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

void MenuItemView::DropMenuClosed(bool notify_delegate) {
  DCHECK(controller_);
  DCHECK(!controller_->IsBlockingRun());
  if (MenuController::GetActiveInstance() == controller_)
    MenuController::SetActiveInstance(NULL);
  delete controller_;
  controller_ = NULL;

  RemoveEmptyMenus();

  if (notify_delegate && delegate_) {
    // Our delegate is null when invoked from the destructor.
    delegate_->DropMenuClosed(this);
  }
  // WARNING: its possible the delegate deleted us at this point.
}

void MenuItemView::PrepareForRun(bool has_mnemonics, bool show_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Force us to have a submenu.
  CreateSubmenu();

  canceled_ = false;

  has_mnemonics_ = has_mnemonics;
  show_mnemonics_ = has_mnemonics && show_mnemonics;

  AddEmptyMenus();

  if (!MenuController::GetActiveInstance()) {
    // Only update the menu size if there are no menus showing, otherwise
    // things may shift around.
    UpdateMenuPartSizes(has_icons_);
  }
}

int MenuItemView::GetDrawStringFlags() {
  int flags = 0;
  if (base::i18n::IsRTL())
    flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
  else
    flags |= gfx::Canvas::TEXT_ALIGN_LEFT;

  if (has_mnemonics_) {
    if (MenuConfig::instance().show_mnemonics ||
        GetRootMenuItem()->show_mnemonics_) {
      flags |= gfx::Canvas::SHOW_PREFIX;
    } else {
      flags |= gfx::Canvas::HIDE_PREFIX;
    }
  }
  return flags;
}

void MenuItemView::AddEmptyMenus() {
  DCHECK(HasSubmenu());
  if (!submenu_->has_children()) {
    submenu_->AddChildViewAt(new EmptyMenuMenuItem(this), 0);
  } else {
    for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
         ++i) {
      MenuItemView* child = submenu_->GetMenuItemAt(i);
      if (child->HasSubmenu())
        child->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Iterate backwards as we may end up removing views, which alters the child
  // view count.
  for (int i = submenu_->child_count() - 1; i >= 0; --i) {
    View* child = submenu_->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void MenuItemView::PaintAccelerator(gfx::Canvas* canvas) {
  string16 accel_text = GetAcceleratorText();
  if (accel_text.empty())
    return;

  const gfx::Font& font = MenuConfig::instance().font;
  int available_height = height() - GetTopMargin() - GetBottomMargin();
  int max_accel_width =
      parent_menu_item_->GetSubmenu()->max_accelerator_width();
  gfx::Rect accel_bounds(width() - item_right_margin_ - max_accel_width,
                         GetTopMargin(), max_accel_width, available_height);
  accel_bounds.set_x(GetMirroredXForRect(accel_bounds));
  int flags = GetRootMenuItem()->GetDrawStringFlags() |
      gfx::Canvas::TEXT_VALIGN_MIDDLE;
  flags &= ~(gfx::Canvas::TEXT_ALIGN_RIGHT | gfx::Canvas::TEXT_ALIGN_LEFT);
  if (base::i18n::IsRTL())
    flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
  else
    flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
  canvas->DrawStringInt(
      accel_text, font, TextButton::kDisabledColor,
      accel_bounds.x(), accel_bounds.y(), accel_bounds.width(),
      accel_bounds.height(), flags);
}

void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
       ++i) {
    submenu_->GetMenuItemAt(i)->DestroyAllMenuHosts();
  }
}

int MenuItemView::GetTopMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_top_margin :
        MenuConfig::instance().item_no_icon_top_margin;
}

int MenuItemView::GetBottomMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_bottom_margin :
        MenuConfig::instance().item_no_icon_bottom_margin;
}

int MenuItemView::GetChildPreferredWidth() {
  if (!has_children())
    return 0;

  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (i)
      width += kChildXPadding;
    width += GetChildViewAt(i)->GetPreferredSize().width();
  }
  return width;
}

string16 MenuItemView::GetAcceleratorText() {
  Accelerator accelerator;
  return (GetDelegate() &&
          GetDelegate()->GetAccelerator(GetCommand(), &accelerator)) ?
      accelerator.GetShortcutText() : string16();
}

}  // namespace views
