// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_screen_media_controls_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_public_account_user_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/note_action_launch_button.h"
#include "ash/login/ui/parent_access_widget.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/proximity_auth/public/mojom/auth_type.mojom.h"
#include "components/user_manager/user_type.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Any non-zero value used for separator height. Makes debugging easier; this
// should not affect visual appearance.
constexpr int kNonEmptyHeightDp = 30;

// Horizontal distance between the auth user and the media controls.
constexpr int kDistanceBetweenAuthUserAndMediaControlsLandscapeDp = 100;
constexpr int kDistanceBetweenAuthUserAndMediaControlsPortraitDp = 50;

// Horizontal distance between two users in the low density layout.
constexpr int kLowDensityDistanceBetweenUsersInLandscapeDp = 118;
constexpr int kLowDensityDistanceBetweenUsersInPortraitDp = 32;

// Margin left of the auth user in the medium density layout.
constexpr int kMediumDensityMarginLeftOfAuthUserLandscapeDp = 98;
constexpr int kMediumDensityMarginLeftOfAuthUserPortraitDp = 0;

// Horizontal distance between the auth user and the medium density user row.
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp = 220;
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp = 84;

// Spacing between the auth error text and the learn more button.
constexpr int kLearnMoreButtonVerticalSpacingDp = 6;

// Blue-ish color for the "learn more" button text.
constexpr SkColor kLearnMoreButtonTextColor =
    SkColorSetARGB(0xFF, 0x7B, 0xAA, 0xF7);

constexpr char kLockContentsViewName[] = "LockContentsView";
constexpr char kAuthErrorContainerName[] = "AuthErrorContainer";

// Sets the preferred width for |view| with an arbitrary height.
void SetPreferredWidthForView(views::View* view, int width) {
  view->SetPreferredSize(gfx::Size(width, kNonEmptyHeightDp));
}

class AuthErrorLearnMoreButton : public views::Button,
                                 public views::ButtonListener {
 public:
  explicit AuthErrorLearnMoreButton(LoginErrorBubble* parent_bubble)
      : views::Button(this), parent_bubble_(parent_bubble) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* label =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(kLearnMoreButtonTextColor);
    label->SetSubpixelRenderingEnabled(false);
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::NORMAL));
    AddChildView(label);

    SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));
  }

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    Shell::Get()->login_screen_controller()->ShowAccountAccessHelpApp();
    parent_bubble_->Hide();
  }

 private:
  LoginErrorBubble* parent_bubble_;

  DISALLOW_COPY_AND_ASSIGN(AuthErrorLearnMoreButton);
};

// Focuses the first or last focusable child of |root|. If |reverse| is false,
// this focuses the first focusable child. If |reverse| is true, this focuses
// the last focusable child.
void FocusFirstOrLastFocusableChild(views::View* root, bool reverse) {
  views::FocusSearch search(root, reverse /*cycle*/,
                            false /*accessibility_mode*/);
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  views::View* focusable_view = search.FindNextFocusableView(
      root,
      reverse ? views::FocusSearch::SearchDirection::kBackwards
              : views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  if (focusable_view)
    focusable_view->RequestFocus();
}

// Make a section of the text bold.
// |label|:       The label to apply mixed styles.
// |text|:        The message to display.
// |bold_start|:  The position in |text| to start bolding.
// |bold_length|: The length of bold text.
void MakeSectionBold(views::StyledLabel* label,
                     const base::string16& text,
                     const base::Optional<int>& bold_start,
                     int bold_length) {
  auto create_style = [&](bool is_bold) {
    views::StyledLabel::RangeStyleInfo style;
    if (is_bold) {
      style.custom_font = label->GetDefaultFontList().Derive(
          0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD);
    }
    style.override_color = SK_ColorWHITE;
    return style;
  };

  auto add_style = [&](const views::StyledLabel::RangeStyleInfo& style,
                       int start, int end) {
    if (start >= end)
      return;

    label->AddStyleRange(gfx::Range(start, end), style);
  };

  views::StyledLabel::RangeStyleInfo regular_style =
      create_style(false /*is_bold*/);
  views::StyledLabel::RangeStyleInfo bold_style =
      create_style(true /*is_bold*/);
  if (!bold_start || bold_length == 0) {
    add_style(regular_style, 0, text.length());
    return;
  }

  add_style(regular_style, 0, *bold_start - 1);
  add_style(bold_style, *bold_start, *bold_start + bold_length);
  add_style(regular_style, *bold_start + bold_length + 1, text.length());
}

keyboard::KeyboardUIController* GetKeyboardControllerForWidget(
    const views::Widget* widget) {
  auto* keyboard_ui_controller = keyboard::KeyboardUIController::Get();
  if (!keyboard_ui_controller->IsEnabled())
    return nullptr;

  aura::Window* keyboard_window = keyboard_ui_controller->GetRootWindow();
  aura::Window* this_window = widget->GetNativeWindow()->GetRootWindow();
  return keyboard_window == this_window ? keyboard_ui_controller : nullptr;
}

bool IsPublicAccountUser(const LoginUserInfo& user) {
  return user.basic_user_info.type == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

//
// Computes a layout described as follows:
//
//    l L R r
//
// L R go from [0, L/R_max_fixed_width]
// l and r go from [0, inf]
//
// First, width is distributed to L and R up to their maximum widths. If there
// is not enough width for them, space will be distributed evenly in the same
// ratio as their original sizes.
//
// If L and R are at max width, l and r will receive all remaining space in the
// specified relative weighting.
//
// l -> left_flex_weight
// L -> left_max_fixed_width
// R -> right_max_fixed_width
// r -> right_flex_weight
//
// Output data is in the member variables.
//
struct MediumViewLayout {
  MediumViewLayout(int width,
                   int left_flex_weight,
                   int left_max_fixed_width,
                   int right_max_fixed_width,
                   int right_flex_weight) {
    // No space to distribute.
    if (width <= 0)
      return;

    auto set_values_from_weight = [](int width, float weight_a, float weight_b,
                                     int* value_a, int* value_b) {
      float total_weight = weight_a + weight_b;
      *value_a = width * (weight_a / total_weight);
      // Subtract to avoid floating point rounding errors, ie, guarantee that
      // that |value_a + value_b = width|.
      *value_b = width - *value_a;
    };

    int flex_width = width - (left_max_fixed_width + right_max_fixed_width);
    if (flex_width < 0) {
      // No flex available, distribute to fixed width only
      set_values_from_weight(width, left_max_fixed_width, right_max_fixed_width,
                             &left_fixed_width, &right_fixed_width);
      DCHECK_EQ(width, left_fixed_width + right_fixed_width);
    } else {
      // Flex is available; fixed goes to maximum size, extra goes to flex.
      left_fixed_width = left_max_fixed_width;
      right_fixed_width = right_max_fixed_width;
      set_values_from_weight(flex_width, left_flex_weight, right_flex_weight,
                             &left_flex_width, &right_flex_width);
      DCHECK_EQ(flex_width, left_flex_width + right_flex_width);
    }
  }

  int left_fixed_width = 0;
  int right_fixed_width = 0;
  int left_flex_width = 0;
  int right_flex_width = 0;
};

}  // namespace

class LockContentsView::AutoLoginUserActivityHandler
    : public ui::UserActivityObserver {
 public:
  AutoLoginUserActivityHandler() {
    observer_.Add(ui::UserActivityDetector::Get());
  }

  ~AutoLoginUserActivityHandler() override = default;

  void OnUserActivity(const ui::Event* event) override {
    if (Shell::Get()->login_screen_controller()) {
      Shell::Get()->login_screen_controller()->NotifyUserActivity();
    }
  }

 private:
  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver> observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AutoLoginUserActivityHandler);
};

LockContentsView::TestApi::TestApi(LockContentsView* view) : view_(view) {}

LockContentsView::TestApi::~TestApi() = default;

LoginBigUserView* LockContentsView::TestApi::primary_big_view() const {
  return view_->primary_big_view_;
}

LoginBigUserView* LockContentsView::TestApi::opt_secondary_big_view() const {
  return view_->opt_secondary_big_view_;
}

ScrollableUsersListView* LockContentsView::TestApi::users_list() const {
  return view_->users_list_;
}

