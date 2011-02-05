// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/native_widget_win.h"

class V2DemoDispatcher : public MessageLoopForUI::Dispatcher {
 public:
  V2DemoDispatcher() {}
  virtual ~V2DemoDispatcher() {}

 private:
  // Overridden from MessageLoopForUI::Dispatcher:
  virtual bool Dispatch(const MSG& msg) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(V2DemoDispatcher);
};

class ColorView : public ui::View {
 public:
  explicit ColorView(SkColor color) : color_(color) {
  }
  ColorView() : color_(SK_ColorBLACK) {}
  virtual ~ColorView() {}

 protected:
  SkColor color() const { return color_; }
  void set_color(SkColor color) { color_ = color; }

 private:
  // Overridden from ui::View:
  virtual void OnPaint(gfx::Canvas* canvas) {
    canvas->FillRectInt(color_, 0, 0, width(), height());
  }
  virtual bool OnMousePressed(const ui::MouseEvent& event) {
    color_ = color_ == SK_ColorBLACK ? SK_ColorWHITE : SK_ColorBLACK;
    Invalidate();
    return true;
  }
  virtual void OnMouseReleased(const ui::MouseEvent& event, bool canceled) {
    color_ = color_ == SK_ColorWHITE ? SK_ColorMAGENTA : SK_ColorGREEN;
    Invalidate();
  }
  virtual void OnMouseMoved(const ui::MouseEvent& event) {
    U8CPU r = SkColorGetR(color_);
    color_ = SkColorSetRGB(++r % 255, SkColorGetG(color_),
                           SkColorGetB(color_));
    Invalidate();
  }
  virtual bool OnMouseDragged(const ui::MouseEvent& event) {
    U8CPU g = SkColorGetG(color_);
    color_ = SkColorSetRGB(SkColorGetR(color_), ++g % 255,
                           SkColorGetB(color_));
    Invalidate();
    return true;
  }

  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(ColorView);
};

class FancyPantsView : public ColorView {
 public:
  FancyPantsView()
      : ColorView(SK_ColorMAGENTA),
        c1_(new ColorView(SK_ColorGREEN)),
        c2_(new ColorView(SK_ColorRED)) {
    AddChildView(c1_);
    AddChildView(c2_);
  }
  virtual ~FancyPantsView() {}

  // Overridden from ui::View:
  virtual void Layout() {
    c1_->SetBounds(20, 20, width() - 40, height() - 40);
    c2_->SetBounds(50, 50, 50, 50);
    Invalidate();
  }
  virtual bool OnMousePressed(const ui::MouseEvent& event) {
    old_color_ = color();
    set_color(SK_ColorWHITE);
    mouse_offset_ = event.location();
    return true;
  }
  virtual bool OnMouseDragged(const ui::MouseEvent& event) {
    gfx::Rect old_bounds = bounds();
    SetPosition(gfx::Point(event.x() - mouse_offset_.x(),
                           event.y() - mouse_offset_.y()));
    gfx::Rect new_bounds = bounds();
    parent()->InvalidateRect(old_bounds.Union(new_bounds));
    return true;
  }
  virtual void OnMouseReleased(const ui::MouseEvent& event) {
    set_color(old_color_);
  }
  virtual void OnMouseCaptureLost() {
    set_color(SK_ColorYELLOW);
  }

 private:
  View* c1_;
  View* c2_;

  gfx::Point mouse_offset_;
  SkColor old_color_;

  DISALLOW_COPY_AND_ASSIGN(FancyPantsView);
};



class ContentsView : public ColorView {
 public:
  ContentsView()
      : c1_(new ColorView(SK_ColorBLUE)),
        c2_(new ColorView(SK_ColorGREEN)),
        c3_(new FancyPantsView()),
        ColorView(SK_ColorRED) {
    set_parent_owned(false);
    AddChildView(c1_);
    AddChildView(c2_);
    c3_->SetPosition(gfx::Point(200, 200));
    AddChildView(c3_);
  }

  virtual ~ContentsView() {}

  void Init() {
    //c3_->SetHasLayer(true);
  }

 private:
  // Overridden from ui::View:
  virtual void Layout() {
    c1_->SetBounds(20, 20, width() - 40, height() - 40);
    c2_->SetBounds(50, 50, 50, 50);
    c3_->SetSize(gfx::Size(75, 75));
    Invalidate();
  }

  View* c1_;
  View* c2_;
  FancyPantsView* c3_;
};

int main(int argc, char **argv) {
  OleInitialize(NULL);
  base::AtExitManager exit_manager;
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  ContentsView cv;
  ui::Widget widget(&cv);
  widget.InitWithNativeViewParent(NULL, gfx::Rect(20, 20, 400, 400));
  cv.Init();
  widget.Show();

  V2DemoDispatcher dispatcher;
  MessageLoopForUI::current()->Run(&dispatcher);

  OleUninitialize();
}
