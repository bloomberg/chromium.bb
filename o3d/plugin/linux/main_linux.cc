/*
 * Copyright 2009, Google Inc.
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


// This file implements the platform specific parts of the plugin for
// the Linux platform.

#include "plugin/cross/main.h"

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "base/logging.h"
#include "base/third_party/nspr/prtypes.h"
#include "core/cross/event.h"
#include "core/linux/display_window_linux.h"
#include "plugin/linux/envvars.h"
#if !defined(O3D_INTERNAL_PLUGIN)
#include "breakpad/linux/breakpad.h"
#endif

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::DisplayWindowLinux;
using o3d::Event;

namespace {

bool g_xembed_support = false;

#if !defined(O3D_INTERNAL_PLUGIN)
o3d::Breakpad g_breakpad;
#endif

#ifdef O3D_PLUGIN_ENV_VARS_FILE
static const char kEnvVarsFilePath[] = O3D_PLUGIN_ENV_VARS_FILE;
#endif

static void DrawPlugin(PluginObject *obj) {
  // Limit drawing to no more than once every timer tick.
  if (!obj->draw_) return;
  // Don't allow re-entrant rendering (can happen in Chrome)
  if (obj->client()->IsRendering()) return;
  obj->client()->RenderClient(true);
  obj->draw_ = false;
}

// Xt support functions

void LinuxTimer(XtPointer data, XtIntervalId* id) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(data);
  DCHECK(obj->xt_interval_ == *id);
  obj->client()->Tick();
  obj->draw_ = true;
  if (obj->renderer()) {
    if (obj->client()->NeedsRender()) {
      // NOTE: this draws no matter what instead of just invalidating the
      // region, which means it will execute even if the plug-in window is
      // invisible.
      DrawPlugin(obj);
    }
  }
  obj->xt_interval_ =
      XtAppAddTimeOut(obj->xt_app_context_, 8, LinuxTimer, obj);
}

void LinuxExposeHandler(Widget w,
                        XtPointer user_data,
                        XEvent *event,
                        Boolean *cont) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  if (event->type != Expose) return;
  DrawPlugin(obj);
}

static int KeySymToDOMKeyCode(KeySym key_sym) {
  // See https://developer.mozilla.org/en/DOM/Event/UIEvent/KeyEvent for the
  // DOM values.
  // X keycodes are not useful, because they describe the geometry, not the
  // associated symbol, so a 'Q' on a QWERTY (US) keyboard has the same keycode
  // as a 'A' on an AZERTY (french) one.
  // Key symbols are closer to what the DOM expects, but they depend on the
  // shift/control/alt combination - the same key has several symbols ('a' vs
  // 'A', '1' vs '!', etc.), so we do extra work so that 'a' and 'A' both
  // generate the same DOM keycode.
  if ((key_sym >= XK_0 && key_sym <= XK_Z)) {
    // DOM keycode matches ASCII value, as does the keysym.
    return key_sym;
  } else if ((key_sym >= XK_a && key_sym <= XK_z)) {
    return key_sym - XK_a + XK_A;
  } else if ((key_sym >= XK_KP_0 && key_sym <= XK_KP_9)) {
    return 0x60 + key_sym - XK_KP_0;
  } else if ((key_sym >= XK_F1 && key_sym <= XK_F24)) {
    return 0x70 + key_sym - XK_F1;
  }
  switch (key_sym) {
    case XK_Cancel:
      return 0x03;
    case XK_Help:
      return 0x06;
    case XK_BackSpace:
      return 0x08;
    case XK_Tab:
      return 0x09;
    case XK_Clear:
      return 0x0C;
    case XK_Return:
      return 0x0D;
    case XK_KP_Enter:
      return 0x0E;
    case XK_Shift_L:
    case XK_Shift_R:
      return 0x10;
    case XK_Control_L:
    case XK_Control_R:
      return 0x11;
    case XK_Alt_L:
    case XK_Alt_R:
      return 0x12;
    case XK_Pause:
      return 0x13;
    case XK_Caps_Lock:
      return 0x14;
    case XK_Escape:
      return 0x1B;
    case XK_space:
      return 0x20;
    case XK_Page_Up:
    case XK_KP_Page_Up:
      return 0x21;
    case XK_Page_Down:
    case XK_KP_Page_Down:
      return 0x22;
    case XK_End:
    case XK_KP_End:
      return 0x23;
    case XK_Home:
    case XK_KP_Home:
      return 0x24;
    case XK_Left:
    case XK_KP_Left:
      return 0x25;
    case XK_Up:
    case XK_KP_Up:
      return 0x26;
    case XK_Right:
    case XK_KP_Right:
      return 0x27;
    case XK_Down:
    case XK_KP_Down:
      return 0x28;
    case XK_Print:
      return 0x2C;
    case XK_Insert:
    case XK_KP_Insert:
      return 0x2D;
    case XK_Delete:
    case XK_KP_Delete:
      return 0x2E;
    case XK_Menu:
      return 0x5D;
    case XK_asterisk:
    case XK_KP_Multiply:
      return 0x6A;
    case XK_plus:
    case XK_KP_Add:
      return 0x6B;
    case XK_underscore:
      return 0x6C;
    case XK_minus:
    case XK_KP_Subtract:
      return 0x6E;
    case XK_KP_Decimal:
      return 0x6E;
    case XK_KP_Divide:
      return 0x6F;
    case XK_Num_Lock:
      return 0x90;
    case XK_Scroll_Lock:
      return 0x91;
    case XK_comma:
      return 0xBC;
    case XK_period:
      return 0xBE;
    case XK_slash:
      return 0xBF;
    case XK_grave:
      return 0xC0;
    case XK_bracketleft:
      return 0xDB;
    case XK_backslash:
      return 0xDC;
    case XK_bracketright:
      return 0xDD;
    case XK_apostrophe:
      return 0xDE;
    case XK_Meta_L:
    case XK_Meta_R:
      return 0xE0;
    default:
      return 0;
  }
}

static int GetXModifierState(int x_state) {
  int modifier_state = 0;
  if (x_state & ControlMask) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (x_state & ShiftMask) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (x_state & Mod1Mask) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  if (x_state & Mod2Mask) {
    modifier_state |= Event::MODIFIER_META;
  }
  return modifier_state;
}

void LinuxKeyHandler(Widget w,
                     XtPointer user_data,
                     XEvent *xevent,
                     Boolean *cont) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  XKeyEvent *key_event = &xevent->xkey;
  Event::Type type;
  switch (xevent->type) {
    case KeyPress:
      type = Event::TYPE_KEYDOWN;
      break;
    case KeyRelease:
      type = Event::TYPE_KEYUP;
      break;
    default:
      return;
  }
  Event event(type);
  char char_code = 0;
  KeySym key_sym;
  int result = XLookupString(key_event, &char_code, sizeof(char_code),
                             &key_sym, NULL);
  event.set_key_code(KeySymToDOMKeyCode(key_sym));
  int modifier_state = GetXModifierState(key_event->state);
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  if (xevent->type == KeyPress && result > 0) {
    event.clear_key_code();
    event.set_char_code(char_code);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
}

// TODO: Any way to query the system for the correct value ? According to
// http://library.gnome.org/devel/gdk/stable/gdk-Event-Structures.html GTK uses
// 250ms.
const unsigned int kDoubleClickTime = 250;  // in ms

void LinuxMouseButtonHandler(Widget w,
                             XtPointer user_data,
                             XEvent *xevent,
                             Boolean *cont) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  XButtonEvent *button_event = &xevent->xbutton;
  Event::Type type;
  switch (xevent->type) {
    case ButtonPress:
      type = Event::TYPE_MOUSEDOWN;
      break;
    case ButtonRelease:
      type = Event::TYPE_MOUSEUP;
      break;
    default:
      return;
  }
  Event event(type);
  switch (button_event->button) {
    case 1:
      event.set_button(Event::BUTTON_LEFT);
      break;
    case 2:
      event.set_button(Event::BUTTON_MIDDLE);
      break;
    case 3:
      event.set_button(Event::BUTTON_RIGHT);
      break;
    case 4:
    case 5:
      // Mouse wheel. 4 is up, 5 is down. Reported by X as Press/Release.
      // Ignore the Press, report the Release as the wheel event.
      if (xevent->type == ButtonPress)
        return;
      event.set_type(Event::TYPE_WHEEL);
      event.set_delta(0, (button_event->button == 4) ? 1 : -1);
      break;
    default:
      return;
  }
  int modifier_state = GetXModifierState(button_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(button_event->x, button_event->y,
                     button_event->x_root, button_event->y_root,
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  if (event.type() == Event::TYPE_MOUSEUP && obj->in_plugin()) {
    // The event manager automatically generates CLICK from MOUSEDOWN, MOUSEUP.
    if (button_event->time < obj->last_click_time() + kDoubleClickTime) {
      obj->set_last_click_time(0);
      event.set_type(Event::TYPE_DBLCLICK);
      obj->client()->AddEventToQueue(event);
    } else {
      obj->set_last_click_time(button_event->time);
    }
  }
}

void LinuxMouseMoveHandler(Widget w,
                           XtPointer user_data,
                           XEvent *xevent,
                           Boolean *cont) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  if (xevent->type != MotionNotify)
    return;
  XMotionEvent *motion_event = &xevent->xmotion;
  Event event(Event::TYPE_MOUSEMOVE);
  int modifier_state = GetXModifierState(motion_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(motion_event->x, motion_event->y,
                     motion_event->x_root, motion_event->y_root,
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
}

void LinuxEnterLeaveHandler(Widget w,
                            XtPointer user_data,
                            XEvent *xevent,
                            Boolean *cont) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  switch (xevent->type) {
    case EnterNotify:
      obj->set_in_plugin(true);
      break;
    case LeaveNotify:
      obj->set_in_plugin(false);
      break;
    default:
      return;
  }
}

// XEmbed / GTK support functions
static int GetGtkModifierState(int gtk_state) {
  int modifier_state = 0;
  if (gtk_state & GDK_CONTROL_MASK) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (gtk_state & GDK_SHIFT_MASK) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (gtk_state & GDK_MOD1_MASK) {
    modifier_state |= Event::MODIFIER_ALT;
  }
#if 0
  // TODO: This code is temporarily disabled until we figure out which exact
  // version of GTK to test for: GDK_META_MASK doesn't exist in older (e.g. 2.8)
  // versions.
  if (gtk_state & GDK_META_MASK) {
    modifier_state |= Event::MODIFIER_META;
  }
#endif
  return modifier_state;
}

static gboolean GtkHandleMouseMove(GtkWidget *widget,
                                   GdkEventMotion *motion_event,
                                   PluginObject *obj) {
  Event event(Event::TYPE_MOUSEMOVE);
  int modifier_state = GetGtkModifierState(motion_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(static_cast<int>(motion_event->x),
                     static_cast<int>(motion_event->y),
                     static_cast<int>(motion_event->x_root),
                     static_cast<int>(motion_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  return TRUE;
}

static gboolean GtkHandleMouseButton(GtkWidget *widget,
                                     GdkEventButton *button_event,
                                     PluginObject *obj) {
  // On a double-click, Gtk produces: BUTTON_PRESS, BUTTON_RELEASE,
  // BUTTON_PRESS, 2BUTTON_PRESS, BUTTON_RELEASE.
  // JavaScript should receive: down, up, [optional move, ] click, down,
  // up, click, dblclick.
  // The EventManager turns (down, up) into click, since we need that on all
  // platforms.
  // So when a 2BUTTON_PRESS occurs, we keep track of this, so that we can
  // issue a corresponding dblclick when BUTTON_RELEASE comes.
  Event::Button button;
  switch (button_event->button) {
    case 1:
      button = Event::BUTTON_LEFT;
      break;
    case 2:
      button = Event::BUTTON_MIDDLE;
      break;
    case 3:
      button = Event::BUTTON_RIGHT;
      break;
    default:
      return FALSE;
  }
  Event::Type type;
  switch (button_event->type) {
    case GDK_BUTTON_PRESS:
      type = Event::TYPE_MOUSEDOWN;
      break;
    case GDK_BUTTON_RELEASE:
      type = Event::TYPE_MOUSEUP;
      break;
    case GDK_2BUTTON_PRESS:
      obj->got_double_click_[button_event->button - 1] = true;
      return TRUE;
    default:
      return FALSE;
  }
  Event event(type);
  int modifier_state = GetGtkModifierState(button_event->state);
  event.set_modifier_state(modifier_state);
  event.set_button(button);
  event.set_position(static_cast<int>(button_event->x),
                     static_cast<int>(button_event->y),
                     static_cast<int>(button_event->x_root),
                     static_cast<int>(button_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  if (event.type() == Event::TYPE_MOUSEUP && obj->in_plugin() &&
      obj->got_double_click_[button_event->button - 1]) {
    obj->got_double_click_[button_event->button - 1] = false;
    event.set_type(Event::TYPE_DBLCLICK);
    obj->client()->AddEventToQueue(event);
  }
  if (event.in_plugin() && event.type() == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(event.x(), event.y())) {
    obj->RequestFullscreenDisplay(button_event->time);
  }
  return TRUE;
}

static gboolean GtkHandleKey(GtkWidget *widget,
                             GdkEventKey *key_event,
                             PluginObject *obj) {
  Event::Type type;
  switch (key_event->type) {
    case GDK_KEY_PRESS:
      type = Event::TYPE_KEYDOWN;
      break;
    case GDK_KEY_RELEASE:
      type = Event::TYPE_KEYUP;
      break;
    default:
      return FALSE;
  }
  Event event(type);
  // Logically, GTK events and X events use a different namespace for the
  // various values, but in practice, all the keys we use have the same values,
  // because one of the paths in GTK uses straight X to do the translation. So
  // we can use the same function here.
  int key_code = KeySymToDOMKeyCode(key_event->keyval);
  event.set_key_code(key_code);
  int modifier_state = GetGtkModifierState(key_event->state);
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  int char_code = gdk_keyval_to_unicode(key_event->keyval);
  if (key_event->type == GDK_KEY_PRESS && char_code != 0) {
    event.clear_key_code();
    event.set_char_code(char_code);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
  // No need to check for Alt+F4 because Gtk (or the window manager?) handles
  // that and delivers a destroy event to us already.
  if (event.type() == Event::TYPE_KEYDOWN &&
      event.key_code() == 0x1B) {  // escape
    obj->CancelFullscreenDisplay();
  }
  return TRUE;
}

static gboolean GtkHandleScroll(GtkWidget *widget,
                                GdkEventScroll *scroll_event,
                                PluginObject *obj) {
  Event event(Event::TYPE_WHEEL);
  switch (scroll_event->direction) {
    case GDK_SCROLL_UP:
      event.set_delta(0, 1);
      break;
    case GDK_SCROLL_DOWN:
      event.set_delta(0, -1);
      break;
    case GDK_SCROLL_LEFT:
      event.set_delta(-1, 0);
      break;
    case GDK_SCROLL_RIGHT:
      event.set_delta(1, 0);
      break;
    default:
      return FALSE;
  }
  int modifier_state = GetGtkModifierState(scroll_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(static_cast<int>(scroll_event->x),
                     static_cast<int>(scroll_event->y),
                     static_cast<int>(scroll_event->x_root),
                     static_cast<int>(scroll_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  return TRUE;
}

static gboolean GtkEventCallback(GtkWidget *widget,
                                 GdkEvent *event,
                                 gpointer user_data) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  DLOG_ASSERT(widget == obj->gtk_event_source_);
  switch (event->type) {
    case GDK_EXPOSE:
      if (GTK_WIDGET_DRAWABLE(widget)) {
        obj->draw_ = true;
        DrawPlugin(obj);
      }
      return TRUE;
    case GDK_ENTER_NOTIFY:
      obj->set_in_plugin(true);
      return TRUE;
    case GDK_LEAVE_NOTIFY:
      obj->set_in_plugin(false);
      return TRUE;
    case GDK_MOTION_NOTIFY:
      return GtkHandleMouseMove(widget, &event->motion, obj);
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return GtkHandleMouseButton(widget, &event->button, obj);
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return GtkHandleKey(widget, &event->key, obj);
    case GDK_SCROLL:
      return GtkHandleScroll(widget, &event->scroll, obj);
    default:
      return FALSE;
  }
}

static gboolean GtkConfigureEventCallback(GtkWidget *widget,
                                          GdkEventConfigure *configure_event,
                                          gpointer user_data) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  return obj->OnGtkConfigure(widget, configure_event);
}

static gboolean GtkDeleteEventCallback(GtkWidget *widget,
                                        GdkEvent *event,
                                        gpointer user_data) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  return obj->OnGtkDelete(widget, event);
}

static gboolean GtkTimeoutCallback(gpointer user_data) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  obj->draw_ = true;
  obj->client()->Tick();
  if (obj->renderer()) {
    if (obj->client()->NeedsRender()) {
      GtkWidget *widget;
      if (obj->fullscreen()) {
        widget = obj->gtk_fullscreen_container_;
      } else {
        widget = obj->gtk_container_;
      }
      gtk_widget_queue_draw(widget);
    }
  }
  return TRUE;
}

}  // end anonymous namespace

namespace o3d {

NPError PlatformPreNPInitialize() {
#if !defined(O3D_INTERNAL_PLUGIN)
  // Setup breakpad
  g_breakpad.Initialize();
  g_breakpad.set_version(O3D_PLUGIN_VERSION);
#endif
  return NPERR_NO_ERROR;
}

NPError PlatformPostNPInitialize() {
#ifdef O3D_PLUGIN_ENV_VARS_FILE
  // Before doing anything more, we first load our environment variables file.
  // This file is a newline-delimited list of any system-specific environment
  // variables that need to be set in the browser. Since we are a shared library
  // and not an executable, we can't set them at browser start time, so we
  // instead set them in every process that loads our shared library. It is
  // important that we do this as early as possible so that any relevant
  // variables are already set when we initialize our shared library
  // dependencies.
  o3d::LoadEnvironmentVariablesFile(kEnvVarsFilePath);
#endif

  // Check for XEmbed support in the browser.
  // Tragically, nspluginwrapper thinks that the type of boolean variables is
  // supposed to be PRBool, which has size of 4, and not NPBool, which has size
  // of 1, so we have to use PRBool here so that an integer-sized write by the
  // plugin host will succeed and not clobber anything else on our stack.
  PRBool xembed_support = PR_FALSE;
  NPError err = NPN_GetValue(NULL, NPNVSupportsXEmbedBool, &xembed_support);
  if (err != NPERR_NO_ERROR)
    xembed_support = PR_FALSE;

  if (xembed_support) {
    // Check for Gtk2 toolkit support in the browser.
    NPNToolkitType toolkit = static_cast<NPNToolkitType>(0);
    err = NPN_GetValue(NULL, NPNVToolkit, &toolkit);
    if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2)
      xembed_support = PR_FALSE;
  }
  g_xembed_support = xembed_support != PR_FALSE;

  return NPERR_NO_ERROR;
}

NPError PlatformPreNPShutdown() {
  return NPERR_NO_ERROR;
}

NPError PlatformPostNPShutdown() {
#if !defined(O3D_INTERNAL_PLUGIN)
  g_breakpad.Shutdown();
#endif

  return NPERR_NO_ERROR;
}

NPError PlatformNPPGetValue(PluginObject *obj,
                            NPPVariable variable,
                            void *value) {
  switch (variable) {
    case NPPVpluginNeedsXEmbed:
      *static_cast<NPBool *>(value) = g_xembed_support;
      return NPERR_NO_ERROR;
    default:
      return NPERR_INVALID_PARAM;
  }
  return NPERR_NO_ERROR;
}

NPError PlatformNPPNew(NPP instance, PluginObject *obj) {
  return NPERR_NO_ERROR;
}

NPError PlatformNPPDestroy(NPP instance, PluginObject *obj) {
  // TODO(tschmelcher): Do we really have to do this before the other teardown
  // below? If not then we can factor out the platform-specific TearDown()
  // calls into NPP_Destroy() in main.cc.
  obj->TearDown();

  if (obj->xt_widget_) {
    // NOTE: This crashes. Not sure why, possibly the widget has
    // already been destroyed, but we haven't received a SetWindow(NULL).
    // XtRemoveEventHandler(obj->xt_widget_, ExposureMask, False,
    //                     LinuxExposeHandler, obj);
    obj->xt_widget_ = NULL;
  }
  if (obj->xt_interval_) {
    XtRemoveTimeOut(obj->xt_interval_);
    obj->xt_interval_ = 0;
  }
  if (obj->timeout_id_) {
    g_source_remove(obj->timeout_id_);
    obj->timeout_id_ = 0;
  }
  if (obj->gtk_container_) {
    gtk_widget_destroy(obj->gtk_container_);
    obj->gtk_container_ = NULL;
  }
  if (obj->gtk_fullscreen_container_) {
    gtk_widget_destroy(obj->gtk_fullscreen_container_);
    obj->gtk_fullscreen_container_ = NULL;
  }
  if (obj->gdk_display_) {
    gdk_display_close(obj->gdk_display_);
    obj->gdk_display_ = NULL;
  }
  obj->gtk_event_source_ = NULL;
  obj->event_handler_id_ = 0;
  obj->window_ = 0;
  obj->drawable_ = 0;
  return NPERR_NO_ERROR;
}

NPError PlatformNPPSetWindow(NPP instance,
                             PluginObject *obj,
                             NPWindow *window) {
  NPSetWindowCallbackStruct *cb_struct =
      static_cast<NPSetWindowCallbackStruct *>(window->ws_info);
  Window xwindow = reinterpret_cast<Window>(window->window);
  if (xwindow != obj->window_) {
    Display *display = cb_struct->display;
    Window drawable = xwindow;
    if (g_xembed_support) {
      // We asked for a XEmbed plugin, the xwindow is a GtkSocket, we create
      // a GtkPlug to go into it.
      obj->gdk_display_ = gdk_display_open(XDisplayString(display));
      LOG_ASSERT(obj->gdk_display_) << "Unable to open X11 display";
      display = GDK_DISPLAY_XDISPLAY(obj->gdk_display_);
      obj->gtk_container_ =
          gtk_plug_new_for_display(obj->gdk_display_, xwindow);
      // Firefox has a bug where it sometimes destroys our parent widget before
      // calling NPP_Destroy. We handle this by hiding our X window instead of
      // destroying it. Without this, future OpenGL calls can raise a
      // GLXBadDrawable error and kill the browser process.
      g_signal_connect(obj->gtk_container_, "delete-event",
                       G_CALLBACK(gtk_widget_hide_on_delete), NULL);
      gtk_widget_set_double_buffered(obj->gtk_container_, FALSE);
      if (!obj->fullscreen()) {
        obj->SetGtkEventSource(obj->gtk_container_);
      }
      gtk_widget_show(obj->gtk_container_);
      drawable = GDK_WINDOW_XID(obj->gtk_container_->window);
      obj->timeout_id_ = g_timeout_add(8, GtkTimeoutCallback, obj);
    } else {
      // No XEmbed support, the xwindow is a Xt Widget.
      Widget widget = XtWindowToWidget(display, xwindow);
      if (!widget) {
        DLOG(ERROR) << "window is not a Widget";
        return NPERR_MODULE_LOAD_FAILED_ERROR;
      }
      obj->xt_widget_ = widget;
      XtAddEventHandler(widget, ExposureMask, 0, LinuxExposeHandler, obj);
      XtAddEventHandler(widget, KeyPressMask|KeyReleaseMask, 0,
                        LinuxKeyHandler, obj);
      XtAddEventHandler(widget, ButtonPressMask|ButtonReleaseMask, 0,
                        LinuxMouseButtonHandler, obj);
      XtAddEventHandler(widget, PointerMotionMask, 0,
                        LinuxMouseMoveHandler, obj);
      XtAddEventHandler(widget, EnterWindowMask|LeaveWindowMask, 0,
                        LinuxEnterLeaveHandler, obj);
      obj->xt_app_context_ = XtWidgetToApplicationContext(widget);
      obj->xt_interval_ =
          XtAppAddTimeOut(obj->xt_app_context_, 8, LinuxTimer, obj);
    }

    // Create and assign the graphics context.
    o3d::DisplayWindowLinux default_display;
    default_display.set_display(display);
    default_display.set_window(drawable);

    obj->CreateRenderer(default_display);
    obj->client()->Init();
    obj->SetDisplay(display);
    obj->window_ = xwindow;
    obj->drawable_ = drawable;
  }
  obj->Resize(window->width, window->height);

  return NPERR_NO_ERROR;
}

void PlatformNPPStreamAsFile(StreamManager *stream_manager,
                             NPStream *stream,
                             const char *fname) {
  stream_manager->SetStreamFile(stream, fname);
}

int16 PlatformNPPHandleEvent(NPP instance, PluginObject *obj, void *event) {
  return 0;
}

}  // namespace o3d

// TODO(tschmelcher): This stuff does not belong in this file.
namespace glue {
namespace _o3d {

void PluginObject::SetGtkEventSource(GtkWidget *widget) {
  if (gtk_event_source_) {
    g_signal_handler_disconnect(G_OBJECT(gtk_event_source_),
                                event_handler_id_);
  }
  gtk_event_source_ = widget;
  if (gtk_event_source_) {
    gtk_widget_add_events(gtk_event_source_,
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_SCROLL_MASK |
                          GDK_KEY_PRESS_MASK |
                          GDK_KEY_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_EXPOSURE_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK);
    event_handler_id_ = g_signal_connect(gtk_event_source_, "event",
                                         G_CALLBACK(GtkEventCallback), this);
  }
}

gboolean PluginObject::OnGtkConfigure(GtkWidget *widget,
                                      GdkEventConfigure *configure_event) {
  DLOG_ASSERT(widget == gtk_fullscreen_container_);
  if (fullscreen_pending_) {
    // Our fullscreen window has been placed and sized. Switch to it.
    fullscreen_pending_ = false;
    fullscreen_window_ = GDK_WINDOW_XID(gtk_fullscreen_container_->window);
    DisplayWindowLinux display;
    display.set_display(display_);
    display.set_window(fullscreen_window_);
    prev_width_ = renderer()->width();
    prev_height_ = renderer()->height();
    if (!renderer()->GoFullscreen(display, fullscreen_region_mode_id_)) {
      gtk_widget_destroy(gtk_fullscreen_container_);
      gtk_fullscreen_container_ = NULL;
      fullscreen_window_ = 0;
      // The return value is for whether we handled the event, not whether it
      // was successful, so return TRUE event for error.
      return TRUE;
    }
    SetGtkEventSource(gtk_fullscreen_container_);
  }
  renderer()->Resize(configure_event->width, configure_event->height);
  client()->SendResizeEvent(renderer()->width(), renderer()->height(),
                            true);
  fullscreen_ = true;
  // Return false here so that the default handler in GTK will still be invoked.
  return FALSE;
}

gboolean PluginObject::OnGtkDelete(GtkWidget *widget,
                                   GdkEvent *event) {
  DLOG_ASSERT(widget == gtk_fullscreen_container_);
  CancelFullscreenDisplay();
  return TRUE;
}

bool PluginObject::GetDisplayMode(int id, o3d::DisplayMode *mode) {
  return renderer()->GetDisplayMode(id, mode);
}

// TODO: Where should this really live?  It's platform-specific, but in
// PluginObject, which mainly lives in cross/o3d_glue.h+cc.
bool PluginObject::RequestFullscreenDisplay(guint32 timestamp) {
  if (fullscreen_ || fullscreen_pending_) {
    return false;
  }
  if (!g_xembed_support) {
    // I tested every Linux browser I could that worked with our plugin and not
    // a single one lacked XEmbed/Gtk2 support, so I don't think that case is
    // worth implementing.
    DLOG(ERROR) << "Fullscreen not supported without XEmbed/Gtk2; please use a "
        "modern web browser";
    return false;
  }
  GtkWidget *widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // The returned object counts as both a widget and a window.
  GtkWindow *window = GTK_WINDOW(widget);
  // Ensure that the fullscreen window is displayed on the same screen as the
  // embedded window.
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(gtk_container_));
  gtk_window_set_screen(window, screen);
  // The window title shouldn't normally be visible, but the user will see it
  // if they Alt+Tab to another app.
  gtk_window_set_title(window, "O3D Application");
  // Suppresses the title bar, resize controls, etc.
  gtk_window_set_decorated(window, FALSE);
  // Stops Gtk from writing an off-screen buffer to the display, which conflicts
  // with our GL rendering.
  gtk_widget_set_double_buffered(widget, FALSE);
  gtk_window_set_keep_above(window, TRUE);
  // In the case of Xinerama or TwinView, these will be the dimensions of the
  // whole desktop, which is wrong, but the window manager is smart enough to
  // restrict our size to that of the main screen.
  gint width = gdk_screen_get_width(screen);
  gint height = gdk_screen_get_height(screen);
  gtk_window_set_default_size(window, width, height);
  // This is probably superfluous since we have already set an appropriate
  // size, but let's do it anyway. It could still be relevant for some window
  // managers.
  gtk_window_fullscreen(window);
  g_signal_connect(window, "configure-event",
                   G_CALLBACK(GtkConfigureEventCallback), this);
  g_signal_connect(window, "delete-event",
                   G_CALLBACK(GtkDeleteEventCallback), this);
  gtk_fullscreen_container_ = widget;
  // The timestamp here is optional, but its inclusion is preferred.
  gtk_window_present_with_time(window, timestamp);
  // Explicitly set focus, otherwise we may not get it depending on the window
  // manager's policy for present.
  gdk_window_focus(widget->window, timestamp);
  // We defer switching to the new window until it gets displayed and assigned
  // it's final dimensions in the configure-event.
  fullscreen_pending_ = true;
  return true;
}

void PluginObject::CancelFullscreenDisplay() {
  if (!fullscreen_) {
    return;
  }
  o3d::DisplayWindowLinux default_display;
  default_display.set_display(display_);
  default_display.set_window(drawable_);
  if (!renderer()->CancelFullscreen(default_display,
                                    prev_width_,
                                    prev_height_)) {
    return;
  }
  renderer()->Resize(prev_width_, prev_height_);
  client()->SendResizeEvent(renderer()->width(), renderer()->height(),
                            false);
  SetGtkEventSource(gtk_container_);
  gtk_widget_destroy(gtk_fullscreen_container_);
  gtk_fullscreen_container_ = NULL;
  fullscreen_window_ = 0;
  fullscreen_ = false;
}
}  // namespace _o3d
}  // namespace glue
