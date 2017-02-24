// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include <algorithm>

#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
const int kButtonGroup = 6666;

#if defined(OS_WIN) || defined(OS_CHROMEOS)
const bool kIsOkButtonOnLeftSide = true;
#else
const bool kIsOkButtonOnLeftSide = false;
#endif

// Returns true if the given view should be shown (i.e. exists and is
// visible).
bool ShouldShow(View* view) {
  return view && view->visible();
}

// Do the layout for a button.
void LayoutButton(LabelButton* button,
                  gfx::Rect* row_bounds,
                  int button_height) {
  if (!button)
    return;

  const gfx::Size size = button->GetPreferredSize();
  row_bounds->set_width(row_bounds->width() - size.width());
  DCHECK_LE(button_height, row_bounds->height());
  button->SetBounds(
      row_bounds->right(),
      row_bounds->y() + (row_bounds->height() - button_height) / 2,
      size.width(), button_height);
  const int spacing =
      ViewsDelegate::GetInstance()->GetDialogRelatedButtonHorizontalSpacing();
  row_bounds->set_width(row_bounds->width() - spacing);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      button_row_insets_(
          ViewsDelegate::GetInstance()->GetDialogButtonInsets()) {
  // Doing this now ensures this accelerator will have lower priority than
  // one set by the contents view.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

DialogClientView::~DialogClientView() {
}

void DialogClientView::AcceptWindow() {
  // Only notify the delegate once. See |delegate_allowed_close_|'s comment.
  if (!delegate_allowed_close_ && GetDialogDelegate()->Accept()) {
    delegate_allowed_close_ = true;
    GetWidget()->Close();
  }
}

void DialogClientView::CancelWindow() {
  // Only notify the delegate once. See |delegate_allowed_close_|'s comment.
  if (!delegate_allowed_close_ && GetDialogDelegate()->Cancel()) {
    delegate_allowed_close_ = true;
    GetWidget()->Close();
  }
}

void DialogClientView::UpdateDialogButtons() {
  SetupViews();
  SetupFocusChain();
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, ClientView overrides:

bool DialogClientView::CanClose() {
  // If the dialog is closing but no Accept or Cancel action has been performed
  // before, it's a Close action.
  if (!delegate_allowed_close_)
    delegate_allowed_close_ = GetDialogDelegate()->Close();
  return delegate_allowed_close_;
}

DialogClientView* DialogClientView::AsDialogClientView() {
  return this;
}

const DialogClientView* DialogClientView::AsDialogClientView() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

gfx::Size DialogClientView::GetPreferredSize() const {
  // Initialize the size to fit the buttons and extra view row.
  gfx::Size size(
      (ok_button_ ? ok_button_->GetPreferredSize().width() : 0) +
          (cancel_button_ ? cancel_button_->GetPreferredSize().width() : 0) +
          (cancel_button_ && ok_button_
               ? ViewsDelegate::GetInstance()
                     ->GetDialogRelatedButtonHorizontalSpacing()
               : 0) +
          (ShouldShow(extra_view_) ? extra_view_->GetPreferredSize().width()
                                   : 0) +
          GetExtraViewSpacing(),
      0);

  int buttons_height = GetButtonsAndExtraViewRowHeight();
  if (buttons_height != 0) {
    size.Enlarge(0, buttons_height);
    // Inset the buttons and extra view.
    const gfx::Insets insets = GetButtonRowInsets();
    size.Enlarge(insets.width(), insets.height());
  }

  // Increase the size as needed to fit the contents view.
  // NOTE: The contents view is not inset on the top or side client view edges.
  gfx::Size contents_size = contents_view()->GetPreferredSize();
  size.Enlarge(0, contents_size.height());
  size.set_width(std::max(size.width(), contents_size.width()));

  size.SetToMax(minimum_size_);

  return size;
}

void DialogClientView::Layout() {
  gfx::Rect bounds = GetContentsBounds();

  // Layout the row containing the buttons and the extra view.
  if (has_dialog_buttons() || ShouldShow(extra_view_)) {
    gfx::Insets button_row_insets = GetButtonRowInsets();
    // Don't apply the top inset here because it's supposed to go above the
    // buttons, not at the top of the dialog.
    bounds.Inset(button_row_insets.left(), 0, button_row_insets.right(),
                 button_row_insets.bottom());
    const int height = GetButtonsAndExtraViewRowHeight();
    gfx::Rect row_bounds(bounds.x(), bounds.bottom() - height,
                         bounds.width(), height);
    // If the |extra_view_| is a also button, then the |button_height| is the
    // maximum height of the three buttons, otherwise it is the maximum height
    // of the ok and cancel buttons.
    const int button_height =
        CustomButton::AsCustomButton(extra_view_) ? height : GetButtonHeight();
    if (kIsOkButtonOnLeftSide) {
      LayoutButton(cancel_button_, &row_bounds, button_height);
      LayoutButton(ok_button_, &row_bounds, button_height);
    } else {
      LayoutButton(ok_button_, &row_bounds, button_height);
      LayoutButton(cancel_button_, &row_bounds, button_height);
    }
    if (extra_view_) {
      int custom_padding = 0;
      if (has_dialog_buttons() &&
          GetDialogDelegate()->GetExtraViewPadding(&custom_padding)) {
        // The padding between buttons applied in LayoutButton() will already
        // have accounted for some of the distance here.
        custom_padding -= ViewsDelegate::GetInstance()
                              ->GetDialogRelatedButtonHorizontalSpacing();
        row_bounds.set_width(row_bounds.width() - custom_padding);
      }
      row_bounds.set_width(std::min(row_bounds.width(),
                                    extra_view_->GetPreferredSize().width()));
      extra_view_->SetBoundsRect(row_bounds);
    }

    if (height > 0) {
      // Inset to the top of the buttons, plus their top padding, in order to
      // exclude that area from the content view's bounds.
      bounds.Inset(0, 0, 0, height + button_row_insets.top());
    }
  }

  // Layout the contents view to the top and side edges of the contents bounds.
  // NOTE: The local insets do not apply to the contents view sides or top.
  const gfx::Rect contents_bounds = GetContentsBounds();
  contents_view()->SetBounds(contents_bounds.x(), contents_bounds.y(),
      contents_bounds.width(), bounds.bottom() - contents_bounds.y());
}

bool DialogClientView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);

  GetWidget()->Close();
  return true;
}

void DialogClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View* const child = details.child;

  // Dialogs must add children to contents_view(), not client_view().
  if (details.is_add && details.parent == this) {
    DCHECK(child == contents_view() || child == ok_button_ ||
           child == cancel_button_ || child == extra_view_);
  }

  ClientView::ViewHierarchyChanged(details);
  if (details.is_add && child == this) {
    UpdateDialogButtons();
  } else if (!details.is_add) {
    if (child == ok_button_)
      ok_button_ = nullptr;
    else if (child == cancel_button_)
      cancel_button_ = nullptr;
    else if (child == extra_view_)
      extra_view_ = nullptr;
  }
}

