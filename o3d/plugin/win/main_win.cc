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
// the Windows platform.

#include "plugin/cross/main.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include "base/logging.h"
#include "core/cross/display_mode.h"
#include "core/cross/event.h"
#include "core/win/display_window_win.h"
#include "v8/include/v8.h"
#if !defined(O3D_INTERNAL_PLUGIN)
#include "breakpad/win/exception_handler_win32.h"
#endif

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::DisplayWindowWindows;
using o3d::Event;

namespace {
// The instance handle of the O3D DLL.
HINSTANCE g_module_instance;
}  // namespace anonymous

#if !defined(O3D_INTERNAL_PLUGIN)
// Used for breakpad crash handling
static ExceptionManager *g_exception_manager = NULL;

extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  g_module_instance = instance;

  if (reason == DLL_PROCESS_DETACH) {
     // Teardown V8 when the plugin dll is unloaded.
     // NOTE: NP_Shutdown would have been a good place for this code but
     //       unfortunately it looks like it gets called even when the dll
     //       isn't really unloaded.  This is a problem since after calling
     //       V8::Dispose(), V8 cannot be initialized again.
     bool v8_disposed = v8::V8::Dispose();
     if (!v8_disposed)
       DLOG(ERROR) << "Failed to release V8 resources.";
     return true;
  }
  return true;
}
#endif  // O3D_INTERNAL_PLUGIN

