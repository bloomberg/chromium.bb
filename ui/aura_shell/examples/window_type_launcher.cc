// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/window_type_launcher.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/example_factory.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/toplevel_frame_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using views::MenuItemView;
using views::MenuRunner;

namespace aura_shell {
namespace examples {

namespace {

SkColor g_colors[] = { SK_ColorRED,
                       SK_ColorYELLOW,
                       SK_ColorBLUE,
                       SK_ColorGREEN };
int g_color_index = 0;

class ModalWindow : public views::WidgetDelegateView,
                    public views::ButtonListener {
 public:
  ModalWindow()
      : color_(g_colors[g_color_index]),
        ALLOW_THIS_IN_INITIALIZER_LIST(open_button_(
            new views::NativeTextButton(this, ASCIIToUTF16("Moar!")))) {
    ++g_color_index %= arraysize(g_colors);
    AddChildView(open_button_);
  }
  virtual ~ModalWindow() {
  }

  static void OpenModalWindow(aura::Window* parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new ModalWindow, parent);
    widget->GetNativeView()->set_name("ModalWindow");
    widget->Show();
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(color_, GetLocalBounds());
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(200, 200);
  }
  virtual void Layout() OVERRIDE {
    gfx::Size open_ps = open_button_->GetPreferredSize();
    gfx::Rect local_bounds = GetLocalBounds();
    open_button_->SetBounds(
        5, local_bounds.bottom() - open_ps.height() - 5,
        open_ps.width(), open_ps.height());
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Modal Window");
  }
  virtual bool IsModal() const OVERRIDE {
    return true;
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    DCHECK(sender == open_button_);
    OpenModalWindow(GetWidget()->GetNativeView());
  }

