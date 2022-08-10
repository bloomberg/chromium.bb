// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_win.h"

#include <shellapi.h>
#include <wininet.h>
#include <winspool.h>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/window_delegate_view.h"
#include "libcef/browser/screen_util.h"
#include "libcef/browser/thread_util.h"

#include "base/base_paths_win.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/aura/window.h"
#include "ui/base/win/shell.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/platform_key_map_win.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

void WriteTempFileAndView(scoped_refptr<base::RefCountedString> str) {
  CEF_REQUIRE_BLOCKING();

  base::FilePath tmp_file;
  if (!base::CreateTemporaryFile(&tmp_file))
    return;

  // The shell command will look at the file extension to identify the correct
  // program to open.
  tmp_file = tmp_file.AddExtension(L"txt");

  const std::string& data = str->data();
  int write_ct = base::WriteFile(tmp_file, data.c_str(), data.size());
  DCHECK_EQ(static_cast<int>(data.size()), write_ct);

  ui::win::OpenFileViaShell(tmp_file);
}

bool HasExternalHandler(const std::string& scheme) {
  base::win::RegKey key;
  const std::wstring registry_path =
      base::ASCIIToWide(scheme + "\\shell\\open\\command");
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str(), KEY_READ);
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size, NULL);
    if (size > 2) {
      // ShellExecute crashes the process when the command is empty.
      // We check for "2" because it always returns the trailing NULL.
      return true;
    }
  }

  return false;
}

// Match the logic in chrome/browser/platform_util_win.cc
// OpenExternalOnWorkerThread.
void ExecuteExternalProtocol(const GURL& url) {
  CEF_REQUIRE_BLOCKING();

  if (!HasExternalHandler(url.scheme()))
    return;

  // Quote the input scheme to be sure that the command does not have
  // parameters unexpected by the external program. This url should already
  // have been escaped.
  std::string escaped_url = url.spec();
  escaped_url.insert(0, "\"");
  escaped_url += "\"";

  // According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
  // "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
  // ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
  // support URLS of 2083 chars in length, 2K is safe."
  //
  // It may be possible to increase this. https://crbug.com/727909
  const size_t kMaxUrlLength = 2048;
  if (escaped_url.length() > kMaxUrlLength)
    return;

  // Specify %windir%\system32 as the CWD so that any new proc spawned does not
  // inherit this proc's CWD. Without this, uninstalls may be broken by a
  // long-lived child proc that holds a handle to the browser's version
  // directory (the browser's CWD). A process's CWD is in the standard list of
  // directories to search when loading a DLL, and precedes the system directory
  // when safe DLL search mode is disabled (not the default). Setting the CWD to
  // the system directory is a nice way to mitigate a potential DLL search order
  // hijack for processes that don't implement their own mitigation.
  base::FilePath system_dir;
  base::PathService::Get(base::DIR_SYSTEM, &system_dir);
  if (reinterpret_cast<ULONG_PTR>(ShellExecuteA(
          NULL, "open", escaped_url.c_str(), NULL,
          system_dir.AsUTF8Unsafe().c_str(), SW_SHOWNORMAL)) <= 32) {
    // On failure, it may be good to display a message to the user.
    // https://crbug.com/727913
    return;
  }
}

// DPI value for 1x scale factor.
#define DPI_1X 96.0f

