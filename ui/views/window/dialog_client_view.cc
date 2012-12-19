// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>
#endif

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/hit_test.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/chrome_style.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
namespace {

const int kDialogMinButtonWidth = 75;
const int kDialogButtonContentSpacing = 5;

const int kChromeStyleDialogButtonLabelSpacing = 24;

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
const int kButtonGroup = 6666;

const gfx::Font& GetDialogButtonFont() {
  static gfx::Font* font = NULL;
  if (!font) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ui::ResourceBundle::BaseFont));
  }
  return *font;
}

// Updates any of the standard buttons according to the delegate.
void UpdateButtonHelper(TextButton* button_view,
                        DialogDelegate* delegate,
                        ui::DialogButton button) {
  string16 label = delegate->GetDialogButtonLabel(button);
  if (!label.empty())
    button_view->SetText(label);
  button_view->SetEnabled(delegate->IsDialogButtonEnabled(button));
  button_view->SetVisible(delegate->IsDialogButtonVisible(button));
}

// DialogButton ----------------------------------------------------------------

// DialogButton forwards AcceleratorPressed to the delegate.  It is used
// for the ok/cancel buttons of the window.

template <class ButtonBase>
class DialogButton : public ButtonBase {
 public:
  // The button's class name.
  static const char kViewClassName[];

  DialogButton(ButtonListener* listener,
               Widget* owner,
               ui::DialogButton type,
               const string16& title)
      : ButtonBase(listener, title),
        owner_(owner),
        type_(type) {
  }

  // Overridden to forward to the delegate.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) {
    if (!owner_->widget_delegate()->AsDialogDelegate()->
        AreAcceleratorsEnabled(type_)) {
      return false;
    }
    return ButtonBase::AcceleratorPressed(accelerator);
  }

  // Overridden from TextButton:
  virtual std::string GetClassName() const {
    return kViewClassName;
  }

 private:
  Widget* owner_;
  const ui::DialogButton type_;

  DISALLOW_COPY_AND_ASSIGN(DialogButton);
};

template <> const char DialogButton<NativeTextButton>::kViewClassName[] =
    "NativeDialogButton";

template <> const char DialogButton<TextButton>::kViewClassName[] =
    "TextDialogButton";
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      ok_button_(NULL),
      cancel_button_(NULL),
      default_button_(NULL),
      extra_view_(NULL),
      size_extra_view_height_to_buttons_(false),
      notified_delegate_(false),
      listening_to_focus_(false),
      saved_focus_manager_(NULL) {
  SkColor bg_color = owner->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  set_background(views::Background::CreateSolidBackground(bg_color));
}

DialogClientView::~DialogClientView() {
}

void DialogClientView::ShowDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();
  if (buttons & ui::DIALOG_BUTTON_OK && !ok_button_) {
    string16 label = dd->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK);
    if (label.empty())
      label = l10n_util::GetStringUTF16(IDS_APP_OK);
    ok_button_ = CreateDialogButton(ui::DIALOG_BUTTON_OK, label);
    if (!(buttons & ui::DIALOG_BUTTON_CANCEL))
      ok_button_->AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    AddChildView(ok_button_);
  }
  if (buttons & ui::DIALOG_BUTTON_CANCEL && !cancel_button_) {
    string16 label = dd->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL);
    if (label.empty()) {
      if (buttons & ui::DIALOG_BUTTON_OK) {
        label = l10n_util::GetStringUTF16(IDS_APP_CANCEL);
      } else {
        label = l10n_util::GetStringUTF16(IDS_APP_CLOSE);
      }
    }
    cancel_button_ = CreateDialogButton(ui::DIALOG_BUTTON_CANCEL, label);
    cancel_button_->AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE,
                                                   ui::EF_NONE));
    AddChildView(cancel_button_);
  }
  if (!buttons) {
    // Register the escape key as an accelerator which will close the window
    // if there are no dialog buttons.
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }
}

void DialogClientView::SetDefaultButton(TextButton* new_default_button) {
  if (default_button_ && default_button_ != new_default_button) {
    default_button_->SetIsDefault(false);
    default_button_ = NULL;
  }

  if (new_default_button) {
    default_button_ = new_default_button;
    default_button_->SetIsDefault(true);
  }
}