void DialogClientView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  // The old dialog style needs an explicit background color, while the new
  // dialog style simply inherits the bubble's frame view color.
  const DialogDelegate* dialog = GetDialogDelegate();

  if (dialog && !dialog->ShouldUseCustomFrame()) {
    set_background(views::Background::CreateSolidBackground(GetNativeTheme()->
        GetSystemColor(ui::NativeTheme::kColorId_DialogBackground)));
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, ButtonListener implementation:

void DialogClientView::ButtonPressed(Button* sender, const ui::Event& event) {
  // Check for a valid delegate to avoid handling events after destruction.
  if (!GetDialogDelegate())
    return;

  if (sender == ok_button_)
    AcceptWindow();
  else if (sender == cancel_button_)
    CancelWindow();
  else
    NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  return GetWidget()->widget_delegate()->AsDialogDelegate();
}

void DialogClientView::ChildPreferredSizeChanged(View* child) {
  if (child == extra_view_)
    Layout();
}

void DialogClientView::ChildVisibilityChanged(View* child) {
  ChildPreferredSizeChanged(child);
}

LabelButton* DialogClientView::CreateDialogButton(ui::DialogButton type) {
  const base::string16 title = GetDialogDelegate()->GetDialogButtonLabel(type);
  LabelButton* button = nullptr;

  const bool is_default =
      GetDialogDelegate()->GetDefaultDialogButton() == type &&
      (type != ui::DIALOG_BUTTON_CANCEL ||
       PlatformStyle::kDialogDefaultButtonCanBeCancel);

  // The default button is always blue in Harmony.
  if (is_default && (ui::MaterialDesignController::IsSecondaryUiMaterial() ||
                     GetDialogDelegate()->ShouldDefaultButtonBeBlue())) {
    button = MdTextButton::CreateSecondaryUiBlueButton(this, title);
  } else {
    button = MdTextButton::CreateSecondaryUiButton(this, title);
  }

  const int minimum_width =
      ViewsDelegate::GetInstance()->GetDialogButtonMinimumWidth();
  button->SetMinSize(gfx::Size(minimum_width, 0));

  button->SetGroup(kButtonGroup);
  return button;
}

int DialogClientView::GetButtonHeight() const {
  return std::max(
      ok_button_ ? ok_button_->GetPreferredSize().height() : 0,
      cancel_button_ ? cancel_button_->GetPreferredSize().height() : 0);
}

int DialogClientView::GetExtraViewHeight() const {
  return ShouldShow(extra_view_) ? extra_view_->GetPreferredSize().height() : 0;
}

int DialogClientView::GetButtonsAndExtraViewRowHeight() const {
  return std::max(GetExtraViewHeight(), GetButtonHeight());
}

gfx::Insets DialogClientView::GetButtonRowInsets() const {
  if (GetButtonsAndExtraViewRowHeight() == 0)
    return gfx::Insets();

  // Some subclasses of DialogClientView, in order to do their own layout, set
  // button_row_insets_ to gfx::Insets(). To avoid breaking behavior of those
  // dialogs, supplying 0 for the top inset of the row falls back to
  // ViewsDelegate::GetRelatedControlVerticalSpacing.
  // TODO(bsep): The top inset should never be 0 when harmony is enabled.
  const int top = button_row_insets_.top() == 0
                      ? ViewsDelegate::GetInstance()
                            ->GetDialogRelatedControlVerticalSpacing()
                      : button_row_insets_.top();
  return gfx::Insets(top, button_row_insets_.left(),
                     button_row_insets_.bottom(), button_row_insets_.right());
}

void DialogClientView::SetupFocusChain() {
  // Create a vector of child views in the order of intended focus.
  std::vector<View*> child_views;
  child_views.push_back(contents_view());
  child_views.push_back(extra_view_);
  if (kIsOkButtonOnLeftSide) {
    child_views.push_back(ok_button_);
    child_views.push_back(cancel_button_);
  } else {
    child_views.push_back(cancel_button_);
    child_views.push_back(ok_button_);
  }

  // Remove all null views from the vector.
  child_views.erase(
      std::remove(child_views.begin(), child_views.end(), nullptr),
      child_views.end());

  // Setup focus by reordering views. It is not safe to use SetNextFocusableView
  // since child views may be added externally to this view.
  for (size_t i = 0; i < child_views.size(); i++)
    ReorderChildView(child_views[i], i);
}

int DialogClientView::GetExtraViewSpacing() const {
  if (!ShouldShow(extra_view_) || !has_dialog_buttons())
    return 0;

  int extra_view_padding = 0;
  if (GetDialogDelegate()->GetExtraViewPadding(&extra_view_padding))
    return extra_view_padding;

  return ViewsDelegate::GetInstance()
      ->GetDialogRelatedButtonHorizontalSpacing();
}

void DialogClientView::SetupViews() {
  const int buttons = GetDialogDelegate()->GetDialogButtons();

  if (buttons & ui::DIALOG_BUTTON_OK) {
    if (!ok_button_) {
      ok_button_ = CreateDialogButton(ui::DIALOG_BUTTON_OK);
      AddChildView(ok_button_);
    }

    GetDialogDelegate()->UpdateButton(ok_button_, ui::DIALOG_BUTTON_OK);
  } else if (ok_button_) {
    delete ok_button_;
    ok_button_ = nullptr;
  }

  if (buttons & ui::DIALOG_BUTTON_CANCEL) {
    if (!cancel_button_) {
      cancel_button_ = CreateDialogButton(ui::DIALOG_BUTTON_CANCEL);
      AddChildView(cancel_button_);
    }

    GetDialogDelegate()->UpdateButton(cancel_button_, ui::DIALOG_BUTTON_CANCEL);
  } else if (cancel_button_) {
    delete cancel_button_;
    cancel_button_ = nullptr;
  }

  if (extra_view_)
    return;

  extra_view_ = GetDialogDelegate()->CreateExtraView();
  if (extra_view_) {
    extra_view_->SetGroup(kButtonGroup);
    AddChildView(extra_view_);
  }
}

}  // namespace views
