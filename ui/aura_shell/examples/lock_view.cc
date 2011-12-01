// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/example_factory.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura_shell {
namespace examples {

class LockView : public views::WidgetDelegateView {
 public:
  LockView() {}
  virtual ~LockView() {}

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(500, 400);
  }

 private:
  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(SK_ColorYELLOW, GetLocalBounds());
    string16 text = ASCIIToUTF16("LOCKED!");
    int string_width = font_.GetStringWidth(text);
    canvas->DrawStringInt(text, font_, SK_ColorRED, (width() - string_width)/ 2,
                          (height() - font_.GetHeight()) / 2,
                          string_width, font_.GetHeight());
  }
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return true;
  }
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    GetWidget()->Close();
  }

  gfx::Font font_;

  DISALLOW_COPY_AND_ASSIGN(LockView);
};

void CreateLockScreen() {
  LockView* lock_view = new LockView;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  gfx::Size ps = lock_view->GetPreferredSize();

  gfx::Size desktop_size  = aura::Desktop::GetInstance()->GetHostSize();
  params.bounds = gfx::Rect((desktop_size.width() - ps.width()) / 2,
                            (desktop_size.height() - ps.height()) / 2,
                            ps.width(), ps.height());
  params.delegate = lock_view;
  widget->Init(params);
  Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockScreenContainer)->
      AddChild(widget->GetNativeView());
  widget->SetContentsView(lock_view);
  widget->Show();
  widget->GetNativeView()->SetName("LockView");
}

}  // namespace examples
}  // namespace aura_shell