LockScreenMediaControlsView* LockContentsView::TestApi::media_controls_view()
    const {
  return view_->media_controls_view_.get();
}

views::View* LockContentsView::TestApi::note_action() const {
  return view_->note_action_;
}

LoginTooltipView* LockContentsView::TestApi::tooltip_bubble() const {
  return view_->tooltip_bubble_;
}

LoginErrorBubble* LockContentsView::TestApi::auth_error_bubble() const {
  return view_->auth_error_bubble_;
}

LoginErrorBubble* LockContentsView::TestApi::detachable_base_error_bubble()
    const {
  return view_->detachable_base_error_bubble_;
}

LoginErrorBubble* LockContentsView::TestApi::warning_banner_bubble() const {
  return view_->warning_banner_bubble_;
}

LoginErrorBubble*
LockContentsView::TestApi::supervised_user_deprecation_bubble() const {
  return view_->supervised_user_deprecation_bubble_;
}

views::View* LockContentsView::TestApi::system_info() const {
  return view_->system_info_;
}

LoginExpandedPublicAccountView* LockContentsView::TestApi::expanded_view()
    const {
  return view_->expanded_view_;
}

views::View* LockContentsView::TestApi::main_view() const {
  return view_->main_view_;
}

LockContentsView::UserState::UserState(const LoginUserInfo& user_info)
    : account_id(user_info.basic_user_info.account_id) {
  fingerprint_state = user_info.fingerprint_state;
  if (user_info.auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN)
    force_online_sign_in = true;
}

LockContentsView::UserState::UserState(UserState&&) = default;

LockContentsView::UserState::~UserState() = default;

// static
const int LockContentsView::kLoginAttemptsBeforeGaiaDialog = 4;

LockContentsView::LockContentsView(
    mojom::TrayActionState initial_note_action_state,
    LockScreen::ScreenType screen_type,
    LoginDataDispatcher* data_dispatcher,
    std::unique_ptr<LoginDetachableBaseModel> detachable_base_model)
    : NonAccessibleView(kLockContentsViewName),
      screen_type_(screen_type),
      data_dispatcher_(data_dispatcher),
      detachable_base_model_(std::move(detachable_base_model)) {
  if (screen_type == LockScreen::ScreenType::kLogin)
    auto_login_user_activity_handler_ =
        std::make_unique<AutoLoginUserActivityHandler>();

  data_dispatcher_->AddObserver(this);
  display_observer_.Add(display::Screen::GetScreen());
  Shell::Get()->system_tray_notifier()->AddSystemTrayFocusObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);

  // We reuse the focusable state on this view as a signal that focus should
  // switch to the system tray. LockContentsView should otherwise not be
  // focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  main_view_ = new NonAccessibleView();
  AddChildView(main_view_);

  // The top header view.
  top_header_ = new views::View();
  auto top_header_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  top_header_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  top_header_->SetLayoutManager(std::move(top_header_layout));
  AddChildView(top_header_);

  system_info_ = new views::View();
  auto* system_info_layout =
      system_info_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(6, 8)));
  system_info_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  system_info_->SetVisible(false);
  top_header_->AddChildView(system_info_);

  note_action_ = new NoteActionLaunchButton(initial_note_action_state);
  top_header_->AddChildView(note_action_);

  // Public Session expanded view.
  expanded_view_ = new LoginExpandedPublicAccountView(
      base::BindRepeating(&LockContentsView::SetDisplayStyle,
                          base::Unretained(this), DisplayStyle::kAll));
  expanded_view_->SetVisible(false);
  AddChildView(expanded_view_);

  supervised_user_deprecation_bubble_ = new LoginErrorBubble();
  supervised_user_deprecation_bubble_->SetPersistent(true);
  AddChildView(supervised_user_deprecation_bubble_);

  detachable_base_error_bubble_ = new LoginErrorBubble();
  detachable_base_error_bubble_->SetPersistent(true);
  AddChildView(detachable_base_error_bubble_);

  tooltip_bubble_ = new LoginTooltipView(base::UTF8ToUTF16("") /*message*/,
                                         nullptr /*anchor_view*/);
  AddChildView(tooltip_bubble_);

  warning_banner_bubble_ = new LoginErrorBubble();
  warning_banner_bubble_->SetPersistent(true);
  AddChildView(warning_banner_bubble_);

  auth_error_bubble_ = new LoginErrorBubble();
  AddChildView(auth_error_bubble_);

  OnLockScreenNoteStateChanged(initial_note_action_state);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  RegisterAccelerators();
}

LockContentsView::~LockContentsView() {
  Shell::Get()->accelerator_controller()->UnregisterAll(this);
  data_dispatcher_->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayFocusObserver(this);

  if (unlock_attempt_ > 0) {
    // Times a password was incorrectly entered until user gives up (sign out
    // current session or shutdown the device). For a successful unlock,
    // unlock_attempt_ should already be reset by OnLockStateChanged.
    Shell::Get()->metrics()->login_metrics_recorder()->RecordNumLoginAttempts(
        unlock_attempt_, false /*success*/);
  }
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void LockContentsView::FocusNextUser() {
  if (users_.empty())
    return;

  if (login_views_utils::HasFocusInAnyChildView(primary_big_view_)) {
    if (opt_secondary_big_view_) {
      SwapActiveAuthBetweenPrimaryAndSecondary(false /*is_primary*/);
      opt_secondary_big_view_->RequestFocus();
    } else if (users_list_) {
      users_list_->user_view_at(0)->RequestFocus();
    }
    return;
  }

  if (opt_secondary_big_view_ &&
      login_views_utils::HasFocusInAnyChildView(opt_secondary_big_view_)) {
    SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
    primary_big_view_->RequestFocus();
    return;
  }

  if (users_list_) {
    for (int i = 0; i < users_list_->user_count(); ++i) {
      LoginUserView* user_view = users_list_->user_view_at(i);
      if (!login_views_utils::HasFocusInAnyChildView(user_view))
        continue;

      if (i == users_list_->user_count() - 1) {
        SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
        primary_big_view_->RequestFocus();
        return;
      }

      user_view->GetNextFocusableView()->RequestFocus();
      return;
    }
  }
}

void LockContentsView::FocusPreviousUser() {
  if (users_.empty())
    return;

  if (login_views_utils::HasFocusInAnyChildView(primary_big_view_)) {
    if (users_list_) {
      users_list_->user_view_at(users_list_->user_count() - 1)->RequestFocus();
    } else if (opt_secondary_big_view_) {
      SwapActiveAuthBetweenPrimaryAndSecondary(false /*is_primary*/);
      opt_secondary_big_view_->RequestFocus();
    }
    return;
  }

  if (opt_secondary_big_view_ &&
      login_views_utils::HasFocusInAnyChildView(opt_secondary_big_view_)) {
    SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
    primary_big_view_->RequestFocus();
    return;
  }

  if (users_list_) {
    for (int i = 0; i < users_list_->user_count(); ++i) {
      LoginUserView* user_view = users_list_->user_view_at(i);
      if (!login_views_utils::HasFocusInAnyChildView(user_view))
        continue;

      if (i == 0) {
        SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
        primary_big_view_->RequestFocus();
        return;
      }

      user_view->GetPreviousFocusableView()->RequestFocus();
      return;
    }
  }
}

void LockContentsView::ShowParentAccessDialog() {
  // ParentAccessDialog should only be shown on lock screen from here.
  DCHECK(primary_big_view_);
  const AccountId account_id =
      CurrentBigUserView()->GetCurrentUser().basic_user_info.account_id;

  DCHECK(!ParentAccessWidget::Get());
  ParentAccessWidget::Show(
      account_id,
      base::BindRepeating(&LockContentsView::OnParentAccessValidationFinished,
                          weak_ptr_factory_.GetWeakPtr(), account_id),
      ParentAccessRequestReason::kUnlockTimeLimits);
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(false);
}

void LockContentsView::RequestSecurityTokenPin(
    SecurityTokenPinRequest request) {
  // Find which of the current big users, if any, should handle the request.
  for (auto* big_user : {primary_big_view_, opt_secondary_big_view_}) {
    if (big_user && big_user->auth_user() &&
        big_user->GetCurrentUser().basic_user_info.account_id ==
            request.account_id) {
      big_user->auth_user()->RequestSecurityTokenPin(std::move(request));
      return;
    }
  }
  // The PIN request is obsolete.
  std::move(request.pin_ui_closed_callback).Run();
}

void LockContentsView::ClearSecurityTokenPinRequest() {
  // Try both big users - it's safe since at most one PIN request can happen at
  // a time, and as clearing a non-existing PIN request is a no-op.
  // Note that if the PIN UI used to be shown in some other big view, then it
  // had already been closed while switching the view(s).
  if (primary_big_view_ && primary_big_view_->auth_user())
    primary_big_view_->auth_user()->ClearSecurityTokenPinRequest();
  if (opt_secondary_big_view_ && opt_secondary_big_view_->auth_user())
    opt_secondary_big_view_->auth_user()->ClearSecurityTokenPinRequest();
}

void LockContentsView::Layout() {
  View::Layout();
  LayoutTopHeader();
  LayoutPublicSessionView();

  if (users_list_)
    users_list_->Layout();
}

void LockContentsView::AddedToWidget() {
  DoLayout();

  // Focus the primary user when showing the UI. This will focus the password.
  if (primary_big_view_)
    primary_big_view_->RequestFocus();
}

void LockContentsView::OnFocus() {
  // If LockContentsView somehow gains focus (ie, a test, but it should not
  // under typical circumstances), immediately forward the focus to the
  // primary_big_view_ since LockContentsView has no real focusable content by
  // itself.
  if (primary_big_view_)
    primary_big_view_->RequestFocus();
}

void LockContentsView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // The LockContentsView itself doesn't have anything to focus. If it gets
  // focused we should change the currently focused widget (ie, to the shelf or
  // status area, or lock screen apps, if they are active).
  if (reverse && lock_screen_apps_active_) {
    Shell::Get()->login_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }

  FocusNextWidget(reverse);
}

void LockContentsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  GetViewAccessibility().OverrideNextFocus(shelf_widget);

  GetViewAccessibility().OverridePreviousFocus(shelf->GetStatusAreaWidget());
  node_data->SetNameExplicitlyEmpty();
}

bool LockContentsView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  auto entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;

  PerformAction(entry->second);
  return true;
}

void LockContentsView::OnUsersChanged(const std::vector<LoginUserInfo>& users) {
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  primary_big_view_ = nullptr;
  opt_secondary_big_view_ = nullptr;
  users_list_ = nullptr;
  media_controls_view_.reset();
  layout_actions_.clear();
  // Removing child views can change focus, which may result in LockContentsView
  // getting focused. Make sure to clear internal references before that happens
  // so there is not stale-pointer usage. See crbug.com/884402.
  main_view_->RemoveAllChildViews(true /*delete_children*/);

  // Build user state list. Preserve previous state if the user already exists.
  std::vector<UserState> new_users;
  for (const LoginUserInfo& user : users) {
    UserState* old_state = FindStateForUser(user.basic_user_info.account_id);
    if (old_state)
      new_users.push_back(std::move(*old_state));
    else
      new_users.push_back(UserState(user));
  }
  users_ = std::move(new_users);

  // If there are no users, show gaia signin if login, otherwise crash.
  if (users.empty()) {
    LOG_IF(FATAL, screen_type_ != LockScreen::ScreenType::kLogin)
        << "Empty user list received";
    Shell::Get()->login_screen_controller()->ShowGaiaSignin(
        false /*can_close*/, EmptyAccountId() /*prefilled_account*/);
    return;
  }

  // Allocate layout and big user, which are common between all densities.
  auto* main_layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  main_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  main_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  primary_big_view_ = AllocateLoginBigUserView(users[0], true /*is_primary*/);

  // Build layout for additional users.
  if (users.size() <= 2)
    CreateLowDensityLayout(users);
  else if (users.size() >= 3 && users.size() <= 6)
    CreateMediumDensityLayout(users);
  else if (users.size() >= 7)
    CreateHighDensityLayout(users, main_layout);

  LayoutAuth(primary_big_view_, opt_secondary_big_view_, false /*animate*/);

  // Big user may be the same if we already built lock screen.
  OnBigUserChanged();

  // Force layout.
  PreferredSizeChanged();
  Layout();

  // If one of the child views had focus before we deleted them, then this view
  // will get focused. Move focus back to the primary big view.
  if (HasFocus())
    primary_big_view_->RequestFocus();
}

void LockContentsView::OnUserAvatarChanged(const AccountId& account_id,
                                           const UserAvatar& avatar) {
  auto replace = [&avatar](const LoginUserInfo& user) {
    auto changed = user;
    changed.basic_user_info.avatar = avatar;
    return changed;
  };

  LoginBigUserView* big =
      TryToFindBigUser(account_id, false /*require_auth_active*/);
  if (big) {
    big->UpdateForUser(replace(big->GetCurrentUser()));
    return;
  }

  LoginUserView* user =
      users_list_ ? users_list_->GetUserView(account_id) : nullptr;
  if (user) {
    user->UpdateForUser(replace(user->current_user()), false /*animate*/);
    return;
  }
}

void LockContentsView::OnPinEnabledForUserChanged(const AccountId& user,
                                                  bool enabled) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when changing PIN state to " << enabled;
    return;
  }

  state->show_pin = enabled;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user())
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnChallengeResponseAuthEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR)
        << "Unable to find user when changing challenge-response auth state to "
        << enabled;
    return;
  }

  state->show_challenge_response_auth = enabled;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, /*require_auth_active=*/true);
  if (big_user && big_user->auth_user())
    LayoutAuth(big_user, /*opt_to_hide=*/nullptr, /*animate=*/true);
}

void LockContentsView::OnFingerprintStateChanged(const AccountId& account_id,
                                                 FingerprintState state) {
  UserState* user_state = FindStateForUser(account_id);
  if (!user_state)
    return;

  user_state->fingerprint_state = state;
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user())
    return;

  // TODO(crbug.com/893298): Re-enable animation once the error bubble supports
  // being displayed on the left. This also requires that we dynamically
  // track/update the position of the bubble, or alternatively we set the bubble
  // location to the target animation position and not the current position.
  bool animate = true;
  if (user_state->fingerprint_state ==
      FingerprintState::DISABLED_FROM_TIMEOUT) {
    animate = false;
  }

  big_view->auth_user()->SetFingerprintState(user_state->fingerprint_state);
  LayoutAuth(big_view, nullptr /*opt_to_hide*/, animate);

  if (user_state->fingerprint_state ==
      FingerprintState::DISABLED_FROM_TIMEOUT) {
    base::string16 error_text = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_TIMEOUT);
    auto* label = new views::Label(error_text);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetSubpixelRenderingEnabled(false);
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::NORMAL));
    label->SetMultiLine(true);
    label->SetAllowCharacterBreak(true);
    // Make sure to set a maximum label width, otherwise text wrapping will
    // significantly increase width and layout may not work correctly if
    // the input string is very long.
    label->SetMaximumWidth(
        big_view->auth_user()->password_view()->GetPreferredSize().width());

    auto* container = new NonAccessibleView();
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container->AddChildView(label);

    auth_error_bubble_->SetAnchorView(big_view->auth_user()->password_view());
    auth_error_bubble_->SetContent(container);
    auth_error_bubble_->SetPersistent(true);
    auth_error_bubble_->Show();
  }
}

void LockContentsView::OnFingerprintAuthResult(const AccountId& account_id,
                                               bool success) {
  // Make sure the display backlight is not forced off if there is a fingerprint
  // authentication attempt. If the display backlight is off, then the device
  // will authenticate and dismiss the lock screen but it will not be visible to
  // the user.
  Shell::Get()->power_button_controller()->StopForcingBacklightsOff();

  // |account_id| comes from IPC, make sure it refers to a valid user. The
  // fingerprint scan could have also happened while switching users, so the
  // associated account is no longer a big user.
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user())
    return;

  big_view->auth_user()->NotifyFingerprintAuthResult(success);
}

void LockContentsView::OnAuthEnabledForUser(const AccountId& user) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when enabling auth.";
    return;
  }

  state->disable_auth = false;
  disable_lock_screen_note_ = state->disable_auth;
  OnLockScreenNoteStateChanged(
      Shell::Get()->tray_action()->GetLockScreenNoteState());

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user())
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnAuthDisabledForUser(
    const AccountId& user,
    const AuthDisabledData& auth_disabled_data) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when disabling auth";
    return;
  }

  state->disable_auth = true;
  disable_lock_screen_note_ = state->disable_auth;
  OnLockScreenNoteStateChanged(mojom::TrayActionState::kNotAvailable);

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
    big_user->auth_user()->SetAuthDisabledMessage(auth_disabled_data);
  }
}

