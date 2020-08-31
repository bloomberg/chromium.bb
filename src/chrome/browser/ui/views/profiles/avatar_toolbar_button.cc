// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"

namespace {

// Note that the non-touchable icon size is larger than the default to make the
// avatar icon easier to read.
constexpr int kIconSizeForNonTouchUi = 22;

}  // namespace

// static
const char AvatarToolbarButton::kAvatarToolbarButtonClassName[] =
    "AvatarToolbarButton";

AvatarToolbarButton::AvatarToolbarButton(Browser* browser)
    : AvatarToolbarButton(browser, nullptr) {}

AvatarToolbarButton::AvatarToolbarButton(Browser* browser,
                                         ToolbarIconContainerView* parent)
    : ToolbarButton(nullptr),
      delegate_(std::make_unique<AvatarToolbarButtonDelegate>()),
      browser_(browser),
      parent_(parent) {
  delegate_->Init(this, browser_->profile());

  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON);

  set_tag(IDC_SHOW_AVATAR_MENU);

  // The avatar should not flip with RTL UI. This does not affect text rendering
  // and LabelButton image/label placement is still flipped like usual.
  EnableCanvasFlippingForRTLUI(false);

  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);

  Init();

  // For consistency with identity representation, we need to have the avatar on
  // the left and the (potential) user name on the right.
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // TODO(crbug.com/922525): DCHECK(parent_) instead of the if, once we always
  // have a parent.
  if (parent_)
    parent_->AddObserver(this);
}

AvatarToolbarButton::~AvatarToolbarButton() {
  // TODO(crbug.com/922525): Remove the if, once we always have a parent.
  if (parent_)
    parent_->RemoveObserver(this);
}

void AvatarToolbarButton::UpdateIcon() {
  // If widget isn't set, the button doesn't have access to the theme provider
  // to set colors. Defer updating until AddedToWidget(). This may get called as
  // a result of OnUserIdentityChanged() called from the constructor when the
  // button is not yet added to the ToolbarView's hierarchy.
  if (!GetWidget())
    return;

  gfx::Image gaia_account_image = delegate_->GetGaiaAccountImage();
  for (auto state : kButtonStates)
    SetImage(state, GetAvatarIcon(state, gaia_account_image));
  delegate_->ShowIdentityAnimation(gaia_account_image);
}

void AvatarToolbarButton::UpdateText() {
  base::Optional<SkColor> color;
  base::string16 text;

  switch (delegate_->GetState()) {
    case State::kIncognitoProfile: {
      int incognito_window_count = delegate_->GetIncognitoWindowsCount();
      SetAccessibleName(l10n_util::GetPluralStringFUTF16(
          IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE, incognito_window_count));
      text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_INCOGNITO,
                                              incognito_window_count);
      break;
    }
    case State::kAnimatedUserIdentity: {
      text = delegate_->GetShortProfileName();
      break;
    }
    case State::kPasswordsOnlySyncError:
    case State::kSyncError:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleRed300, gfx::kGoogleRed600,
          gfx::kGoogleRed050, gfx::kGoogleRed900);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
      break;
    case State::kSyncPaused:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleBlue300, gfx::kGoogleBlue600,
          gfx::kGoogleBlue050, gfx::kGoogleBlue900);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
      break;
    case State::kGuestSession:
      text = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
      break;
    case State::kGenericProfile:
    case State::kNormal:
      if (delegate_->IsHighlightAnimationVisible()) {
        color = AdjustHighlightColorForContrast(
            GetThemeProvider(), gfx::kGoogleBlue300, gfx::kGoogleBlue600,
            gfx::kGoogleBlue050, gfx::kGoogleBlue900);
      }
      break;
  }

  SetInsets();
  SetTooltipText(GetAvatarTooltipText());
  SetHighlight(text, color);

  // TODO(crbug.com/1078221): this is a hack because toolbar buttons don't
  // correctly calculate their preferred size until they've been laid out once
  // or twice, because they modify their own borders and insets in response to
  // their size and have their own preferred size caching mechanic. These should
  // both ideally be handled with a modern layout manager instead.
  //
  // In the meantime, to ensure that correct (or nearly correct) bounds are set,
  // we will force a resize then invalidate layout to let the layout manager
  // take over.
  SizeToPreferredSize();
  InvalidateLayout();
}