float GetWindowScaleFactor(HWND hwnd) {
  DCHECK(hwnd);

  if (base::win::IsProcessPerMonitorDpiAware()) {
    // Let Windows tell us the correct DPI.
    static auto get_dpi_for_window_func = []() {
      return reinterpret_cast<decltype(::GetDpiForWindow)*>(
          GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow"));
    }();
    if (get_dpi_for_window_func)
      return static_cast<float>(get_dpi_for_window_func(hwnd)) / DPI_1X;
  }

  // Fallback to the monitor that contains the window center point.
  RECT cr;
  GetWindowRect(hwnd, &cr);
  return display::Screen::GetScreen()
      ->GetDisplayNearestPoint(
          gfx::Point((cr.right - cr.left) / 2, (cr.bottom - cr.top) / 2))
      .device_scale_factor();
}

struct ScreenInfo {
  float scale_factor;
  CefRect rect;
};

ScreenInfo GetScreenInfo(int x, int y) {
  const auto display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(gfx::Point(x, y));
  const auto rect = display.work_area();

  return ScreenInfo{display.device_scale_factor(),
                    CefRect(rect.x(), rect.y(), rect.width(), rect.height())};
}

CefRect GetFrameRectFromLogicalContentRect(CefRect content,
                                           DWORD style,
                                           DWORD ex_style,
                                           bool has_menu,
                                           float scale) {
  const auto scaled_rect = gfx::ScaleToRoundedRect(
      gfx::Rect(content.x, content.y, content.width, content.height), scale);

  RECT rect = {0, 0, scaled_rect.width(), scaled_rect.height()};

  AdjustWindowRectEx(&rect, style, has_menu, ex_style);

  return CefRect(scaled_rect.x(), scaled_rect.y(), rect.right - rect.left,
                 rect.bottom - rect.top);
}

CefRect GetAdjustedWindowRect(CefRect content,
                              DWORD style,
                              DWORD ex_style,
                              bool has_menu) {
  // If height or width is not provided, let OS determine position and size,
  // similarly to Chromium behavior
  if (content.width == CW_USEDEFAULT || content.height == CW_USEDEFAULT) {
    return CefRect(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
  }

  if (content.x == CW_USEDEFAULT) {
    content.x = 0;
  }

  if (content.y == CW_USEDEFAULT) {
    content.y = 0;
  }

  const ScreenInfo screen = GetScreenInfo(content.x, content.y);
  const CefRect rect = MakeVisibleOnScreenRect(content, screen.rect);

  return GetFrameRectFromLogicalContentRect(rect, style, ex_style, has_menu,
                                            screen.scale_factor);
}

}  // namespace

CefBrowserPlatformDelegateNativeWin::CefBrowserPlatformDelegateNativeWin(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color) {}

void CefBrowserPlatformDelegateNativeWin::set_widget(
    views::Widget* widget,
    CefWindowHandle widget_handle) {
  DCHECK(!window_widget_);
  window_widget_ = widget;
  DCHECK(!window_info_.window);
  window_info_.window = widget_handle;
}

void CefBrowserPlatformDelegateNativeWin::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNativeAura::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeWin::CreateHostWindow() {
  RegisterWindowClass();

  if (window_info_.style == 0) {
    // Client didn't intialize the CefWindowInfo. Provide reasonable defaults.
    window_info_.SetAsPopup(nullptr, CefString());
  }

  has_frame_ = !(window_info_.style & WS_CHILD);

  const std::wstring windowName(CefString(&window_info_.window_name));

  CefRect window_rect = window_info_.bounds;

  if (!window_info_.parent_window) {
    const bool has_menu =
        !(window_info_.style & WS_CHILD) && (window_info_.menu != NULL);
    window_rect = GetAdjustedWindowRect(window_rect, window_info_.style,
                                        window_info_.ex_style, has_menu);
  }

  // Create the new browser window.
  CreateWindowEx(window_info_.ex_style, GetWndClass(), windowName.c_str(),
                 window_info_.style, window_rect.x, window_rect.y,
                 window_rect.width, window_rect.height,
                 window_info_.parent_window, window_info_.menu,
                 ::GetModuleHandle(NULL), this);

  // It's possible for CreateWindowEx to fail if the parent window was
  // destroyed between the call to CreateBrowser and the above one.
  DCHECK(window_info_.window);
  if (!window_info_.window)
    return false;

  host_window_created_ = true;

  // Add a reference that will later be released in DestroyBrowser().
  browser_->AddRef();

  if (!called_enable_non_client_dpi_scaling_ && has_frame_ &&
      base::win::IsProcessPerMonitorDpiAware()) {
    // This call gets Windows to scale the non-client area when WM_DPICHANGED
    // is fired on Windows versions < 10.0.14393.0.
    // Derived signature; not available in headers.
    static auto enable_child_window_dpi_message_func = []() {
      using EnableChildWindowDpiMessagePtr = LRESULT(WINAPI*)(HWND, BOOL);
      return reinterpret_cast<EnableChildWindowDpiMessagePtr>(GetProcAddress(
          GetModuleHandle(L"user32.dll"), "EnableChildWindowDpiMessage"));
    }();
    if (enable_child_window_dpi_message_func)
      enable_child_window_dpi_message_func(window_info_.window, TRUE);
  }

  DCHECK(!window_widget_);

  // Convert from device coordinates to logical coordinates.
  RECT cr;
  GetClientRect(window_info_.window, &cr);
  gfx::Point point = gfx::Point(cr.right, cr.bottom);
  const float scale = GetWindowScaleFactor(window_info_.window);
  point =
      gfx::ToFlooredPoint(gfx::ScalePoint(gfx::PointF(point), 1.0f / scale));

  // Stay on top if top-most window hosting the web view is topmost.
  HWND top_level_window = GetAncestor(window_info_.window, GA_ROOT);
  DWORD top_level_window_ex_styles =
      GetWindowLongPtr(top_level_window, GWL_EXSTYLE);
  bool always_on_top =
      (top_level_window_ex_styles & WS_EX_TOPMOST) == WS_EX_TOPMOST;

  CefWindowDelegateView* delegate_view = new CefWindowDelegateView(
      GetBackgroundColor(), always_on_top, GetBoundsChangedCallback());
  delegate_view->Init(window_info_.window, web_contents_,
                      gfx::Rect(0, 0, point.x(), point.y()));

  window_widget_ = delegate_view->GetWidget();

  const HWND widget_hwnd = HWNDForWidget(window_widget_);
  DCHECK(widget_hwnd);
  const DWORD widget_ex_styles = GetWindowLongPtr(widget_hwnd, GWL_EXSTYLE);

  if (window_info_.ex_style & WS_EX_NOACTIVATE) {
    // Add the WS_EX_NOACTIVATE style on the DesktopWindowTreeHostWin HWND
    // so that HWNDMessageHandler::Show() called via Widget::Show() does not
    // activate the window.
    SetWindowLongPtr(widget_hwnd, GWL_EXSTYLE,
                     widget_ex_styles | WS_EX_NOACTIVATE);
  }

  window_widget_->Show();

  if (window_info_.ex_style & WS_EX_NOACTIVATE) {
    // Remove the WS_EX_NOACTIVATE style so that future mouse clicks inside the
    // browser correctly activate and focus the window.
    SetWindowLongPtr(widget_hwnd, GWL_EXSTYLE, widget_ex_styles);
  }

  return true;
}

void CefBrowserPlatformDelegateNativeWin::CloseHostWindow() {
  if (window_info_.window != NULL) {
    HWND frameWnd = GetAncestor(window_info_.window, GA_ROOT);
    PostMessage(frameWnd, WM_CLOSE, 0, 0);
  }
}

CefWindowHandle CefBrowserPlatformDelegateNativeWin::GetHostWindowHandle()
    const {
  if (windowless_handler_)
    return windowless_handler_->GetParentWindowHandle();
  return window_info_.window;
}

views::Widget* CefBrowserPlatformDelegateNativeWin::GetWindowWidget() const {
  return window_widget_;
}

void CefBrowserPlatformDelegateNativeWin::SetFocus(bool setFocus) {
  if (!setFocus)
    return;

  if (window_widget_) {
    // Give native focus to the DesktopWindowTreeHostWin ("Chrome_WidgetWin_0")
    // associated with the root window. The currently focused HWND may be
    // "CefBrowserWindow" if we're called in response to our WndProc receiving
    // the WM_SETFOCUS event (possibly due to "CefBrowserWindow" recieving the
    // top-level WM_ACTIVATE event), or some other HWND if the client calls
    // CefBrowserHost::SetFocus(true) directly. DesktopWindowTreeHostWin may
    // also receive focus/blur and mouse click events from the OS directly, in
    // which case this method will not be called but the below discussion still
    // applies.
    //
    // The DesktopWindowTreeHostWin::HandleNativeFocus/HandleNativeBlur methods
    // are called in response to WM_SETFOCUS/WM_KILLFOCUS respectively. The
    // DesktopWindowTreeHostWin::HandleMouseEvent method is called if the user
    // clicks on the WebContents. These methods have all been patched to call
    // HandleActivationChanged (indirectly via ::SetFocus in the case of mouse
    // clicks). HandleActivationChanged will then trigger the following
    // behaviors:
    // 1. Update focus/activation state of the aura::Window indirectly via
    //    wm::FocusController. This allows focus-related behaviors (e.g. focus
    //    rings, flashing caret, onFocus/onBlur JS events, etc.) to work as
    //    expected (see issue #1677) and also triggers an initial call to
    //    WebContents::Focus which gives logical focus to the
    //    RenderWidgetHostViewAura in the views hierarchy (see issue #3306).
    // 2. Update focus state of the ui::InputMethod. If this does not occur
    //    then:
    //    (a) InputMethodBase::GetTextInputClient will return NULL and
    //    InputMethodWin::OnChar will fail to send character events to the
    //    renderer (see issue #1700); and
    //    (b) InputMethodWinBase::IsWindowFocused will return false due to
    //    ::GetFocus() returning the currently focused HWND (e.g.
    //    "CefBrowserWindow") instead of the expected "Chrome_WidgetWin_0" HWND,
    //    causing TSF not to handle IME events (see issue #3306). For this same
    //    reason, ::SetFocus needs to be called before WebContents::Focus which
    //    sends the InputMethod OnWillChangeFocusedClient notification that then
    //    calls IsWindowFocused (e.g. WebContents::Focus is intentionally called
    //    multiple times).
    //
    // This differs from activation in Chrome which is handled via
    // HWNDMessageHandler::PostProcessActivateMessage (Widget::Show indirectly
    // calls HWNDMessageHandler::Activate which calls ::SetForegroundWindow
    // resulting in a WM_ACTIVATE message being sent to the window). The Chrome
    // code path doesn't work for CEF because IsTopLevelWindow in
    // hwnd_message_handler.cc will return false and consequently
    // HWNDMessageHandler::PostProcessActivateMessage will not be called.
    //
    // Activation events are usually reserved for the top-level window so
    // triggering activation based on focus events may be incorrect in some
    // circumstances. Revisit this implementation if additional problems are
    // discovered.
    ::SetFocus(HWNDForWidget(window_widget_));
  }

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus. When
    // |window_widget_| exists this additional Focus() call is necessary to
    // correctly assign focus/input state after native focus resulting from
    // window activation (see the InputMethod discussion above).
    web_contents_->Focus();
  }
}