void LockContentsView::OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                                          bool enabled) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user enabling click to auth";
    return;
  }
  state->enable_tap_auth = enabled;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user())
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnForceOnlineSignInForUser(const AccountId& user) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user forcing online sign in";
    return;
  }
  state->force_online_sign_in = true;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user())
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnShowEasyUnlockIcon(const AccountId& user,
                                            const EasyUnlockIconOptions& icon) {
  UserState* state = FindStateForUser(user);
  if (!state)
    return;

  state->easy_unlock_state = icon;
  UpdateEasyUnlockIconForUser(user);

  // Show tooltip only if the user is actively showing auth.
  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (!big_user || !big_user->auth_user())
    return;

  if (tooltip_bubble_->GetVisible())
    tooltip_bubble_->Hide();

  if (icon.autoshow_tooltip) {
    tooltip_bubble_->SetAnchorView(big_user->auth_user()->password_view());
    tooltip_bubble_->SetText(icon.tooltip);
    tooltip_bubble_->Show();
    tooltip_bubble_->SetVisible(true);
  }
}

void LockContentsView::OnWarningMessageUpdated(const base::string16& message) {
  if (message.empty()) {
    if (warning_banner_bubble_->GetVisible())
      warning_banner_bubble_->Hide();
    return;
  }

  if (!CurrentBigUserView() || !CurrentBigUserView()->auth_user()) {
    LOG(ERROR) << "Unable to find the current active big user to show a "
                  "warning banner.";
    return;
  }

  if (warning_banner_bubble_->GetVisible())
    warning_banner_bubble_->Hide();
  // Shows warning banner as a persistent error bubble.
  views::Label* label =
      new views::Label(message, views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT,
                       views::style::STYLE_PRIMARY);
  label->SetMultiLine(true);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SK_ColorWHITE);

  warning_banner_bubble_->SetAnchorView(
      CurrentBigUserView()->auth_user()->password_view());
  warning_banner_bubble_->SetContent(label);
  warning_banner_bubble_->Show();
}

void LockContentsView::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  if (disable_lock_screen_note_)
    state = mojom::TrayActionState::kNotAvailable;

  bool old_lock_screen_apps_active = lock_screen_apps_active_;
  lock_screen_apps_active_ = state == mojom::TrayActionState::kActive;
  note_action_->UpdateVisibility(state);
  LayoutTopHeader();

  // If lock screen apps just got deactivated - request focus for primary auth,
  // which should focus the password field.
  if (old_lock_screen_apps_active && !lock_screen_apps_active_ &&
      primary_big_view_) {
    primary_big_view_->RequestFocus();
  }
}

void LockContentsView::OnSystemInfoChanged(
    bool show,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {
  DCHECK(!os_version_label_text.empty() || !enterprise_info_text.empty() ||
         !bluetooth_name.empty());

  // Helper function to create a label for the system info view.
  auto create_info_label = []() -> views::Label* {
    views::Label* label = new views::Label();
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetFontList(views::Label::GetDefaultFontList().Derive(
        -1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    label->SetSubpixelRenderingEnabled(false);
    return label;
  };

  // Initialize the system info view.
  if (system_info_->children().empty()) {
    for (int i = 0; i < 3; ++i)
      system_info_->AddChildView(create_info_label());
  }

  if (show)
    system_info_->SetVisible(true);

  auto update_label = [&](size_t index, const std::string& text) {
    views::Label* label =
        static_cast<views::Label*>(system_info_->children()[index]);
    label->SetText(base::UTF8ToUTF16(text));
    label->SetVisible(!text.empty());
  };
  update_label(0, os_version_label_text);
  update_label(1, enterprise_info_text);
  update_label(2, bluetooth_name);

  LayoutTopHeader();
}

void LockContentsView::OnPublicSessionDisplayNameChanged(
    const AccountId& account_id,
    const std::string& display_name) {
  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user()))
    return;

  LoginUserInfo user_info = user_view->current_user();
  user_info.basic_user_info.display_name = display_name;
  user_view->UpdateForUser(user_info, false /*animate*/);
}

void LockContentsView::OnPublicSessionLocalesChanged(
    const AccountId& account_id,
    const std::vector<LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {
  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user()))
    return;

  LoginUserInfo user_info = user_view->current_user();
  user_info.public_account_info->available_locales = locales;
  user_info.public_account_info->default_locale = default_locale;
  user_info.public_account_info->show_advanced_view = show_advanced_view;
  user_view->UpdateForUser(user_info, false /*animate*/);
}

void LockContentsView::OnPublicSessionKeyboardLayoutsChanged(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<InputMethodItem>& keyboard_layouts) {
  // Update expanded view because keyboard layouts is user interactive content.
  // I.e. user selects a language locale and the corresponding keyboard layouts
  // will be changed.
  if (expanded_view_->GetVisible() &&
      expanded_view_->current_user().basic_user_info.account_id == account_id) {
    LoginUserInfo user_info = expanded_view_->current_user();
    user_info.public_account_info->default_locale = locale;
    user_info.public_account_info->keyboard_layouts = keyboard_layouts;
    expanded_view_->UpdateForUser(user_info);
  }

  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user())) {
    LOG(ERROR) << "Unable to find public account user.";
    return;
  }

  LoginUserInfo user_info = user_view->current_user();
  // Skip updating keyboard layouts if |locale| is not the default locale
  // of the user. I.e. user changed the default locale in the expanded view,
  // and it should be handled by expanded view.
  if (user_info.public_account_info->default_locale != locale)
    return;

  user_info.public_account_info->keyboard_layouts = keyboard_layouts;
  user_view->UpdateForUser(user_info, false /*animate*/);
}

void LockContentsView::OnPublicSessionShowFullManagementDisclosureChanged(
    bool show_full_management_disclosure) {
  expanded_view_->SetShowFullManagementDisclosure(
      show_full_management_disclosure);
}

void LockContentsView::OnDetachableBasePairingStatusChanged(
    DetachableBasePairingStatus pairing_status) {
  // If the current big user is public account user, or the base is not paired,
  // or the paired base matches the last used by the current user, the
  // detachable base error bubble should be hidden. Otherwise, the bubble should
  // be shown.
  if (!CurrentBigUserView() || !CurrentBigUserView()->auth_user() ||
      pairing_status == DetachableBasePairingStatus::kNone ||
      (pairing_status == DetachableBasePairingStatus::kAuthenticated &&
       detachable_base_model_->PairedBaseMatchesLastUsedByUser(
           CurrentBigUserView()->GetCurrentUser().basic_user_info))) {
    if (detachable_base_error_bubble_->GetVisible())
      detachable_base_error_bubble_->Hide();
    return;
  }

  if (auth_error_bubble_->GetVisible())
    auth_error_bubble_->Hide();

  base::string16 error_text =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_DETACHABLE_BASE_CHANGED);

  views::Label* label =
      new views::Label(error_text, views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT,
                       views::style::STYLE_PRIMARY);
  label->SetMultiLine(true);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SK_ColorWHITE);

  detachable_base_error_bubble_->SetContent(label);
  detachable_base_error_bubble_->SetAnchorView(
      CurrentBigUserView()->auth_user()->password_view());
  detachable_base_error_bubble_->Show();

  // Remove the focus from the password field, to make user less likely to enter
  // the password without seeing the warning about detachable base change.
  if (GetWidget()->IsActive())
    GetWidget()->GetFocusManager()->ClearFocus();
}

void LockContentsView::OnFocusLeavingLockScreenApps(bool reverse) {
  if (!reverse || lock_screen_apps_active_)
    FocusNextWidget(reverse);
  else
    FocusFirstOrLastFocusableChild(this, reverse);
}

