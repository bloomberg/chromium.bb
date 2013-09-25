// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/stdafx.h"
#include "win8/metro_driver/chrome_app_view_ash.h"

#include <corewindow.h>
#include <shellapi.h>
#include <windows.foundation.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "base/win/metro.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"
#include "ui/events/gestures/gesture_sequence.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "win8/metro_driver/file_picker_ash.h"
#include "win8/metro_driver/metro_driver.h"
#include "win8/metro_driver/winrt_utils.h"
#include "win8/viewer/metro_viewer_constants.h"

typedef winfoundtn::ITypedEventHandler<
    winapp::Core::CoreApplicationView*,
    winapp::Activation::IActivatedEventArgs*> ActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::PointerEventArgs*> PointerEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::KeyEventArgs*> KeyEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreDispatcher*,
    winui::Core::AcceleratorKeyEventArgs*> AcceleratorKeyEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::CharacterReceivedEventArgs*> CharEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::VisibilityChangedEventArgs*> VisibilityChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowActivatedEventArgs*> WindowActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowSizeChangedEventArgs*> SizeChangedHandler;

// This function is exported by chrome.exe.
typedef int (__cdecl *BreakpadExceptionHandler)(EXCEPTION_POINTERS* info);

// Global information used across the metro driver.
struct Globals {
  winapp::Activation::ApplicationExecutionState previous_state;
  winapp::Core::ICoreApplicationExit* app_exit;
  BreakpadExceptionHandler breakpad_exception_handler;
} globals;

namespace {

// TODO(robertshield): Share this with chrome_app_view.cc
void MetroExit() {
  globals.app_exit->Exit();
}

class ChromeChannelListener : public IPC::Listener {
 public:
  ChromeChannelListener(base::MessageLoop* ui_loop, ChromeAppViewAsh* app_view)
      : ui_proxy_(ui_loop->message_loop_proxy()), app_view_(app_view) {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(ChromeChannelListener, message)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_OpenURLOnDesktop,
                          OnOpenURLOnDesktop)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetCursor, OnSetCursor)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_DisplayFileOpen,
                          OnDisplayFileOpenDialog)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_DisplayFileSaveAs,
                          OnDisplayFileSaveAsDialog)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_DisplaySelectFolder,
                          OnDisplayFolderPicker)
      IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetCursorPos, OnSetCursorPos)
      IPC_MESSAGE_UNHANDLED(__debugbreak())
    IPC_END_MESSAGE_MAP()
    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    DVLOG(1) << "Channel error";
    MetroExit();
  }

 private:
  void OnOpenURLOnDesktop(const base::FilePath& shortcut,
                          const string16& url) {
    ui_proxy_->PostTask(FROM_HERE,
        base::Bind(&ChromeAppViewAsh::OnOpenURLOnDesktop,
        base::Unretained(app_view_),
        shortcut, url));
  }

  void OnSetCursor(int64 cursor) {
    ui_proxy_->PostTask(FROM_HERE,
                        base::Bind(&ChromeAppViewAsh::OnSetCursor,
                                   base::Unretained(app_view_),
                                   reinterpret_cast<HCURSOR>(cursor)));
  }

  void OnDisplayFileOpenDialog(const string16& title,
                               const string16& filter,
                               const base::FilePath& default_path,
                               bool allow_multiple_files) {
    ui_proxy_->PostTask(FROM_HERE,
                        base::Bind(&ChromeAppViewAsh::OnDisplayFileOpenDialog,
                                   base::Unretained(app_view_),
                                   title,
                                   filter,
                                   default_path,
                                   allow_multiple_files));
  }

  void OnDisplayFileSaveAsDialog(
    const MetroViewerHostMsg_SaveAsDialogParams& params) {
    ui_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&ChromeAppViewAsh::OnDisplayFileSaveAsDialog,
                   base::Unretained(app_view_),
                   params));
  }

  void OnDisplayFolderPicker(const string16& title) {
    ui_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&ChromeAppViewAsh::OnDisplayFolderPicker,
                   base::Unretained(app_view_),
                   title));
  }

  void OnSetCursorPos(int x, int y) {
    VLOG(1) << "In IPC OnSetCursorPos: " << x << ", " << y;
    ui_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&ChromeAppViewAsh::OnSetCursorPos,
                   base::Unretained(app_view_),
                   x, y));
  }


  scoped_refptr<base::MessageLoopProxy> ui_proxy_;
  ChromeAppViewAsh* app_view_;
};

