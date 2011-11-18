// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/desktop.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "views/widget/widget.h"
#include "views/widget/widget_delegate.h"

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from aura::WindowDelegate:
  virtual void OnBoundsChanging(gfx::Rect* new_bounds) OVERRIDE {}
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(aura::KeyEvent* event) OVERRIDE {
    return false;
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCLIENT;
  }
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE {
    return true;
  }
  virtual ui::TouchStatus OnTouchEvent(aura::TouchEvent* event) OVERRIDE {
    return ui::TOUCH_STATUS_END;
  }
  virtual bool ShouldActivate(aura::Event* event) OVERRIDE {
    return true;
  }
  virtual void OnActivated() OVERRIDE {}
  virtual void OnLostActive() OVERRIDE {}
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->GetSkCanvas()->drawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnWindowDestroying() OVERRIDE {
  }
  virtual void OnWindowDestroyed() OVERRIDE {
  }
  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE {
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class TestView : public views::View {
 public:
  TestView() : color_shifting_(false), color_(SK_ColorYELLOW) {}
  virtual ~TestView() {}

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) {
    canvas->FillRect(color_, GetLocalBounds());
  }
  virtual bool OnMousePressed(const views::MouseEvent& event) {
    color_shifting_ = true;
    return true;
  }
  virtual void OnMouseMoved(const views::MouseEvent& event) {
    if (color_shifting_) {
      color_ = SkColorSetRGB((SkColorGetR(color_) + 5) % 255,
                             SkColorGetG(color_),
                             SkColorGetB(color_));
      SchedulePaint();
    }
  }
  virtual void OnMouseReleased(const views::MouseEvent& event) {
    color_shifting_ = false;
  }

  bool color_shifting_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

class TestWindowContents : public views::WidgetDelegateView {
 public:
  TestWindowContents() {}
  virtual ~TestWindowContents() {}

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(SK_ColorGRAY, GetLocalBounds());
  }

  // Overridden from views::WidgetDelegateView:
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Test Window!");
  }
  virtual View* GetContentsView() OVERRIDE {
    return this;
  }

  DISALLOW_COPY_AND_ASSIGN(TestWindowContents);
};

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

  // Create the message-loop here before creating the desktop.
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  aura::Desktop::GetInstance();

  // Create a hierarchy of test windows.
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window* window1 = new aura::Window(&window_delegate1);
  window1->set_id(1);
  window1->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window1->SetBounds(gfx::Rect(100, 100, 400, 400));
  window1->Show();
  window1->SetParent(NULL);

  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window* window2 = new aura::Window(&window_delegate2);
  window2->set_id(2);
  window2->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window2->SetBounds(gfx::Rect(200, 200, 350, 350));
  window2->Show();
  window2->SetParent(NULL);

  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window* window3 = new aura::Window(&window_delegate3);
  window3->set_id(3);
  window3->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window3->SetBounds(gfx::Rect(10, 10, 50, 50));
  window3->Show();
  window3->SetParent(window2);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(75, 75, 80, 80);
  params.parent = window2;
  widget->Init(params);
  widget->SetContentsView(new TestView);

  TestWindowContents* contents = new TestWindowContents;
  views::Widget* views_window = views::Widget::CreateWindowWithParentAndBounds(
      contents, window2, gfx::Rect(120, 150, 200, 200));
  views_window->Show();

  aura::Desktop::GetInstance()->Run();

  delete aura::Desktop::GetInstance();

  return 0;
}