void LockContentsView::OnOobeDialogStateChanged(OobeDialogState state) {
  oobe_dialog_visible_ = state != OobeDialogState::HIDDEN;

  // Deactivate the lock screen widget while the dialog is visible to
  // prevent lock screen from grabbing focus and hiding the OOBE dialog.
  GetWidget()->widget_delegate()->SetCanActivate(!oobe_dialog_visible_);

  // Block login screen events, to prevent actions on user pods shown in the
  // background (e.g. hover over user name, or clicking a pod) from having
  // effect.
  set_can_process_events_within_subtree(!oobe_dialog_visible_);

  if (state == OobeDialogState::HIDDEN && primary_big_view_)
    primary_big_view_->RequestFocus();
}

void LockContentsView::OnFocusLeavingSystemTray(bool reverse) {
  // This function is called when the system tray is losing focus. We want to
  // focus the first or last child in this view, a lock screen app window if
  // one is active (in which case lock contents should not have focus), or the
  // OOBE dialog modal if it's active. In the later cases, still focus lock
  // screen first, to synchronously take focus away from the system shelf (or
  // tray) - lock shelf view expect the focus to be taken when it passes it
  // to lock screen view, and can misbehave in case the focus is kept in it.
  FocusFirstOrLastFocusableChild(this, reverse);

  if (lock_screen_apps_active_) {
    Shell::Get()->login_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }

  if (oobe_dialog_visible_)
    Shell::Get()->login_screen_controller()->FocusOobeDialog();
}

void LockContentsView::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  // Ignore all metrics except for those listed in |filter|.
  uint32_t filter = DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_WORK_AREA |
                    DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
                    DISPLAY_METRIC_ROTATION;
  if ((filter & changed_metrics) == 0)
    return;

  DoLayout();
}

void LockContentsView::OnLockStateChanged(bool locked) {
  if (!locked) {
    // Successfully unlock the screen.
    Shell::Get()->metrics()->login_metrics_recorder()->RecordNumLoginAttempts(
        unlock_attempt_, true /*success*/);
    unlock_attempt_ = 0;
  }
}

void LockContentsView::OnKeyboardVisibilityChanged(bool is_visible) {
  if (!primary_big_view_ || keyboard_shown_ == is_visible)
    return;

  keyboard_shown_ = is_visible;
  LayoutAuth(CurrentBigUserView(), nullptr /*opt_to_hide*/, false /*animate*/);
}

void LockContentsView::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  LoginBigUserView* big_user = CurrentBigUserView();
  if (big_user && big_user->auth_user())
    big_user->auth_user()->password_view()->Clear();
}

void LockContentsView::ShowAuthErrorMessageForDebug(int unlock_attempt) {
  unlock_attempt_ = unlock_attempt;
  ShowAuthErrorMessage();
}

void LockContentsView::FocusNextWidget(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse) {
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }
}

void LockContentsView::SetLowDensitySpacing(views::View* spacing_middle,
                                            views::View* secondary_view,
                                            int landscape_dist,
                                            int portrait_dist,
                                            bool landscape) {
  int total_width = GetPreferredSize().width();
  int available_width =
      total_width - (primary_big_view_->GetPreferredSize().width() +
                     secondary_view->GetPreferredSize().width());
  if (available_width <= 0) {
    SetPreferredWidthForView(spacing_middle, 0);
    return;
  }

  int desired_width = landscape ? landscape_dist : portrait_dist;
  SetPreferredWidthForView(spacing_middle,
                           std::min(available_width, desired_width));
}

bool LockContentsView::AreMediaControlsEnabled() const {
  return screen_type_ == LockScreen::ScreenType::kLock &&
         !expanded_view_->GetVisible() &&
         Shell::Get()->media_controller()->AreLockScreenMediaKeysEnabled();
}

void LockContentsView::HideMediaControlsLayout() {
  main_view_->RemoveChildView(media_controls_view_->GetMiddleSpacingView());
  main_view_->RemoveChildView(media_controls_view_.get());

  // Don't allow media keys to be used on lock screen since controls are hidden.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(true);

  Layout();
}

void LockContentsView::CreateMediaControlsLayout() {
  // |media_controls_view_| should not be attached to a parent.
  DCHECK(!media_controls_view_->parent());

  // Space between primary user and media controls.
  main_view_->AddChildView(media_controls_view_->GetMiddleSpacingView());

  // Media controls view.
  main_view_->AddChildView(media_controls_view_.get());

  // Set |spacing_middle|.
  AddDisplayLayoutAction(base::BindRepeating(
      &LockContentsView::SetLowDensitySpacing, base::Unretained(this),
      media_controls_view_->GetMiddleSpacingView(), media_controls_view_.get(),
      kDistanceBetweenAuthUserAndMediaControlsLandscapeDp,
      kDistanceBetweenAuthUserAndMediaControlsPortraitDp));

  Layout();
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<LoginUserInfo>& users) {
  DCHECK_LE(users.size(), 2u);

  main_view_->AddChildView(primary_big_view_);

  // Build media controls view. Using base::Unretained(this) is safe here
  // because these callbacks are used by |media_controls_view_|, which is
  // owned by |this|.
  LockScreenMediaControlsView::Callbacks media_controls_callbacks;
  media_controls_callbacks.media_controls_enabled = base::BindRepeating(
      &LockContentsView::AreMediaControlsEnabled, base::Unretained(this));
  media_controls_callbacks.hide_media_controls = base::BindRepeating(
      &LockContentsView::HideMediaControlsLayout, base::Unretained(this));
  media_controls_callbacks.show_media_controls = base::BindRepeating(
      &LockContentsView::CreateMediaControlsLayout, base::Unretained(this));

  media_controls_view_ = std::make_unique<LockScreenMediaControlsView>(
      Shell::Get()->connector(), media_controls_callbacks);
  media_controls_view_->set_owned_by_client();

  if (users.size() > 1) {
    // Space between primary user and secondary user.
    auto* spacing_middle = new NonAccessibleView();
    main_view_->AddChildView(spacing_middle);

    // Build secondary auth user.
    opt_secondary_big_view_ =
        AllocateLoginBigUserView(users[1], false /*is_primary*/);
    main_view_->AddChildView(opt_secondary_big_view_);

    // Set |spacing_middle|.
    AddDisplayLayoutAction(base::BindRepeating(
        &LockContentsView::SetLowDensitySpacing, base::Unretained(this),
        spacing_middle, opt_secondary_big_view_,
        kLowDensityDistanceBetweenUsersInLandscapeDp,
        kLowDensityDistanceBetweenUsersInPortraitDp));
  }
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<LoginUserInfo>& users) {
  // Here is a diagram of this layout:
  //
  //    a A x B y b
  //
  // a, A: spacing_left
  // x: primary_big_view_
  // B: spacing_middle
  // y: users_list_
  // b: spacing_right
  //
  // A and B are fixed-width spaces; a and b are flexible space that consume any
  // additional width.
  //
  // A and B are the reason for custom layout; no layout manager currently
  // supports a fixed-width view that can shrink, but not grow (ie, bounds from
  // [0,x]). Custom layout logic is used instead, which is contained inside of
  // the AddDisplayLayoutAction call below.

  // Construct instances.
  auto* spacing_left = new NonAccessibleView();
  auto* spacing_middle = new NonAccessibleView();
  auto* spacing_right = new NonAccessibleView();
  users_list_ = BuildScrollableUsersListView(users, LoginDisplayStyle::kSmall);

  // Add views as described above.
  main_view_->AddChildView(spacing_left);
  main_view_->AddChildView(primary_big_view_);
  main_view_->AddChildView(spacing_middle);
  main_view_->AddChildView(users_list_);
  main_view_->AddChildView(spacing_right);

  // Set width for the |spacing_*| views.
  AddDisplayLayoutAction(base::BindRepeating(
      [](views::View* host_view, views::View* big_user_view,
         views::View* users_list, views::View* spacing_left,
         views::View* spacing_middle, views::View* spacing_right,
         bool landscape) {
        int total_width = host_view->GetPreferredSize().width();
        int available_width =
            total_width - (big_user_view->GetPreferredSize().width() +
                           users_list->GetPreferredSize().width());

        int left_max_fixed_width =
            landscape ? kMediumDensityMarginLeftOfAuthUserLandscapeDp
                      : kMediumDensityMarginLeftOfAuthUserPortraitDp;
        int right_max_fixed_width =
            landscape ? kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp
                      : kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp;

        int left_flex_weight = landscape ? 1 : 2;
        int right_flex_weight = 1;

        MediumViewLayout medium_layout(
            available_width, left_flex_weight, left_max_fixed_width,
            right_max_fixed_width, right_flex_weight);

        SetPreferredWidthForView(
            spacing_left,
            medium_layout.left_flex_width + medium_layout.left_fixed_width);
        SetPreferredWidthForView(spacing_middle,
                                 medium_layout.right_fixed_width);
        SetPreferredWidthForView(spacing_right, medium_layout.right_flex_width);
      },
      this, primary_big_view_, users_list_, spacing_left, spacing_middle,
      spacing_right));
}