namespace {
const wchar_t* const kO3DWindowClassName = L"O3DWindowClass";

void CleanupAllWindows(PluginObject *obj);

static int HandleKeyboardEvent(PluginObject *obj,
                               HWND hWnd,
                               UINT Msg,
                               WPARAM wParam,
                               LPARAM lParam) {
  DCHECK(obj);
  DCHECK(obj->client());
  Event::Type type;
  // First figure out which kind of event to create, and do any event-specific
  // processing that can be done prior to creating it.
  switch (Msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      type = Event::TYPE_KEYDOWN;
      break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      type = Event::TYPE_KEYUP;
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      type = Event::TYPE_KEYPRESS;
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << Msg;
  }
  Event event(type);
  switch (Msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      event.set_key_code(wParam);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      event.set_char_code(wParam);
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << Msg;
  }
  // TODO: Try out TranslateAccelerator to see if that causes
  // accelerators to start working.  They would then evade JavaScript handlers,
  // though, so perhaps we'd have to check to see if we want to handle them
  // first?  That would require going around the event queue, at least
  // partially.  OTOH I see hints in blog posts that only Firefox allows
  // handlers to suppress syskeys anyway, so maybe it's not that big a
  // deal...unless apps want to use those keys...and depending on what happens
  // in other browsers.

  unsigned char keyboard_state[256];
  if (!::GetKeyboardState(static_cast<PBYTE>(keyboard_state))) {
    LOG(ERROR) << "GetKeyboardState failed.";
    return 1;
  }

  int modifier_state = 0;
  if (keyboard_state[VK_CONTROL] & 0x80) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (keyboard_state[VK_SHIFT] & 0x80) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (keyboard_state[VK_MENU] & 0x80) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  event.set_modifier_state(modifier_state);

  if (event.type() == Event::TYPE_KEYDOWN &&
      (wParam == VK_ESCAPE ||
      (wParam == VK_F4 && (modifier_state & Event::MODIFIER_ALT)))) {
    obj->CancelFullscreenDisplay();
  }

  obj->client()->AddEventToQueue(event);
  return 0;
}

void HandleMouseEvent(PluginObject *obj,
                      HWND hWnd,
                      UINT Msg,
                      WPARAM wParam,
                      LPARAM lParam) {
  DCHECK(obj);
  DCHECK(obj->client());
  bool fake_dblclick = false;
  Event::Type type;
  int x = GET_X_LPARAM(lParam);
  int y = GET_Y_LPARAM(lParam);
  int screen_x, screen_y;
  bool in_plugin = false;
  {
    RECT rect;
    if (!::GetWindowRect(hWnd, &rect)) {
      DCHECK(false);
      return;
    }
    if (Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL ||
        Msg == WM_CONTEXTMENU) {
      // These messages return screen-relative coordinates, not
      // window-relative coordinates.
      screen_x = x;
      screen_y = y;
      x -= rect.left;
      y -= rect.top;
    } else {
      screen_x = x + rect.left;
      screen_y = y + rect.top;
    }
    if (x >= 0 && x < rect.right - rect.left &&
        y >= 0 && y < rect.bottom - rect.top) {
      // x, y are 0-based from the top-left corner of the plugin.  Rect is in
      // screen coordinates, with bottom > top, right > left.
      in_plugin = true;
    }
  }
  // First figure out which kind of event to create, and do any event-specific
  // processing that can be done prior to creating it.
  switch (Msg) {
    case WM_MOUSEMOVE:
      type = Event::TYPE_MOUSEMOVE;
      break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
#endif
      type = Event::TYPE_MOUSEDOWN;
      obj->set_got_dblclick(false);
      SetCapture(hWnd);  // Capture mouse to make sure we get the mouseup.
      break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONUP:
#endif
      type = Event::TYPE_MOUSEUP;
      if (obj->got_dblclick()) {
        fake_dblclick = in_plugin;
        obj->set_got_dblclick(false);
      }
      ReleaseCapture();
      break;

    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDBLCLK:
#endif
      // On a double-click, windows produces: down, up, move, dblclick, up.
      // JavaScript should receive: down, up, [optional move, ] click, down,
      // up, click, dblclick.
      // The EventManager turns (down, up) into click, since we need that on all
      // platforms.  Windows then has to turn (dblclick, up) into (down, up,
      // click, dblclick) IFF both events took place in the plugin.  If only the
      // dblclick did, it just turns into a down.  If only the up did, it's just
      // an up, and we shouldn't be passing along the down/dblclick anyway.  So
      // we turn the doubleclick into a mousedown, store the fact that it was a
      // doubleclick, and wait for the corresponding mouseup to finish off the
      // sequence.  If we get anything that indicates that we missed the mouseup
      // [because it went to a different window or element], we forget about the
      // dblclick.
      DCHECK(in_plugin);
      obj->set_got_dblclick(true);
      type = Event::TYPE_MOUSEDOWN;
      SetCapture(hWnd);  // Capture mouse to make sure we get the mouseup.
      break;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      type = Event::TYPE_WHEEL;
      break;

    case WM_CONTEXTMENU:
      type = Event::TYPE_CONTEXTMENU;
      break;

    default:
      LOG(FATAL) << "Unknown mouse event: " << Msg;
  }
  Event event(type);
  // Now do any event-specific code that requires an Event object.
  switch (Msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
      event.set_button(Event::BUTTON_LEFT);
      break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
      event.set_button(Event::BUTTON_RIGHT);
      break;

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
      event.set_button(Event::BUTTON_MIDDLE);
      break;

#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
      if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
        event.set_button(Event::BUTTON_4);
      } else {
        event.set_button(Event::BUTTON_5);
      }
      break;
#endif
    case WM_MOUSEWHEEL:
      event.set_delta(0, GET_WHEEL_DELTA_WPARAM(wParam));
      break;
    case WM_MOUSEHWHEEL:
      event.set_delta(GET_WHEEL_DELTA_WPARAM(wParam), 0);
      break;

    default:
      break;
  }

  if (event.type() != WM_CONTEXTMENU) {
    // Only the context menu event doesn't get this information.
    int modifier_state = 0;
    if (wParam & MK_CONTROL) {
      modifier_state |= Event::MODIFIER_CTRL;
    }
    if (wParam & MK_SHIFT) {
      modifier_state |= Event::MODIFIER_SHIFT;
    }
    if (::GetKeyState(VK_MENU) < 0) {
      modifier_state |= Event::MODIFIER_ALT;
    }
    event.set_modifier_state(modifier_state);
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);
  if (fake_dblclick) {
    event.set_type(Event::TYPE_DBLCLICK);
    obj->client()->AddEventToQueue(event);
  }
  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}