void DialogClientView::OnWillChangeFocus(View* focused_before,
                                         View* focused_now) {
  // Chrome style dialogs do not move the default button with the focus.
  // TODO(wittman): Remove all support for updating the default button on focus
  // change once we move completely to Chrome style.  See
  // http://codereview.chromium.org/10230 for a rough idea of changes to be
  // undone.
  if (DialogDelegate::UseNewStyle())
    return;

  TextButton* new_default_button = NULL;
  // TODO(wittman): Identify TextButtons in a better way than
  // enumerating every possible subclass.
  if (focused_now &&
      ((focused_now->GetClassName() == NativeTextButton::kViewClassName) ||
       (focused_now->GetClassName() == TextButton::kViewClassName) ||
       (focused_now->GetClassName() ==
        DialogButton<NativeTextButton>::kViewClassName) ||
       (focused_now->GetClassName() ==
        DialogButton<TextButton>::kViewClassName))) {
    new_default_button = static_cast<TextButton*>(focused_now);
  } else {
    // The focused view is not a button, get the default button from the
    // delegate.
    DialogDelegate* dd = GetDialogDelegate();
    if ((dd->GetDefaultDialogButton() & ui::DIALOG_BUTTON_OK) != 0)
      new_default_button = ok_button_;
    if ((dd->GetDefaultDialogButton() & ui::DIALOG_BUTTON_CANCEL)
        != 0)
      new_default_button = cancel_button_;
  }
  SetDefaultButton(new_default_button);
}

void DialogClientView::OnDidChangeFocus(View* focused_before,
                                        View* focused_now) {
}

// Changing dialog labels will change button widths.
void DialogClientView::UpdateDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();

  if (buttons & ui::DIALOG_BUTTON_OK)
    UpdateButtonHelper(ok_button_, dd, ui::DIALOG_BUTTON_OK);

  if (buttons & ui::DIALOG_BUTTON_CANCEL) {
    UpdateButtonHelper(cancel_button_, dd, ui::DIALOG_BUTTON_CANCEL);
  }

  LayoutDialogButtons();
  SchedulePaint();
}

void DialogClientView::AcceptWindow() {
  if (notified_delegate_) {
    // Only notify the delegate once. See comment in header above
    // notified_delegate_ for details.
    return;
  }
  if (GetDialogDelegate()->Accept(false)) {
    notified_delegate_ = true;
    Close();
  }
}

void DialogClientView::CancelWindow() {
  // Call the standard Close handler, which checks with the delegate before
  // proceeding. This checking _isn't_ done here, but in the WM_CLOSE handler,
  // so that the close box on the window also shares this code path.
  Close();
}

