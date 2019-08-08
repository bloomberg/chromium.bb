// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#endif

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

constexpr int kFixedMenuWidthPreDice = 240;
constexpr int kFixedMenuWidthDice = 288;
constexpr int kIconSize = 16;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

// static
void ProfileMenuViewBase::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::Button* anchor_button,
    gfx::NativeView parent_window,
    const gfx::Rect& anchor_rect,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  DCHECK_EQ(browser->profile()->IsIncognito(),
            view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO);

  ProfileMenuViewBase* bubble;
  if (view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    bubble = new IncognitoMenuView(anchor_button, anchor_rect, parent_window,
                                   browser);
  } else {
#if !defined(OS_CHROMEOS)
    bubble = new ProfileChooserView(
        anchor_button, anchor_rect, parent_window, browser, view_mode,
        manage_accounts_params.service_type, access_point);
#else
    NOTREACHED();
    return;
#endif
  }

  views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();
  if (is_source_keyboard)
    bubble->FocusButtonOnKeyboardOpen();
}

// static
bool ProfileMenuViewBase::IsShowing() {
  return g_profile_bubble_ != nullptr;
}

// static
void ProfileMenuViewBase::Hide() {
  if (g_profile_bubble_)
    g_profile_bubble_->GetWidget()->Close();
}

// static
ProfileMenuViewBase* ProfileMenuViewBase::GetBubbleForTesting() {
  return g_profile_bubble_;
}

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         const gfx::Rect& anchor_rect,
                                         gfx::NativeView parent_window,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      menu_width_(0),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  DCHECK(!g_profile_bubble_);
  g_profile_bubble_ = this;
  // TODO(sajadm): Remove when fixing https://crbug.com/822075
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  set_margins(gfx::Insets(2, 0));
  if (anchor_button) {
    anchor_button->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  } else {
    DCHECK(parent_window);
    SetAnchorRect(anchor_rect);
    set_parent_window(parent_window);
  }

  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));

  bool dice_enabled = AccountConsistencyModeManager::IsDiceEnabledForProfile(
      browser->profile());
  menu_width_ = dice_enabled ? kFixedMenuWidthDice : kFixedMenuWidthPreDice;
}

ProfileMenuViewBase::~ProfileMenuViewBase() {
  // Items stored for menu generation are removed after menu is finalized, hence
  // it's not expected to have while destroying the object.
  DCHECK(g_profile_bubble_ != this);
  DCHECK(menu_item_groups_.empty());
}

ax::mojom::Role ProfileMenuViewBase::GetAccessibleWindowRole() {
  // Return |ax::mojom::Role::kDialog| which will make screen readers announce
  // the following in the listed order:
  // the title of the dialog, labels (if any), the focused View within the
  // dialog (if any)
  return ax::mojom::Role::kDialog;
}

void ProfileMenuViewBase::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(native_theme);
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
}

bool ProfileMenuViewBase::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDialogDelegateView::AcceleratorPressed(accelerator);

  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

int ProfileMenuViewBase::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ProfileMenuViewBase::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileMenuViewBase::WindowClosing() {
  DCHECK_EQ(g_profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  g_profile_bubble_ = nullptr;
}

void ProfileMenuViewBase::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  chrome::ShowSettings(browser_);
}

int ProfileMenuViewBase::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if defined(OS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileMenuViewBase::Reset() {
  menu_item_groups_.clear();
}

void ProfileMenuViewBase::AddMenuItems(MenuItems& menu_items, bool new_group) {
  if (new_group || menu_item_groups_.empty())
    menu_item_groups_.emplace_back();

  auto& last_group = menu_item_groups_.back();
  for (auto& item : menu_items)
    last_group.push_back(std::move(item));
}

void ProfileMenuViewBase::RepopulateViewFromMenuItems() {
  RemoveAllChildViews(true);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int border_insets =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);

  std::unique_ptr<views::View> main_view = std::make_unique<views::View>();
  main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(border_insets, 0),
      border_insets));

  for (size_t i = 0; i < menu_item_groups_.size(); i++) {
    if (i > 0)
      main_view->AddChildView(new views::Separator());

    views::View* sub_view = new views::View();
    sub_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(i ? border_insets : 0, 0)));

    for (std::unique_ptr<views::View>& item : menu_item_groups_[i])
      sub_view->AddChildView(std::move(item));

    main_view->AddChildView(sub_view);
  }

  menu_item_groups_.clear();

  // TODO(https://crbug.com/934689): Simplify. This part is only done to set
  // the menu width.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     menu_width_, menu_width_);

  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->set_hide_horizontal_scrollbar(true);

  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->set_draw_overflow_indicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(main_view));

  layout->StartRow(1.0, 0);
  layout->AddView(scroll_view);
  if (GetBubbleFrameView()) {
    SizeToContents();
    // SizeToContents() will perform a layout, but only if the size changed.
    Layout();
  }
}

gfx::ImageSkia ProfileMenuViewBase::CreateVectorIcon(
    const gfx::VectorIcon& icon) {
  return gfx::CreateVectorIcon(
      icon, kIconSize,
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey);
}

int ProfileMenuViewBase::GetDefaultIconSize() {
  return kIconSize;
}

bool ProfileMenuViewBase::ShouldProvideInitiallyFocusedView() const {
#if defined(OS_MACOSX)
  // On Mac, buttons are not focusable when full keyboard access is turned off,
  // causing views::Widget to fall back to focusing the first focusable View.
  // This behavior is not desired in profile menus because of the menu-like
  // design using |HoverButtons|.
  if (!GetFocusManager() || !GetFocusManager()->keyboard_accessible())
    return false;
#endif
  return true;
}