LRESULT HandleDragAndDrop(PluginObject *obj, WPARAM wParam) {
  HDROP hDrop = reinterpret_cast<HDROP>(wParam);
  UINT num_files = ::DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
  if (!num_files) {
    ::DragFinish(hDrop);
    return 0;
  }
  UINT path_len = ::DragQueryFile(hDrop, 0, NULL, 0);
  // Let's limit that length, just in case.
  if (!path_len || path_len > 4096) {
    ::DragFinish(hDrop);
    return 0;
  }
  scoped_array<TCHAR> path(new TCHAR[path_len + 1]);  // Add 1 for the NUL.
  UINT num_chars = ::DragQueryFile(hDrop, 0, path.get(), path_len + 1);
  DCHECK(num_chars == path_len);
  ::DragFinish(hDrop);

  char *path_to_use = NULL;
#ifdef UNICODE  // TCHAR is WCHAR
  // TODO: We definitely need to move this to a string utility class.
  int bytes_needed = ::WideCharToMultiByte(CP_UTF8,
                                           0,
                                           path.get(),
                                           num_chars + 1,
                                           NULL,
                                           0,
                                           NULL,
                                           NULL);
  // Let's limit that length, just in case.
  if (!bytes_needed || bytes_needed > 4096) {
    return 0;
  }
  scoped_array<char> utf8_path(new char[bytes_needed]);
  int bytes_conv = ::WideCharToMultiByte(CP_UTF8,
                                         0,
                                         path.get(),
                                         num_chars + 1,
                                         utf8_path.get(),
                                         bytes_needed,
                                         NULL,
                                         NULL);
  DCHECK(bytes_conv == bytes_needed);
  path_to_use = utf8_path.get();
  num_chars = bytes_conv;
#else
  path_to_use = path.get();
#endif

  for (UINT i = 0; i < num_chars; ++i) {
    if (path_to_use[i] == '\\') {
      path_to_use[i] = '/';
    }
  }
  obj->RedirectToFile(path_to_use);

  return 1;
}