bool WaitForChromeIPCConnection(const std::string& channel_name) {
  int ms_elapsed = 0;
  while (!IPC::Channel::IsNamedServerInitialized(channel_name) &&
         ms_elapsed < 10000) {
    ms_elapsed += 500;
    Sleep(500);
  }
  return IPC::Channel::IsNamedServerInitialized(channel_name);
}

// This class helps decoding the pointer properties of an event.
class PointerInfoHandler {
 public:
  PointerInfoHandler() :
      x_(0),
      y_(0),
      wheel_delta_(0),
      update_kind_(winui::Input::PointerUpdateKind_Other),
      timestamp_(0),
      pointer_id_(0) {}

  HRESULT Init(winui::Core::IPointerEventArgs* args) {
    HRESULT hr = args->get_CurrentPoint(&pointer_point_);
    if (FAILED(hr))
      return hr;

    winfoundtn::Point point;
    hr = pointer_point_->get_Position(&point);
    if (FAILED(hr))
      return hr;

    mswr::ComPtr<winui::Input::IPointerPointProperties> properties;
    hr = pointer_point_->get_Properties(&properties);
    if (FAILED(hr))
      return hr;

    hr = properties->get_PointerUpdateKind(&update_kind_);
    if (FAILED(hr))
      return hr;

    hr = properties->get_MouseWheelDelta(&wheel_delta_);
    if (FAILED(hr))
      return hr;
    x_ = point.X;
    y_ = point.Y;
    pointer_point_->get_Timestamp(&timestamp_);
    pointer_point_->get_PointerId(&pointer_id_);
    // Map the OS touch event id to a range allowed by the gesture recognizer.
    if (IsTouch())
      pointer_id_ %= ui::GestureSequence::kMaxGesturePoints;
    return S_OK;
  }

  bool IsType(windevs::Input::PointerDeviceType type) const {
    mswr::ComPtr<windevs::Input::IPointerDevice> pointer_device;
    CheckHR(pointer_point_->get_PointerDevice(&pointer_device));
    windevs::Input::PointerDeviceType device_type;
    CheckHR(pointer_device->get_PointerDeviceType(&device_type));
    return  (device_type == type);
  }

  bool IsMouse() const {
    return IsType(windevs::Input::PointerDeviceType_Mouse);
  }

  bool IsTouch() const {
    return IsType(windevs::Input::PointerDeviceType_Touch);
  }

  int32 wheel_delta() const {
    return wheel_delta_;
  }

  ui::EventFlags flags() {
    switch (update_kind_) {
      case winui::Input::PointerUpdateKind_LeftButtonPressed:
        return ui::EF_LEFT_MOUSE_BUTTON;
      case winui::Input::PointerUpdateKind_LeftButtonReleased:
        return ui::EF_LEFT_MOUSE_BUTTON;
      case winui::Input::PointerUpdateKind_RightButtonPressed:
        return ui::EF_RIGHT_MOUSE_BUTTON;
      case winui::Input::PointerUpdateKind_RightButtonReleased:
        return ui::EF_RIGHT_MOUSE_BUTTON;
      case winui::Input::PointerUpdateKind_MiddleButtonPressed:
        return ui::EF_MIDDLE_MOUSE_BUTTON;
      case winui::Input::PointerUpdateKind_MiddleButtonReleased:
        return ui::EF_MIDDLE_MOUSE_BUTTON;
      default:
        return ui::EF_NONE;
    };
  }

  int x() const { return x_; }
  int y() const { return y_; }

  uint32 pointer_id() const {
    return pointer_id_;
  }

  uint64 timestamp() const { return timestamp_; }

 private:
  int x_;
  int y_;
  int wheel_delta_;
  uint32 pointer_id_;
  winui::Input::PointerUpdateKind update_kind_;
  mswr::ComPtr<winui::Input::IPointerPoint> pointer_point_;
  uint64 timestamp_;
};

void RunMessageLoop(winui::Core::ICoreDispatcher* dispatcher) {
  // We're entering a nested message loop, let's allow dispatching
  // tasks while we're in there.
  base::MessageLoop::current()->SetNestableTasksAllowed(true);

  // Enter main core message loop. There are several ways to exit it
  // Nicely:
  // 1 - User action like ALT-F4.
  // 2 - Calling ICoreApplicationExit::Exit().
  // 3-  Posting WM_CLOSE to the core window.
  HRESULT hr = dispatcher->ProcessEvents(
      winui::Core::CoreProcessEventsOption
          ::CoreProcessEventsOption_ProcessUntilQuit);

  // Wind down the thread's chrome message loop.
  base::MessageLoop::current()->Quit();
}