void CefBrowserPlatformDelegateNativeWin::NotifyMoveOrResizeStarted() {
  // Call the parent method to dismiss any existing popups.
  CefBrowserPlatformDelegateNativeAura::NotifyMoveOrResizeStarted();

  if (!window_widget_)
    return;

  // Notify DesktopWindowTreeHostWin of move events so that screen rectangle
  // information is communicated to the renderer process and popups are
  // displayed in the correct location.
  views::DesktopWindowTreeHostWin* tree_host =
      static_cast<views::DesktopWindowTreeHostWin*>(
          aura::WindowTreeHost::GetForAcceleratedWidget(
              HWNDForWidget(window_widget_)));
  DCHECK(tree_host);
  if (tree_host) {
    // Cast to HWNDMessageHandlerDelegate so we can access HandleMove().
    static_cast<views::HWNDMessageHandlerDelegate*>(tree_host)->HandleMove();
  }
}

void CefBrowserPlatformDelegateNativeWin::SizeTo(int width, int height) {
  HWND window = window_info_.window;

  const DWORD style = GetWindowLong(window, GWL_STYLE);
  const DWORD ex_style = GetWindowLong(window, GWL_EXSTYLE);
  const bool has_menu = !(style & WS_CHILD) && (GetMenu(window) != NULL);
  const float scale = GetWindowScaleFactor(window);

  const CefRect content_rect(0, 0, width, height);
  const CefRect frame_rect = GetFrameRectFromLogicalContentRect(
      content_rect, style, ex_style, has_menu, scale);

  // Size the window. The left/top values may be negative.
  SetWindowPos(window, NULL, 0, 0, frame_rect.width, frame_rect.height,
               SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

void CefBrowserPlatformDelegateNativeWin::ViewText(const std::string& text) {
  std::string str = text;
  scoped_refptr<base::RefCountedString> str_ref =
      base::RefCountedString::TakeString(&str);
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(WriteTempFileAndView, str_ref));
}

bool CefBrowserPlatformDelegateNativeWin::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Any unhandled keyboard/character messages are sent to DefWindowProc so that
  // shortcut keys work correctly.
  if (event.os_event) {
    const auto& msg = event.os_event->native_event();
    return !DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
  } else {
    MSG msg = {};

    msg.hwnd = GetHostWindowHandle();
    if (!msg.hwnd)
      return false;

    switch (event.GetType()) {
      case blink::WebInputEvent::Type::kRawKeyDown:
        msg.message = event.is_system_key ? WM_SYSKEYDOWN : WM_KEYDOWN;
        break;
      case blink::WebInputEvent::Type::kKeyUp:
        msg.message = event.is_system_key ? WM_SYSKEYUP : WM_KEYUP;
        break;
      case blink::WebInputEvent::Type::kChar:
        msg.message = event.is_system_key ? WM_SYSCHAR : WM_CHAR;
        break;
      default:
        NOTREACHED();
        return false;
    }

    msg.wParam = event.windows_key_code;

    UINT scan_code = ::MapVirtualKeyW(event.windows_key_code, MAPVK_VK_TO_VSC);
    msg.lParam = (scan_code << 16) |  // key scan code
                 1;                   // key repeat count
    if (event.GetModifiers() & content::NativeWebKeyboardEvent::kAltKey)
      msg.lParam |= (1 << 29);

    return !DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
  }
}

