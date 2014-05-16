// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <X11/extensions/shape.h>
#include <X11/Xlib.h>

// Get rid of X11 macros which conflict with gtest.
#undef Bool
#undef None

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace views {

namespace {

// Blocks till |window| becomes maximized.
class MaximizeWaiter : public ui::PlatformEventDispatcher {
  public:
   explicit MaximizeWaiter(XID window)
       : xwindow_(window),
         wait_(true) {
     const char* kAtomsToCache[] = {
         "_NET_WM_STATE",
         "_NET_WM_STATE_MAXIMIZED_VERT",
         NULL
     };
     atom_cache_.reset(new ui::X11AtomCache(gfx::GetXDisplay(), kAtomsToCache));

     // Override the dispatcher so that we get events before
     // DesktopWindowTreeHostX11 does. We must do this because
     // DesktopWindowTreeHostX11 stops propagation.
     dispatcher_ = ui::PlatformEventSource::GetInstance()->
         OverrideDispatcher(this).Pass();
   }

   virtual ~MaximizeWaiter() {
   }

   void Wait() {
     if (wait_) {
       base::RunLoop run_loop;
       quit_closure_ = run_loop.QuitClosure();
       run_loop.Run();
     }
     dispatcher_.reset();
   }

   virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
     NOTREACHED();
     return true;
   }

   virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
     if (event->type != PropertyNotify ||
         event->xproperty.window != xwindow_ ||
         event->xproperty.atom != atom_cache_->GetAtom("_NET_WM_STATE")) {
       return ui::POST_DISPATCH_PERFORM_DEFAULT;
     }

     std::vector<Atom> wm_states;
     if (!ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &wm_states))
       return ui::POST_DISPATCH_PERFORM_DEFAULT;

     std::vector<Atom>::iterator it = std::find(
         wm_states.begin(),
         wm_states.end(),
         atom_cache_->GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"));
     if (it == wm_states.end())
       return ui::POST_DISPATCH_PERFORM_DEFAULT;

     wait_ = false;
     if (!quit_closure_.is_null())
       quit_closure_.Run();
     return ui::POST_DISPATCH_PERFORM_DEFAULT;
   }

   // The window we are waiting to get maximized.
   XID xwindow_;

   // Whether Wait() should block.
   bool wait_;

   // Ends the run loop.
   base::Closure quit_closure_;

   scoped_ptr<ui::ScopedEventDispatcher> dispatcher_;

   scoped_ptr<ui::X11AtomCache> atom_cache_;

   DISALLOW_COPY_AND_ASSIGN(MaximizeWaiter);
};

// A NonClientFrameView with a window mask with the bottom right corner cut out.
class ShapedNonClientFrameView : public NonClientFrameView {
 public:
  explicit ShapedNonClientFrameView() {
  }

  virtual ~ShapedNonClientFrameView() {
  }

  // NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE {
    return bounds();
  }
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE {
    return client_bounds;
  }
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE {
    return HTNOWHERE;
  }
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {
    int right = size.width();
    int bottom = size.height();

    window_mask->moveTo(0, 0);
    window_mask->lineTo(0, bottom);
    window_mask->lineTo(right, bottom);
    window_mask->lineTo(right, 10);
    window_mask->lineTo(right - 10, 10);
    window_mask->lineTo(right - 10, 0);
    window_mask->close();
  }
  virtual void ResetWindowControls() OVERRIDE {
  }
  virtual void UpdateWindowIcon() OVERRIDE {
  }
  virtual void UpdateWindowTitle() OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShapedNonClientFrameView);
};

class ShapedWidgetDelegate : public WidgetDelegateView {
 public:
  ShapedWidgetDelegate() {
  }

  virtual ~ShapedWidgetDelegate() {
  }

  // WidgetDelegateView:
  virtual NonClientFrameView* CreateNonClientFrameView(
      Widget* widget) OVERRIDE {
    return new ShapedNonClientFrameView;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShapedWidgetDelegate);
};

