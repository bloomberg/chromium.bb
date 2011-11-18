// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>
#elif defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/hit_test.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/button/text_button.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/gfx/native_theme.h"
#else
#include "ui/gfx/skia_utils_gtk.h"
#endif

namespace views {
namespace {

// Updates any of the standard buttons according to the delegate.
void UpdateButtonHelper(NativeTextButton* button_view,
                        DialogDelegate* delegate,
                        ui::DialogButton button) {
  string16 label = delegate->GetDialogButtonLabel(button);
  if (!label.empty())
    button_view->SetText(label);
  button_view->SetEnabled(delegate->IsDialogButtonEnabled(button));
  button_view->SetVisible(delegate->IsDialogButtonVisible(button));
}

#if defined(OS_WIN)
void FillViewWithSysColor(gfx::Canvas* canvas, View* view, COLORREF color) {
  SkColor sk_color =
      SkColorSetRGB(GetRValue(color), GetGValue(color), GetBValue(color));
  canvas->FillRect(sk_color, view->GetLocalBounds());
}
#endif

// DialogButton ----------------------------------------------------------------

// DialogButtons is used for the ok/cancel buttons of the window. DialogButton
// forwards AcceleratorPressed to the delegate.

class DialogButton : public NativeTextButton {
 public:
  DialogButton(ButtonListener* listener,
               Widget* owner,
               ui::DialogButton type,
               const string16& title,
               bool is_default)
      : NativeTextButton(listener, title),
        owner_(owner),
        type_(type) {
    SetIsDefault(is_default);
  }

  // Overridden to forward to the delegate.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) {
    if (!owner_->widget_delegate()->AsDialogDelegate()->
        AreAcceleratorsEnabled(type_)) {
      return false;
    }
    return NativeTextButton::AcceleratorPressed(accelerator);
  }

 private:
  Widget* owner_;
  const ui::DialogButton type_;

  DISALLOW_COPY_AND_ASSIGN(DialogButton);
};

}  // namespace

// static
gfx::Font* DialogClientView::dialog_button_font_ = NULL;
static const int kDialogMinButtonWidth = 75;
static const int kDialogButtonLabelSpacing = 16;
static const int kDialogButtonContentSpacing = 5;

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
static const int kButtonGroup = 6666;

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
      saved_focus_manager_(NULL),
      bottom_view_(NULL) {
  InitClass();
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
    bool is_default_button =
        (dd->GetDefaultDialogButton() & ui::DIALOG_BUTTON_OK) != 0;
    ok_button_ = new DialogButton(this,
                                  GetWidget(),
                                  ui::DIALOG_BUTTON_OK,
                                  label,
                                  is_default_button);
    ok_button_->SetGroup(kButtonGroup);
    if (is_default_button)
      default_button_ = ok_button_;
    if (!(buttons & ui::DIALOG_BUTTON_CANCEL))
      ok_button_->AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE,
                                                 false, false, false));
    AddChildView(ok_button_);
  }
  if (buttons & ui::DIALOG_BUTTON_CANCEL && !cancel_button_) {
    string16 label =
        dd->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL);
    if (label.empty()) {
      if (buttons & ui::DIALOG_BUTTON_OK) {
        label = l10n_util::GetStringUTF16(IDS_APP_CANCEL);
      } else {
        label = l10n_util::GetStringUTF16(IDS_APP_CLOSE);
      }
    }
    bool is_default_button =
        (dd->GetDefaultDialogButton() & ui::DIALOG_BUTTON_CANCEL)
        != 0;
    cancel_button_ = new DialogButton(this,
                                      GetWidget(),
                                      ui::DIALOG_BUTTON_CANCEL,
                                      label,
                                      is_default_button);
    cancel_button_->SetGroup(kButtonGroup);
    cancel_button_->AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE,
                                                   false, false, false));
    if (is_default_button)
      default_button_ = ok_button_;
    AddChildView(cancel_button_);
  }
  if (!buttons) {
    // Register the escape key as an accelerator which will close the window
    // if there are no dialog buttons.
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, false, false, false));
  }
}