int DialogClientView::GetBottomMargin() {
  return style_params_.button_shadow_margin;
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

void DialogClientView::NativeViewHierarchyChanged(
    bool attached,
    gfx::NativeView native_view,
    internal::RootView* root_view) {
  if (attached) {
    UpdateFocusListener();
  }
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, ClientView overrides:

bool DialogClientView::CanClose() {
  if (notified_delegate_)
    return true;

  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();
  bool close = true;
  if ((buttons & ui::DIALOG_BUTTON_CANCEL) ||
      (buttons == ui::DIALOG_BUTTON_NONE))
    close = dd->Cancel();
  else if (buttons & ui::DIALOG_BUTTON_OK)
    close = dd->Accept(true);
  notified_delegate_ = close;
  return close;
}

void DialogClientView::WidgetClosing() {
  if (listening_to_focus_) {
    DCHECK(saved_focus_manager_);
    if (saved_focus_manager_)
       saved_focus_manager_->RemoveFocusChangeListener(this);
  }
}

int DialogClientView::NonClientHitTest(const gfx::Point& point) {
  if (size_box_bounds_.Contains(point.x() - x(), point.y() - y()))
    return HTBOTTOMRIGHT;
  return ClientView::NonClientHitTest(point);
}

DialogClientView* DialogClientView::AsDialogClientView() {
  return this;
}

const DialogClientView* DialogClientView::AsDialogClientView() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

void DialogClientView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  if (!GetWidget()->IsMaximized() && !GetWidget()->IsMinimized())
    PaintSizeBox(canvas);
}

void DialogClientView::Layout() {
  if (has_dialog_buttons())
    LayoutDialogButtons();
  LayoutContentsView();
}

void DialogClientView::ViewHierarchyChanged(bool is_add, View* parent,
                                            View* child) {
  if (is_add && child == this) {
    // Can only add and update the dialog buttons _after_ they are added to the
    // view hierarchy since they are native controls and require the
    // Container's HWND.
    ShowDialogButtons();
    ClientView::ViewHierarchyChanged(is_add, parent, child);

    UpdateFocusListener();

    // The "extra view" must be created and installed after the contents view
    // has been inserted into the view hierarchy.
    CreateExtraView();
    UpdateDialogButtons();
    Layout();
  }
}

gfx::Size DialogClientView::GetPreferredSize() {
  gfx::Size prefsize = contents_view()->GetPreferredSize();
  int button_height = GetDialogButtonsAreaHeight();
  if (has_dialog_buttons()) {
    // Make sure the view is sized to the buttons's width if they are wider than
    // the contents.
    int width = 0;
    if (cancel_button_)
      width += cancel_button_->GetPreferredSize().width();
    if (ok_button_) {
      width += ok_button_->GetPreferredSize().width();
      if (cancel_button_)
        width += style_params_.related_button_hspacing;
    }
    if (extra_view_) {
      width += extra_view_->GetPreferredSize().width();
      if (cancel_button_ || ok_button_)
        width += style_params_.related_button_hspacing;
    }
    if (width > 0) {
      width += 2 * style_params_.button_hedge_margin;
      prefsize.set_width(std::max(prefsize.width(), width));
    }
  }
  prefsize.Enlarge(0, button_height);
  return prefsize;
}

bool DialogClientView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // We only expect Escape key.
  DCHECK(accelerator.key_code() == ui::VKEY_ESCAPE);
  Close();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, ButtonListener implementation:

void DialogClientView::ButtonPressed(Button* sender, const ui::Event& event) {
  // We NULL check the delegate here since the buttons can receive WM_COMMAND
  // messages even after they (and the window containing us) are destroyed.
  if (!GetDialogDelegate())
    return;

  if (sender == ok_button_) {
    AcceptWindow();
  } else if (sender == cancel_button_) {
    CancelWindow();
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

DialogClientView::StyleParams::StyleParams()
    : button_vedge_margin(kButtonVEdgeMargin),
      button_hedge_margin(kButtonHEdgeMargin),
      button_shadow_margin(0),
      button_content_spacing(kDialogButtonContentSpacing),
      related_button_hspacing(kRelatedButtonHSpacing) {
  if (DialogDelegate::UseNewStyle()) {
    button_vedge_margin = 0;
    button_hedge_margin = 0;
    button_shadow_margin = GetChromeStyleButtonShadowMargin();
    button_content_spacing = 0;
    related_button_hspacing = 10;
  }
}

TextButton* DialogClientView::CreateDialogButton(ui::DialogButton type,
                                                 const string16& title) {
  TextButton* button = NULL;
  if (DialogDelegate::UseNewStyle())
    button = new DialogButton<TextButton>(this, GetWidget(), type, title);
  else
    button = new DialogButton<NativeTextButton>(this, GetWidget(), type, title);

  if (!DialogDelegate::UseNewStyle())
    button->set_min_width(kDialogMinButtonWidth);

  button->SetGroup(kButtonGroup);

  if (GetDialogDelegate()->GetDefaultDialogButton() & type) {
    default_button_ = button;
    button->SetIsDefault(true);
  }

  if (DialogDelegate::UseNewStyle())
    ApplyChromeStyle(button);

  return button;
}

void DialogClientView::PaintSizeBox(gfx::Canvas* canvas) {
  if (GetWidget()->widget_delegate()->CanResize() ||
      GetWidget()->widget_delegate()->CanMaximize()) {
#if defined(OS_WIN)
    ui::NativeTheme* theme = GetNativeTheme();
    ui::NativeTheme::ExtraParams extra;
    gfx::Size gripper_size = theme->GetPartSize(
        ui::NativeTheme::kWindowResizeGripper, ui::NativeTheme::kNormal, extra);

    // TODO(beng): (http://b/1085509) In "classic" rendering mode, there isn't
    //             a theme-supplied gripper. We should probably improvise
    //             something, which would also require changing |gripper_size|
    //             to have different default values, too...
    size_box_bounds_ = GetContentsBounds();
    size_box_bounds_.set_x(size_box_bounds_.right() - gripper_size.width());
    size_box_bounds_.set_y(size_box_bounds_.bottom() - gripper_size.height());

    theme->Paint(canvas->sk_canvas(),
                 ui::NativeTheme::kWindowResizeGripper,
                 ui::NativeTheme::kNormal,
                 size_box_bounds_,
                 extra);
#else
    NOTIMPLEMENTED();
    // TODO(port): paint size box
#endif
  }
}

int DialogClientView::GetButtonsHeight() const {
  int button_height = 0;
  if (cancel_button_)
    button_height = std::max(button_height,
                             cancel_button_->GetPreferredSize().height());
  if (ok_button_)
    button_height = std::max(button_height,
                             ok_button_->GetPreferredSize().height());
  return button_height;
}

int DialogClientView::GetDialogButtonsAreaHeight() const {
  return !has_dialog_buttons() ? 0 :
      GetButtonsHeight() + style_params_.button_content_spacing +
      style_params_.button_vedge_margin;
}

void DialogClientView::LayoutDialogButtons() {
  gfx::Rect lb = GetContentsBounds();
  gfx::Rect extra_bounds;
  int bottom_y = lb.bottom() - style_params_.button_vedge_margin;
  int button_height = GetButtonsHeight();
  if (cancel_button_) {
    gfx::Size ps = cancel_button_->GetPreferredSize();
    int button_x = lb.right() - ps.width() - style_params_.button_hedge_margin;
    int button_y = bottom_y - ps.height();
    cancel_button_->SetBounds(button_x, button_y, ps.width(), ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, cancel_button_->x()));
    extra_bounds.set_y(cancel_button_->y());
  }
  if (ok_button_) {
    gfx::Size ps = ok_button_->GetPreferredSize();
    int ok_button_right = lb.right() - style_params_.button_hedge_margin;
    if (cancel_button_)
      ok_button_right = cancel_button_->x() -
          style_params_.related_button_hspacing;
    int button_x = ok_button_right - ps.width();
    int button_y = bottom_y - ps.height();
    ok_button_->SetBounds(button_x, button_y, ok_button_right - button_x,
                          ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, ok_button_->x()));
    extra_bounds.set_y(ok_button_->y());
  }
  if (extra_view_) {
    gfx::Size ps = extra_view_->GetPreferredSize();
    extra_bounds.set_x(lb.x() + style_params_.button_hedge_margin);
    int height = size_extra_view_height_to_buttons_ ?
        std::max(ps.height(), button_height) : ps.height();
    extra_bounds.set_height(height);
    extra_view_->SetBoundsRect(extra_bounds);
  }
}

void DialogClientView::LayoutContentsView() {
  gfx::Rect lb = GetContentsBounds();
  lb.set_height(std::max(0, lb.height() - GetDialogButtonsAreaHeight()));
  contents_view()->SetBoundsRect(lb);
  contents_view()->Layout();
}

void DialogClientView::CreateExtraView() {
  View* extra_view = GetDialogDelegate()->GetExtraView();
  if (extra_view && !extra_view_) {
    extra_view_ = extra_view;
    extra_view_->SetGroup(kButtonGroup);
    AddChildView(extra_view_);
    size_extra_view_height_to_buttons_ =
        GetDialogDelegate()->GetSizeExtraViewHeightToButtons();
  }
}

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  return GetWidget()->widget_delegate()->AsDialogDelegate();
}

void DialogClientView::Close() {
  GetWidget()->Close();
  GetDialogDelegate()->OnClose();
}

void DialogClientView::UpdateFocusListener() {
  FocusManager* focus_manager = GetFocusManager();
  // Listen for focus change events so we can update the default button.
  // focus_manager can be NULL when the dialog is created on un-shown view.
  // We start listening for focus changes when the page is visible.
  // Focus manager could also change if window host changes a parent.
  if (listening_to_focus_) {
    if (saved_focus_manager_ == focus_manager)
      return;
    DCHECK(saved_focus_manager_);
    if (saved_focus_manager_)
      saved_focus_manager_->RemoveFocusChangeListener(this);
    listening_to_focus_ = false;
  }
  saved_focus_manager_ = focus_manager;
  // Listen for focus change events so we can update the default button.
  if (focus_manager) {
    focus_manager->AddFocusChangeListener(this);
    listening_to_focus_ = true;
  }
}

}  // namespace views
