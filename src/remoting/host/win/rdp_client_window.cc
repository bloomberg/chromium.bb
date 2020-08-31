// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client_window.h"

#include <wtsdefs.h>

#include <list>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "base/win/scoped_bstr.h"

namespace remoting {

namespace {

// RDP session disconnect reason codes that should not be interpreted as errors.
constexpr long kDisconnectReasonNoInfo = 0;
constexpr long kDisconnectReasonLocalNotError = 1;
constexpr long kDisconnectReasonRemoteByUser = 2;
constexpr long kDisconnectReasonByServer = 3;

// Maximum length of a window class name including the terminating nullptr.
constexpr int kMaxWindowClassLength = 256;

// Each member of the array returned by GetKeyboardState() contains status data
// for a virtual key. If the high-order bit is 1, the key is down; otherwise, it
// is up.
constexpr BYTE kKeyPressedFlag = 0x80;

constexpr int kKeyboardStateLength = 256;

constexpr base::TimeDelta kReapplyResolutionPeriod =
    base::TimeDelta::FromMilliseconds(250);

// We want to try to reapply resolution changes for ~5 seconds (20 * 250ms).
constexpr int kMaxResolutionReapplyAttempts = 20;

// The RDP control creates 'IHWindowClass' window to handle keyboard input.
constexpr wchar_t kRdpInputWindowClass[] = L"IHWindowClass";

enum RdpAudioMode {
  // Redirect sounds to the client. This is the default value.
  kRdpAudioModeRedirect = 0,

  // Play sounds at the remote computer. Equivalent to |kRdpAudioModeNone| if
  // the remote computer is running a server SKU.
  kRdpAudioModePlayOnServer = 1,

  // Disable sound redirection; do not play sounds at the remote computer.
  kRdpAudioModeNone = 2
};

// Points to a per-thread instance of the window activation hook handle.
base::LazyInstance<base::ThreadLocalPointer<RdpClientWindow::WindowHook>>::
    DestructorAtExit g_window_hook = LAZY_INSTANCE_INITIALIZER;

// Finds a child window with the class name matching |class_name|. Unlike
// FindWindowEx() this function walks the tree of windows recursively. The walk
// is done in breadth-first order. The function returns nullptr if the child
// window could not be found.
HWND FindWindowRecursively(HWND parent, const base::string16& class_name) {
  std::list<HWND> windows;
  windows.push_back(parent);

  while (!windows.empty()) {
    HWND child = FindWindowEx(windows.front(), nullptr, nullptr, nullptr);
    while (child != nullptr) {
      // See if the window class name matches |class_name|.
      WCHAR name[kMaxWindowClassLength];
      int length = GetClassName(child, name, base::size(name));
      if (base::string16(name, length)  == class_name)
        return child;

      // Remember the window to look through its children.
      windows.push_back(child);

      // Go to the next child.
      child = FindWindowEx(windows.front(), child, nullptr, nullptr);
    }

    windows.pop_front();
  }

  return nullptr;
}

}  // namespace

// Used to close any windows activated on a particular thread. It installs
// a WH_CBT window hook to track window activations and close all activated
// windows. There should be only one instance of |WindowHook| per thread
// at any given moment.
class RdpClientWindow::WindowHook
    : public base::RefCounted<WindowHook> {
 public:
  static scoped_refptr<WindowHook> Create();

 private:
  friend class base::RefCounted<WindowHook>;

  WindowHook();
  virtual ~WindowHook();

  static LRESULT CALLBACK CloseWindowOnActivation(
      int code, WPARAM wparam, LPARAM lparam);

  HHOOK hook_;

  DISALLOW_COPY_AND_ASSIGN(WindowHook);
};

RdpClientWindow::RdpClientWindow(const net::IPEndPoint& server_endpoint,
                                 const std::string& terminal_id,
                                 EventHandler* event_handler)
    : event_handler_(event_handler),
      server_endpoint_(server_endpoint),
      terminal_id_(terminal_id) {
}

RdpClientWindow::~RdpClientWindow() {
  if (m_hWnd) {
    DestroyWindow();
  }

  DCHECK(!client_.Get());
  DCHECK(!client_9_.Get());
  DCHECK(!client_settings_.Get());
}

bool RdpClientWindow::Connect(const ScreenResolution& resolution) {
  DCHECK(!m_hWnd);

  screen_resolution_ = resolution;
  RECT rect = {0, 0, screen_resolution_.dimensions().width(),
               screen_resolution_.dimensions().height()};
  bool result = Create(nullptr, rect, nullptr) != nullptr;

  // Hide the window since this class is about establishing a connection, not
  // about showing a UI to the user.
  if (result) {
    ShowWindow(SW_HIDE);
  }

  return result;
}

void RdpClientWindow::Disconnect() {
  if (m_hWnd) {
    SendMessage(WM_CLOSE);
  }
}

void RdpClientWindow::InjectSas() {
  if (!m_hWnd) {
    return;
  }

  // Find the window handling the keyboard input.
  HWND input_window = FindWindowRecursively(m_hWnd, kRdpInputWindowClass);
  if (!input_window) {
    LOG(ERROR) << "Failed to find the window handling the keyboard input.";
    return;
  }

  VLOG(3) << "Injecting Ctrl+Alt+End to emulate SAS.";

  BYTE keyboard_state[kKeyboardStateLength];
  if (!GetKeyboardState(keyboard_state)) {
    PLOG(ERROR) << "Failed to get the keyboard state.";
    return;
  }

  // This code is running in Session 0, so we expect no keys to be pressed.
  DCHECK(!(keyboard_state[VK_CONTROL] & kKeyPressedFlag));
  DCHECK(!(keyboard_state[VK_MENU] & kKeyPressedFlag));
  DCHECK(!(keyboard_state[VK_END] & kKeyPressedFlag));

  // Map virtual key codes to scan codes.
  UINT control = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
  UINT alt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
  UINT end = MapVirtualKey(VK_END, MAPVK_VK_TO_VSC) | KF_EXTENDED;
  UINT up = KF_UP | KF_REPEAT;

  // Press 'Ctrl'.
  keyboard_state[VK_CONTROL] |= kKeyPressedFlag;
  keyboard_state[VK_LCONTROL] |= kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYDOWN, VK_CONTROL, MAKELPARAM(1, control));

