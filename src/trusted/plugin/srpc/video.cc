/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#if NACL_OSX
#include <Carbon/Carbon.h>
#endif  // NACL_OSX
#if NACL_WINDOWS
#include <windows.h>
#include <windowsx.h>
#endif  // NACL_WINDOWS
#if NACL_LINUX && defined(MOZ_X11)
#include <sched.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#endif  // NACL_LINUX && defined(MOZ_X11)

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"


#include "native_client/src/trusted/plugin/srpc/portable_handle.h"

#include "native_client/src/trusted/plugin/srpc/video.h"

namespace nacl_srpc {
  class Plugin;
}

// TODO(nfullagar): this could be cleaner/more readable for
// platform-specific implementations to go into separate files under
// platform subdirectories, rather than using ifdefs.

namespace nacl {

class GlobalVideoMutex {
 private:
  NaClMutex mutex_;
 public:
  void Lock() { NaClMutexLock(&mutex_); }
  void Unlock() { NaClMutexUnlock(&mutex_); }
  GlobalVideoMutex() { NaClMutexCtor(&mutex_); }
  ~GlobalVideoMutex() { NaClMutexDtor(&mutex_); }
};

static GlobalVideoMutex globalVideoMutex;

void VideoGlobalLock() {
  globalVideoMutex.Lock();
}

void VideoGlobalUnlock() {
  globalVideoMutex.Unlock();
}


#if NACL_LINUX && defined(MOZ_X11)
static int XKeysymToNaCl(KeySym xsym) {
  if ((xsym & 0xFF00) == 0x0000)
    return xsym;
  switch (xsym) {
    case XK_Alt_L: return NACL_KEY_LALT;
    case XK_Alt_R: return NACL_KEY_RALT;
    case XK_BackSpace: return NACL_KEY_BACKSPACE;
    case XK_Break: return NACL_KEY_BREAK;
    case XK_Caps_Lock: return NACL_KEY_CAPSLOCK;
    case XK_Clear: return NACL_KEY_CLEAR;
    case XK_Control_L: return NACL_KEY_LCTRL;
    case XK_Control_R: return NACL_KEY_RCTRL;
    case XK_Delete: return NACL_KEY_DELETE;
    case XK_Down: return NACL_KEY_DOWN;
    case XK_End: return NACL_KEY_END;
    case XK_Escape: return NACL_KEY_ESCAPE;
    case XK_F1: return NACL_KEY_F1;
    case XK_F2: return NACL_KEY_F2;
    case XK_F3: return NACL_KEY_F3;
    case XK_F4: return NACL_KEY_F4;
    case XK_F5: return NACL_KEY_F5;
    case XK_F6: return NACL_KEY_F6;
    case XK_F7: return NACL_KEY_F7;
    case XK_F8: return NACL_KEY_F8;
    case XK_F9: return NACL_KEY_F9;
    case XK_F10: return NACL_KEY_F10;
    case XK_F11: return NACL_KEY_F11;
    case XK_F12: return NACL_KEY_F12;
    case XK_F13: return NACL_KEY_F13;
    case XK_F14: return NACL_KEY_F14;
    case XK_F15: return NACL_KEY_F15;
    case XK_Help: return NACL_KEY_HELP;
    case XK_Home: return NACL_KEY_HOME;
    case XK_Hyper_R: return NACL_KEY_MENU;
    case XK_Insert: return NACL_KEY_INSERT;
    case XK_KP_0: return NACL_KEY_KP0;
    case XK_KP_1: return NACL_KEY_KP1;
    case XK_KP_2: return NACL_KEY_KP2;
    case XK_KP_3: return NACL_KEY_KP3;
    case XK_KP_4: return NACL_KEY_KP4;
    case XK_KP_5: return NACL_KEY_KP5;
    case XK_KP_6: return NACL_KEY_KP6;
    case XK_KP_7: return NACL_KEY_KP7;
    case XK_KP_8: return NACL_KEY_KP8;
    case XK_KP_9: return NACL_KEY_KP9;
    case XK_KP_Add: return NACL_KEY_KP_PLUS;
    case XK_KP_Begin: return NACL_KEY_KP5;
    case XK_KP_Decimal: return NACL_KEY_KP_PERIOD;
    case XK_KP_Delete: return NACL_KEY_KP_PERIOD;
    case XK_KP_Divide: return NACL_KEY_KP_DIVIDE;
    case XK_KP_Down: return NACL_KEY_KP2;
    case XK_KP_End: return NACL_KEY_KP1;
    case XK_KP_Enter: return NACL_KEY_KP_ENTER;
    case XK_KP_Equal: return NACL_KEY_KP_EQUALS;
    case XK_KP_Home: return NACL_KEY_KP7;
    case XK_KP_Insert: return NACL_KEY_KP0;
    case XK_KP_Left: return NACL_KEY_KP4;
    case XK_KP_Multiply: return NACL_KEY_KP_MULTIPLY;
    case XK_KP_Page_Down: return NACL_KEY_KP3;
    case XK_KP_Page_Up: return NACL_KEY_KP9;
    case XK_KP_Right: return NACL_KEY_KP6;
    case XK_KP_Subtract: return NACL_KEY_KP_MINUS;
    case XK_KP_Up: return NACL_KEY_KP8;
    case XK_Left: return NACL_KEY_LEFT;
    case XK_Num_Lock: return NACL_KEY_NUMLOCK;
    case XK_Menu: return NACL_KEY_MENU;
    case XK_Meta_R: return NACL_KEY_RMETA;
    case XK_Meta_L: return NACL_KEY_LMETA;
    case XK_Mode_switch: return NACL_KEY_MODE;
    case XK_Multi_key: return NACL_KEY_COMPOSE;
    case XK_Page_Down: return NACL_KEY_PAGEDOWN;
    case XK_Page_Up: return NACL_KEY_PAGEUP;
    case XK_Pause: return NACL_KEY_PAUSE;
    case XK_Print: return NACL_KEY_PRINT;
    case XK_Return: return NACL_KEY_RETURN;
    case XK_Right: return NACL_KEY_RIGHT;
    case XK_Scroll_Lock: return NACL_KEY_SCROLLOCK;
    case XK_Shift_R: return NACL_KEY_RSHIFT;
    case XK_Shift_L: return NACL_KEY_LSHIFT;
    case XK_Super_L: return NACL_KEY_LSUPER;
    case XK_Super_R: return NACL_KEY_RSUPER;
    case XK_Sys_Req: return NACL_KEY_SYSREQ;
    case XK_Tab: return NACL_KEY_TAB;
    case XK_Up: return NACL_KEY_UP;
  }
  return NACL_KEY_UNKNOWN;
}

void VideoMap::RedrawAsync(void *platform_parm) {
  dprintf(("VideoMap::RedrawAsync %p\n", static_cast<void *>(platform_parm)));
  if (0 != untrusted_video_share_->u.h.video_ready) {
    Display *display = reinterpret_cast<Display *>(platform_parm);
    Drawable window = reinterpret_cast<Drawable>(window_->window);
    GC gc = XCreateGC(display, window, 0, NULL);
    XImage image;
    memset(&image, 0, sizeof image);
    image.format = ZPixmap;
    image.data = reinterpret_cast<char*>
        (&untrusted_video_share_->video_pixels[0]);
    image.width = window_->width;
    image.height = window_->height;
    image.xoffset = 0;
    image.byte_order = LSBFirst;
    image.bitmap_bit_order = MSBFirst;
    image.bits_per_pixel = 32;
    image.bytes_per_line = 4 * window_->width;
    image.bitmap_unit = 32;
    image.bitmap_pad = 32;
    image.depth = 24;
    XPutImage(display, window, gc, &image, 0, 0, 0, 0,
          window_->width, window_->height);
    XFreeGC(display, gc);
    XFlush(display);
  }
  sched_yield();
}

void VideoMap::Redraw() {
  Display* display =
      static_cast<NPSetWindowCallbackStruct*>(window_->ws_info)->display;
  this->RedrawAsync(reinterpret_cast<void *>(display));
}

void VideoMap::XEventHandler(Widget widget,
                             VideoMap* video,
                             XEvent* xevent,
                             Boolean* b) {
  VideoScopedGlobalLock video_lock;

  UNREFERENCED_PARAMETER(widget);
  UNREFERENCED_PARAMETER(b);
  if ((NULL == video) || (NULL == video->window_) ||
      (NULL == video->untrusted_video_share_))
    return;
  Window xwin = reinterpret_cast<Window>(video->window_->window);
  NPSetWindowCallbackStruct* wcbs =
      reinterpret_cast<NPSetWindowCallbackStruct*>(video->window_->ws_info);
  Display* dpy = wcbs->display;
  union NaClMultimediaEvent nacl_event;
  KeySym xsym;
  int button;
  int nsym;
  uint16_t x;
  uint16_t y;

  switch (xevent->type) {
    case Expose:
      // Exposure events come in multiples, one per rectangle uncovered.
      // We just look at one and redraw the whole region.
      while (XCheckTypedWindowEvent(dpy, xwin, Expose, xevent)) {
        /* Empty body to avoid warning */
      }
      video->Redraw();
      break;
    case KeyPress:
      nacl_event.type = NACL_EVENT_KEY_DOWN;
      nacl_event.key.which = 0;
      nacl_event.key.state = kSet;
      nacl_event.key.keysym.scancode = xevent->xkey.keycode;
      xsym = XKeycodeToKeysym(dpy, xevent->xkey.keycode, 0);
      nsym = XKeysymToNaCl(xsym);
      video->SetKeyMod(nsym, kSet);
      nacl_event.key.keysym.sym = nsym;
      nacl_event.key.keysym.mod = video->event_state_key_mod_;
      nacl_event.key.keysym.unicode = 0;
      video->EventQueuePut(&nacl_event);
      break;
    case KeyRelease:
      nacl_event.type = NACL_EVENT_KEY_UP;
      nacl_event.key.which = 0;
      nacl_event.key.state = kClear;
      nacl_event.key.keysym.scancode = xevent->xkey.keycode;
      xsym = XKeycodeToKeysym(dpy, xevent->xkey.keycode, 0);
      nsym = XKeysymToNaCl(xsym);
      video->SetKeyMod(nsym, kClear);
      nacl_event.key.keysym.sym = nsym;
      nacl_event.key.keysym.mod = video->event_state_key_mod_;
      nacl_event.key.keysym.unicode = 0;
      video->EventQueuePut(&nacl_event);
      break;
    case MotionNotify:
      x = static_cast<uint16_t>(xevent->xmotion.x);
      y = static_cast<uint16_t>(xevent->xmotion.y);
      nacl_event.type = NACL_EVENT_MOUSE_MOTION;
      nacl_event.motion.which = 0;
      nacl_event.motion.state = video->GetButton();
      nacl_event.motion.x = x;
      nacl_event.motion.y = y;
      video->GetRelativeMotion(x, y,
                               &nacl_event.motion.xrel,
                               &nacl_event.motion.yrel);
      video->SetMotion(xevent->xmotion.x, xevent->xmotion.y, 1);
      video->EventQueuePut(&nacl_event);
      break;
    case ButtonPress:
      button = xevent->xbutton.button;
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_DOWN;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kSet;
      video->GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      video->SetButton(button, 1);
      video->EventQueuePut(&nacl_event);
      break;
    case ButtonRelease:
      button = xevent->xbutton.button;
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_UP;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kClear;
      video->GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      video->SetButton(button, 0);
      video->EventQueuePut(&nacl_event);
      break;
    case LeaveNotify:
      nacl_event.type = NACL_EVENT_ACTIVE;
      nacl_event.active.state = kClear;
      video->EventQueuePut(&nacl_event);
      break;
    case EnterNotify:
      nacl_event.type = NACL_EVENT_ACTIVE;
      nacl_event.active.state = kSet;
      video->SetMotion(0, 0, 0);
      video->EventQueuePut(&nacl_event);
      break;
    default:
      // Other types of events should be handled here.
      dprintf(("VideoMap::XEventHandler()     Other\n"));
      break;
  }
}
#endif  // NACL_LINUX && defined(MOZ_X11)

#if NACL_OSX
// convert global mouse coordinate to local space
// TODO(nfullagar): why do these only work in the paint event?
static void GlobalToLocalHelper(NPWindow* window, Point *point) {
  NP_Port* npport = static_cast<NP_Port*>(window->window);
  CGrafPtr our_port = npport->port;
  GrafPtr save;
  GetPort(&save);
  SetPort(reinterpret_cast<GrafPtr>(our_port));
  GlobalToLocal(point);
  dprintf(("mouse local pos %d %d\n", point->h, point->v));
  SetPort(save);
}

// converts mac raw key to nacl key
static int MacKeyToNaCl(int mk) {
  switch (mk) {
    case 0x00: return NACL_KEY_a;
    case 0x0B: return NACL_KEY_b;
    case 0x08: return NACL_KEY_c;
    case 0x02: return NACL_KEY_d;
    case 0x0E: return NACL_KEY_e;
    case 0x03: return NACL_KEY_f;
    case 0x05: return NACL_KEY_g;
    case 0x04: return NACL_KEY_h;
    case 0x22: return NACL_KEY_i;
    case 0x26: return NACL_KEY_j;
    case 0x28: return NACL_KEY_k;
    case 0x25: return NACL_KEY_l;
    case 0x2E: return NACL_KEY_m;
    case 0x2D: return NACL_KEY_n;
    case 0x1F: return NACL_KEY_o;
    case 0x23: return NACL_KEY_p;
    case 0x0C: return NACL_KEY_q;
    case 0x0F: return NACL_KEY_r;
    case 0x01: return NACL_KEY_s;
    case 0x11: return NACL_KEY_t;
    case 0x20: return NACL_KEY_u;
    case 0x09: return NACL_KEY_v;
    case 0x0D: return NACL_KEY_w;
    case 0x07: return NACL_KEY_x;
    case 0x10: return NACL_KEY_y;
    case 0x06: return NACL_KEY_z;
    case 0x12: return NACL_KEY_1;
    case 0x13: return NACL_KEY_2;
    case 0x14: return NACL_KEY_3;
    case 0x15: return NACL_KEY_4;
    case 0x17: return NACL_KEY_5;
    case 0x16: return NACL_KEY_6;
    case 0x1A: return NACL_KEY_7;
    case 0x1C: return NACL_KEY_8;
    case 0x19: return NACL_KEY_9;
    case 0x1D: return NACL_KEY_0;
    case 0x32: return NACL_KEY_BACKQUOTE;
    case 0x2A: return NACL_KEY_BACKSLASH;
    case 0x33: return NACL_KEY_BACKSPACE;
    case 0x39: return NACL_KEY_CAPSLOCK;
    case 0x2B: return NACL_KEY_COMMA;
    case 0x75: return NACL_KEY_DELETE;
    case 0x7D: return NACL_KEY_DOWN;
    case 0x77: return NACL_KEY_END;
    case 0x18: return NACL_KEY_EQUALS;
    case 0x35: return NACL_KEY_ESCAPE;
    case 0x73: return NACL_KEY_HOME;
    case 0x72: return NACL_KEY_INSERT;
    case 0x52: return NACL_KEY_KP0;
    case 0x53: return NACL_KEY_KP1;
    case 0x54: return NACL_KEY_KP2;
    case 0x55: return NACL_KEY_KP3;
    case 0x56: return NACL_KEY_KP4;
    case 0x57: return NACL_KEY_KP5;
    case 0x58: return NACL_KEY_KP6;
    case 0x59: return NACL_KEY_KP7;
    case 0x5B: return NACL_KEY_KP8;
    case 0x5C: return NACL_KEY_KP9;
    case 0x4B: return NACL_KEY_KP_DIVIDE;
    case 0x34: return NACL_KEY_KP_ENTER;
    case 0x4C: return NACL_KEY_KP_ENTER;
    case 0x51: return NACL_KEY_KP_EQUALS;
    case 0x4E: return NACL_KEY_KP_MINUS;
    case 0x43: return NACL_KEY_KP_MULTIPLY;
    case 0x41: return NACL_KEY_KP_PERIOD;
    case 0x45: return NACL_KEY_KP_PLUS;
    case 0x3A: return NACL_KEY_LALT;
    case 0x3B: return NACL_KEY_LCTRL;
    case 0x7B: return NACL_KEY_LEFT;
    case 0x21: return NACL_KEY_LEFTBRACKET;
    case 0x37: return NACL_KEY_LMETA;
    case 0x38: return NACL_KEY_LSHIFT;
    case 0x1B: return NACL_KEY_MINUS;
    case 0x47: return NACL_KEY_NUMLOCK;
    case 0x79: return NACL_KEY_PAGEDOWN;
    case 0x74: return NACL_KEY_PAGEUP;
    case 0x71: return NACL_KEY_PAUSE;
    case 0x2F: return NACL_KEY_PERIOD;
    case 0x69: return NACL_KEY_PRINT;
    case 0x27: return NACL_KEY_QUOTE;
    case 0x3D: return NACL_KEY_RALT;
    case 0x3E: return NACL_KEY_RCTRL;
    case 0x24: return NACL_KEY_RETURN;
    case 0x7C: return NACL_KEY_RIGHT;
    case 0x1E: return NACL_KEY_RIGHTBRACKET;
    case 0x36: return NACL_KEY_RMETA;
    case 0x3C: return NACL_KEY_RSHIFT;
    case 0x6B: return NACL_KEY_SCROLLOCK;
    case 0x29: return NACL_KEY_SEMICOLON;
    case 0x30: return NACL_KEY_TAB;
    case 0x2C: return NACL_KEY_SLASH;
    case 0x31: return NACL_KEY_SPACE;
    case 0x7E: return NACL_KEY_UP;
  }
  return NACL_KEY_UNKNOWN;
}

int16_t VideoMap::HandleEvent(void *param) {
  dprintf(("VideoMap::HandleEvent(%p, %p)\n",
           static_cast<void *>(this),
           static_cast<void *>(param)));
  uint16_t x;
  uint16_t y;
  uint16_t button;
  const int rightMouseButtonMod = 4096;
  VideoScopedGlobalLock video_lock;

  if ((NULL == window_) || (NULL == untrusted_video_share_))
    return 0;

  // MacOS delivers its events through the NPAPI HandleEvent API.
  // TODO(nfullagar): switch this to a carbon event handler
  EventRecord* event = static_cast<EventRecord*>(param);
  NaClMultimediaEvent nacl_event;

  if (!event) {
    return 0;
  }
  dprintf(("Event type %d\n", event->what));

  // poll the mouse and see if it moved within our window
  // TODO(nfullagar): only mouse events during updateEvt seem to
  // TODO(nfullagar): provide good local coordinates.
  // TODO(nfullagar): hmm... osEvt+18 works much better in FF3
  // osEvt + 18 is NPAPIs mousemove event
  const int mouseMoveBoundToEvent = osEvt + 18;
  if (event->what == mouseMoveBoundToEvent) {
    Point mouse_position;
    Point global_mouse_position;
    GetGlobalMouse(&global_mouse_position);
    mouse_position = global_mouse_position;
    if (NULL != window_) {
      GlobalToLocalHelper(window_, &mouse_position);
      if ((mouse_position.h >= 0) && (mouse_position.v >= 0) &&
          (static_cast<uint32_t>(mouse_position.h) < window_->width) &&
          (static_cast<uint32_t>(mouse_position.v) < window_->height)) {
        uint16_t last_x, last_y;
        x = static_cast<uint16_t>(mouse_position.h);
        y = static_cast<uint16_t>(mouse_position.v);
        GetMotion(&last_x, &last_y);
        if ((x != last_x) ||
            (y != last_y)) {
          dprintf(("The mouse moved %d %d  (event %d)\n",
                   static_cast<int>(x),
                   static_cast<int>(y),
                   event->what));
          nacl_event.type = NACL_EVENT_MOUSE_MOTION;
          nacl_event.motion.which = 0;
          nacl_event.motion.state = GetButton();
          nacl_event.motion.x = x;
          nacl_event.motion.y = y;
          GetRelativeMotion(x, y,
              &nacl_event.motion.xrel, &nacl_event.motion.yrel);
          SetMotion(x, y, 1);
          EventQueuePut(&nacl_event);
        }
      }
    }
  }

  if ((event->what != 0) && (event->what != updateEvt))
    dprintf(("an event happened %d\n", event->what));

  switch (event->what) {
    case (osEvt + 16): {  // NPAPI getFocusEvent:
      nacl_event.type = NACL_EVENT_ACTIVE;
      nacl_event.active.state = kSet;
      EventQueuePut(&nacl_event);
      dprintf(("Focus gained\n"));
      break;
    }
    case (osEvt + 17): {  // NPAPI loseFocusEvent:
      nacl_event.type = NACL_EVENT_ACTIVE;
      nacl_event.active.state = kClear;
      EventQueuePut(&nacl_event);
      dprintf(("Focus lost\n"));
      break;
    }
    case keyUp:
    case keyDown: {
      int scancode = (event->message & keyCodeMask) >> 8;
      int nsym = MacKeyToNaCl(scancode);
      if (keyDown == event->what) {
        nacl_event.type = NACL_EVENT_KEY_DOWN;
        nacl_event.key.state = kSet;
      } else {
        nacl_event.type = NACL_EVENT_KEY_UP;
        nacl_event.key.state = kClear;
      }
      nacl_event.key.which = 0;
      nacl_event.key.keysym.scancode = scancode;
      SetKeyMod(nsym, nacl_event.key.state);
      nacl_event.key.keysym.sym = nsym;
      nacl_event.key.keysym.mod = event_state_key_mod_;
      nacl_event.key.keysym.unicode = 0;
      EventQueuePut(&nacl_event);
      dprintf(("A key press (scan: %d sym: %d state: %d)\n",
          scancode, nsym, nacl_event.key.state));
      break;
    }
    case mouseDown: {
      dprintf(("A mouse down happened\n"));
      button = 1;
      if (event->modifiers & rightMouseButtonMod) {
        button = 3;
      }
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_DOWN;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kSet;
      GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      SetButton(button, kSet);
      EventQueuePut(&nacl_event);
      break;
    }
    case mouseUp: {
      dprintf(("A mouse up happened\n"));
      button = 1;
      if (event->modifiers & rightMouseButtonMod) {
        button = 3;
      }
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_UP;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kClear;
      GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      SetButton(button, kClear);
      EventQueuePut(&nacl_event);
      break;
    }
    case nullEvent: {
      // Null events are set up by the browser to occur every ~16ms.
      // We use this to redraw whenever an upcall has requested one.
      if (request_redraw_ && (NULL != window_)) {
        request_redraw_ = false;
        // invalidate rect to generate an updateEvt
        Invalidate();
      }
      break;
    }
    case updateEvt: {
      Redraw();
      break;
    }
    default: {
      // we didn't handle this event...
      return 0;
    }
  }
  return 1;
}

void VideoMap::RedrawAsync(void* platform_parm) {
  dprintf(("VideoMap::RedrawAsync(%p)\n", static_cast<void*>(platform_parm)));
  if (NULL == platform_parm) return;

  if (!window_ || !window_->window || !untrusted_video_share_ ||
      window_->clipRect.right <= window_->clipRect.left) {
    return;
  }

  RgnHandle saved_clip = NewRgn();
  if (!saved_clip) {
    return;
  }
  GrafPtr saved_port;
  Rect revealed_rect;
  short saved_port_top;
  short saved_port_left;

  NP_Port* npport = static_cast<NP_Port*>(window_->window);
  CGrafPtr our_port = npport->port;
  GetPort(&saved_port);
  SetPort(reinterpret_cast<GrafPtr>(our_port));

  Rect port_rect;
  GetPortBounds(our_port, &port_rect);
  saved_port_top = port_rect.top;
  saved_port_left = port_rect.left;
  GetClip(saved_clip);

  revealed_rect.top = window_->clipRect.top + npport->porty;
  revealed_rect.left = window_->clipRect.left + npport->portx;
  revealed_rect.bottom = window_->clipRect.bottom + npport->porty;
  revealed_rect.right = window_->clipRect.right + npport->portx;
  SetOrigin(npport->portx, npport->porty);
  ClipRect(&revealed_rect);

  Rect bounds;
  // NB: the asymmetry in bounding box sizes is due to the fact that a pixmap
  // appears not to contain its last row value. Hence we need one more
  // in height than in width.
  // TODO(nfullagar): get to the bottom of this
  SetRect(&bounds, 0, 0, window_->width - 1, window_->height);
  GWorldPtr gworld;
  OSErr result = NewGWorld(&gworld, 32, &bounds, 0, 0, 0);
  if (result == noErr) {
    PixMapHandle pixmap;
    pixmap = GetGWorldPixMap(gworld);
    if (LockPixels(pixmap)) {
      // TODO(shiki): Find a way to avoid bitmap copy and conversion
      //              (ARGB to RGBA) operations.
      uint32_t* dst = reinterpret_cast<uint32_t*>(GetPixBaseAddr(pixmap));
      uint32_t* src = reinterpret_cast<uint32_t*>
          (&untrusted_video_share_->video_pixels[0]);
      for (unsigned int n = 0; n < window_->width * window_->height; ++n) {
        // Convert from ARGB to RGBA.
        uint32_t color = *src++;
        color = (0x000000ff & (color >> 24) |
                (0x0000ff00 & (color >> 8)) |
                (0x00ff0000 & (color << 8)) |
                (0xff000000 & (color << 24)));
        *dst++ = color;
      }
      UnlockPixels(pixmap);
    }
    LockPortBits(gworld);
    CopyBits(GetPortBitMapForCopyBits(gworld),
             GetPortBitMapForCopyBits(our_port),
             &bounds,
             &bounds,
             srcCopy,
             NULL);
    UnlockPortBits(gworld);
    DisposeGWorld(gworld);
  }

  SetOrigin(saved_port_left, saved_port_top);
  SetClip(saved_clip);
  SetPort(saved_port);
  DisposeRgn(saved_clip);
}

void VideoMap::Redraw() {
  int i = 0;
  // non-null to tell it we're from npapi thread
  // TODO(nfullagar): figure out a thread safe way to render on mac
  this->RedrawAsync(&i);
}
#endif  // NACL_OSX

#if NACL_WINDOWS
static int WinKeyToNacl(int vk) {
  switch (vk) {
    case VK_ADD: return NACL_KEY_KP_PLUS;
    case VK_BACK: return NACL_KEY_BACKSPACE;
    case VK_CAPITAL: return NACL_KEY_CAPSLOCK;
    case VK_CLEAR: return NACL_KEY_CLEAR;
    case VK_DECIMAL: return NACL_KEY_KP_PERIOD;
    case VK_DELETE: return NACL_KEY_DELETE;
    case VK_DIVIDE: return NACL_KEY_KP_DIVIDE;
    case VK_DOWN: return NACL_KEY_DOWN;
    case VK_END: return NACL_KEY_END;
    case VK_ESCAPE: return NACL_KEY_ESCAPE;
    case VK_F1: return NACL_KEY_F1;
    case VK_F2: return NACL_KEY_F2;
    case VK_F3: return NACL_KEY_F3;
    case VK_F4: return NACL_KEY_F4;
    case VK_F5: return NACL_KEY_F5;
    case VK_F6: return NACL_KEY_F6;
    case VK_F7: return NACL_KEY_F7;
    case VK_F8: return NACL_KEY_F8;
    case VK_F9: return NACL_KEY_F9;
    case VK_F10: return NACL_KEY_F10;
    case VK_F11: return NACL_KEY_F11;
    case VK_F12: return NACL_KEY_F12;
    case VK_F13: return NACL_KEY_F13;
    case VK_F14: return NACL_KEY_F14;
    case VK_F15: return NACL_KEY_F15;
    case VK_HELP: return NACL_KEY_HELP;
    case VK_HOME: return NACL_KEY_HOME;
    case VK_INSERT: return NACL_KEY_INSERT;
    case VK_LCONTROL: return NACL_KEY_LCTRL;
    case VK_LEFT: return NACL_KEY_LEFT;
    case VK_LMENU: return NACL_KEY_LALT;
    case VK_LSHIFT: return NACL_KEY_LSHIFT;
    case VK_MULTIPLY: return NACL_KEY_KP_MULTIPLY;
    case VK_NEXT: return NACL_KEY_PAGEDOWN;
    case VK_NUMLOCK: return NACL_KEY_NUMLOCK;
    case VK_NUMPAD0: return NACL_KEY_KP0;
    case VK_NUMPAD1: return NACL_KEY_KP1;
    case VK_NUMPAD2: return NACL_KEY_KP2;
    case VK_NUMPAD3: return NACL_KEY_KP3;
    case VK_NUMPAD4: return NACL_KEY_KP4;
    case VK_NUMPAD5: return NACL_KEY_KP5;
    case VK_NUMPAD6: return NACL_KEY_KP6;
    case VK_NUMPAD7: return NACL_KEY_KP7;
    case VK_NUMPAD8: return NACL_KEY_KP8;
    case VK_NUMPAD9: return NACL_KEY_KP9;
    case VK_OEM_1: return NACL_KEY_SEMICOLON;
    case VK_OEM_2: return NACL_KEY_SLASH;
    case VK_OEM_3: return NACL_KEY_BACKQUOTE;
    case VK_OEM_4: return NACL_KEY_LEFTBRACKET;
    case VK_OEM_5: return NACL_KEY_BACKSLASH;
    case VK_OEM_6: return NACL_KEY_RIGHTBRACKET;
    case VK_OEM_7: return NACL_KEY_QUOTE;
    case VK_OEM_8: return NACL_KEY_BACKQUOTE;
    case VK_OEM_COMMA: return NACL_KEY_COMMA;
    case VK_OEM_MINUS: return NACL_KEY_MINUS;
    case VK_OEM_PERIOD: return NACL_KEY_PERIOD;
    case VK_OEM_PLUS: return NACL_KEY_EQUALS;
    case VK_OEM_102: return NACL_KEY_LESS;
    case VK_PAUSE: return NACL_KEY_PAUSE;
    case VK_PRIOR: return NACL_KEY_PAGEUP;
    case VK_RCONTROL: return NACL_KEY_RCTRL;
    case VK_RETURN: return NACL_KEY_RETURN;
    case VK_RIGHT: return NACL_KEY_RIGHT;
    case VK_RMENU: return NACL_KEY_RALT;
    case VK_RSHIFT: return NACL_KEY_RSHIFT;
    case VK_SCROLL: return NACL_KEY_SCROLLOCK;
    case VK_SPACE: return NACL_KEY_SPACE;
    case VK_SUBTRACT: return NACL_KEY_KP_MINUS;
    case VK_TAB: return NACL_KEY_TAB;
    case VK_UP: return NACL_KEY_UP;
    case '0': return NACL_KEY_0;
    case '1': return NACL_KEY_1;
    case '2': return NACL_KEY_2;
    case '3': return NACL_KEY_3;
    case '4': return NACL_KEY_4;
    case '5': return NACL_KEY_5;
    case '6': return NACL_KEY_6;
    case '7': return NACL_KEY_7;
    case '8': return NACL_KEY_8;
    case '9': return NACL_KEY_9;
    case 'A': return NACL_KEY_a;
    case 'B': return NACL_KEY_b;
    case 'C': return NACL_KEY_c;
    case 'D': return NACL_KEY_d;
    case 'E': return NACL_KEY_e;
    case 'F': return NACL_KEY_f;
    case 'G': return NACL_KEY_g;
    case 'H': return NACL_KEY_h;
    case 'I': return NACL_KEY_i;
    case 'J': return NACL_KEY_j;
    case 'K': return NACL_KEY_k;
    case 'L': return NACL_KEY_l;
    case 'M': return NACL_KEY_m;
    case 'N': return NACL_KEY_n;
    case 'O': return NACL_KEY_o;
    case 'P': return NACL_KEY_p;
    case 'Q': return NACL_KEY_q;
    case 'R': return NACL_KEY_r;
    case 'S': return NACL_KEY_s;
    case 'T': return NACL_KEY_t;
    case 'U': return NACL_KEY_u;
    case 'V': return NACL_KEY_v;
    case 'W': return NACL_KEY_w;
    case 'X': return NACL_KEY_x;
    case 'Y': return NACL_KEY_y;
    case 'Z': return NACL_KEY_z;
  }
  return NACL_KEY_UNKNOWN;
}

LRESULT CALLBACK VideoMap::WindowProcedure(HWND hwnd,
                                           UINT msg,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  dprintf(("VideoMap::VideoWindowProcedure()\n"));
  VideoScopedGlobalLock video_lock;
  uint16_t x;
  uint16_t y;
  uint8_t button;
  union NaClMultimediaEvent nacl_event;
  int nsym;
  const int KEY_REPEAT_BIT = (1 << 30);
  const int KEY_EXTENDED_BIT = (1 << 24);
  VideoMap *video = reinterpret_cast<VideoMap*>(
      GetWindowLong(hwnd, GWL_USERDATA));
  if ((NULL == video) || (NULL == video->untrusted_video_share_)) {
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }

  switch (msg) {
    case WM_ERASEBKGND:
      // return non-zero value to indicate no further erasing is required
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (NULL != video->untrusted_video_share_) {
        uint32_t* pixel_bits = reinterpret_cast<uint32_t*>
            (&video->untrusted_video_share_->video_pixels[0]);
        BITMAPINFO bmp_info;
        bmp_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmp_info.bmiHeader.biWidth = video->window_->width;
        bmp_info.bmiHeader.biHeight =
            -static_cast<LONG>(video->window_->height);  // top-down
        bmp_info.bmiHeader.biPlanes = 1;
        bmp_info.bmiHeader.biBitCount = 32;
        bmp_info.bmiHeader.biCompression = BI_RGB;
        bmp_info.bmiHeader.biSizeImage = 0;
        bmp_info.bmiHeader.biXPelsPerMeter = 0;
        bmp_info.bmiHeader.biYPelsPerMeter = 0;
        bmp_info.bmiHeader.biClrUsed = 0;
        bmp_info.bmiHeader.biClrImportant = 0;
        SetDIBitsToDevice(hdc,
                          0,
                          0,
                          video->window_->width,
                          video->window_->height,
                          0,
                          0,
                          0,
                          video->window_->height,
                          pixel_bits,
                          &bmp_info,
                          DIB_RGB_COLORS);
      }
      EndPaint(hwnd, &ps);
      break;
    }
    case WM_MOUSEMOVE:
      x = static_cast<uint16_t>(LOWORD(lparam));
      y = static_cast<uint16_t>(HIWORD(lparam));
      nacl_event.type = NACL_EVENT_MOUSE_MOTION;
      nacl_event.motion.which = 0;
      nacl_event.motion.state = video->GetButton();
      nacl_event.motion.x = x;
      nacl_event.motion.y = y;
      video->GetRelativeMotion(x, y,
          &nacl_event.motion.xrel, &nacl_event.motion.yrel);
      video->SetMotion(x, y, 1);
      video->EventQueuePut(&nacl_event);
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
      SetFocus(hwnd);
      switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
          button = 1;
          break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
          button = 2;
          break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
          button = 3;
          break;
        default:
          button = 4;
          break;
      }
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_DOWN;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kSet;
      video->GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      video->SetButton(button, 1);
      video->EventQueuePut(&nacl_event);
      break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
      switch (msg) {
        case WM_LBUTTONUP:
          button = 1;
          break;
        case WM_MBUTTONUP:
          button = 2;
          break;
        case WM_RBUTTONUP:
          button = 3;
          break;
        default:
          button = 4;
          break;
      }
      nacl_event.type = NACL_EVENT_MOUSE_BUTTON_UP;
      nacl_event.button.which = 0;
      nacl_event.button.button = button;
      nacl_event.button.state = kClear;
      video->GetMotion(&nacl_event.button.x, &nacl_event.button.y);
      video->SetButton(button, 0);
      video->EventQueuePut(&nacl_event);
      break;
    case WM_KEYUP:
    case WM_KEYDOWN:
      if (wparam == VK_CONTROL) {
        if (lparam & KEY_EXTENDED_BIT) {
          wparam = VK_RCONTROL;
        } else {
          wparam = VK_LCONTROL;
        }
      }
      if (msg == WM_KEYDOWN) {
        if (lparam & KEY_REPEAT_BIT) return 0;  // disable repeat
        nacl_event.type = NACL_EVENT_KEY_DOWN;
        nacl_event.key.state = kSet;
      } else {
        nacl_event.type = NACL_EVENT_KEY_UP;
        nacl_event.key.state = kClear;
      }
      nacl_event.key.which = 0;
      nacl_event.key.keysym.scancode = HIWORD(lparam) & 0x1FF;
      nsym = WinKeyToNacl(wparam);
      nacl_event.key.keysym.sym = nsym;
      nacl_event.key.keysym.mod = video->event_state_key_mod_;
      nacl_event.key.keysym.unicode = 0;
      video->EventQueuePut(&nacl_event);
      break;

