// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include "base/i18n/case_conversion.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_separator.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

namespace {

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent)
      : MenuItemView(parent, 0, EMPTY) {
    // Set this so that we're not identified as a normal menu item.
    set_id(kEmptyMenuItemViewID);
    SetTitle(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU));
    SetEnabled(false);
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE {
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
      has_icons_(false),
      top_margin_(-1),
      bottom_margin_(-1),
      requested_menu_position_(POSITION_BEST_FIT),
      actual_menu_position_(requested_menu_position_) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes supplies a
  // NULL delegate.
  Init(NULL, 0, SUBMENU, delegate);
}

void MenuItemView::ChildPreferredSizeChanged(View* child) {
  pref_size_.SetSize(0, 0);
  PreferredSizeChanged();
}

bool MenuItemView::GetTooltipText(const gfx::Point& p,
                                  string16* tooltip) const {
  *tooltip = tooltip_;
  if (!tooltip->empty())
    return true;

  if (GetType() == SEPARATOR)
    return false;

  const MenuController* controller = GetMenuController();
  if (!controller || controller->exit_type() != MenuController::EXIT_NONE) {
    // Either the menu has been closed or we're in the process of closing the
    // menu. Don't attempt to query the delegate as it may no longer be valid.
    return false;
  }

  const MenuItemView* root_menu_item = GetRootMenuItem();
  if (root_menu_item->canceled_) {
    // TODO(sky): if |canceled_| is true, controller->exit_type() should be
    // something other than EXIT_NONE, but crash reports seem to indicate
    // otherwise. Figure out why this is needed.
    return false;
  }

  const MenuDelegate* delegate = GetDelegate();
  CHECK(delegate);
  gfx::Point location(p);
  ConvertPointToScreen(this, &location);
  *tooltip = delegate->GetTooltipText(command_, location);
  return !tooltip->empty();
}

void MenuItemView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_MENUITEM;

  string16 item_text;
  if (IsContainer()) {
    // The first child is taking over, just use its accessible name instead of
    // |title_|.
    View* child = child_at(0);
    ui::AccessibleViewState state;
    child->GetAccessibleState(&state);
    item_text = state.name;
  } else {
    item_text = title_;
  }
  state->name = GetAccessibleNameForMenuItem(item_text, GetAcceleratorText());

  switch (GetType()) {
    case SUBMENU:
      state->state |= ui::AccessibilityTypes::STATE_HASPOPUP;
      break;
    case CHECKBOX:
    case RADIO:
      state->state |= GetDelegate()->IsItemChecked(GetCommand()) ?
          ui::AccessibilityTypes::STATE_CHECKED : 0;
      break;
    case NORMAL:
    case SEPARATOR:
    case EMPTY:
      // No additional accessibility states currently for these menu states.
      break;
  }
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

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(MenuController::EXIT_ALL);
  }
}