  // Press 'Alt'.
  keyboard_state[VK_MENU] |= kKeyPressedFlag;
  keyboard_state[VK_LMENU] |= kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYDOWN, VK_MENU,
              MAKELPARAM(1, alt | KF_ALTDOWN));

  // Press and release 'End'.
  SendMessage(input_window, WM_KEYDOWN, VK_END,
              MAKELPARAM(1, end | KF_ALTDOWN));
  SendMessage(input_window, WM_KEYUP, VK_END,
              MAKELPARAM(1, end | up | KF_ALTDOWN));

  // Release 'Alt'.
  keyboard_state[VK_MENU] &= ~kKeyPressedFlag;
  keyboard_state[VK_LMENU] &= ~kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYUP, VK_MENU, MAKELPARAM(1, alt | up));

  // Release 'Ctrl'.
  keyboard_state[VK_CONTROL] &= ~kKeyPressedFlag;
  keyboard_state[VK_LCONTROL] &= ~kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYUP, VK_CONTROL, MAKELPARAM(1, control | up));
}

void RdpClientWindow::ChangeResolution(const ScreenResolution& resolution) {
  // Stop any pending resolution changes.
  apply_resolution_timer_.Stop();
  screen_resolution_ = resolution;
  HRESULT result = UpdateDesktopResolution();
  if (FAILED(result)) {
    LOG(WARNING) << "UpdateSessionDisplaySettings() failed: 0x" << std::hex
                 << result;
  }
}

void RdpClientWindow::OnClose() {
  if (!client_.Get()) {
    NotifyDisconnected();
    return;
  }

  // Request a graceful shutdown.
  mstsc::ControlCloseStatus close_status;
  HRESULT result = client_->RequestClose(&close_status);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to request a graceful shutdown of an RDP connection"
               << ", result=0x" << std::hex << result;
    NotifyDisconnected();
    return;
  }

  if (close_status != mstsc::controlCloseWaitForEvents) {
    NotifyDisconnected();
    return;
  }

  // Expect IMsTscAxEvents::OnConfirmClose() or IMsTscAxEvents::OnDisconnect()
  // to be called if mstsc::controlCloseWaitForEvents was returned.
}