// Helper to return the state of the shift/control/alt keys.
uint32 GetKeyboardEventFlags() {
  uint32 flags = 0;
  if (base::win::IsShiftPressed())
    flags |= ui::EF_SHIFT_DOWN;
  if (base::win::IsCtrlPressed())
    flags |= ui::EF_CONTROL_DOWN;
  if (base::win::IsAltPressed())
    flags |= ui::EF_ALT_DOWN;
  return flags;
}

bool LaunchChromeBrowserProcess (const wchar_t* additional_parameters) {
  DVLOG(1) << "Launching chrome server";
  base::FilePath chrome_exe_path;

  if (!PathService::Get(base::FILE_EXE, &chrome_exe_path))
    return false;

  string16 parameters = L"--silent-launch --viewer-connect ";
  if (additional_parameters)
    parameters += additional_parameters;

  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.nShow = SW_SHOWNORMAL;
  sei.lpFile = chrome_exe_path.value().c_str();
  sei.lpDirectory = L"";
  sei.lpParameters = parameters.c_str();
  ::ShellExecuteEx(&sei);
  return true;
}

}  // namespace

ChromeAppViewAsh::ChromeAppViewAsh()
    : mouse_down_flags_(ui::EF_NONE),
      ui_channel_(nullptr),
      core_window_hwnd_(NULL),
      ui_loop_(base::MessageLoop::TYPE_UI) {
  DVLOG(1) << __FUNCTION__;
  globals.previous_state =
      winapp::Activation::ApplicationExecutionState_NotRunning;
}

ChromeAppViewAsh::~ChromeAppViewAsh() {
  DVLOG(1) << __FUNCTION__;
}

IFACEMETHODIMP
ChromeAppViewAsh::Initialize(winapp::Core::ICoreApplicationView* view) {
  view_ = view;
  DVLOG(1) << __FUNCTION__;
  HRESULT hr = view_->add_Activated(mswr::Callback<ActivatedHandler>(
      this, &ChromeAppViewAsh::OnActivate).Get(),
      &activated_token_);
  CheckHR(hr);
  return hr;
}

IFACEMETHODIMP
ChromeAppViewAsh::SetWindow(winui::Core::ICoreWindow* window) {
  window_ = window;
  DVLOG(1) << __FUNCTION__;

  // Retrieve the native window handle via the interop layer.
  mswr::ComPtr<ICoreWindowInterop> interop;
  HRESULT hr = window->QueryInterface(interop.GetAddressOf());
  CheckHR(hr);
  hr = interop->get_WindowHandle(&core_window_hwnd_);
  CheckHR(hr);

  hr = window_->add_SizeChanged(mswr::Callback<SizeChangedHandler>(
      this, &ChromeAppViewAsh::OnSizeChanged).Get(),
      &sizechange_token_);
  CheckHR(hr);

  // Register for pointer and keyboard notifications. We forward
  // them to the browser process via IPC.
  hr = window_->add_PointerMoved(mswr::Callback<PointerEventHandler>(
      this, &ChromeAppViewAsh::OnPointerMoved).Get(),
      &pointermoved_token_);
  CheckHR(hr);

  hr = window_->add_PointerPressed(mswr::Callback<PointerEventHandler>(
      this, &ChromeAppViewAsh::OnPointerPressed).Get(),
      &pointerpressed_token_);
  CheckHR(hr);

  hr = window_->add_PointerReleased(mswr::Callback<PointerEventHandler>(
      this, &ChromeAppViewAsh::OnPointerReleased).Get(),
      &pointerreleased_token_);
  CheckHR(hr);

  hr = window_->add_KeyDown(mswr::Callback<KeyEventHandler>(
      this, &ChromeAppViewAsh::OnKeyDown).Get(),
      &keydown_token_);
  CheckHR(hr);

  hr = window_->add_KeyUp(mswr::Callback<KeyEventHandler>(
      this, &ChromeAppViewAsh::OnKeyUp).Get(),
      &keyup_token_);
  CheckHR(hr);

  mswr::ComPtr<winui::Core::ICoreDispatcher> dispatcher;
  hr = window_->get_Dispatcher(&dispatcher);
  CheckHR(hr, "Get Dispatcher failed.");

  mswr::ComPtr<winui::Core::ICoreAcceleratorKeys> accelerator_keys;
  hr = dispatcher.CopyTo(__uuidof(winui::Core::ICoreAcceleratorKeys),
                         reinterpret_cast<void**>(
                            accelerator_keys.GetAddressOf()));
  CheckHR(hr, "QI for ICoreAcceleratorKeys failed.");
  hr = accelerator_keys->add_AcceleratorKeyActivated(
      mswr::Callback<AcceleratorKeyEventHandler>(
          this, &ChromeAppViewAsh::OnAcceleratorKeyDown).Get(),
      &accel_keydown_token_);
  CheckHR(hr);

  hr = window_->add_PointerWheelChanged(mswr::Callback<PointerEventHandler>(
      this, &ChromeAppViewAsh::OnWheel).Get(),
      &wheel_token_);
  CheckHR(hr);

  hr = window_->add_CharacterReceived(mswr::Callback<CharEventHandler>(
      this, &ChromeAppViewAsh::OnCharacterReceived).Get(),
      &character_received_token_);
  CheckHR(hr);

  hr = window_->add_VisibilityChanged(mswr::Callback<VisibilityChangedHandler>(
      this, &ChromeAppViewAsh::OnVisibilityChanged).Get(),
      &visibility_changed_token_);
  CheckHR(hr);

  hr = window_->add_Activated(mswr::Callback<WindowActivatedHandler>(
      this, &ChromeAppViewAsh::OnWindowActivated).Get(),
      &window_activated_token_);
  CheckHR(hr);

  // By initializing the direct 3D swap chain with the corewindow
  // we can now directly blit to it from the browser process.
  direct3d_helper_.Initialize(window);
  DVLOG(1) << "Initialized Direct3D.";
  return S_OK;
}