MenuItemView* MenuItemView::AddMenuItemAt(int index,
                                          int item_id,
                                          const string16& label,
                                          const SkBitmap& icon,
                                          Type type) {
  DCHECK_NE(type, EMPTY);
  DCHECK_LE(0, index);
  if (!submenu_)
    CreateSubmenu();
  DCHECK_GE(submenu_->child_count(), index);
  if (type == SEPARATOR) {
    submenu_->AddChildViewAt(new MenuSeparator(), index);
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
  submenu_->AddChildViewAt(item, index);
  return item;
}

void MenuItemView::RemoveMenuItemAt(int index) {
  DCHECK(submenu_);
  DCHECK_LE(0, index);
  DCHECK_GT(submenu_->child_count(), index);

  View* item = submenu_->child_at(index);
  DCHECK(item);
  submenu_->RemoveChildView(item);

  // RemoveChildView() does not delete the item, which is a good thing
  // in case a submenu is being displayed while items are being removed.
  // Deletion will be done by ChildrenChanged() or at destruction.
  removed_items_.push_back(item);
}

MenuItemView* MenuItemView::AppendMenuItem(int item_id,
                                           const string16& label,
                                           Type type) {
  return AppendMenuItemImpl(item_id, label, SkBitmap(), type);
}

MenuItemView* MenuItemView::AppendSubMenu(int item_id,
                                          const string16& label) {
  return AppendMenuItemImpl(item_id, label, SkBitmap(), SUBMENU);
}

MenuItemView* MenuItemView::AppendSubMenuWithIcon(int item_id,
                                                  const string16& label,
                                                  const SkBitmap& icon) {
  return AppendMenuItemImpl(item_id, label, icon, SUBMENU);
}

MenuItemView* MenuItemView::AppendMenuItemWithLabel(int item_id,
                                                    const string16& label) {
  return AppendMenuItem(item_id, label, NORMAL);
}

MenuItemView* MenuItemView::AppendDelegateMenuItem(int item_id) {
  return AppendMenuItem(item_id, string16(), NORMAL);
}

void MenuItemView::AppendSeparator() {
  AppendMenuItemImpl(0, string16(), SkBitmap(), SEPARATOR);
}

MenuItemView* MenuItemView::AppendMenuItemWithIcon(int item_id,
                                                   const string16& label,
                                                   const SkBitmap& icon) {
  return AppendMenuItemImpl(item_id, label, icon, NORMAL);
}

MenuItemView* MenuItemView::AppendMenuItemFromModel(ui::MenuModel* model,
                                                    int index,
                                                    int id) {
  SkBitmap icon;
  string16 label;
  MenuItemView::Type type;
  ui::MenuModel::ItemType menu_type = model->GetTypeAt(index);
  switch (menu_type) {
    case ui::MenuModel::TYPE_COMMAND:
      model->GetIconAt(index, &icon);
      type = MenuItemView::NORMAL;
      label = model->GetLabelAt(index);
      break;
    case ui::MenuModel::TYPE_CHECK:
      type = MenuItemView::CHECKBOX;
      label = model->GetLabelAt(index);
      break;
    case ui::MenuModel::TYPE_RADIO:
      type = MenuItemView::RADIO;
      label = model->GetLabelAt(index);
      break;
    case ui::MenuModel::TYPE_SEPARATOR:
      type = MenuItemView::SEPARATOR;
      break;
    case ui::MenuModel::TYPE_SUBMENU:
      model->GetIconAt(index, &icon);
      type = MenuItemView::SUBMENU;
      label = model->GetLabelAt(index);
      break;
    default:
      NOTREACHED();
      type = MenuItemView::NORMAL;
      break;
  }

  return AppendMenuItemImpl(id, label, icon, type);
}

MenuItemView* MenuItemView::AppendMenuItemImpl(int item_id,
                                               const string16& label,
                                               const SkBitmap& icon,
                                               Type type) {
  const int index = submenu_ ? submenu_->child_count() : 0;
  return AddMenuItemAt(index, item_id, label, icon, type);
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_)
    submenu_ = new SubmenuView(this);
  return submenu_;
}

bool MenuItemView::HasSubmenu() const {
  return (submenu_ != NULL);
}

SubmenuView* MenuItemView::GetSubmenu() const {
  return submenu_;
}

void MenuItemView::SetTitle(const string16& title) {
  title_ = title;
  pref_size_.SetSize(0, 0);  // Triggers preferred size recalculation.
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetTooltip(const string16& tooltip, int item_id) {
  MenuItemView* item = GetMenuItemByID(item_id);
  DCHECK(item);
  item->tooltip_ = tooltip;
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

void MenuItemView::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PB_NORMAL);
}

gfx::Size MenuItemView::GetPreferredSize() {
  if (pref_size_.IsEmpty())
    pref_size_ = CalculatePreferredSize();
  return pref_size_;
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_;
}