void LockContentsView::CreateHighDensityLayout(
    const std::vector<LoginUserInfo>& users,
    views::BoxLayout* main_layout) {
  // Insert spacing before the auth view.
  auto* fill = new NonAccessibleView();
  main_view_->AddChildView(fill);
  main_layout->SetFlexForView(fill, 1);

  main_view_->AddChildView(primary_big_view_);

  // Insert spacing after the auth view.
  fill = new NonAccessibleView();
  main_view_->AddChildView(fill);
  main_layout->SetFlexForView(fill, 1);

  users_list_ =
      BuildScrollableUsersListView(users, LoginDisplayStyle::kExtraSmall);
  main_view_->AddChildView(users_list_);

  // User list size may change after a display metric change.
  AddDisplayLayoutAction(base::BindRepeating(
      [](views::View* view, bool landscape) { view->SizeToPreferredSize(); },
      users_list_));
}

void LockContentsView::DoLayout() {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetWidget()->GetNativeWindow());

  // Set preferred size before running layout actions, as layout actions may
  // depend on the preferred size to determine layout.
  gfx::Size preferred_size = display.size();
  preferred_size.set_height(preferred_size.height() -
                            keyboard::KeyboardUIController::Get()
                                ->GetWorkspaceOccludedBoundsInScreen()
                                .height());
  SetPreferredSize(preferred_size);

  bool landscape = login_views_utils::ShouldShowLandscape(GetWidget());
  for (auto& action : layout_actions_)
    action.Run(landscape);

  // SizeToPreferredSize will call Layout().
  SizeToPreferredSize();
}

void LockContentsView::LayoutTopHeader() {
  int preferred_width = system_info_->GetPreferredSize().width() +
                        note_action_->GetPreferredSize().width();
  int preferred_height = std::max(system_info_->GetPreferredSize().height(),
                                  note_action_->GetPreferredSize().height());
  top_header_->SetPreferredSize(gfx::Size(preferred_width, preferred_height));
  top_header_->SizeToPreferredSize();
  top_header_->Layout();
  // Position the top header - the origin is offset to the left from the top
  // right corner of the entire view by the width of this top header view.
  top_header_->SetPosition(GetLocalBounds().top_right() -
                           gfx::Vector2d(preferred_width, 0));
}

void LockContentsView::LayoutPublicSessionView() {
  gfx::Rect bounds = GetContentsBounds();
  bounds.ClampToCenteredSize(expanded_view_->GetPreferredSize());
  expanded_view_->SetBoundsRect(bounds);
}

void LockContentsView::AddDisplayLayoutAction(
    const DisplayLayoutAction& layout_action) {
  layout_action.Run(login_views_utils::ShouldShowLandscape(GetWidget()));
  layout_actions_.push_back(layout_action);
}

void LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary(
    bool is_primary) {
  // Do not allow user-swap during authentication.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating())
    return;

  if (is_primary) {
    if (!primary_big_view_->IsAuthEnabled()) {
      LayoutAuth(primary_big_view_, opt_secondary_big_view_, true /*animate*/);
      OnBigUserChanged();
    } else {
      primary_big_view_->RequestFocus();
    }
  } else if (!is_primary && opt_secondary_big_view_) {
    if (!opt_secondary_big_view_->IsAuthEnabled()) {
      LayoutAuth(opt_secondary_big_view_, primary_big_view_, true /*animate*/);
      OnBigUserChanged();
    } else {
      opt_secondary_big_view_->RequestFocus();
    }
  }
}

void LockContentsView::OnAuthenticate(bool auth_success,
                                      bool display_error_messages) {
  if (auth_success) {
    if (auth_error_bubble_->GetVisible())
      auth_error_bubble_->Hide();

    if (detachable_base_error_bubble_->GetVisible())
      detachable_base_error_bubble_->Hide();

    // Now that the user has been authenticated, update the user's last used
    // detachable base (if one is attached). This will prevent further
    // detachable base change notifications from appearing for this base (until
    // the user uses another detachable base).
    if (CurrentBigUserView()->auth_user() &&
        detachable_base_model_->GetPairingStatus() ==
            DetachableBasePairingStatus::kAuthenticated) {
      detachable_base_model_->SetPairedBaseAsLastUsedByUser(
          CurrentBigUserView()->GetCurrentUser().basic_user_info);
    }
  } else {
    ++unlock_attempt_;
    if (display_error_messages)
      ShowAuthErrorMessage();
  }
}

LockContentsView::UserState* LockContentsView::FindStateForUser(
    const AccountId& user) {
  for (UserState& state : users_) {
    if (state.account_id == user)
      return &state;
  }

  return nullptr;
}

void LockContentsView::LayoutAuth(LoginBigUserView* to_update,
                                  LoginBigUserView* opt_to_hide,
                                  bool animate) {
  DCHECK(to_update);

  auto capture_animation_state_pre_layout = [&](LoginBigUserView* view) {
    if (!animate || !view)
      return;
    if (view->auth_user())
      view->auth_user()->CaptureStateForAnimationPreLayout();
  };

  auto enable_auth = [&](LoginBigUserView* view) {
    DCHECK(view);
    if (view->auth_user()) {
      UserState* state = FindStateForUser(
          view->auth_user()->current_user().basic_user_info.account_id);
      uint32_t to_update_auth;
      if (state->force_online_sign_in) {
        to_update_auth = LoginAuthUserView::AUTH_ONLINE_SIGN_IN;
      } else if (state->disable_auth) {
        to_update_auth = LoginAuthUserView::AUTH_DISABLED;
      } else if (state->show_challenge_response_auth) {
        // Currently the challenge-response authentication can't be combined
        // with the password or PIN based one.
        to_update_auth = LoginAuthUserView::AUTH_CHALLENGE_RESPONSE;
      } else {
        to_update_auth = LoginAuthUserView::AUTH_PASSWORD;
        // Need to check |GetKeyboardControllerForView| as the keyboard may be
        // visible, but the keyboard is in a different root window or the view
        // has not been added to the widget. In these cases, the keyboard does
        // not interfere with PIN entry.
        const bool is_keyboard_visible_for_view =
            GetKeyboardControllerForView() ? keyboard_shown_ : false;
        if (state->show_pin && !is_keyboard_visible_for_view)
          to_update_auth |= LoginAuthUserView::AUTH_PIN;
        if (state->enable_tap_auth)
          to_update_auth |= LoginAuthUserView::AUTH_TAP;
        if (state->fingerprint_state != FingerprintState::UNAVAILABLE &&
            state->fingerprint_state !=
                FingerprintState::DISABLED_FROM_TIMEOUT) {
          to_update_auth |= LoginAuthUserView::AUTH_FINGERPRINT;
        }

        // External binary based authentication is only available for unlock.
        if (screen_type_ == LockScreen::ScreenType::kLock &&
            base::FeatureList::IsEnabled(features::kUnlockWithExternalBinary)) {
          to_update_auth |= LoginAuthUserView::AUTH_EXTERNAL_BINARY;
        }
      }

      view->auth_user()->SetAuthMethods(to_update_auth, state->show_pin);
    } else if (view->public_account()) {
      view->public_account()->SetAuthEnabled(true /*enabled*/, animate);
    }
  };

  auto disable_auth = [&](LoginBigUserView* view) {
    if (!view)
      return;
    if (view->auth_user()) {
      view->auth_user()->SetAuthMethods(LoginAuthUserView::AUTH_NONE,
                                        false /*can_use_pin*/);
    } else if (view->public_account()) {
      view->public_account()->SetAuthEnabled(false /*enabled*/, animate);
    }
  };

  auto apply_animation_post_layout = [&](LoginBigUserView* view) {
    if (!animate || !view)
      return;
    if (view->auth_user())
      view->auth_user()->ApplyAnimationPostLayout();
  };

  // The high-level layout flow:
  capture_animation_state_pre_layout(to_update);
  capture_animation_state_pre_layout(opt_to_hide);
  enable_auth(to_update);
  disable_auth(opt_to_hide);
  Layout();
  apply_animation_post_layout(to_update);
  apply_animation_post_layout(opt_to_hide);
}