void DialogClientView::SetDefaultButton(NativeTextButton* new_default_button) {
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
  NativeTextButton* new_default_button = NULL;
  if (focused_now &&
      focused_now->GetClassName() == NativeTextButton::kViewClassName) {
    new_default_button = static_cast<NativeTextButton*>(focused_now);
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

void DialogClientView::SetBottomView(View* bottom_view) {
  if (bottom_view_) {
    RemoveChildView(bottom_view_);
    delete bottom_view_;
  }
  bottom_view_ = bottom_view;
  if (bottom_view_)
    AddChildView(bottom_view_);
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
  if (buttons & ui::DIALOG_BUTTON_CANCEL)
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

void DialogClientView::OnPaint(gfx::Canvas* canvas) {
#if defined(OS_WIN)
  FillViewWithSysColor(canvas, this, GetSysColor(COLOR_3DFACE));
#elif defined(USE_WAYLAND) || defined(USE_AURA)
  SkColor sk_color = SkColorSetARGB(200, 255, 255, 255);
  canvas->FillRect(sk_color, GetLocalBounds());
#else
  GtkWidget* widget = GetWidget()->GetNativeView();
  if (GTK_IS_WINDOW(widget)) {
    GtkStyle* window_style = gtk_widget_get_style(widget);
    canvas->FillRect(gfx::GdkColorToSkColor(window_style->bg[GTK_STATE_NORMAL]),
                     GetLocalBounds());
  }
#endif
}

void DialogClientView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  if (!GetWidget()->IsMaximized() && !GetWidget()->IsMinimized())
    PaintSizeBox(canvas);
}

void DialogClientView::Layout() {
  if (has_dialog_buttons())
    LayoutDialogButtons();
  if (bottom_view_) {
    gfx::Rect bounds = GetContentsBounds();
    gfx::Size pref = bottom_view_->GetPreferredSize();
    bottom_view_->SetBounds(bounds.x(),
        bounds.bottom() - pref.height() - kButtonVEdgeMargin,
        bounds.width(), pref.height());
  }
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
  int button_height = 0;
  if (has_dialog_buttons()) {
    if (cancel_button_)
      button_height = cancel_button_->height();
    else
      button_height = ok_button_->height();
    // Account for padding above and below the button.
    button_height += kDialogButtonContentSpacing + kButtonVEdgeMargin;

    // Make sure the view is sized to the buttons's width if they are wider than
    // the contents.
    int width = 0;
    if (cancel_button_)
      width += GetButtonWidth(ui::DIALOG_BUTTON_CANCEL);
    if (ok_button_) {
      width += GetButtonWidth(ui::DIALOG_BUTTON_OK);
      if (cancel_button_)
        width += kRelatedButtonHSpacing;
    }
    if (extra_view_) {
      width += extra_view_->GetPreferredSize().width();
      if (cancel_button_ || ok_button_)
        width += kRelatedButtonHSpacing;
    }
    if (width > 0) {
      width += 2 * kButtonHEdgeMargin;
      prefsize.set_width(std::max(prefsize.width(), width));
    }
  }
  if (bottom_view_) {
    gfx::Size bottom_pref = bottom_view_->GetPreferredSize();
    prefsize.Enlarge(0, bottom_pref.height() + kButtonVEdgeMargin);
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

void DialogClientView::ButtonPressed(
    Button* sender, const views::Event& event) {
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

void DialogClientView::PaintSizeBox(gfx::Canvas* canvas) {
  if (GetWidget()->widget_delegate()->CanResize() ||
      GetWidget()->widget_delegate()->CanMaximize()) {
#if defined(OS_WIN)
    gfx::NativeTheme::ExtraParams extra;
    gfx::Size gripper_size = gfx::NativeTheme::instance()->GetPartSize(
        gfx::NativeTheme::kWindowResizeGripper, gfx::NativeTheme::kNormal,
        extra);

    // TODO(beng): (http://b/1085509) In "classic" rendering mode, there isn't
    //             a theme-supplied gripper. We should probably improvise
    //             something, which would also require changing |gripper_size|
    //             to have different default values, too...
    size_box_bounds_ = GetContentsBounds();
    size_box_bounds_.set_x(size_box_bounds_.right() - gripper_size.width());
    size_box_bounds_.set_y(size_box_bounds_.bottom() - gripper_size.height());

    gfx::NativeTheme::instance()->Paint(canvas->GetSkCanvas(),
                                        gfx::NativeTheme::kWindowResizeGripper,
                                        gfx::NativeTheme::kNormal,
                                        size_box_bounds_,
                                        extra);
#else
    NOTIMPLEMENTED();
    // TODO(port): paint size box
#endif
  }
}

int DialogClientView::GetButtonWidth(int button) const {
  DialogDelegate* dd = GetDialogDelegate();
  string16 button_label = dd->GetDialogButtonLabel(
      static_cast<ui::DialogButton>(button));
  int string_width = dialog_button_font_->GetStringWidth(button_label);
  return std::max(string_width + kDialogButtonLabelSpacing,
                  kDialogMinButtonWidth);
}

int DialogClientView::GetButtonsHeight() const {
  if (has_dialog_buttons()) {
    if (cancel_button_)
      return cancel_button_->height() + kDialogButtonContentSpacing;
    return ok_button_->height() + kDialogButtonContentSpacing;
  }
  return 0;
}

void DialogClientView::LayoutDialogButtons() {
  gfx::Rect lb = GetContentsBounds();
  gfx::Rect extra_bounds;
  int bottom_y = lb.bottom() - kButtonVEdgeMargin;
  int button_height = 0;
  if (bottom_view_) {
    gfx::Size bottom_pref = bottom_view_->GetPreferredSize();
    bottom_y -= bottom_pref.height() + kButtonVEdgeMargin + kButtonVEdgeMargin;
  }
  if (cancel_button_) {
    gfx::Size ps = cancel_button_->GetPreferredSize();
    int button_width = std::max(
        GetButtonWidth(ui::DIALOG_BUTTON_CANCEL), ps.width());
    int button_x = lb.right() - button_width - kButtonHEdgeMargin;
    int button_y = bottom_y - ps.height();
    cancel_button_->SetBounds(button_x, button_y, button_width, ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, cancel_button_->x()));
    extra_bounds.set_y(cancel_button_->y());
    button_height = std::max(button_height, ps.height());
  }
  if (ok_button_) {
    gfx::Size ps = ok_button_->GetPreferredSize();
    int button_width = std::max(
        GetButtonWidth(ui::DIALOG_BUTTON_OK), ps.width());
    int ok_button_right = lb.right() - kButtonHEdgeMargin;
    if (cancel_button_)
      ok_button_right = cancel_button_->x() - kRelatedButtonHSpacing;
    int button_x = ok_button_right - button_width;
    int button_y = bottom_y - ps.height();
    ok_button_->SetBounds(button_x, button_y, ok_button_right - button_x,
                          ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, ok_button_->x()));
    extra_bounds.set_y(ok_button_->y());
    button_height = std::max(button_height, ps.height());
  }
  if (extra_view_) {
    gfx::Size ps = extra_view_->GetPreferredSize();
    extra_bounds.set_x(lb.x() + kButtonHEdgeMargin);
    int height = size_extra_view_height_to_buttons_ ?
        std::max(ps.height(), button_height) : ps.height();
    extra_bounds.set_height(height);
    extra_view_->SetBoundsRect(extra_bounds);
  }
}

void DialogClientView::LayoutContentsView() {
  gfx::Rect lb = GetContentsBounds();
  lb.set_height(std::max(0, lb.height() - GetButtonsHeight()));
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

// static
void DialogClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    dialog_button_font_ = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
    initialized = true;
  }
}

}  // namespace views
