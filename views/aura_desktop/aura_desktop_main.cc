// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"
#include "aura/desktop_host.h"
#include "aura/window.h"
#include "aura/window_delegate.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/skia/include/core/SkXfermode.h"
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

  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCLIENT;
  }
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE {
    return true;
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->AsCanvasSkia()->drawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnWindowDestroyed() OVERRIDE {
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
    canvas->FillRectInt(color_, 0, 0, width(), height());
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
    canvas->FillRectInt(SK_ColorGRAY, 0, 0, width(), height());
  }

  // Overridden from views::WidgetDelegateView:
  virtual std::wstring GetWindowTitle() const OVERRIDE {
    return L"Test Window!";
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

#if defined(USE_X11)
  base::MessagePumpX::DisableGtkMessagePump();
#endif

  aura::Desktop::GetInstance();

  // Create a hierarchy of test windows.
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window window1(&window_delegate1);
  window1.set_id(1);
  window1.Init();
  window1.SetBounds(gfx::Rect(100, 100, 400, 400), 0);
  window1.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window1.SetParent(NULL);

  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window window2(&window_delegate2);
  window2.set_id(2);
  window2.Init();
  window2.SetBounds(gfx::Rect(200, 200, 350, 350), 0);
  window2.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window2.SetParent(NULL);

  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window window3(&window_delegate3);
  window3.set_id(3);
  window3.Init();
  window3.SetBounds(gfx::Rect(10, 10, 50, 50), 0);
  window3.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window3.SetParent(&window2);

  views::Widget widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(75, 75, 80, 80);
  params.parent = &window2;
  widget.Init(params);
  widget.SetContentsView(new TestView);

  TestWindowContents* contents = new TestWindowContents;
  views::Widget* views_window = views::Widget::CreateWindowWithParentAndBounds(
      contents, &window2, gfx::Rect(120, 150, 200, 200));
  views_window->Show();

  aura::Desktop::GetInstance()->Run();
  return 0;
}