IFACEMETHODIMP
ChromeAppViewAsh::Load(HSTRING entryPoint) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

IFACEMETHODIMP
ChromeAppViewAsh::Run() {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<winui::Core::ICoreDispatcher> dispatcher;
  HRESULT hr = window_->get_Dispatcher(&dispatcher);
  CheckHR(hr, "Dispatcher failed.");

  hr = window_->Activate();
  if (FAILED(hr)) {
    DLOG(WARNING) << "activation failed hr=" << hr;
    return hr;
  }

  // Create the IPC channel IO thread. It needs to out-live the ChannelProxy.
  base::Thread io_thread("metro_IO_thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Start up Chrome and wait for the desired IPC server connection to exist.
  WaitForChromeIPCConnection(win8::kMetroViewerIPCChannelName);

  // In Aura mode we create an IPC channel to the browser, then ask it to
  // connect to us.
  ChromeChannelListener ui_channel_listener(&ui_loop_, this);
  IPC::ChannelProxy ui_channel(win8::kMetroViewerIPCChannelName,
                               IPC::Channel::MODE_NAMED_CLIENT,
                               &ui_channel_listener,
                               io_thread.message_loop_proxy());
  ui_channel_ = &ui_channel;

  // Upon receipt of the MetroViewerHostMsg_SetTargetSurface message the
  // browser will use D3D from the browser process to present to our Window.
  ui_channel_->Send(new MetroViewerHostMsg_SetTargetSurface(
                    gfx::NativeViewId(core_window_hwnd_)));
  DVLOG(1) << "ICoreWindow sent " << core_window_hwnd_;

  // Send an initial size message so that the Ash root window host gets sized
  // correctly.
  RECT rect = {0};
  ::GetWindowRect(core_window_hwnd_, &rect);
  ui_channel_->Send(
      new MetroViewerHostMsg_WindowSizeChanged(rect.right - rect.left,
                                               rect.bottom - rect.top));

  // And post the task that'll do the inner Metro message pumping to it.
  ui_loop_.PostTask(FROM_HERE, base::Bind(&RunMessageLoop, dispatcher.Get()));
  ui_loop_.Run();

  DVLOG(0) << "ProcessEvents done, hr=" << hr;
  return hr;
}

IFACEMETHODIMP
ChromeAppViewAsh::Uninitialize() {
  DVLOG(1) << __FUNCTION__;
  window_ = nullptr;
  view_ = nullptr;
  core_window_hwnd_ = NULL;
  return S_OK;
}

// static
HRESULT ChromeAppViewAsh::Unsnap() {
  mswr::ComPtr<winui::ViewManagement::IApplicationViewStatics> view_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ViewManagement_ApplicationView,
      view_statics.GetAddressOf());
  CheckHR(hr);

  winui::ViewManagement::ApplicationViewState state =
      winui::ViewManagement::ApplicationViewState_FullScreenLandscape;
  hr = view_statics->get_Value(&state);
  CheckHR(hr);

  if (state == winui::ViewManagement::ApplicationViewState_Snapped) {
    boolean success = FALSE;
    hr = view_statics->TryUnsnap(&success);

    if (FAILED(hr) || !success) {
      LOG(ERROR) << "Failed to unsnap. Error 0x" << hr;
      if (SUCCEEDED(hr))
        hr = E_UNEXPECTED;
    }
  }
  return hr;
}