// Creates a widget of size 100x100.
scoped_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = delegate;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(params);
  return widget.Pass();
}

// Returns the list of rectangles which describe |xid|'s bounding region via the
// X shape extension.
std::vector<gfx::Rect> GetShapeRects(XID xid) {
  int dummy;
  int shape_rects_size;
  XRectangle* shape_rects = XShapeGetRectangles(gfx::GetXDisplay(),
                                                xid,
                                                ShapeBounding,
                                                &shape_rects_size,
                                                &dummy);

  std::vector<gfx::Rect> shape_vector;
  for (int i = 0; i < shape_rects_size; ++i) {
    shape_vector.push_back(gfx::Rect(
        shape_rects[i].x,
        shape_rects[i].y,
        shape_rects[i].width,
        shape_rects[i].height));
  }
  XFree(shape_rects);
  return shape_vector;
}

// Returns true if one of |rects| contains point (x,y).
bool ShapeRectContainsPoint(const std::vector<gfx::Rect>& shape_rects,
                            int x,
                            int y) {
  gfx::Point point(x, y);
  for (size_t i = 0; i < shape_rects.size(); ++i) {
    if (shape_rects[i].Contains(point))
      return true;
  }
  return false;
}

}  // namespace

class DesktopWindowTreeHostX11Test : public ViewsTestBase {
 public:
  DesktopWindowTreeHostX11Test() {
  }
  virtual ~DesktopWindowTreeHostX11Test() {
  }

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();

    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    XSynchronize(gfx::GetXDisplay(), True);
  }

  virtual void TearDown() OVERRIDE {
    XSynchronize(gfx::GetXDisplay(), False);
    ViewsTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11Test);
};

// Tests that the shape is properly set on the x window.
TEST_F(DesktopWindowTreeHostX11Test, Shape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  // 1) Test setting the window shape via the NonClientFrameView. This technique
  // is used to get rounded corners on Chrome windows when not using the native
  // window frame.
  scoped_ptr<Widget> widget1 = CreateWidget(new ShapedWidgetDelegate());
  widget1->Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  XID xid1 = widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  std::vector<gfx::Rect> shape_rects = GetShapeRects(xid1);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 85, 5));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 95, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Changing widget's size should update the shape.
  widget1->SetBounds(gfx::Rect(100, 100, 200, 200));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  shape_rects = GetShapeRects(xid1);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 85, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 185, 5));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 195, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 195, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 205, 15));

  // The shape should be changed to a rectangle which fills the entire screen
  // when |widget1| is maximized.
  {
    MaximizeWaiter waiter(xid1);
    widget1->Maximize();
    waiter.Wait();
  }

  // xvfb does not support Xrandr so we cannot check the maximized window's
  // bounds.
  gfx::Rect maximized_bounds;
  ui::GetWindowRect(xid1, &maximized_bounds);

  shape_rects = GetShapeRects(xid1);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects,
                                     maximized_bounds.width() - 1,
                                     5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects,
                                     maximized_bounds.width() - 1,
                                     15));

  // 2) Test setting the window shape via Widget::SetShape().
  gfx::Path shape2;
  shape2.moveTo(10, 0);
  shape2.lineTo(10, 10);
  shape2.lineTo(0, 10);
  shape2.lineTo(0, 100);
  shape2.lineTo(100, 100);
  shape2.lineTo(100, 0);
  shape2.close();

  scoped_ptr<Widget> widget2(CreateWidget(NULL));
  widget2->Show();
  widget2->SetShape(shape2.CreateNativeRegion());
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  XID xid2 = widget2->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  shape_rects = GetShapeRects(xid2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Changing the widget's size should not affect the shape.
  widget2->SetBounds(gfx::Rect(100, 100, 200, 200));
  shape_rects = GetShapeRects(xid2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));
}

}  // namespace views