static const UINT kDestroyWindowMessageID = WM_USER + 1;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  // To work around deadlocks caused by calling DestroyWindow
  // synchronously during plugin destruction in Chrome's multi-process
  // architecture, we call DestroyWindow asynchronously.
  if (Msg == kDestroyWindowMessageID) {
    ::DestroyWindow(hWnd);
    // Do not touch anything related to the plugin; it has likely
    // already been destroyed.
    return 0;
  }

  PluginObject *obj = PluginObject::GetPluginProperty(hWnd);
  if (obj == NULL) {                   // It's not my window
    return 1;  // 0 often means we handled it.
  }

  // Limit the ways in which we can be reentrant.  Note that this WindowProc
  // may be called by different threads.  For example, IE will register plugin
  // instances on separate threads.
  o3d::Client::ScopedIncrement reentrance_count(obj->client());

  switch (Msg) {
    case WM_PAINT: {
      PAINTSTRUCT paint_struct;
      HDC hdc = ::BeginPaint(hWnd, &paint_struct);
      if (paint_struct.rcPaint.right - paint_struct.rcPaint.left != 0 ||
          paint_struct.rcPaint.bottom - paint_struct.rcPaint.top != 0) {
        if (obj->renderer()) {
          // It appears to be necessary to use GDI to paint something at least
          // once before D3D rendering will work in Vista with Aero.
          if (!obj->RecordPaint()) {
            ::SetPixelV(hdc, 0, 0, RGB(0, 0, 0));
          }
          obj->renderer()->set_need_to_render(true);
        } else {
          // If the Client has no Renderer associated with it, paint the
          // draw area gray.
          ::SelectObject(paint_struct.hdc, GetStockObject(DKGRAY_BRUSH));
          ::Rectangle(paint_struct.hdc,
                      paint_struct.rcPaint.left,
                      paint_struct.rcPaint.top,
                      paint_struct.rcPaint.right,
                      paint_struct.rcPaint.bottom);
        }
      }
      ::EndPaint(hWnd, &paint_struct);
      return 0;
    }
    case WM_SETCURSOR: {
      obj->set_cursor(obj->cursor());
      return 1;
    }
    case WM_ERASEBKGND: {
      // Tell windows we don't need the background cleared.
      return 1;
    }

    case WM_TIMER: {
      if (reentrance_count.get() > 1) {
        break;  // Ignore this message; we're reentrant.
      }

#if !defined(O3D_INTERNAL_PLUGIN)
      if (g_logger) g_logger->UpdateLogging();
#endif

      // If rendering continuously, invalidate the window and force a paint if
      // it is visible. The paint invalidates the renderer and Tick will later
      // repaint the window.
      if (obj->client()->NeedsContinuousRender()) {
        InvalidateRect(obj->GetHWnd(), NULL, FALSE);
        UpdateWindow(obj->GetHWnd());
      }

      obj->AsyncTick();

      break;
    }
    case WM_NCDESTROY: {
      // Win32 docs say we must remove all our properties before destruction.
      // However, this message doesn't appear to come early enough to be useful
      // when running in Chrome, at least.
      PluginObject::ClearPluginProperty(hWnd);
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
#endif
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      // Without this SetFocus, if you alt+tab away from Firefox, then click
      // back in the plugin, Firefox will get keyboard focus but we won't.
      // However, if we do it on mouseup as well, then the click that triggers
      // fullscreen, and is followed by a mouseup in the plugin, which will grab
      // the focus back from the fullscreen window.
      ::SetFocus(hWnd);
    // FALL THROUGH
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONUP:
#endif
    case WM_MOUSEMOVE:
    case WM_CONTEXTMENU:
      HandleMouseEvent(obj, hWnd, Msg, wParam, lParam);
      break;

    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    case WM_UNICHAR:
      // I believe these are redundant; TODO: Test this on a non-US
      // keyboard.
      break;

    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      return HandleKeyboardEvent(obj, hWnd, Msg, wParam, lParam);

    case WM_DROPFILES:
      return HandleDragAndDrop(obj, wParam);

    case WM_ACTIVATE:
      // We don't receive WM_KILLFOCUS when Alt-Tabbing away from a
      // full-screen window. We do however get WM_ACTIVATE.
      if (LOWORD(wParam) == WA_INACTIVE) {
        if (obj->fullscreen()) {
          obj->CancelFullscreenDisplay();
        }
      }
      return 0;

    case WM_KILLFOCUS:
      // If we lose focus [which also happens on alt+f4 killing the fullscreen
      // window] fall back to plugin mode to avoid lost-device awkwardness.
      // TODO: We'll have problems with this when dealing with e.g.
      // Japanese text input IME windows.
      if (obj->fullscreen()) {
        // TODO(kbr): consider doing this somehow more asynchronously;
        // not supposed to cause window activation in the WM_KILLFOCUS
        // handler
        obj->CancelFullscreenDisplay();
        return 0;
      }
      // FALL THROUGH
    case WM_SETFOCUS:
    default:
      // Decrement reentrance_count here.  It's OK if this call
      // boomerangs back to us, given that we're not in the middle of doing
      // anything caused by this message.  Since we decrement reentrance_count
      // manually, its destructor will know not to.
      reentrance_count.decrement();

      return ::CallWindowProc(::DefWindowProc,
                              hWnd,
                              Msg,
                              wParam,
                              lParam);
  }
  return 0;
}

static const wchar_t* kOrigWndProcName = L"o3dOrigWndProc";

LRESULT CALLBACK PluginWindowInterposer(HWND hWnd,
                                        UINT Msg,
                                        WPARAM wParam,
                                        LPARAM lParam) {
  PluginObject *obj = PluginObject::GetPluginProperty(hWnd);

  // Need to get the original window proc before potentially calling
  // CleanupAllWindows which will clear it.
  WNDPROC proc = static_cast<WNDPROC>(GetProp(hWnd, kOrigWndProcName));
  DCHECK(proc != NULL);

  switch (Msg) {
    case WM_PAINT: {
      // For nicer startup appearance, allow the browser to paint the
      // plugin window until we start to draw 3D content. Forbid the
      // browser from painting once we have started to draw to prevent
      // a flash in Firefox upon our receiving focus the first time.
      if (obj != NULL && obj->renderer() != NULL) {
        if (obj->renderer()->presented_once()) {
          // Tell Windows we painted the window region.
          ::ValidateRect(hWnd, NULL);
          return 0;
        }
      }
      // Break out to call the original window procedure to paint the
      // window.
      break;
    }

    case WM_DESTROY:
      if (obj != NULL) {
        CleanupAllWindows(obj);
      }
      break;

    default:
      break;
  }

  return CallWindowProc(proc, hWnd, Msg, wParam, lParam);
}