LRESULT RdpClientWindow::OnCreate(CREATESTRUCT* create_struct) {
  CAxWindow2 activex_window;
  Microsoft::WRL::ComPtr<IUnknown> control;
  HRESULT result = E_FAIL;
  Microsoft::WRL::ComPtr<mstsc::IMsTscSecuredSettings> secured_settings;
  Microsoft::WRL::ComPtr<mstsc::IMsRdpClientSecuredSettings> secured_settings2;
  base::win::ScopedBstr server_name(
      base::UTF8ToUTF16(server_endpoint_.ToStringWithoutPort()));
  base::win::ScopedBstr terminal_id(base::UTF8ToUTF16(terminal_id_));

  // Create the child window that actually hosts the ActiveX control.
  RECT rect = {0, 0, screen_resolution_.dimensions().width(),
               screen_resolution_.dimensions().height()};
  activex_window.Create(m_hWnd, rect, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_BORDER);
  if (activex_window.m_hWnd == nullptr)
    return LogOnCreateError(HRESULT_FROM_WIN32(GetLastError()));

  // Instantiate the RDP ActiveX control.
  result = activex_window.CreateControlEx(
      OLESTR("MsTscAx.MsTscAx"), nullptr, nullptr, control.GetAddressOf(),
      __uuidof(mstsc::IMsTscAxEvents),
      reinterpret_cast<IUnknown*>(static_cast<RdpEventsSink*>(this)));
  if (FAILED(result))
    return LogOnCreateError(result);

  result = control.CopyTo(client_.GetAddressOf());
  if (FAILED(result))
    return LogOnCreateError(result);

  // Use 32-bit color.
  result = client_->put_ColorDepth(32);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Set dimensions of the remote desktop.
  result = client_->put_DesktopWidth(screen_resolution_.dimensions().width());
  if (FAILED(result))
    return LogOnCreateError(result);
  result = client_->put_DesktopHeight(screen_resolution_.dimensions().height());
  if (FAILED(result))
    return LogOnCreateError(result);

  // Check to see if the platform exposes the interface used for resizing.
  result = client_.CopyTo(client_9_.GetAddressOf());
  if (FAILED(result) && result != E_NOINTERFACE) {
    return LogOnCreateError(result);
  }

  // Set the server name to connect to.
  result = client_->put_Server(server_name.Get());
  if (FAILED(result))
    return LogOnCreateError(result);

  // Fetch IMsRdpClientAdvancedSettings interface for the client.
  result = client_->get_AdvancedSettings2(client_settings_.GetAddressOf());
  if (FAILED(result))
    return LogOnCreateError(result);

  // Disable background input mode.
  result = client_settings_->put_allowBackgroundInput(0);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Do not use bitmap cache.
  result = client_settings_->put_BitmapPersistence(0);
  if (SUCCEEDED(result))
    result = client_settings_->put_CachePersistenceActive(0);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Do not use compression.
  result = client_settings_->put_Compress(0);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Enable the Ctrl+Alt+Del screen.
  result = client_settings_->put_DisableCtrlAltDel(0);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Disable printer and clipboard redirection.
  result = client_settings_->put_DisableRdpdr(FALSE);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Do not display the connection bar.
  result = client_settings_->put_DisplayConnectionBar(VARIANT_FALSE);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Do not grab focus on connect.
  result = client_settings_->put_GrabFocusOnConnect(VARIANT_FALSE);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Enable enhanced graphics, font smoothing and desktop composition.
  const LONG kDesiredFlags = WTS_PERF_ENABLE_ENHANCED_GRAPHICS |
                             WTS_PERF_ENABLE_FONT_SMOOTHING |
                             WTS_PERF_ENABLE_DESKTOP_COMPOSITION;
  result = client_settings_->put_PerformanceFlags(kDesiredFlags);
  if (FAILED(result))
    return LogOnCreateError(result);

  // Set the port to connect to.
  result = client_settings_->put_RDPPort(server_endpoint_.port());
  if (FAILED(result))
    return LogOnCreateError(result);

  result = client_->get_SecuredSettings2(secured_settings2.GetAddressOf());
  if (SUCCEEDED(result)) {
    result =
        secured_settings2->put_AudioRedirectionMode(kRdpAudioModeRedirect);
    if (FAILED(result))
      return LogOnCreateError(result);
  }

  result = client_->get_SecuredSettings(secured_settings.GetAddressOf());
  if (FAILED(result))
    return LogOnCreateError(result);

  // Set the terminal ID as the working directory for the initial program. It is
  // observed that |WorkDir| is used only if an initial program is also
  // specified, but is still passed to the RDP server and can then be read back
  // from the session parameters. This makes it possible to use |WorkDir| to
  // match the RDP connection with the session it is attached to.
  //
  // This code should be in sync with WtsTerminalMonitor::LookupTerminalId().
  result = secured_settings->put_WorkDir(terminal_id.Get());
  if (FAILED(result))
    return LogOnCreateError(result);

  result = client_->Connect();
  if (FAILED(result))
    return LogOnCreateError(result);

  return 0;
}