// static
void CefBrowserPlatformDelegate::HandleExternalProtocol(const GURL& url) {
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(ExecuteExternalProtocol, url));
}

CefEventHandle CefBrowserPlatformDelegateNativeWin::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  if (!event.os_event)
    return NULL;
  return ChromeToWindowsType(
      const_cast<CHROME_MSG*>(&event.os_event->native_event()));
}

ui::KeyEvent CefBrowserPlatformDelegateNativeWin::TranslateUiKeyEvent(
    const CefKeyEvent& key_event) const {
  int flags = TranslateUiEventModifiers(key_event.modifiers);
  ui::KeyboardCode key_code =
      ui::KeyboardCodeForWindowsKeyCode(key_event.windows_key_code);
  ui::DomCode dom_code =
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code);
  base::TimeTicks time_stamp = GetEventTimeStamp();

  if (key_event.type == KEYEVENT_CHAR) {
    return ui::KeyEvent(key_event.windows_key_code /* character */, key_code,
                        dom_code, flags, time_stamp);
  }

  ui::EventType type = ui::ET_UNKNOWN;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      type = ui::ET_KEY_PRESSED;
      break;
    case KEYEVENT_KEYUP:
      type = ui::ET_KEY_RELEASED;
      break;
    default:
      NOTREACHED();
  }

  ui::DomKey dom_key =
      ui::PlatformKeyMap::DomKeyFromKeyboardCode(key_code, &flags);
  return ui::KeyEvent(type, key_code, dom_code, flags, dom_key, time_stamp);
}