 private:
  SkColor color_;
  views::NativeTextButton* open_button_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class NonModalTransient : public views::WidgetDelegateView {
 public:
  NonModalTransient()
      : color_(g_colors[g_color_index]) {
    ++g_color_index %= arraysize(g_colors);
  }
  virtual ~NonModalTransient() {
  }

  static void OpenNonModalTransient(aura::Window* parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
    widget->GetNativeView()->set_name("NonModalTransient");
    widget->Show();
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(color_, GetLocalBounds());
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(250, 250);
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Non-Modal Transient");
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(NonModalTransient);
};

}  // namespace

void InitWindowTypeLauncher() {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new WindowTypeLauncher,
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name("WindowTypeLauncher");
  widget->Show();
}

WindowTypeLauncher::WindowTypeLauncher()
    : ALLOW_THIS_IN_INITIALIZER_LIST(create_button_(
          new views::NativeTextButton(this, ASCIIToUTF16("Create Window")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(create_nonresizable_button_(
          new views::NativeTextButton(
              this, ASCIIToUTF16("Create Non-Resizable Window")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(bubble_button_(
          new views::NativeTextButton(
              this, ASCIIToUTF16("Create Pointy Bubble")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(lock_button_(
          new views::NativeTextButton(this, ASCIIToUTF16("Lock Screen")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(widgets_button_(
          new views::NativeTextButton(
              this, ASCIIToUTF16("Show Example Widgets")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(modal_button_(
          new views::NativeTextButton(
              this, ASCIIToUTF16("Open Modal Window")))),
      ALLOW_THIS_IN_INITIALIZER_LIST(transient_button_(
          new views::NativeTextButton(
              this, ASCIIToUTF16("Open Non-Modal Transient Window")))) {
  AddChildView(create_button_);
  AddChildView(create_nonresizable_button_);
  AddChildView(bubble_button_);
  AddChildView(lock_button_);
  AddChildView(widgets_button_);
  AddChildView(modal_button_);
  AddChildView(transient_button_);
  set_context_menu_controller(this);
}

WindowTypeLauncher::~WindowTypeLauncher() {
}

void WindowTypeLauncher::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(SK_ColorWHITE, GetLocalBounds());
}

void WindowTypeLauncher::Layout() {
  gfx::Size create_button_ps = create_button_->GetPreferredSize();
  gfx::Rect local_bounds = GetLocalBounds();
  create_button_->SetBounds(
      5, local_bounds.bottom() - create_button_ps.height() - 5,
      create_button_ps.width(), create_button_ps.height());

  gfx::Size bubble_button_ps = bubble_button_->GetPreferredSize();
  bubble_button_->SetBounds(
      5, create_button_->y() - bubble_button_ps.height() - 5,
      bubble_button_ps.width(), bubble_button_ps.height());

  gfx::Size create_nr_button_ps =
      create_nonresizable_button_->GetPreferredSize();
  create_nonresizable_button_->SetBounds(
      5, bubble_button_->y() - create_nr_button_ps.height() - 5,
      create_nr_button_ps.width(), create_nr_button_ps.height());

  gfx::Size lock_ps = lock_button_->GetPreferredSize();
  lock_button_->SetBounds(
      5, create_nonresizable_button_->y() - lock_ps.height() - 5,
      lock_ps.width(), lock_ps.height());

  gfx::Size widgets_ps = widgets_button_->GetPreferredSize();
  widgets_button_->SetBounds(
      5, lock_button_->y() - widgets_ps.height() - 5,
      widgets_ps.width(), widgets_ps.height());

  gfx::Size modal_ps = modal_button_->GetPreferredSize();
  modal_button_->SetBounds(
      5, widgets_button_->y() - modal_ps.height() - 5,
      modal_ps.width(), modal_ps.height());

  gfx::Size transient_ps = transient_button_->GetPreferredSize();
  transient_button_->SetBounds(
      5, modal_button_->y() - transient_ps.height() - 5,
      transient_ps.width(), transient_ps.height());
}

gfx::Size WindowTypeLauncher::GetPreferredSize() {
  return gfx::Size(300, 500);
}

bool WindowTypeLauncher::OnMousePressed(const views::MouseEvent& event) {
  // Overridden so we get OnMouseReleased and can show the context menu.
  return true;
}

views::View* WindowTypeLauncher::GetContentsView() {
  return this;
}

bool WindowTypeLauncher::CanResize() const {
  return true;
}

string16 WindowTypeLauncher::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Window Builder");
}

views::NonClientFrameView* WindowTypeLauncher::CreateNonClientFrameView() {
  return new aura_shell::internal::ToplevelFrameView;
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  if (sender == create_button_) {
    ToplevelWindow::CreateParams params;
    params.can_resize = true;
    ToplevelWindow::CreateToplevelWindow(params);
  } else if (sender == create_nonresizable_button_) {
    ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
  } else if (sender == bubble_button_) {
    CreatePointyBubble(sender);
  } else if (sender == lock_button_) {
    CreateLockScreen();
  } else if (sender == widgets_button_) {
    CreateWidgetsWindow();
  } else if (sender == modal_button_) {
    ModalWindow::OpenModalWindow(GetWidget()->GetNativeView());
  } else if (sender == transient_button_) {
    NonModalTransient::OpenNonModalTransient(GetWidget()->GetNativeView());
  }
}

void WindowTypeLauncher::ExecuteCommand(int id) {
  switch (id) {
    case COMMAND_NEW_WINDOW:
      InitWindowTypeLauncher();
      break;
    case COMMAND_TOGGLE_FULLSCREEN:
      GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
      break;
    default:
      break;
  }
}

void WindowTypeLauncher::ShowContextMenuForView(views::View* source,
                                                const gfx::Point& p,
                                                bool is_mouse_gesture) {
  MenuItemView* root = new MenuItemView(this);
  root->AppendMenuItem(COMMAND_NEW_WINDOW,
                       ASCIIToUTF16("New Window"),
                       MenuItemView::NORMAL);
  root->AppendMenuItem(COMMAND_TOGGLE_FULLSCREEN,
                       ASCIIToUTF16("Toggle FullScreen"),
                       MenuItemView::NORMAL);
  // MenuRunner takes ownership of root.
  menu_runner_.reset(new MenuRunner(root));
  if (menu_runner_->RunMenuAt(GetWidget(), NULL, gfx::Rect(p, gfx::Size(0, 0)),
        MenuItemView::TOPLEFT,
        MenuRunner::HAS_MNEMONICS) == MenuRunner::MENU_DELETED)
    return;
}

}  // namespace examples
}  // namespace aura_shell