bool RegisterO3DWindowClass() {
  WNDCLASSEX window_class;
  ZeroMemory(&window_class, sizeof(window_class));
  window_class.cbSize = sizeof(window_class);
  window_class.hInstance = g_module_instance;
  window_class.lpfnWndProc = WindowProc;
  window_class.lpszClassName = kO3DWindowClassName;
  // We use CS_OWNDC in case we are rendering OpenGL into this window.
  window_class.style = CS_DBLCLKS | CS_OWNDC;
  return RegisterClassEx(&window_class) != 0;
}

void UnregisterO3DWindowClass() {
  UnregisterClass(kO3DWindowClassName, g_module_instance);
}

void CleanupAllWindows(PluginObject *obj) {
  if (obj->GetContentHWnd()) {
    ::KillTimer(obj->GetContentHWnd(), 0);
    PluginObject::ClearPluginProperty(obj->GetContentHWnd());
    ::PostMessage(obj->GetContentHWnd(), kDestroyWindowMessageID, 0, 0);
    obj->SetContentHWnd(NULL);
  }

  if (obj->GetPluginHWnd()) {
    // Restore the original WNDPROC on the plugin window so that we
    // don't attempt to call into the O3D DLL after it's been unloaded.
    LONG_PTR origWndProc = reinterpret_cast<LONG_PTR>(
        GetProp(obj->GetPluginHWnd(),
                kOrigWndProcName));
    DCHECK(origWndProc != NULL);
    // TODO: this leaks.
    RemoveProp(obj->GetPluginHWnd(), kOrigWndProcName);
    SetWindowLongPtr(obj->GetPluginHWnd(), GWLP_WNDPROC, origWndProc);

    PluginObject::ClearPluginProperty(obj->GetPluginHWnd());
    obj->SetPluginHWnd(NULL);
  }

  obj->SetHWnd(NULL);
}

// Re-parents the content_hwnd into the containing_hwnd, resizing the
// content_hwnd to the given width and height in the process.
void ReplaceContentWindow(HWND content_hwnd,
                          HWND containing_hwnd,
                          int width, int height) {
  ::ShowWindow(content_hwnd, SW_HIDE);
  LONG_PTR style = ::GetWindowLongPtr(content_hwnd, GWL_STYLE);
  style |= WS_CHILD;
  ::SetWindowLongPtr(content_hwnd, GWL_STYLE, style);
  LONG_PTR exstyle = ::GetWindowLongPtr(content_hwnd, GWL_EXSTYLE);
  exstyle &= ~WS_EX_TOOLWINDOW;
  ::SetWindowLongPtr(content_hwnd, GWL_EXSTYLE, exstyle);
  ::SetParent(content_hwnd, containing_hwnd);
  BOOL res = ::SetWindowPos(content_hwnd, containing_hwnd,
                            0, 0, width, height,
                            SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
  DCHECK(res);
  ::ShowWindow(content_hwnd, SW_SHOW);
}

// Get the screen rect of the monitor the window is on, in virtual screen
// coordinates.
// Return true on success, false on failure.
bool GetScreenRect(HWND hwnd,
                   RECT* rect) {
  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
  if (monitor == NULL)
    return false;

  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  if (GetMonitorInfo(monitor, &monitor_info)) {
    *rect = monitor_info.rcMonitor;
    return true;
  } else {
    return false;
  }
}

}  // namespace anonymous