gfx::Vector2d CefBrowserPlatformDelegateNativeWin::GetUiWheelEventOffset(
    int deltaX,
    int deltaY) const {
  static const ULONG defaultScrollCharsPerWheelDelta = 1;
  static const FLOAT scrollbarPixelsPerLine = 100.0f / 3.0f;
  static const ULONG defaultScrollLinesPerWheelDelta = 3;

  float wheelDeltaX = float(deltaX) / WHEEL_DELTA;
  float wheelDeltaY = float(deltaY) / WHEEL_DELTA;
  float scrollDeltaX = wheelDeltaX;
  float scrollDeltaY = wheelDeltaY;

  ULONG scrollChars = defaultScrollCharsPerWheelDelta;
  SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0);
  scrollDeltaX *= static_cast<FLOAT>(scrollChars) * scrollbarPixelsPerLine;

  ULONG scrollLines = defaultScrollLinesPerWheelDelta;
  SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
  scrollDeltaY *= static_cast<FLOAT>(scrollLines) * scrollbarPixelsPerLine;

  return gfx::Vector2d(scrollDeltaX, scrollDeltaY);
}

// static
void CefBrowserPlatformDelegateNativeWin::RegisterWindowClass() {
  static bool registered = false;
  if (registered)
    return;

  // Register the window class
  WNDCLASSEX wcex = {
      /* cbSize = */ sizeof(WNDCLASSEX),
      /* style = */ CS_HREDRAW | CS_VREDRAW,
      /* lpfnWndProc = */ CefBrowserPlatformDelegateNativeWin::WndProc,
      /* cbClsExtra = */ 0,
      /* cbWndExtra = */ 0,
      /* hInstance = */ ::GetModuleHandle(NULL),
      /* hIcon = */ NULL,
      /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
      /* hbrBackground = */ 0,
      /* lpszMenuName = */ NULL,
      /* lpszClassName = */ CefBrowserPlatformDelegateNativeWin::GetWndClass(),
      /* hIconSm = */ NULL,
  };
  RegisterClassEx(&wcex);

  registered = true;
}