void LockContentsView::SwapToBigUser(int user_index) {
  // Do not allow user-swap during authentication.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating())
    return;

  DCHECK(users_list_);
  LoginUserView* view = users_list_->user_view_at(user_index);
  DCHECK(view);
  LoginUserInfo previous_big_user = primary_big_view_->GetCurrentUser();
  LoginUserInfo new_big_user = view->current_user();

  view->UpdateForUser(previous_big_user, true /*animate*/);
  primary_big_view_->UpdateForUser(new_big_user);
  LayoutAuth(primary_big_view_, nullptr, true /*animate*/);
  OnBigUserChanged();
}

void LockContentsView::OnRemoveUserWarningShown(bool is_primary) {
  Shell::Get()->login_screen_controller()->OnRemoveUserWarningShown();
}

void LockContentsView::RemoveUser(bool is_primary) {
  // Do not allow removing a user during authentication, such as if the user
  // tried to remove the currently authenticating user.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating())
    return;

  LoginBigUserView* to_remove =
      is_primary ? primary_big_view_ : opt_secondary_big_view_;
  DCHECK(to_remove->GetCurrentUser().can_remove);
  AccountId user = to_remove->GetCurrentUser().basic_user_info.account_id;

  // Ask chrome to remove the user.
  Shell::Get()->login_screen_controller()->RemoveUser(user);

  // Display the new user list less |user|.
  std::vector<LoginUserInfo> new_users;
  if (!is_primary)
    new_users.push_back(primary_big_view_->GetCurrentUser());
  if (is_primary && opt_secondary_big_view_)
    new_users.push_back(opt_secondary_big_view_->GetCurrentUser());
  if (users_list_) {
    for (int i = 0; i < users_list_->user_count(); ++i) {
      new_users.push_back(users_list_->user_view_at(i)->current_user());
    }
  }
  data_dispatcher_->SetUserList(new_users);
}

void LockContentsView::OnBigUserChanged() {
  const LoginUserInfo& big_user = CurrentBigUserView()->GetCurrentUser();
  const AccountId big_user_account_id = big_user.basic_user_info.account_id;

  CurrentBigUserView()->RequestFocus();

  Shell::Get()->login_screen_controller()->OnFocusPod(big_user_account_id);
  UpdateEasyUnlockIconForUser(big_user_account_id);

  if (unlock_attempt_ > 0) {
    // Times a password was incorrectly entered until user gives up (change
    // user pod).
    Shell::Get()->metrics()->login_metrics_recorder()->RecordNumLoginAttempts(
        unlock_attempt_, false /*success*/);

    // Reset unlock attempt when the auth user changes.
    unlock_attempt_ = 0;
  }

  // http://crbug/866790: After Supervised Users are deprecated, remove this.
  if (ash::features::IsSupervisedUserDeprecationNoticeEnabled() &&
      big_user.basic_user_info.type == user_manager::USER_TYPE_SUPERVISED) {
    base::string16 message = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_LEGACY_SUPERVISED_EXPIRATION_WARNING);
    // Shows supervised user deprecation message as a persistent error bubble.
    views::Label* label =
        new views::Label(message, views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT,
                         views::style::STYLE_PRIMARY);
    label->SetMultiLine(true);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(SK_ColorWHITE);

    supervised_user_deprecation_bubble_->SetContent(label);
    supervised_user_deprecation_bubble_->SetAnchorView(
        CurrentBigUserView()->auth_user()->password_view());
    supervised_user_deprecation_bubble_->Show();
  } else if (supervised_user_deprecation_bubble_->GetVisible()) {
    supervised_user_deprecation_bubble_->Hide();
  }

  // The new auth user might have different last used detachable base - make
  // sure the detachable base pairing error is updated if needed.
  OnDetachableBasePairingStatusChanged(
      detachable_base_model_->GetPairingStatus());

  if (!detachable_base_error_bubble_->GetVisible())
    CurrentBigUserView()->RequestFocus();
}

void LockContentsView::UpdateEasyUnlockIconForUser(const AccountId& user) {
  // Try to find an big view for |user|. If there is none, there is no state to
  // update.
  LoginBigUserView* big_view =
      TryToFindBigUser(user, false /*require_auth_active*/);
  if (!big_view || !big_view->auth_user())
    return;

  UserState* state = FindStateForUser(user);
  DCHECK(state);

  // Hide easy unlock icon if there is no data is available.
  if (!state->easy_unlock_state) {
    big_view->auth_user()->SetEasyUnlockIcon(EasyUnlockIconId::NONE,
                                             base::string16());
    return;
  }

  // TODO(jdufault): Make easy unlock backend always send aria_label, right now
  // it is only sent if there is no tooltip.
  base::string16 accessibility_label = state->easy_unlock_state->aria_label;
  if (accessibility_label.empty())
    accessibility_label = state->easy_unlock_state->tooltip;

  big_view->auth_user()->SetEasyUnlockIcon(state->easy_unlock_state->icon,
                                           accessibility_label);
}

LoginBigUserView* LockContentsView::CurrentBigUserView() {
  if (opt_secondary_big_view_ && opt_secondary_big_view_->IsAuthEnabled()) {
    DCHECK(!primary_big_view_->IsAuthEnabled());
    return opt_secondary_big_view_;
  }

  return primary_big_view_;
}

void LockContentsView::ShowAuthErrorMessage() {
  LoginBigUserView* big_view = CurrentBigUserView();
  if (!big_view->auth_user())
    return;

  // Show gaia signin if this is login and the user has failed too many times.
  if (screen_type_ == LockScreen::ScreenType::kLogin &&
      unlock_attempt_ >= kLoginAttemptsBeforeGaiaDialog) {
    Shell::Get()->login_screen_controller()->ShowGaiaSignin(
        true /*can_close*/,
        big_view->auth_user()->current_user().basic_user_info.account_id);
    return;
  }

  base::string16 error_text = l10n_util::GetStringUTF16(
      unlock_attempt_ > 1 ? IDS_ASH_LOGIN_ERROR_AUTHENTICATING_2ND_TIME
                          : IDS_ASH_LOGIN_ERROR_AUTHENTICATING);
  ImeController* ime_controller = Shell::Get()->ime_controller();
  if (ime_controller->IsCapsLockEnabled()) {
    error_text += base::ASCIIToUTF16(" ") +
                  l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_CAPS_LOCK_HINT);
  }

  base::Optional<int> bold_start;
  int bold_length = 0;
  // Display a hint to switch keyboards if there are other active input
  // methods in clamshell mode.
  if (ime_controller->available_imes().size() > 1 && !IsTabletMode()) {
    error_text += base::ASCIIToUTF16(" ");
    bold_start = error_text.length();
    base::string16 shortcut =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SWITCH_SHORTCUT);
    bold_length = shortcut.length();

    size_t shortcut_offset_in_string;
    error_text +=
        l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_ERROR_KEYBOARD_SWITCH_HINT,
                                   shortcut, &shortcut_offset_in_string);
    *bold_start += shortcut_offset_in_string;
  }

  views::StyledLabel* label = new views::StyledLabel(error_text, this);
  MakeSectionBold(label, error_text, bold_start, bold_length);
  label->SetAutoColorReadabilityEnabled(false);

  auto* learn_more_button = new AuthErrorLearnMoreButton(auth_error_bubble_);

  auto* container = new NonAccessibleView(kAuthErrorContainerName);
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kLearnMoreButtonVerticalSpacingDp));
  container->AddChildView(label);
  container->AddChildView(learn_more_button);

  auth_error_bubble_->SetAnchorView(big_view->auth_user()->password_view());
  auth_error_bubble_->SetContent(container);
  auth_error_bubble_->SetPersistent(false);
  auth_error_bubble_->Show();
}