namespace o3d {

NPError PlatformPreNPInitialize() {
#if !defined(O3D_INTERNAL_PLUGIN)
  // Setup crash handler
  if (!g_exception_manager) {
    g_exception_manager = new ExceptionManager(false);
    g_exception_manager->StartMonitoring();
  }
#endif  // O3D_INTERNAL_PLUGIN
  return NPERR_NO_ERROR;
}

NPError PlatformPostNPInitialize() {
  if (!RegisterO3DWindowClass())
    return NPERR_MODULE_LOAD_FAILED_ERROR;

  return NPERR_NO_ERROR;
}

NPError PlatformPreNPShutdown() {
  // TODO(tschmelcher): Is there a reason we need to do this before the main
  // shutdown tasks? If not, we can axe the PlatformPreNPShutdown() function.
  UnregisterO3DWindowClass();
  return NPERR_NO_ERROR;
}

NPError PlatformPostNPShutdown() {
#if !defined(O3D_INTERNAL_PLUGIN)
  // TODO : This is commented out until we can determine if
  // it's safe to shutdown breakpad at this stage (Gears, for
  // example, never deletes...)
  // Shutdown breakpad
  // delete g_exception_manager;
#endif

  return NPERR_NO_ERROR;
}

NPError PlatformNPPNew(NPP instance, PluginObject *obj) {
  return NPERR_NO_ERROR;
}

NPError PlatformNPPDestroy(NPP instance, PluginObject *obj) {
  if (obj->GetHWnd()) {
    CleanupAllWindows(obj);
  }

  obj->TearDown();
  return NPERR_NO_ERROR;
}

NPError PlatformNPPGetValue(PluginObject *obj,
                            NPPVariable variable,
                            void *value) {
  return NPERR_INVALID_PARAM;
}

NPError PlatformNPPSetWindow(NPP instance,
                             PluginObject *obj,
                             NPWindow *window) {
  HWND hWnd = static_cast<HWND>(window->window);
  if (!hWnd) {
    // Chrome calls us this way before NPP_Destroy.
    if (obj->GetHWnd()) {
      CleanupAllWindows(obj);
    }
    return NPERR_NO_ERROR;
  }
  if (obj->GetPluginHWnd() == hWnd) {
    // May need to resize the content window.
    DCHECK(obj->GetContentHWnd());
    // Avoid spurious resize requests.
    if (window->width != obj->width() ||
        window->height != obj->height()) {
      if (!obj->fullscreen() && window->width > 0 && window->height > 0) {
        ::SetWindowPos(obj->GetContentHWnd(), obj->GetPluginHWnd(), 0, 0,
                       window->width, window->height,
                       SWP_NOZORDER | SWP_NOREPOSITION);
      }
      // Even if we are in full-screen mode, store off the new width
      // and height to restore to them later.
      obj->Resize(window->width, window->height);
      // Only propagate this resize event to the client if it isn't in
      // full-screen mode.
      if (!obj->fullscreen()) {
        obj->client()->SendResizeEvent(obj->width(), obj->height(), false);
      }
    }
    return NPERR_NO_ERROR;
  }

  DCHECK(!obj->GetPluginHWnd());
  obj->SetPluginHWnd(hWnd);

  // Subclass the plugin window's window procedure to avoid processing
  // WM_PAINT. This seems to only be necessary for Firefox, which
  // overdraws our plugin the first time it gains focus.
  SetProp(hWnd, kOrigWndProcName,
          reinterpret_cast<HANDLE>(GetWindowLongPtr(hWnd, GWLP_WNDPROC)));
  PluginObject::StorePluginPropertyUnsafe(hWnd, obj);
  SetWindowLongPtr(hWnd, GWLP_WNDPROC,
                   reinterpret_cast<LONG_PTR>(PluginWindowInterposer));

  // Create the content window, into which O3D always renders, rather
  // than alternating rendering between the browser's window and a
  // separate full-screen window. The O3D window is removed from the
  // browser's hierarchy and made top-level in order to go to
  // full-screen mode via Direct3D. This solves fundamental focus
  // fighting problems seen on Windows Vista.
  HWND content_window =
    CreateWindow(kO3DWindowClassName,
                 L"O3D Window",
                 WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                 0, 0,
                 window->width, window->height,
                 hWnd,
                 NULL,
                 g_module_instance,
                 NULL);
  obj->SetContentHWnd(content_window);
  PluginObject::StorePluginProperty(content_window, obj);
  ::ShowWindow(content_window, SW_SHOW);
  ::ShowWindow(hWnd, SW_SHOW);

  // create and assign the graphics context
  DisplayWindowWindows default_display;
  default_display.set_hwnd(obj->GetHWnd());
  obj->CreateRenderer(default_display);
  obj->client()->Init();
  obj->Resize(window->width, window->height);

  // we set the timer to 10ms or 100fps. At the time of this comment
  // the renderer does a vsync the max fps it will run will be the refresh
  // rate of the monitor or 100fps, which ever is lower.
  ::SetTimer(obj->GetHWnd(), 0, 10, NULL);

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
bool PluginObject::GetDisplayMode(int mode_id, o3d::DisplayMode *mode) {
  return renderer()->GetDisplayMode(mode_id, mode);
}

// TODO: Where should this really live?  It's platform-specific, but in
// PluginObject, which mainly lives in cross/o3d_glue.h+cc.
bool PluginObject::RequestFullscreenDisplay() {
  bool success = false;
  DCHECK(GetPluginHWnd());
  DCHECK(GetContentHWnd());
  if (!fullscreen_ && renderer_ && fullscreen_region_valid_) {
    DCHECK(renderer_->fullscreen() == fullscreen_);
    // The focus window we pass into IDirect3D9::CreateDevice must not
    // fight with the full-screen window for the focus. The best way
    // to achieve this is to re-use the content window for full-screen
    // mode.
    ::ShowWindow(GetContentHWnd(), SW_HIDE);
    ::SetParent(GetContentHWnd(), NULL);
    // Remove WS_CHILD from the window style
    LONG_PTR style = ::GetWindowLongPtr(GetContentHWnd(), GWL_STYLE);
    style &= ~WS_CHILD;
    ::SetWindowLongPtr(GetContentHWnd(), GWL_STYLE, style);
    // Add WS_EX_TOOLWINDOW to the window exstyle so the content window won't
    // show in the Taskbar.
    LONG_PTR exstyle = ::GetWindowLongPtr(GetContentHWnd(), GWL_EXSTYLE);
    exstyle |= WS_EX_TOOLWINDOW;
    ::SetWindowLongPtr(GetContentHWnd(), GWL_EXSTYLE, exstyle);
    ::ShowWindow(GetContentHWnd(), SW_SHOW);
    // We need to resize the full-screen window to the desired size of
    // the display mode early, before calling
    // Renderer::GoFullscreen().
    RECT screen_rect;
    if (GetScreenRect(GetPluginHWnd(), &screen_rect)) {
      ::SetWindowPos(GetContentHWnd(), HWND_TOPMOST,
                     screen_rect.left, screen_rect.top,
                     screen_rect.right - screen_rect.left + 1,
                     screen_rect.bottom - screen_rect.top + 1,
                     SWP_NOZORDER | SWP_NOREPOSITION | SWP_ASYNCWINDOWPOS);
      DisplayWindowWindows display;
      display.set_hwnd(GetContentHWnd());
      if (renderer_->GoFullscreen(display,
                                  fullscreen_region_mode_id_)) {
        fullscreen_ = true;
        client()->SendResizeEvent(renderer_->width(), renderer_->height(),
                                  true);
        success = true;
      }
    }

    if (!success) {
      ReplaceContentWindow(GetContentHWnd(), GetPluginHWnd(),
                           prev_width_, prev_height_);
      LOG(ERROR) << "Failed to switch to fullscreen mode.";
    }
  }
  return success;
}

void PluginObject::CancelFullscreenDisplay() {
  DCHECK(GetPluginHWnd());
  if (fullscreen_) {
    DCHECK(renderer());
    DCHECK(renderer()->fullscreen());
    fullscreen_ = false;
    DisplayWindowWindows display;
    display.set_hwnd(GetContentHWnd());
    if (!renderer_->CancelFullscreen(display, prev_width_, prev_height_)) {
      LOG(FATAL) << "Failed to get the renderer out of fullscreen mode!";
    }
    ReplaceContentWindow(GetContentHWnd(), GetPluginHWnd(),
                         prev_width_, prev_height_);
    client()->SendResizeEvent(prev_width_, prev_height_, false);
  }
}
}  // namespace _o3d
}  // namespace glue