// static
LPCTSTR CefBrowserPlatformDelegateNativeWin::GetWndClass() {
  return L"CefBrowserWindow";
}

// static
LRESULT CALLBACK CefBrowserPlatformDelegateNativeWin::WndProc(HWND hwnd,
                                                              UINT message,
                                                              WPARAM wParam,
                                                              LPARAM lParam) {
  CefBrowserPlatformDelegateNativeWin* platform_delegate = nullptr;
  CefBrowserHostBase* browser = nullptr;

  if (message != WM_NCCREATE) {
    platform_delegate = static_cast<CefBrowserPlatformDelegateNativeWin*>(
        gfx::GetWindowUserData(hwnd));
    if (platform_delegate) {
      browser = platform_delegate->browser_;
    }
  }

  switch (message) {
    case WM_CLOSE:
      if (browser && !browser->TryCloseBrowser()) {
        // Cancel the close.
        return 0;
      }

      // Allow the close.
      break;

    case WM_NCCREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
      platform_delegate =
          reinterpret_cast<CefBrowserPlatformDelegateNativeWin*>(
              cs->lpCreateParams);
      DCHECK(platform_delegate);
      // Associate |platform_delegate| with the window handle.
      gfx::SetWindowUserData(hwnd, platform_delegate);
      platform_delegate->window_info_.window = hwnd;

      if (platform_delegate->has_frame_ &&
          base::win::IsProcessPerMonitorDpiAware()) {
        // This call gets Windows to scale the non-client area when
        // WM_DPICHANGED is fired on Windows versions >= 10.0.14393.0.
        static auto enable_non_client_dpi_scaling_func = []() {
          return reinterpret_cast<decltype(::EnableNonClientDpiScaling)*>(
              GetProcAddress(GetModuleHandle(L"user32.dll"),
                             "EnableNonClientDpiScaling"));
        }();
        platform_delegate->called_enable_non_client_dpi_scaling_ =
            !!(enable_non_client_dpi_scaling_func &&
               enable_non_client_dpi_scaling_func(hwnd));
      }
    } break;

    case WM_NCDESTROY:
      if (platform_delegate) {
        // Clear the user data pointer.
        gfx::SetWindowUserData(hwnd, NULL);

        // Force the browser to be destroyed. This will result in a call to
        // BrowserDestroyed() that will release the reference added in
        // CreateHostWindow().
        static_cast<AlloyBrowserHostImpl*>(browser)->WindowDestroyed();
      }
      break;

    case WM_SIZE:
      if (platform_delegate && platform_delegate->window_widget_) {
        // Pass window resize events to the HWND for the DesktopNativeWidgetAura
        // root window. Passing size 0x0 (wParam == SIZE_MINIMIZED, for example)
        // will cause the widget to be hidden which reduces resource usage.
        RECT rc;
        GetClientRect(hwnd, &rc);
        SetWindowPos(HWNDForWidget(platform_delegate->window_widget_), NULL,
                     rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER);
      }
      return 0;

    case WM_MOVING:
    case WM_MOVE:
      if (browser)
        browser->NotifyMoveOrResizeStarted();
      return 0;

    case WM_SETFOCUS:
      // Selecting "Close window" from the task bar menu may send a focus
      // notification even though the window is currently disabled (e.g. while
      // a modal JS dialog is displayed).
      if (browser && ::IsWindowEnabled(hwnd))
        browser->SetFocus(true);
      return 0;

    case WM_ERASEBKGND:
      return 0;

    case WM_DPICHANGED:
      if (platform_delegate && platform_delegate->has_frame_) {
        // Suggested size and position of the current window scaled for the
        // new DPI.
        const RECT* rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(platform_delegate->GetHostWindowHandle(), NULL, rect->left,
                     rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
      }
      break;

    case WM_ENABLE:
      if (wParam == TRUE && browser) {
        // Give focus to the browser after EnableWindow enables this window
        // (e.g. after a modal dialog is dismissed).
        browser->SetFocus(true);
        return 0;
      }
      break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}