    default:
      break;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

void VideoMap::RedrawAsync(void *platform_parm) {
  dprintf(("VideoMap::RedrawAsync()\n"));
  HWND hwnd = static_cast<HWND>(window_->window);
  HDC hdc = GetDC(hwnd);
  VideoMap *video = reinterpret_cast<VideoMap*>(
      GetWindowLong(hwnd, GWL_USERDATA));
  if ((NULL == video) || (NULL == video->untrusted_video_share_)) {
    return;
  }

  uint32_t* pixel_bits = reinterpret_cast<uint32_t*>
      (&video->untrusted_video_share_->video_pixels[0]);
  BITMAPINFO bmp_info;
  bmp_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmp_info.bmiHeader.biWidth = video->window_->width;
  bmp_info.bmiHeader.biHeight =
      -static_cast<LONG>(video->window_->height);  // top-down
  bmp_info.bmiHeader.biPlanes = 1;
  bmp_info.bmiHeader.biBitCount = 32;
  bmp_info.bmiHeader.biCompression = BI_RGB;
  bmp_info.bmiHeader.biSizeImage = 0;
  bmp_info.bmiHeader.biXPelsPerMeter = 0;
  bmp_info.bmiHeader.biYPelsPerMeter = 0;
  bmp_info.bmiHeader.biClrUsed = 0;
  bmp_info.bmiHeader.biClrImportant = 0;
  SetDIBitsToDevice(hdc,
                    0,
                    0,
                    video->window_->width,
                    video->window_->height,
                    0,
                    0,
                    0,
                    video->window_->height,
                    pixel_bits,
                    &bmp_info,
                    DIB_RGB_COLORS);
  ReleaseDC(hwnd, hdc);
}

void VideoMap::Redraw() {
  this->RedrawAsync(NULL);
}
#endif  // NACL_WINDOWS

// assumes event is already normalized to NaClMultimediaEvent
int VideoMap::EventQueuePut(union NaClMultimediaEvent *event) {
  int windex;
  if (EventQueueIsFull()) {
    return -1;
  }
  // apply mask to untrusted event_write_index
  windex = (untrusted_video_share_->u.h.event_write_index &
    (NACL_EVENT_RING_BUFFER_SIZE - 1));
  untrusted_video_share_->u.h.event_queue[windex] = *event;
  windex = (windex + 1) & (NACL_EVENT_RING_BUFFER_SIZE - 1);
  untrusted_video_share_->u.h.event_write_index = windex;
  return 0;
}

// tests event queue to see if it is full
int VideoMap::EventQueueIsFull() {
  int windex;
  windex = untrusted_video_share_->u.h.event_write_index;
  windex = (windex + 1) & (NACL_EVENT_RING_BUFFER_SIZE - 1);
  return (windex == untrusted_video_share_->u.h.event_read_index);
}

// Gets the last recorded mouse motion (position)
// note: outputs normalized for NaCl events
void VideoMap::GetMotion(uint16_t *x, uint16_t *y) {
  *x = event_state_motion_last_x_;
  *y = event_state_motion_last_y_;
}

// Sets the mouse motion (position)
// note: the inputs must be normalized for NaCl events
void VideoMap::SetMotion(uint16_t x, uint16_t y, int last_valid) {
  event_state_motion_last_x_ = x;
  event_state_motion_last_y_ = y;
  event_state_motion_last_valid_ = last_valid;
}

// Gets the relative mouse motion and updates position
// note: the inputs must be normalized for NaCl events
void VideoMap::GetRelativeMotion(uint16_t x, uint16_t y,
    int16_t *rel_x, int16_t *rel_y) {
  if (event_state_motion_last_valid_) {
    *rel_x = x - event_state_motion_last_x_;
    *rel_y = y - event_state_motion_last_y_;
    event_state_motion_last_x_ = x;
    event_state_motion_last_y_ = y;
    event_state_motion_last_valid_ = 1;
  } else {
    *rel_x = 0;
    *rel_y = 0;
  }
}

// Gets the current mouse button state
// note: output is normalized for NaCl events
int VideoMap::GetButton() {
  return event_state_button_;
}

// Modifies the mouse button state
// note: input must be normalized for NaCl events
void VideoMap::SetButton(int button, int state) {
  if (kClear == state) {
    event_state_button_ &= ~(1 << button);
  } else {
    event_state_button_ |= (1 << button);
  }
}

// Modifies the keyboard modifier state (shift, ctrl, alt, etc)
// note: input must be normalized for NaCl events
void VideoMap::SetKeyMod(int nsym, int state) {
  int mask = 0;
  switch (nsym) {
    // fill in with actual key modifiers
    case NACL_KEY_LCTRL: mask = NACL_KEYMOD_LCTRL; break;
    case NACL_KEY_RCTRL: mask = NACL_KEYMOD_RCTRL; break;
    case NACL_KEY_LSHIFT: mask = NACL_KEYMOD_LSHIFT; break;
    case NACL_KEY_RSHIFT: mask = NACL_KEYMOD_RSHIFT; break;
    case NACL_KEY_LALT: mask = NACL_KEYMOD_LALT; break;
    case NACL_KEY_RALT: mask = NACL_KEYMOD_RALT; break;
    default: mask = NACL_KEYMOD_NONE; break;
  }
  if (kClear == state) {
    event_state_key_mod_ &= ~(mask);
  } else {
    event_state_key_mod_ |= (mask);
  }
}

// Gets the current keyboard modifier state
// note: output is normalized for NaCl Events
int VideoMap::GetKeyMod() {
  return event_state_key_mod_;
}

// handle video map specific NPAPI SetWindow
bool VideoMap::SetWindow(PluginWindow *window) {
  dprintf(("VideoMap::SetWindow(%p)\n", static_cast<void *>(window)));

  // first check window->width & window->height...
  if ((NULL == window_) && (window->window) &&
      (window->width > 0) && (window->height > 0)) {
    int width, height;
    // then check width & height properties... (needed for Safari)
    nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* scriptable_plugin =
        plugin_interface_->plugin();
    nacl_srpc::Plugin* plugin =
        static_cast<nacl_srpc::Plugin*>(scriptable_plugin->get_handle());
    width = plugin->width();
    height = plugin->height();
    if ((width > 0) && (height > 0)) {
      dprintf(("VideoMap::SetWindow calling Initialize(%p)\n",
               static_cast<void *>(window)));
      // don't allow invalid window sizes
      if ((width < kNaClVideoMinWindowSize) ||
          (width > kNaClVideoMaxWindowSize) ||
          (height < kNaClVideoMinWindowSize) ||
          (height > kNaClVideoMaxWindowSize)) {
        dprintf(("VideoMap::SetWindow got invalid window size (%d, %d)\n",
                width, height));
        return false;
      }

      if (-1 == InitializeSharedMemory(window)) {
        return false;
      }

#if NACL_WINDOWS
      dprintf(("VideoMap::SetWindow adding Windows event listener\n"));
      HWND hwnd = static_cast<HWND>(window->window);
      original_window_procedure_ = SubclassWindow(hwnd,
          reinterpret_cast<WNDPROC>(VideoMap::WindowProcedure));
      SetWindowLong(hwnd, GWL_USERDATA, reinterpret_cast<LONG>(this));
#endif  // NACL_WINDOWS
#if NACL_LINUX && defined(MOZ_X11)
      // open X11 display, add X11 event listener
      dprintf(("VideoMap::SetWindow adding X11 display\n"));
      set_platform_specific(XOpenDisplay(NULL));
      dprintf(("VideoMap::SetWindow adding X11 event listener\n"));
      Window xwin = reinterpret_cast<Window>(window->window);
      NPSetWindowCallbackStruct* npsw =
          reinterpret_cast<NPSetWindowCallbackStruct*>(window->ws_info);
      Widget widget = XtWindowToWidget(npsw->display, xwin);
      if (widget) {
        long event_mask = StructureNotifyMask | KeyPressMask |
               KeyReleaseMask | ExposureMask | PointerMotionMask |
               ButtonPressMask | ButtonReleaseMask |
               LeaveWindowMask | EnterWindowMask;
        XSelectInput(npsw->display, xwin, event_mask);
        XtAddEventHandler(widget, event_mask, False,
            reinterpret_cast<XtEventHandler>(&VideoMap::XEventHandler), this);
      }
#endif  // NACL_LINUX && defined(MOZ_X11)
    }
  }
  return true;
}

#if !NACL_OSX
int16_t VideoMap::HandleEvent(void *param) {
  dprintf(("VideoMap::HandleEvent(%p, %p)\n",
           static_cast<void *>(this),
           static_cast<void *>(param)));
  return 0;
}
#endif  // !NACL_OSX

nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>*
    VideoMap::VideoSharedMemorySetup() {
  dprintf(("VideoMap::VideoSharedMemorySetup(%p, %p, %p)\n",
           static_cast<void *>(this),
           static_cast<void *>(video_shared_memory_),
           static_cast<void *>(video_handle_)));
  if (NULL == video_shared_memory_) {
    if (kInvalidHtpHandle == video_handle_) {
      // The plugin has not set up a shared memory object for the display.
      // This indicates either the plugin was set to be invisible, or had
      // an error when SetWindow was invoked.  In either case, it is an
      // error to get the shared memory object.
      dprintf(("VideoMap::VideoSharedMemorySetup returning NULL\n"));
      return NULL;
    } else {
      // Create a SharedMemory object that the script can get at.
      // SRPC_Plugin initially takes exclusive ownership
      dprintf(("VideoMap::VideoSharedMemorySetup plugin_interface_ (%p)\n",
               static_cast<void *>(plugin_interface_)));
      dprintf(("VideoMap::VideoSharedMemorySetup plugin (%p)\n",
               static_cast<void *>(plugin_interface_->plugin())));
      nacl_srpc::SharedMemoryInitializer init_info(
        static_cast<PortablePluginInterface*>(plugin_interface_),
        video_handle_,
        static_cast<nacl_srpc::Plugin*>(
            plugin_interface_->plugin()->get_handle()));

      video_shared_memory_ =
          nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>::
        New(static_cast<nacl_srpc::PortableHandleInitializer*>(&init_info));
    }
  }
  dprintf(("VideoMap::VideoSharedMemorySetup returns %p\n",
           static_cast<void *>(video_shared_memory_)));
  // Anyone requesting video_shared_memory_ begins sharing ownership.
  video_shared_memory_->AddRef();
  return video_shared_memory_;
}

// Note: Graphics functionality similar to npapi_bridge.
// TODO(sehr): Make only one copy of the graphics code.
void VideoMap::Invalidate() {
  if (window_ && window_->window) {
#if NACL_WINDOWS
    HWND hwnd = static_cast<HWND>(window_->window);
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = window_->width;
    rect.bottom = window_->height;
    ::InvalidateRect(hwnd, &rect, FALSE);
#endif
#if NACL_OSX
    NPRect rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = window_->width;
    rect.bottom = window_->height;
    ::NPN_InvalidateRect(plugin_interface_->GetPluginIdentifier(),
                         const_cast<NPRect*>(&rect));
#endif
  }
}

VideoCallbackData* VideoMap::InitCallbackData(NaClHandle h,
                                            PortablePluginInterface *p,
                                            nacl_srpc::MultimediaSocket *msp) {
  // initialize with refcount set to 2
  video_callback_data_ = new(std::nothrow) VideoCallbackData(h, p, 2, msp);
  return video_callback_data_;
}

// static method
VideoCallbackData* VideoMap::ReleaseCallbackData(VideoCallbackData* vcd) {
  dprintf(("VideoMap::ReleaseCallbackData (%p), refcount was %d\n",
           static_cast<void *>(vcd), vcd->refcount));
  --vcd->refcount;
  if (0 == vcd->refcount) {
    delete vcd;
    return NULL;
  }
  return vcd;
}

// static method
// normally, the Release version is preferred, which reference counts.
// in some cases we may just need to forcefully delete the callback data,
// and ignore the refcount.
void VideoMap::ForceDeleteCallbackData(VideoCallbackData* vcd) {
  dprintf(("VideoMap::ForceDeleteCallbackData (%p)\n",
           static_cast<void *>(vcd)));
  delete vcd;
}

// opens shared memory map into untrusted space
// returns 0 success, -1 failure
int VideoMap::InitializeSharedMemory(PluginWindow *window) {
  int width = window->width;
  int height = window->height;
  const int bytes_per_pixel = 4;
  int image_size = width * height * bytes_per_pixel;
  int vps_size = sizeof(struct NaClVideoShare) + image_size;

  dprintf(("VideoMap::Initialize(%p)  this: %p\n",
           static_cast<void *>(window),
           static_cast<void *>(this)));
  dprintf(("VideoMap::Initialize width, height: %d, %d\n", width, height));
  if (NULL == window_) {
    // verify event struct size
    if (sizeof(union NaClMultimediaEvent) >
        sizeof(struct NaClMultimediaPadEvent)) {
      dprintf(("VideoMap::Initialize event struct size failed\n"));
      return -1;
    }
    // verify header size
    if ((NACL_VIDEO_SHARE_HEADER_SIZE + NACL_VIDEO_SHARE_PIXEL_PAD) !=
        sizeof(struct NaClVideoShare)) {
      dprintf(("VideoMap::Initialize video share size failed\n"));
      return -1;
    }
    dprintf(("VideoMap::Initialize assigning window to this(%p)\n",
             static_cast<void *>(this)));
    // map video & event shared memory structure between trusted & untrusted
    window_ = window;
    vps_size = NaClRoundAllocPage(vps_size);
    video_size_ = vps_size;
    video_handle_ = CreateShmDesc(CreateMemoryObject(video_size_),
        video_size_);
    if (kInvalidHtpHandle == video_handle_) {
      video_size_ = 0;
      return -1;
    }
    dprintf(("VideoMap::Initialize about to Map...\n"));
    untrusted_video_share_ = static_cast<NaClVideoShare*>(
      Map(NULL,
          video_size_,
          kProtRead | kProtWrite,
          kMapShared,
          video_handle_,
          0));
    if (kMapFailed == untrusted_video_share_) {
      untrusted_video_share_ = NULL;
      nacl::Close(video_handle_);
      untrusted_video_share_ = NULL;
      video_size_ = 0;
      window_ = NULL;
      return -1;
    }

    // clear entire untrusted memory window
    memset(untrusted_video_share_, 0, video_size_);

    // pass some information to untrusted code via the shared memory window
    // note: everything untrusted_video_share_ points to is untrusted, and could
    // be maliciously modified at any time.
    untrusted_video_share_->u.h.map_size = video_size_;
    untrusted_video_share_->u.h.video_width = width;
    untrusted_video_share_->u.h.video_height = height;
    untrusted_video_share_->u.h.video_size = image_size;

    // explicitly init states
    event_state_button_ = kClear;
    event_state_key_mod_ = kClear;
    event_state_motion_last_x_ = 0;
    event_state_motion_last_y_ = 0;
    event_state_motion_last_valid_ = 0;
  }
  return 0;
}

// this is the draw method the upcall thread should invoke
void VideoMap::RequestRedraw() {
  if (!IsEnabled()) {
    return;
  }
  // TODO(nfullagar): based on video_update_ member variable,
  // either render from the paint/expose event, or asynchronously
  // (at the moment, MacOSX always renders from paint event, but the
  // other platforms always render asynchronously)
#if NACL_OSX
  request_redraw_ = true;
#else
  RedrawAsync(platform_specific());
#endif
}

VideoMap::VideoMap(PortablePluginInterface *plugin_interface)
  : event_state_button_(kClear),
    event_state_key_mod_(kClear),
    event_state_motion_last_x_(0),
    event_state_motion_last_y_(0),
    event_state_motion_last_valid_(0),
    platform_specific_(NULL),
    request_redraw_(false),
    untrusted_video_share_(NULL),
    video_handle_(kInvalidHtpHandle),
    video_size_(0),
    video_enabled_(false),
    video_callback_data_(NULL),
    video_shared_memory_(NULL),
    video_update_mode_(0) {
  plugin_interface_ = plugin_interface;
  window_ = NULL;
  // retrieve update property from plugin
  if (NULL != plugin_interface_) {
    nacl_srpc::ScriptableHandle<nacl_srpc::Plugin> *plugin =
        plugin_interface_->plugin();
    if (NULL != plugin) {
      NPVariant variant;
      nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>::GetProperty(plugin,
          (NPIdentifier)PortablePluginInterface::kVideoUpdateModeIdent,
          &variant);
      if (!nacl_srpc::NPVariantToScalar(&variant, &video_update_mode_)) {
        // BUG: unhandled type error in the constructor.
        // TODO(nfullagar,sehr): Break the constructor to handle errors.
      }
    }
  }
}

VideoMap::~VideoMap() {
  Disable();
  dprintf(("VideoMap::~VideoMap() releasing video_callback_data_ (%p)\n",
           static_cast<void *>(video_callback_data_)));
  if (NULL != video_callback_data_) {
    video_callback_data_->portable_plugin = NULL;
    video_callback_data_ = ReleaseCallbackData(video_callback_data_);
  }
  if (NULL != platform_specific()) {
#if NACL_LINUX && defined(MOZ_X11)
    XCloseDisplay(static_cast<Display *>(platform_specific()));
#endif
    set_platform_specific(NULL);
  }
#if NACL_WINDOWS
  if (window_ && window_->window) {
    SubclassWindow(reinterpret_cast<HWND>(window_->window),
        original_window_procedure_);
  }
#endif  // NACL_WINDOWS
}

}  // namespace nacl