const MenuController* MenuItemView::GetMenuController() const {
  return GetRootMenuItem()->controller_;
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

const MenuDelegate* MenuItemView::GetDelegate() const {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  return const_cast<MenuItemView*>(
      static_cast<const MenuItemView*>(this)->GetRootMenuItem());
}

const MenuItemView* MenuItemView::GetRootMenuItem() const {
  const MenuItemView* item = this;
  for (const MenuItemView* parent = GetParentMenuItem(); parent;
       parent = item->GetParentMenuItem())
    item = parent;
  return item;
}

char16 MenuItemView::GetMnemonic() {
  if (!GetRootMenuItem()->has_mnemonics_)
    return 0;

  size_t index = 0;
  do {
    index = title_.find('&', index);
    if (index != string16::npos) {
      if (index + 1 != title_.size() && title_[index + 1] != '&') {
        char16 char_array[] = { title_[index + 1], 0 };
        // TODO(jshin): What about Turkish locale? See http://crbug.com/81719.
        // If the mnemonic is capital I and the UI language is Turkish,
        // lowercasing it results in 'small dotless i', which is different
        // from a 'dotted i'. Similar issues may exist for az and lt locales.
        return base::i18n::ToLower(char_array)[0];
      }
      index++;
    }
  } while (index != string16::npos);
  return 0;
}

MenuItemView* MenuItemView::GetMenuItemByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return NULL;
  for (int i = 0; i < GetSubmenu()->child_count(); ++i) {
    View* child = GetSubmenu()->child_at(i);
    if (child->id() == MenuItemView::kMenuItemViewID) {
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
  if (controller) {
    // Handles the case where we were empty and are no longer empty.
    RemoveEmptyMenus();

    // Handles the case where we were not empty, but now are.
    AddEmptyMenus();

    controller->MenuChildrenChanged(this);

    if (submenu_) {
      // Force a paint and layout. This handles the case of the top
      // level window's size remaining the same, resulting in no
      // change to the submenu's size and no layout.
      submenu_->Layout();
      submenu_->SchedulePaint();
    }
  }

  STLDeleteElements(&removed_items_);
}

void MenuItemView::Layout() {
  if (!has_children())
    return;

  if (IsContainer()) {
    View* child = child_at(0);
    gfx::Size size = child->GetPreferredSize();
    child->SetBounds(0, GetTopMargin(), size.width(), size.height());
  } else {
    // Child views are laid out right aligned and given the full height. To
    // right align start with the last view and progress to the first.
    for (int i = child_count() - 1, x = width() - item_right_margin_; i >= 0;
         --i) {
      View* child = child_at(i);
      int width = child->GetPreferredSize().width();
      child->SetBounds(x - width, 0, width, height());
      x -= width - kChildXPadding;
    }
  }
}

int MenuItemView::GetAcceleratorTextWidth() {
  string16 text = GetAcceleratorText();
  return text.empty() ? 0 : GetFont().GetStringWidth(text);
}

void MenuItemView::SetMargins(int top_margin, int bottom_margin) {
  top_margin_ = top_margin;
  bottom_margin_ = bottom_margin;

  // invalidate GetPreferredSize() cache
  pref_size_.SetSize(0,0);
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
      has_icons_(false),
      top_margin_(-1),
      bottom_margin_(-1),
      requested_menu_position_(POSITION_BEST_FIT),
      actual_menu_position_(requested_menu_position_) {
  Init(parent, command, type, NULL);
}

MenuItemView::~MenuItemView() {
  delete submenu_;
  STLDeleteElements(&removed_items_);
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
  menu_item.SetTitle(ASCIIToUTF16("blah"));  // Text doesn't matter here.
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
  set_id(kMenuItemViewID);
  has_icons_ = false;

  // Don't request enabled status from the root menu item as it is just
  // a container for real items.  EMPTY items will be disabled.
  MenuDelegate* root_delegate = GetDelegate();
  if (parent && type != EMPTY && root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

void MenuItemView::PrepareForRun(bool has_mnemonics, bool show_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Force us to have a submenu.
  CreateSubmenu();
  actual_menu_position_ = requested_menu_position_;
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

const gfx::Font& MenuItemView::GetFont() {
  // Check for item-specific font.
  const MenuDelegate* delegate = GetDelegate();
  return delegate ?
      delegate->GetLabelFont(GetCommand()) : MenuConfig::instance().font;
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
    View* child = submenu_->child_at(i);
    if (child->id() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->id() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
      delete child;
      child = NULL;
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

  const gfx::Font& font = GetFont();
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
  if (top_margin_ >= 0)
    return top_margin_;

  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_top_margin :
        MenuConfig::instance().item_no_icon_top_margin;
}

int MenuItemView::GetBottomMargin() {
  if (bottom_margin_ >= 0)
    return bottom_margin_;

  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_bottom_margin :
        MenuConfig::instance().item_no_icon_bottom_margin;
}

gfx::Size MenuItemView::GetChildPreferredSize() {
  if (!has_children())
    return gfx::Size();

  if (IsContainer()) {
    View* child = child_at(0);
    return child->GetPreferredSize();
  }

  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (i)
      width += kChildXPadding;
    width += child_at(i)->GetPreferredSize().width();
  }
  // Return a height of 0 to indicate that we should use the title height
  // instead.
  return gfx::Size(width, 0);
}

gfx::Size MenuItemView::CalculatePreferredSize() {
  gfx::Size child_size = GetChildPreferredSize();
  if (IsContainer()) {
    return gfx::Size(
        child_size.width(),
        child_size.height() + GetBottomMargin() + GetTopMargin());
  }

  const gfx::Font& font = GetFont();
  int height = font.GetHeight();
  return gfx::Size(
      font.GetStringWidth(title_) + label_start_ +
          item_right_margin_ + child_size.width(),
      std::max(height, child_size.height()) + GetBottomMargin() +
          GetTopMargin());
}

string16 MenuItemView::GetAcceleratorText() {
  if (id() == kEmptyMenuItemViewID) {
    // Don't query the delegate for menus that represent no children.
    return string16();
  }

  if(!MenuConfig::instance().show_accelerators)
    return string16();

  ui::Accelerator accelerator;
  return (GetDelegate() &&
          GetDelegate()->GetAccelerator(GetCommand(), &accelerator)) ?
      accelerator.GetShortcutText() : string16();
}

bool MenuItemView::IsContainer() const {
  // Let the first child take over |this| when we only have one child and no
  // title.  Note that what child_count() returns is the number of children,
  // not the number of menu items.
  return child_count() == 1 && title_.empty();
}

}  // namespace views