void RdpClientWindow::OnDestroy() {
  client_.Reset();
  client_9_.Reset();
  client_settings_.Reset();
  apply_resolution_timer_.Stop();
}

HRESULT RdpClientWindow::OnAuthenticationWarningDisplayed() {
  LOG(WARNING) << "RDP: authentication warning is about to be shown.";

  // Hook window activation to cancel any modal UI shown by the RDP control.
  // This does not affect creation of other instances of the RDP control on this
  // thread because the RDP control's window is hidden and is not activated.
  window_activate_hook_ = WindowHook::Create();
  return S_OK;
}

HRESULT RdpClientWindow::OnAuthenticationWarningDismissed() {
  LOG(WARNING) << "RDP: authentication warning has been dismissed.";

  window_activate_hook_ = nullptr;
  return S_OK;
}

HRESULT RdpClientWindow::OnConnected() {
  VLOG(1) << "RDP: successfully connected to " << server_endpoint_.ToString();

  NotifyConnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnLoginComplete() {
  VLOG(1) << "RDP: user successfully logged in.";

  user_logged_in_ = true;

  // Set up a timer to periodically apply pending screen size changes to the
  // desktop.  Attempting to set the resolution now seems to fail consistently,
  // but succeeds after a brief timeout.
  if (client_9_) {
    apply_resolution_attempts_ = 0;
    apply_resolution_timer_.Start(
        FROM_HERE, kReapplyResolutionPeriod,
        base::BindRepeating(&RdpClientWindow::ReapplyDesktopResolution,
                            Microsoft::WRL::ComPtr<RdpClientWindow>(this)));
  }

  return S_OK;
}

HRESULT RdpClientWindow::OnDisconnected(long reason) {
  if (reason == kDisconnectReasonNoInfo ||
      reason == kDisconnectReasonLocalNotError ||
      reason == kDisconnectReasonRemoteByUser ||
      reason == kDisconnectReasonByServer) {
    VLOG(1) << "RDP: disconnected from " << server_endpoint_.ToString()
            << ", reason=" << reason;
    NotifyDisconnected();
    return S_OK;
  }

  // Get the extended disconnect reason code.
  mstsc::ExtendedDisconnectReasonCode extended_code;
  HRESULT result = client_->get_ExtendedDisconnectReason(&extended_code);
  if (FAILED(result))
    extended_code = mstsc::exDiscReasonNoInfo;

  // Get the error message as well.
  base::win::ScopedBstr error_message;
  Microsoft::WRL::ComPtr<mstsc::IMsRdpClient5> client5;
  result = client_.CopyTo(client5.GetAddressOf());
  if (SUCCEEDED(result)) {
    result = client5->GetErrorDescription(reason, extended_code,
                                          error_message.Receive());
    if (FAILED(result))
      error_message.Reset();
  }

  LOG(ERROR) << "RDP: disconnected from " << server_endpoint_.ToString() << ": "
             << error_message.Get() << " (reason=" << reason
             << ", extended_code=" << extended_code << ")";

  NotifyDisconnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnFatalError(long error_code) {
  LOG(ERROR) << "RDP: an error occured: error_code="
             << error_code;

  NotifyDisconnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnConfirmClose(VARIANT_BOOL* allow_close) {
  *allow_close = VARIANT_TRUE;

  NotifyDisconnected();
  return S_OK;
}

int RdpClientWindow::LogOnCreateError(HRESULT error) {
  LOG(ERROR) << "RDP: failed to initiate a connection to "
             << server_endpoint_.ToString() << ": error=" << std::hex << error;
  client_.Reset();
  client_9_.Reset();
  client_settings_.Reset();
  return -1;
}

void RdpClientWindow::NotifyConnected() {
  if (event_handler_) {
    event_handler_->OnConnected();
  }
}

void RdpClientWindow::NotifyDisconnected() {
  if (event_handler_) {
    EventHandler* event_handler = event_handler_;
    event_handler_ = nullptr;
    event_handler->OnDisconnected();
  }
}

HRESULT RdpClientWindow::UpdateDesktopResolution() {
  if (!client_9_ || !user_logged_in_) {
    return S_FALSE;
  }

  // UpdateSessionDisplaySettings() is poorly documented in MSDN and has a few
  // quirks that should be noted.
  // 1.) This method will only work when the user is logged into their session.
  // 2.) The method may return E_UNEXPECTED until some amount of time (seconds)
  //     have elapsed after logging in to the user's session.
  return client_9_->UpdateSessionDisplaySettings(
      screen_resolution_.dimensions().width(),
      screen_resolution_.dimensions().height(),
      screen_resolution_.dimensions().width(),
      screen_resolution_.dimensions().height(),
      /*ulOrientation=*/0,
      screen_resolution_.dpi().x(),
      screen_resolution_.dpi().y());
}

void RdpClientWindow::ReapplyDesktopResolution() {
  DCHECK_LT(apply_resolution_attempts_, kMaxResolutionReapplyAttempts);

  HRESULT result = UpdateDesktopResolution();
  apply_resolution_attempts_++;

  if (SUCCEEDED(result)) {
    // Successfully applied the new resolution so stop the retry timer.
    apply_resolution_timer_.Stop();
  } else if (apply_resolution_attempts_ == kMaxResolutionReapplyAttempts) {
    // Only log an error on the last attempt to reduce log spam since a few
    // errors can be expected and don't signal an actual failure.
    LOG(WARNING) << "All UpdateSessionDisplaySettings() retries failed: 0x"
                 << std::hex << result;
    apply_resolution_timer_.Stop();
  }
}

scoped_refptr<RdpClientWindow::WindowHook>
RdpClientWindow::WindowHook::Create() {
  scoped_refptr<WindowHook> window_hook = g_window_hook.Pointer()->Get();

  if (!window_hook.get()) {
    window_hook = new WindowHook();
  }

  return window_hook;
}

RdpClientWindow::WindowHook::WindowHook() : hook_(nullptr) {
  DCHECK(!g_window_hook.Pointer()->Get());

  // Install a window hook to be called on window activation.
  hook_ = SetWindowsHookEx(WH_CBT,
                           &WindowHook::CloseWindowOnActivation,
                           nullptr,
                           GetCurrentThreadId());
  // Without the hook installed, RdpClientWindow will not be able to cancel
  // modal UI windows. This will block the UI message loop so it is better to
  // terminate the process now.
  CHECK(hook_);

  // Let CloseWindowOnActivation() to access the hook handle.
  g_window_hook.Pointer()->Set(this);
}

RdpClientWindow::WindowHook::~WindowHook() {
  DCHECK(g_window_hook.Pointer()->Get() == this);

  g_window_hook.Pointer()->Set(nullptr);

  BOOL result = UnhookWindowsHookEx(hook_);
  DCHECK(result);
}

// static
LRESULT CALLBACK RdpClientWindow::WindowHook::CloseWindowOnActivation(
    int code, WPARAM wparam, LPARAM lparam) {
  // Get the hook handle.
  HHOOK hook = g_window_hook.Pointer()->Get()->hook_;

  if (code != HCBT_ACTIVATE) {
    return CallNextHookEx(hook, code, wparam, lparam);
  }

  // Close the window once all pending window messages are processed.
  HWND window = reinterpret_cast<HWND>(wparam);
  LOG(WARNING) << "RDP: closing a window: " << std::hex << window;
  ::PostMessage(window, WM_CLOSE, 0, 0);
  return 0;
}

}  // namespace remoting
