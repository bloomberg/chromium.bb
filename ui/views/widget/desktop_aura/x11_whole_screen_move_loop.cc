// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"

#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/run_loop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// The minimum alpha before we declare a pixel transparent when searching in
// our source image.
const uint32 kMinAlpha = 32;
const unsigned char kDragWidgetOpacity = 0xc0;

class ScopedCapturer {
 public:
  explicit ScopedCapturer(aura::WindowTreeHost* host)
      : host_(host) {
    host_->SetCapture();
  }

  ~ScopedCapturer() {
    host_->ReleaseCapture();
  }

 private:
  aura::WindowTreeHost* host_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCapturer);
};

}  // namespace

X11WholeScreenMoveLoop::X11WholeScreenMoveLoop(
    X11WholeScreenMoveLoopDelegate* delegate)
    : delegate_(delegate),
      in_move_loop_(false),
      should_reset_mouse_flags_(false),
      grab_input_window_(None),
      weak_factory_(this) {
  last_xmotion_.type = LASTEvent;
}

X11WholeScreenMoveLoop::~X11WholeScreenMoveLoop() {}

void X11WholeScreenMoveLoop::DispatchMouseMovement() {
  if (!weak_factory_.HasWeakPtrs())
    return;
  weak_factory_.InvalidateWeakPtrs();
  DCHECK_EQ(MotionNotify, last_xmotion_.type);
  delegate_->OnMouseMovement(&last_xmotion_);
  last_xmotion_.type = LASTEvent;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, ui::PlatformEventDispatcher implementation:

bool X11WholeScreenMoveLoop::CanDispatchEvent(const ui::PlatformEvent& event) {
  return event->xany.window == grab_input_window_;
}

uint32_t X11WholeScreenMoveLoop::DispatchEvent(const ui::PlatformEvent& event) {
  XEvent* xev = event;

  // Note: the escape key is handled in the tab drag controller, which has
  // keyboard focus even though we took pointer grab.
  switch (xev->type) {
    case MotionNotify: {
      if (drag_widget_.get()) {
        gfx::Screen* screen = gfx::Screen::GetNativeScreen();
        gfx::Point location = gfx::ToFlooredPoint(
            screen->GetCursorScreenPoint() - drag_offset_);
        drag_widget_->SetBounds(gfx::Rect(location, drag_image_.size()));
        drag_widget_->StackAtTop();
      }
      last_xmotion_ = xev->xmotion;
      if (!weak_factory_.HasWeakPtrs()) {
        // Post a task to dispatch mouse movement event when control returns to
        // the message loop. This allows smoother dragging since the events are
        // dispatched without waiting for the drag widget updates.
        base::MessageLoopForUI::current()->PostTask(
            FROM_HERE,
            base::Bind(&X11WholeScreenMoveLoop::DispatchMouseMovement,
                       weak_factory_.GetWeakPtr()));
      }
      break;
    }
    case ButtonRelease: {
      if (xev->xbutton.button == Button1) {
        // Assume that drags are being done with the left mouse button. Only
        // break the drag if the left mouse button was released.
        DispatchMouseMovement();
        delegate_->OnMouseReleased();
      }
      break;
    }
    case KeyPress: {
      if (ui::KeyboardCodeFromXKeyEvent(xev) == ui::VKEY_ESCAPE)
        EndMoveLoop();
      break;
    }
  }

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, aura::client::WindowMoveClient implementation:

bool X11WholeScreenMoveLoop::RunMoveLoop(aura::Window* source,
                                         gfx::NativeCursor cursor) {
  // Start a capture on the host, so that it continues to receive events during
  // the drag. This may be second time we are capturing the mouse events - the
  // first being when a mouse is first pressed. That first capture needs to be
  // released before the call to GrabPointerAndKeyboard below, otherwise it may
  // get released while we still need the pointer grab, which is why we restrict
  // the scope here.
  {
    ScopedCapturer capturer(source->GetHost());

    DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
    in_move_loop_ = true;

    XDisplay* display = gfx::GetXDisplay();

    grab_input_window_ = CreateDragInputWindow(display);
    if (!drag_image_.isNull() && CheckIfIconValid())
      CreateDragImageWindow();
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    // Releasing ScopedCapturer ensures that any other instance of
    // X11ScopedCapture will not prematurely release grab that will be acquired
    // below.
  }
  // TODO(varkha): Consider integrating GrabPointerAndKeyboard with
  // ScopedCapturer to avoid possibility of logically keeping multiple grabs.
  if (!GrabPointerAndKeyboard(cursor))
    return false;

  // We are handling a mouse drag outside of the aura::RootWindow system. We
  // must manually make aura think that the mouse button is pressed so that we
  // don't draw extraneous tooltips.
  aura::Env* env = aura::Env::GetInstance();
  if (!env->IsMouseButtonDown()) {
    env->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
    should_reset_mouse_flags_ = true;
  }

  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return true;
}

void X11WholeScreenMoveLoop::UpdateCursor(gfx::NativeCursor cursor) {
  if (in_move_loop_) {
    // If we're still in the move loop, regrab the pointer with the updated
    // cursor. Note: we can be called from handling an XdndStatus message after
    // EndMoveLoop() was called, but before we return from the nested RunLoop.
    GrabPointerAndKeyboard(cursor);
  }
}

void X11WholeScreenMoveLoop::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  // Prevent DispatchMouseMovement from dispatching any posted motion event.
  weak_factory_.InvalidateWeakPtrs();
  last_xmotion_.type = LASTEvent;

  // We undo our emulated mouse click from RunMoveLoop();
  if (should_reset_mouse_flags_) {
    aura::Env::GetInstance()->set_mouse_button_flags(0);
    should_reset_mouse_flags_ = false;
  }

  // TODO(erg): Is this ungrab the cause of having to click to give input focus
  // on drawn out windows? Not ungrabbing here screws the X server until I kill
  // the chrome process.

  // Ungrab before we let go of the window.
  XDisplay* display = gfx::GetXDisplay();
  XUngrabPointer(display, CurrentTime);
  XUngrabKeyboard(display, CurrentTime);

  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  drag_widget_.reset();
  delegate_->OnMoveLoopEnded();
  XDestroyWindow(display, grab_input_window_);

  in_move_loop_ = false;
  quit_closure_.Run();
}

void X11WholeScreenMoveLoop::SetDragImage(const gfx::ImageSkia& image,
                                          gfx::Vector2dF offset) {
  drag_image_ = image;
  drag_offset_ = offset;
  // Reset the Y offset, so that the drag-image is always just below the cursor,
  // so that it is possible to see where the cursor is going.
  drag_offset_.set_y(0.f);
}

bool X11WholeScreenMoveLoop::GrabPointerAndKeyboard(gfx::NativeCursor cursor) {
  XDisplay* display = gfx::GetXDisplay();
  XGrabServer(display);

  XUngrabPointer(display, CurrentTime);
  int ret = XGrabPointer(
      display,
      grab_input_window_,
      False,
      ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      cursor.platform(),
      CurrentTime);
  if (ret != GrabSuccess) {
    DLOG(ERROR) << "Grabbing pointer for dragging failed: "
                << ui::GetX11ErrorString(display, ret);
  } else {
    XUngrabKeyboard(display, CurrentTime);
    ret = XGrabKeyboard(
        display,
        grab_input_window_,
        False,
        GrabModeAsync,
        GrabModeAsync,
        CurrentTime);
    if (ret != GrabSuccess) {
      DLOG(ERROR) << "Grabbing keyboard for dragging failed: "
                  << ui::GetX11ErrorString(display, ret);
    }
  }

  XUngrabServer(display);
  return ret == GrabSuccess;
}

Window X11WholeScreenMoveLoop::CreateDragInputWindow(XDisplay* display) {
  // Creates an invisible, InputOnly toplevel window. This window will receive
  // all mouse movement for drags. It turns out that normal windows doing a
  // grab doesn't redirect pointer motion events if the pointer isn't over the
  // grabbing window. But InputOnly windows are able to grab everything. This
  // is what GTK+ does, and I found a patch to KDE that did something similar.
  unsigned long attribute_mask = CWEventMask | CWOverrideRedirect;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                   KeyPressMask | KeyReleaseMask | StructureNotifyMask;
  swa.override_redirect = True;
  Window window = XCreateWindow(display,
                                DefaultRootWindow(display),
                                -100, -100, 10, 10,
                                0, CopyFromParent, InputOnly, CopyFromParent,
                                attribute_mask, &swa);
  XMapRaised(display, window);
  ui::X11EventSource::GetInstance()->BlockUntilWindowMapped(window);
  return window;
}

void X11WholeScreenMoveLoop::CreateDragImageWindow() {
  Widget* widget = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_DRAG);
  params.opacity = Widget::InitParams::OPAQUE_WINDOW;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;

  gfx::Point location = gfx::ToFlooredPoint(
      gfx::Screen::GetNativeScreen()->GetCursorScreenPoint() - drag_offset_);
  params.bounds = gfx::Rect(location, drag_image_.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(Widget::FRAME_TYPE_FORCE_NATIVE);
  widget->Init(params);
  widget->SetOpacity(kDragWidgetOpacity);
  widget->GetNativeWindow()->SetName("DragWindow");

  ImageView* image = new ImageView();
  image->SetImage(drag_image_);
  image->SetBounds(0, 0, drag_image_.width(), drag_image_.height());
  widget->SetContentsView(image);
  widget->Show();
  widget->GetNativeWindow()->layer()->SetFillsBoundsOpaquely(false);

  drag_widget_.reset(widget);
}

bool X11WholeScreenMoveLoop::CheckIfIconValid() {
  // TODO(erg): I've tried at least five different strategies for trying to
  // build a mask based off the alpha channel. While all of them have worked,
  // none of them have been performant and introduced multiple second
  // delays. (I spent a day getting a rectangle segmentation algorithm polished
  // here...and then found that even through I had the rectangle extraction
  // down to mere milliseconds, SkRegion still fell over on the number of
  // rectangles.)
  //
  // Creating a mask here near instantaneously should be possible, as GTK does
  // it, but I've blown days on this and I'm punting now.

  const SkBitmap* in_bitmap = drag_image_.bitmap();
  SkAutoLockPixels in_lock(*in_bitmap);
  for (int y = 0; y < in_bitmap->height(); ++y) {
    uint32* in_row = in_bitmap->getAddr32(0, y);

    for (int x = 0; x < in_bitmap->width(); ++x) {
      if (SkColorGetA(in_row[x]) > kMinAlpha)
        return true;
    }
  }

  return false;
}

}  // namespace views