void LockContentsView::OnEasyUnlockIconHovered() {
  LoginBigUserView* big_view = CurrentBigUserView();
  if (!big_view->auth_user())
    return;

  UserState* state =
      FindStateForUser(big_view->GetCurrentUser().basic_user_info.account_id);
  DCHECK(state);
  DCHECK(state->easy_unlock_state);

  if (!state->easy_unlock_state->tooltip.empty()) {
    tooltip_bubble_->SetAnchorView(big_view->auth_user()->password_view());
    tooltip_bubble_->SetText(state->easy_unlock_state->tooltip);
    tooltip_bubble_->Show();
  }
}

void LockContentsView::OnEasyUnlockIconTapped() {
  UserState* state = FindStateForUser(
      CurrentBigUserView()->GetCurrentUser().basic_user_info.account_id);
  DCHECK(state);
  DCHECK(state->easy_unlock_state);

  if (state->easy_unlock_state->hardlock_on_click) {
    AccountId user =
        CurrentBigUserView()->GetCurrentUser().basic_user_info.account_id;
    Shell::Get()->login_screen_controller()->HardlockPod(user);
    // TODO(jdufault): This should get called as a result of HardlockPod.
    OnTapToUnlockEnabledForUserChanged(user, false /*enabled*/);
  }
}

void LockContentsView::OnParentAccessValidationFinished(
    const AccountId& account_id,
    bool access_granted) {
  LockContentsView::UserState* state = FindStateForUser(account_id);
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(
      state && state->disable_auth && !access_granted);
}

keyboard::KeyboardUIController* LockContentsView::GetKeyboardControllerForView()
    const {
  return GetWidget() ? GetKeyboardControllerForWidget(GetWidget()) : nullptr;
}

void LockContentsView::OnPublicAccountTapped(bool is_primary) {
  const LoginBigUserView* user = CurrentBigUserView();
  // If the pod should not show an expanded view, tapping on it will launch
  // Public Session immediately.
  if (!user->GetCurrentUser().public_account_info->show_expanded_view) {
    std::string default_input_method;
    for (const auto& keyboard :
         user->GetCurrentUser().public_account_info->keyboard_layouts) {
      if (keyboard.selected) {
        default_input_method = keyboard.ime_id;
        break;
      }
    }
    Shell::Get()->login_screen_controller()->LaunchPublicSession(
        user->GetCurrentUser().basic_user_info.account_id,
        user->GetCurrentUser().public_account_info->default_locale,
        default_input_method);
    return;
  }

  // Set the public account user to be the active user.
  SwapActiveAuthBetweenPrimaryAndSecondary(is_primary);

  // Update expanded_view_ in case CurrentBigUserView has changed.
  // 1. It happens when the active big user is changed. For example both
  // primary and secondary big user are public account and user switches from
  // primary to secondary.
  // 2. LoginUserInfo in the big user could be changed if we get updates from
  // OnPublicSessionDisplayNameChanged and OnPublicSessionLocalesChanged.
  expanded_view_->UpdateForUser(user->GetCurrentUser());
  SetDisplayStyle(DisplayStyle::kExclusivePublicAccountExpandedView);
}

LoginBigUserView* LockContentsView::AllocateLoginBigUserView(
    const LoginUserInfo& user,
    bool is_primary) {
  LoginAuthUserView::Callbacks auth_user_callbacks;
  auth_user_callbacks.on_auth = base::BindRepeating(
      &LockContentsView::OnAuthenticate, base::Unretained(this)),
  auth_user_callbacks.on_tap = base::BindRepeating(
      &LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary,
      base::Unretained(this), is_primary),
  auth_user_callbacks.on_remove_warning_shown =
      base::BindRepeating(&LockContentsView::OnRemoveUserWarningShown,
                          base::Unretained(this), is_primary);
  auth_user_callbacks.on_remove = base::BindRepeating(
      &LockContentsView::RemoveUser, base::Unretained(this), is_primary);
  auth_user_callbacks.on_easy_unlock_icon_hovered = base::BindRepeating(
      &LockContentsView::OnEasyUnlockIconHovered, base::Unretained(this));
  auth_user_callbacks.on_easy_unlock_icon_tapped = base::BindRepeating(
      &LockContentsView::OnEasyUnlockIconTapped, base::Unretained(this));

  LoginPublicAccountUserView::Callbacks public_account_callbacks;
  public_account_callbacks.on_tap = auth_user_callbacks.on_tap;
  public_account_callbacks.on_public_account_tapped =
      base::BindRepeating(&LockContentsView::OnPublicAccountTapped,
                          base::Unretained(this), is_primary);

  return new LoginBigUserView(user, auth_user_callbacks,
                              public_account_callbacks);
}

LoginBigUserView* LockContentsView::TryToFindBigUser(const AccountId& user,
                                                     bool require_auth_active) {
  LoginBigUserView* view = nullptr;

  // Find auth instance.
  if (primary_big_view_ &&
      primary_big_view_->GetCurrentUser().basic_user_info.account_id == user) {
    view = primary_big_view_;
  } else if (opt_secondary_big_view_ &&
             opt_secondary_big_view_->GetCurrentUser()
                     .basic_user_info.account_id == user) {
    view = opt_secondary_big_view_;
  }

  // Make sure auth instance is active if required.
  if (require_auth_active && view && !view->IsAuthEnabled())
    view = nullptr;

  return view;
}

LoginUserView* LockContentsView::TryToFindUserView(const AccountId& user) {
  // Try to find |user| in big user view first.
  LoginBigUserView* big_view =
      TryToFindBigUser(user, false /*require_auth_active*/);
  if (big_view)
    return big_view->GetUserView();

  // Try to find |user| in users_list_.
  return users_list_->GetUserView(user);
}

ScrollableUsersListView* LockContentsView::BuildScrollableUsersListView(
    const std::vector<LoginUserInfo>& users,
    LoginDisplayStyle display_style) {
  auto* view = new ScrollableUsersListView(
      users,
      base::BindRepeating(&LockContentsView::SwapToBigUser,
                          base::Unretained(this)),
      display_style);
  view->ClipHeightTo(view->contents()->size().height(), size().height());
  return view;
}

void LockContentsView::SetDisplayStyle(DisplayStyle style) {
  const bool show_expanded_view =
      style == DisplayStyle::kExclusivePublicAccountExpandedView;
  expanded_view_->SetVisible(show_expanded_view);
  main_view_->SetVisible(!show_expanded_view);
  top_header_->SetVisible(!show_expanded_view);
  Layout();
}

void LockContentsView::RegisterAccelerators() {
  // Accelerators that apply on login and lock:
  accel_map_[ui::Accelerator(ui::VKEY_RIGHT, 0)] =
      AcceleratorAction::kFocusNextUser;
  accel_map_[ui::Accelerator(ui::VKEY_LEFT, 0)] =
      AcceleratorAction::kFocusPreviousUser;
  accel_map_[ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)] =
      AcceleratorAction::kShowSystemInfo;

  // Login-only accelerators:
  if (screen_type_ == LockScreen::ScreenType::kLogin) {
    accel_map_[ui::Accelerator(ui::VKEY_I,
                               ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)] =
        AcceleratorAction::kShowFeedback;

    // Show reset conflicts with rotate screen when --ash-dev-shortcuts is
    // passed. Favor --ash-dev-shortcuts since that is explicitly added.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAshDeveloperShortcuts)) {
      accel_map_[ui::Accelerator(ui::VKEY_R, ui::EF_CONTROL_DOWN |
                                                 ui::EF_SHIFT_DOWN |
                                                 ui::EF_ALT_DOWN)] =
          AcceleratorAction::kShowResetScreen;
    }
  }

  // Register the accelerators.
  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  for (const auto& item : accel_map_)
    controller->Register({item.first}, this);
}

void LockContentsView::PerformAction(AcceleratorAction action) {
  switch (action) {
    case AcceleratorAction::kFocusNextUser:
      FocusNextUser();
      break;
    case AcceleratorAction::kFocusPreviousUser:
      FocusPreviousUser();
      break;
    case AcceleratorAction::kShowSystemInfo:
      if (!system_info_->GetVisible()) {
        system_info_->SetVisible(true);
        LayoutTopHeader();
      }
      break;
    case AcceleratorAction::kShowFeedback:
      Shell::Get()->login_screen_controller()->ShowFeedback();
      break;
    case AcceleratorAction::kShowResetScreen:
      Shell::Get()->login_screen_controller()->ShowResetScreen();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace ash