void ChromeAppViewAsh::OnOpenURLOnDesktop(const base::FilePath& shortcut,
    const string16& url) {
  base::FilePath::StringType file = shortcut.value();
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_LOG_USAGE;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpFile = file.c_str();
  sei.lpDirectory = L"";
  sei.lpParameters = url.c_str();
  BOOL result = ShellExecuteEx(&sei);
}

void ChromeAppViewAsh::OnSetCursor(HCURSOR cursor) {
  ::SetCursor(HCURSOR(cursor));
}

void ChromeAppViewAsh::OnDisplayFileOpenDialog(
    const string16& title,
    const string16& filter,
    const base::FilePath& default_path,
    bool allow_multiple_files) {
  DVLOG(1) << __FUNCTION__;

  // The OpenFilePickerSession instance is deleted when we receive a
  // callback from the OpenFilePickerSession class about the completion of the
  // operation.
  FilePickerSessionBase* file_picker_ =
      new OpenFilePickerSession(this,
                                title,
                                filter,
                                default_path,
                                allow_multiple_files);
  file_picker_->Run();
}

void ChromeAppViewAsh::OnDisplayFileSaveAsDialog(
    const MetroViewerHostMsg_SaveAsDialogParams& params) {
  DVLOG(1) << __FUNCTION__;

  // The SaveFilePickerSession instance is deleted when we receive a
  // callback from the SaveFilePickerSession class about the completion of the
  // operation.
  FilePickerSessionBase* file_picker_ =
      new SaveFilePickerSession(this, params);
  file_picker_->Run();
}

void ChromeAppViewAsh::OnDisplayFolderPicker(const string16& title) {
  DVLOG(1) << __FUNCTION__;
  // The FolderPickerSession instance is deleted when we receive a
  // callback from the FolderPickerSession class about the completion of the
  // operation.
  FilePickerSessionBase* file_picker_ = new FolderPickerSession(this, title);
  file_picker_->Run();
}

void ChromeAppViewAsh::OnSetCursorPos(int x, int y) {
  if (ui_channel_) {
    ::SetCursorPos(x, y);
    DVLOG(1) << "In UI OnSetCursorPos: " << x << ", " << y;
    ui_channel_->Send(new MetroViewerHostMsg_SetCursorPosAck());
    // Generate a fake mouse move which matches the SetCursor coordinates as
    // the browser expects to receive a mouse move for these coordinates.
    // It is not clear why we don't receive a real mouse move in response to
    // the SetCursorPos calll above.
    ui_channel_->Send(new MetroViewerHostMsg_MouseMoved(x, y, 0));
  }
}

void ChromeAppViewAsh::OnOpenFileCompleted(
    OpenFilePickerSession* open_file_picker,
    bool success) {
  DVLOG(1) << __FUNCTION__;
  DVLOG(1) << "Success: " << success;
  if (ui_channel_) {
    if (open_file_picker->allow_multi_select()) {
      ui_channel_->Send(new MetroViewerHostMsg_MultiFileOpenDone(
          success, open_file_picker->filenames()));
    } else {
      ui_channel_->Send(new MetroViewerHostMsg_FileOpenDone(
          success, base::FilePath(open_file_picker->result())));
    }
  }
  delete open_file_picker;
}

void ChromeAppViewAsh::OnSaveFileCompleted(
    SaveFilePickerSession* save_file_picker,
    bool success) {
  DVLOG(1) << __FUNCTION__;
  DVLOG(1) << "Success: " << success;
  if (ui_channel_) {
    ui_channel_->Send(new MetroViewerHostMsg_FileSaveAsDone(
        success,
        base::FilePath(save_file_picker->result()),
        save_file_picker->filter_index()));
  }
  delete save_file_picker;
}