void AvatarToolbarButton::ShowAvatarHighlightAnimation() {
  delegate_->ShowHighlightAnimation();
}

bool AvatarToolbarButton::IsParentHighlighted() const {
  return parent_ && parent_->IsHighlighted();
}

void AvatarToolbarButton::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AvatarToolbarButton::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AvatarToolbarButton::NotifyHighlightAnimationFinished() {
  for (AvatarToolbarButton::Observer& observer : observer_list_)
    observer.OnAvatarHighlightAnimationFinished();
}

const char* AvatarToolbarButton::GetClassName() const {
  return kAvatarToolbarButtonClassName;
}

void AvatarToolbarButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  delegate_->NotifyClick();
  // TODO(bsep): Other toolbar buttons have ToolbarView as a listener and let it
  // call ExecuteCommandWithDisposition on their behalf. Unfortunately, it's not
  // possible to plumb IsKeyEvent through, so this has to be a special case.
  browser_->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      event.IsKeyEvent());
}

void AvatarToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  delegate_->OnMouseExited();
  ToolbarButton::OnMouseExited(event);
}

void AvatarToolbarButton::OnBlur() {
  delegate_->OnBlur();
  ToolbarButton::OnBlur();
}

void AvatarToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnHighlightChanged() {
  DCHECK(parent_);
  delegate_->OnHighlightChanged();
}

base::string16 AvatarToolbarButton::GetAvatarTooltipText() const {
  switch (delegate_->GetState()) {
    case State::kIncognitoProfile:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);
    case State::kGuestSession:
      return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
    case State::kGenericProfile:
      return l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL);
    case State::kAnimatedUserIdentity:
      return delegate_->GetShortProfileName();
    case State::kPasswordsOnlySyncError:
      return l10n_util::GetStringFUTF16(
          IDS_AVATAR_BUTTON_SYNC_ERROR_PASSWORDS_TOOLTIP,
          delegate_->GetProfileName());
    case State::kSyncError:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP,
                                        delegate_->GetProfileName());
    case State::kSyncPaused:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED_TOOLTIP,
                                        delegate_->GetProfileName());
    case State::kNormal:
      return delegate_->GetProfileName();
  }
  NOTREACHED();
  return base::string16();
}

gfx::ImageSkia AvatarToolbarButton::GetAvatarIcon(
    ButtonState state,
    const gfx::Image& gaia_account_image) const {
  const int icon_size = ui::TouchUiController::Get()->touch_ui()
                            ? kDefaultTouchableIconSize
                            : kIconSizeForNonTouchUi;
  SkColor icon_color = GetForegroundColor(state);

  switch (delegate_->GetState()) {
    case State::kIncognitoProfile:
      return gfx::CreateVectorIcon(kIncognitoIcon, icon_size, icon_color);
    case State::kGuestSession:
      return profiles::GetGuestAvatar(icon_size);
    case State::kGenericProfile:
      return gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size,
                                   icon_color);
    case State::kAnimatedUserIdentity:
    case State::kPasswordsOnlySyncError:
    case State::kSyncError:
    case State::kSyncPaused:
    case State::kNormal:
      return profiles::GetSizedAvatarIcon(
                 delegate_->GetProfileAvatarImage(gaia_account_image), true,
                 icon_size, icon_size, profiles::SHAPE_CIRCLE)
          .AsImageSkia();
  }
  NOTREACHED();
  return gfx::ImageSkia();
}

void AvatarToolbarButton::SetInsets() {
  // In non-touch mode we use a larger-than-normal icon size for avatars so we
  // need to compensate it by smaller insets.
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  gfx::Insets layout_insets(
      touch_ui ? 0 : (kDefaultIconSize - kIconSizeForNonTouchUi) / 2);
  SetLayoutInsetDelta(layout_insets);
}