void ChromeAppViewAsh::OnFolderPickerCompleted(
    FolderPickerSession* folder_picker,
    bool success) {
  DVLOG(1) << __FUNCTION__;
  DVLOG(1) << "Success: " << success;
  if (ui_channel_) {
    ui_channel_->Send(new MetroViewerHostMsg_SelectFolderDone(
        success,
        base::FilePath(folder_picker->result())));
  }
  delete folder_picker;
}

HRESULT ChromeAppViewAsh::OnActivate(
    winapp::Core::ICoreApplicationView*,
    winapp::Activation::IActivatedEventArgs* args) {
  DVLOG(1) << __FUNCTION__;
  // Note: If doing more work in this function, you migth need to call
  // get_PreviousExecutionState() and skip the work if  the result is
  // ApplicationExecutionState_Running and globals.previous_state is too.
  args->get_PreviousExecutionState(&globals.previous_state);
  DVLOG(1) << "Previous Execution State: " << globals.previous_state;

  winapp::Activation::ActivationKind activation_kind;
  CheckHR(args->get_Kind(&activation_kind));
  DVLOG(1) << "Activation kind: " << activation_kind;

  if (activation_kind == winapp::Activation::ActivationKind_Search)
    HandleSearchRequest(args);
  else if (activation_kind == winapp::Activation::ActivationKind_Protocol)
    HandleProtocolRequest(args);
  else
    LaunchChromeBrowserProcess(NULL);
  // We call ICoreWindow::Activate after the handling for the search/protocol
  // requests because Chrome can be launched to handle a search request which
  // in turn launches the chrome browser process in desktop mode via
  // ShellExecute. If we call ICoreWindow::Activate before this, then
  // Windows kills the metro chrome process when it calls ShellExecute. Seems
  // to be a bug.
  window_->Activate();
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnPointerMoved(winui::Core::ICoreWindow* sender,
                                         winui::Core::IPointerEventArgs* args) {
  PointerInfoHandler pointer;
  HRESULT hr = pointer.Init(args);
  if (FAILED(hr))
    return hr;

  if (pointer.IsMouse()) {
    ui_channel_->Send(new MetroViewerHostMsg_MouseMoved(
        pointer.x(),
        pointer.y(),
        mouse_down_flags_ | GetKeyboardEventFlags()));
  } else {
    DCHECK(pointer.IsTouch());
    ui_channel_->Send(new MetroViewerHostMsg_TouchMoved(pointer.x(),
                                                        pointer.y(),
                                                        pointer.timestamp(),
                                                        pointer.pointer_id()));
  }
  return S_OK;
}

// NOTE: From experimentation, it seems like Metro only sends a PointerPressed
// event for the first button pressed and the last button released in a sequence
// of mouse events.
// For example, a sequence of LEFT_DOWN, RIGHT_DOWN, LEFT_UP, RIGHT_UP results
// only in PointerPressed(LEFT)/PointerReleased(RIGHT) events.
HRESULT ChromeAppViewAsh::OnPointerPressed(
    winui::Core::ICoreWindow* sender,
    winui::Core::IPointerEventArgs* args) {
  PointerInfoHandler pointer;
  HRESULT hr = pointer.Init(args);
  if (FAILED(hr))
    return hr;

  if (pointer.IsMouse()) {
    mouse_down_flags_ = pointer.flags();
    ui_channel_->Send(new MetroViewerHostMsg_MouseButton(
        pointer.x(),
        pointer.y(),
        0,
        ui::ET_MOUSE_PRESSED,
        static_cast<ui::EventFlags>(
            mouse_down_flags_ | GetKeyboardEventFlags())));
  } else {
    DCHECK(pointer.IsTouch());
    ui_channel_->Send(new MetroViewerHostMsg_TouchDown(pointer.x(),
                                                       pointer.y(),
                                                       pointer.timestamp(),
                                                       pointer.pointer_id()));
  }
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnPointerReleased(
    winui::Core::ICoreWindow* sender,
    winui::Core::IPointerEventArgs* args) {
  PointerInfoHandler pointer;
  HRESULT hr = pointer.Init(args);
  if (FAILED(hr))
    return hr;

  if (pointer.IsMouse()) {
    mouse_down_flags_ = ui::EF_NONE;
    ui_channel_->Send(new MetroViewerHostMsg_MouseButton(
        pointer.x(),
        pointer.y(),
        0,
        ui::ET_MOUSE_RELEASED,
        static_cast<ui::EventFlags>(
            pointer.flags() | GetKeyboardEventFlags())));
  } else {
    DCHECK(pointer.IsTouch());
    ui_channel_->Send(new MetroViewerHostMsg_TouchUp(pointer.x(),
                                                     pointer.y(),
                                                     pointer.timestamp(),
                                                     pointer.pointer_id()));
  }
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnWheel(
    winui::Core::ICoreWindow* sender,
    winui::Core::IPointerEventArgs* args) {
  PointerInfoHandler pointer;
  HRESULT hr = pointer.Init(args);
  if (FAILED(hr))
    return hr;
  DCHECK(pointer.IsMouse());
  ui_channel_->Send(new MetroViewerHostMsg_MouseButton(pointer.x(), pointer.y(),
                                                       pointer.wheel_delta(),
                                                       ui::ET_MOUSEWHEEL,
                                                       ui::EF_NONE));
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnKeyDown(
    winui::Core::ICoreWindow* sender,
    winui::Core::IKeyEventArgs* args) {
  winsys::VirtualKey virtual_key;
  HRESULT hr = args->get_VirtualKey(&virtual_key);
  if (FAILED(hr))
    return hr;
  winui::Core::CorePhysicalKeyStatus status;
  hr = args->get_KeyStatus(&status);
  if (FAILED(hr))
    return hr;

  ui_channel_->Send(new MetroViewerHostMsg_KeyDown(virtual_key,
                                                   status.RepeatCount,
                                                   status.ScanCode,
                                                   GetKeyboardEventFlags()));
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnKeyUp(
    winui::Core::ICoreWindow* sender,
    winui::Core::IKeyEventArgs* args) {
  winsys::VirtualKey virtual_key;
  HRESULT hr = args->get_VirtualKey(&virtual_key);
  if (FAILED(hr))
    return hr;
  winui::Core::CorePhysicalKeyStatus status;
  hr = args->get_KeyStatus(&status);
  if (FAILED(hr))
    return hr;

  ui_channel_->Send(new MetroViewerHostMsg_KeyUp(virtual_key,
                                                 status.RepeatCount,
                                                 status.ScanCode,
                                                 GetKeyboardEventFlags()));
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnAcceleratorKeyDown(
    winui::Core::ICoreDispatcher* sender,
    winui::Core::IAcceleratorKeyEventArgs* args) {
  winsys::VirtualKey virtual_key;
  HRESULT hr = args->get_VirtualKey(&virtual_key);
  if (FAILED(hr))
    return hr;
  winui::Core::CorePhysicalKeyStatus status;
  hr = args->get_KeyStatus(&status);
  if (FAILED(hr))
    return hr;

  winui::Core::CoreAcceleratorKeyEventType event_type;
  hr = args->get_EventType(&event_type);
  if (FAILED(hr))
    return hr;

  uint32 keyboard_flags = GetKeyboardEventFlags();

  switch (event_type) {
    case winui::Core::CoreAcceleratorKeyEventType_SystemCharacter:
      ui_channel_->Send(new MetroViewerHostMsg_Character(virtual_key,
                                                         status.RepeatCount,
                                                         status.ScanCode,
                                                         keyboard_flags));
      break;

    case winui::Core::CoreAcceleratorKeyEventType_SystemKeyDown:
      ui_channel_->Send(new MetroViewerHostMsg_KeyDown(virtual_key,
                                                       status.RepeatCount,
                                                       status.ScanCode,
                                                       keyboard_flags));
      break;

    case winui::Core::CoreAcceleratorKeyEventType_SystemKeyUp:
      ui_channel_->Send(new MetroViewerHostMsg_KeyUp(virtual_key,
                                                     status.RepeatCount,
                                                     status.ScanCode,
                                                     keyboard_flags));
      break;

    default:
      break;
  }
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnCharacterReceived(
  winui::Core::ICoreWindow* sender,
  winui::Core::ICharacterReceivedEventArgs* args) {
  unsigned int char_code = 0;
  HRESULT hr = args->get_KeyCode(&char_code);
  if (FAILED(hr))
    return hr;

  winui::Core::CorePhysicalKeyStatus status;
  hr = args->get_KeyStatus(&status);
  if (FAILED(hr))
    return hr;

  ui_channel_->Send(new MetroViewerHostMsg_Character(char_code,
                                                     status.RepeatCount,
                                                     status.ScanCode,
                                                     GetKeyboardEventFlags()));
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnVisibilityChanged(
    winui::Core::ICoreWindow* sender,
    winui::Core::IVisibilityChangedEventArgs* args) {
  boolean visible = false;
  HRESULT hr = args->get_Visible(&visible);
  if (FAILED(hr))
    return hr;

  ui_channel_->Send(new MetroViewerHostMsg_VisibilityChanged(!!visible));
  return S_OK;
}

HRESULT ChromeAppViewAsh::OnWindowActivated(
    winui::Core::ICoreWindow* sender,
    winui::Core::IWindowActivatedEventArgs* args) {
  winui::Core::CoreWindowActivationState state;
  HRESULT hr = args->get_WindowActivationState(&state);
  if (FAILED(hr))
    return hr;
  return S_OK;
}

HRESULT ChromeAppViewAsh::HandleSearchRequest(
    winapp::Activation::IActivatedEventArgs* args) {
  mswr::ComPtr<winapp::Activation::ISearchActivatedEventArgs> search_args;
  CheckHR(args->QueryInterface(
          winapp::Activation::IID_ISearchActivatedEventArgs, &search_args));

  if (!ui_channel_) {
    DVLOG(1) << "Launched to handle search request";
    LaunchChromeBrowserProcess(L"--windows8-search");
  }

  mswrw::HString search_string;
  CheckHR(search_args->get_QueryText(search_string.GetAddressOf()));
  string16 search_text(MakeStdWString(search_string.Get()));

  ui_loop_.PostTask(FROM_HERE,
                    base::Bind(&ChromeAppViewAsh::OnSearchRequest,
                    base::Unretained(this),
                    search_text));
  return S_OK;
}

HRESULT ChromeAppViewAsh::HandleProtocolRequest(
    winapp::Activation::IActivatedEventArgs* args) {
  DVLOG(1) << __FUNCTION__;
  if (!ui_channel_)
    DVLOG(1) << "Launched to handle url request";

  mswr::ComPtr<winapp::Activation::IProtocolActivatedEventArgs>
      protocol_args;
  CheckHR(args->QueryInterface(
          winapp::Activation::IID_IProtocolActivatedEventArgs,
          &protocol_args));

  mswr::ComPtr<winfoundtn::IUriRuntimeClass> uri;
  protocol_args->get_Uri(&uri);
  mswrw::HString url;
  uri->get_AbsoluteUri(url.GetAddressOf());
  string16 actual_url(MakeStdWString(url.Get()));
  DVLOG(1) << "Received url request: " << actual_url;

  ui_loop_.PostTask(FROM_HERE,
                    base::Bind(&ChromeAppViewAsh::OnNavigateToUrl,
                               base::Unretained(this),
                               actual_url));
  return S_OK;
}

void ChromeAppViewAsh::OnSearchRequest(const string16& search_string) {
  DCHECK(ui_channel_);
  ui_channel_->Send(new MetroViewerHostMsg_SearchRequest(search_string));
}

void ChromeAppViewAsh::OnNavigateToUrl(const string16& url) {
  DCHECK(ui_channel_);
 ui_channel_->Send(new MetroViewerHostMsg_OpenURL(url));
}

HRESULT ChromeAppViewAsh::OnSizeChanged(winui::Core::ICoreWindow* sender,
    winui::Core::IWindowSizeChangedEventArgs* args) {
  if (!window_) {
    return S_OK;
  }

  winfoundtn::Size size;
  HRESULT hr = args->get_Size(&size);
  if (FAILED(hr))
    return hr;

  uint32 cx = static_cast<uint32>(size.Width);
  uint32 cy = static_cast<uint32>(size.Height);

  DVLOG(1) << "Window size changed: width=" << cx << ", height=" << cy;
  ui_channel_->Send(new MetroViewerHostMsg_WindowSizeChanged(cx, cy));
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

ChromeAppViewFactory::ChromeAppViewFactory(
    winapp::Core::ICoreApplication* icore_app,
    LPTHREAD_START_ROUTINE host_main,
    void* host_context) {
  mswr::ComPtr<winapp::Core::ICoreApplication> core_app(icore_app);
  mswr::ComPtr<winapp::Core::ICoreApplicationExit> app_exit;
  CheckHR(core_app.As(&app_exit));
  globals.app_exit = app_exit.Detach();
}

IFACEMETHODIMP
ChromeAppViewFactory::CreateView(winapp::Core::IFrameworkView** view) {
  *view = mswr::Make<ChromeAppViewAsh>().Detach();
  return (*view) ? S_OK :  E_OUTOFMEMORY;
}
