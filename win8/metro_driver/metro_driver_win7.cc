// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include <corewindow.h>

#include "base/logging.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
int g_window_count = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                         WPARAM wparam, LPARAM lparam) {
  PAINTSTRUCT ps;
  HDC hdc;
  switch (message) {
    case WM_CREATE:
      ++g_window_count;
      break;
    case WM_PAINT:
      hdc = ::BeginPaint(hwnd, &ps);
      ::EndPaint(hwnd, &ps);
      break;
    case WM_LBUTTONUP:
      //  TODO(cpu): Remove this test code.
      ::InvalidateRect(hwnd, NULL, TRUE);
      break;
    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      --g_window_count;
      if (!g_window_count)
        ::PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

HWND CreateMetroTopLevelWindow() {
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(&__ImageBase);
  WNDCLASSEXW wcex;
  wcex.cbSize = sizeof(wcex);
  wcex.style              = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc        = WndProc;
  wcex.cbClsExtra         = 0;
  wcex.cbWndExtra         = 0;
  wcex.hInstance          = hInst;
  wcex.hIcon              = 0;
  wcex.hCursor            = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground      = (HBRUSH)(COLOR_INACTIVECAPTION+1);
  wcex.lpszMenuName       = 0;
  wcex.lpszClassName      = L"Windows.UI.Core.CoreWindow";
  wcex.hIconSm            = 0;

  HWND hwnd = ::CreateWindowExW(0,
                                MAKEINTATOM(::RegisterClassExW(&wcex)),
                                L"metro_win7",
                                WS_POPUP | WS_VISIBLE,
                                0, 0, 1600, 900,
                                NULL, NULL, hInst, NULL);
  return hwnd;
}

typedef winfoundtn::ITypedEventHandler<
    winapp::Core::CoreApplicationView*,
    winapp::Activation::IActivatedEventArgs*> ActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowActivatedEventArgs*> WindowActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::AutomationProviderRequestedEventArgs*>
        AutomationProviderHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::CharacterReceivedEventArgs*> CharEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::CoreWindowEventArgs*> CoreWindowEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::InputEnabledEventArgs*> InputEnabledEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::KeyEventArgs*> KeyEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::PointerEventArgs*> PointerEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowSizeChangedEventArgs*> SizeChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::TouchHitTestingEventArgs*> TouchHitTestHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::VisibilityChangedEventArgs*> VisibilityChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreDispatcher*,
    winui::Core::AcceleratorKeyEventArgs*> AcceleratorKeyEventHandler;

// The following classes are the emulation of the WinRT system as exposed
// to metro applications. There is one application (ICoreApplication) which
// contains a series of Views (ICoreApplicationView) each one of them
// containing a CoreWindow which represents a surface that can drawn to
// and that receives events.
//
// Here is the general dependency hierachy in terms of interfaces:
//
//  IFrameworkViewSource --> IFrameworkView
//          ^                     |
//          |                     |                          metro app
//  ---------------------------------------------------------------------
//          |                     |                         winRT system
//          |                     v
//  ICoreApplication     ICoreApplicationView
//                                |
//                                v
//                          ICoreWindow -----> ICoreWindowInterop
//                                |                  |
//                                |                  |
//                                v                  V
//                         ICoreDispatcher  <==>  real HWND
//
class CoreDispacherEmulation :
    public mswr::RuntimeClass<
        winui::Core::ICoreDispatcher,
        winui::Core::ICoreAcceleratorKeys> {
 public:
  // ICoreDispatcher implementation:
  virtual HRESULT STDMETHODCALLTYPE get_HasThreadAccess(boolean* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE ProcessEvents(
      winui::Core::CoreProcessEventsOption options) {
    // We don't support the other message pump modes. So we basically enter a
    // traditional message loop that we only exit a teardown.
    if (options != winui::Core::CoreProcessEventsOption_ProcessUntilQuit)
      return E_FAIL;

    MSG msg = {0};
    while((::GetMessage(&msg, NULL, 0, 0) != 0) && g_window_count > 0) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    // TODO(cpu): figure what to do with msg.WParam which we would normally
    // return here.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE RunAsync(
      winui::Core::CoreDispatcherPriority priority,
      winui::Core::IDispatchedHandler *agileCallback,
      ABI::Windows::Foundation::IAsyncAction** asyncAction) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE RunIdleAsync(
      winui::Core::IIdleDispatchedHandler *agileCallback,
      winfoundtn::IAsyncAction** asyncAction) {
    return S_OK;
  }

  // ICoreAcceleratorKeys implementation:
  virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyActivated(
      AcceleratorKeyEventHandler* handler,
      EventRegistrationToken *pCookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyActivated(
      EventRegistrationToken cookie) {
    return S_OK;
  }

};

class CoreWindowEmulation
    : public mswr::RuntimeClass<
        mswr::RuntimeClassFlags<mswr::WinRtClassicComMix>,
        winui::Core::ICoreWindow, ICoreWindowInterop> {
 public:
  CoreWindowEmulation() : core_hwnd_(NULL) {
    dispatcher_ = mswr::Make<CoreDispacherEmulation>();
    core_hwnd_ = CreateMetroTopLevelWindow();
  }

  ~CoreWindowEmulation() {
    if (core_hwnd_)
      ::DestroyWindow(core_hwnd_);
  }

  // ICoreWindow implementation:
  virtual HRESULT STDMETHODCALLTYPE get_AutomationHostProvider(
      IInspectable** value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_Bounds(
      winfoundtn::Rect* value) {
    RECT rect;
    if (!::GetClientRect(core_hwnd_, &rect))
      return E_FAIL;
    value->Width = rect.right;
    value->Height = rect.bottom;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_CustomProperties(
      winfoundtn::Collections::IPropertySet** value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_Dispatcher(
      winui::Core::ICoreDispatcher** value) {
    return dispatcher_.CopyTo(value);
  }

  virtual HRESULT STDMETHODCALLTYPE get_FlowDirection(
      winui::Core::CoreWindowFlowDirection* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE put_FlowDirection(
      winui::Core::CoreWindowFlowDirection value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_IsInputEnabled(
      boolean* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE put_IsInputEnabled(
      boolean value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_PointerCursor(
      winui::Core::ICoreCursor** value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE put_PointerCursor(
       winui::Core::ICoreCursor* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_PointerPosition(
      winfoundtn::Point* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_Visible(
      boolean* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE Activate(void) {
    // After we fire OnActivate on the View, Chrome calls us back here.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE Close(void) {
    ::PostMessage(core_hwnd_, WM_CLOSE, 0, 0);
    core_hwnd_ = NULL;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetAsyncKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      winui::Core::CoreVirtualKeyStates* KeyState) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      winui::Core::CoreVirtualKeyStates* KeyState) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE ReleasePointerCapture(void) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE SetPointerCapture(void) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_Activated(
      WindowActivatedHandler* handler,
      EventRegistrationToken* pCookie) {
    // TODO(cpu) implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Activated(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_AutomationProviderRequested(
      AutomationProviderHandler* handler,
      EventRegistrationToken* cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_AutomationProviderRequested(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_CharacterReceived(
      CharEventHandler* handler,
      EventRegistrationToken* pCookie) {
    // TODO(cpu) : implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_CharacterReceived(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_Closed(
      CoreWindowEventHandler* handler,
      EventRegistrationToken* pCookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Closed(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_InputEnabled(
      InputEnabledEventHandler* handler,
      EventRegistrationToken* pCookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_InputEnabled(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_KeyDown(
      KeyEventHandler* handler,
      EventRegistrationToken* pCookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_KeyDown(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_KeyUp(
      KeyEventHandler* handler,
      EventRegistrationToken* pCookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_KeyUp(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerCaptureLost(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerCaptureLost(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerEntered(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerEntered(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerExited(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerExited(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerMoved(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    // TODO(cpu) : implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerMoved(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerPressed(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerPressed(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerReleased(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerReleased(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_TouchHitTesting(
      TouchHitTestHandler* handler,
      EventRegistrationToken* pCookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_TouchHitTesting(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_PointerWheelChanged(
      PointerEventHandler* handler,
      EventRegistrationToken* cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_PointerWheelChanged(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_SizeChanged(
      SizeChangedHandler* handler,
      EventRegistrationToken* pCookie) {
    // TODO(cpu): implement this.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_SizeChanged(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_VisibilityChanged(
      VisibilityChangedHandler* handler,
      EventRegistrationToken* pCookie) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_VisibilityChanged(
      EventRegistrationToken cookie) {
    return S_OK;
  }

  // ICoreWindowInterop implementation:
  virtual HRESULT STDMETHODCALLTYPE get_WindowHandle(HWND* hwnd) {
    if (!core_hwnd_)
      return E_FAIL;
    *hwnd = core_hwnd_;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE put_MessageHandled(
    boolean value) {
    return S_OK;
  }

 private:
   HWND core_hwnd_;
   mswr::ComPtr<winui::Core::ICoreDispatcher> dispatcher_;
};

class ActivatedEvent
    : public mswr::RuntimeClass<winapp::Activation::IActivatedEventArgs> {
 public:
  ActivatedEvent(winapp::Activation::ActivationKind activation_kind)
    : activation_kind_(activation_kind) {
  }

  virtual HRESULT STDMETHODCALLTYPE get_Kind(
    winapp::Activation::ActivationKind *value) {
    *value = activation_kind_;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_PreviousExecutionState(
    winapp::Activation::ApplicationExecutionState *value) {
    *value = winapp::Activation::ApplicationExecutionState_ClosedByUser;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_SplashScreen(
    winapp::Activation::ISplashScreen **value) {
    return E_FAIL;
  }

 private:
  winapp::Activation::ActivationKind activation_kind_;
};

class CoreApplicationViewEmulation
    : public mswr::RuntimeClass<winapp::Core::ICoreApplicationView> {
 public:
   CoreApplicationViewEmulation() {
      core_window_ = mswr::Make<CoreWindowEmulation>();
   }

  HRESULT Activate() {
    if (activated_handler_) {
      auto ae = mswr::Make<ActivatedEvent>(
        winapp::Activation::ActivationKind_File);
      return activated_handler_->Invoke(this, ae.Get());
    } else {
      return S_OK;
    }
  }

  HRESULT Close() {
    return core_window_->Close();
  }

  // ICoreApplicationView implementation:
  virtual HRESULT STDMETHODCALLTYPE get_CoreWindow(
    winui::Core::ICoreWindow** value) {
    if (!core_window_)
      return E_FAIL;
    return core_window_.CopyTo(value);
  }

  virtual HRESULT STDMETHODCALLTYPE add_Activated(
     ActivatedHandler* handler,
    EventRegistrationToken* token) {
    // The real component supports multiple handles but we don't yet.
    if (activated_handler_)
      return E_FAIL;
    activated_handler_ = handler;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Activated(
    EventRegistrationToken token) {
    // Chrome never unregisters handlers, so we don't care about it.
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_IsMain(
    boolean* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_IsHosted(
    boolean* value) {
    return S_OK;
  }

 private:
  mswr::ComPtr<CoreWindowEmulation> core_window_;
  mswr::ComPtr<ActivatedHandler> activated_handler_;
};

class CoreApplicationWin7Emulation
    : public mswr::RuntimeClass<winapp::Core::ICoreApplication,
                                winapp::Core::ICoreApplicationExit> {
 public:
   // ICoreApplication implementation:

  virtual HRESULT STDMETHODCALLTYPE get_Id(
      HSTRING* value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_Suspending(
      winfoundtn::IEventHandler<winapp::SuspendingEventArgs*>* handler,
      EventRegistrationToken* token) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Suspending(
      EventRegistrationToken token) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE add_Resuming(
       winfoundtn::IEventHandler<IInspectable*>* handler,
      EventRegistrationToken* token) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Resuming(
      EventRegistrationToken token) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE get_Properties(
      winfoundtn::Collections::IPropertySet** value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetCurrentView(
      winapp::Core::ICoreApplicationView** value) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE Run(
      winapp::Core::IFrameworkViewSource* viewSource) {
    HRESULT hr = viewSource->CreateView(app_view_.GetAddressOf());
    if (FAILED(hr))
      return hr;
    view_emulation_ = mswr::Make<CoreApplicationViewEmulation>();
    hr = app_view_->Initialize(view_emulation_.Get());
    if (FAILED(hr))
      return hr;
    mswr::ComPtr<winui::Core::ICoreWindow> core_window;
    hr = view_emulation_->get_CoreWindow(core_window.GetAddressOf());
    if (FAILED(hr))
      return hr;
    hr = app_view_->SetWindow(core_window.Get());
    if (FAILED(hr))
      return hr;
    hr = app_view_->Load(NULL);
    if (FAILED(hr))
      return hr;
    hr = view_emulation_->Activate();
    if (FAILED(hr))
      return hr;
    return app_view_->Run();
  }

  virtual HRESULT STDMETHODCALLTYPE RunWithActivationFactories(
      winfoundtn::IGetActivationFactory* activationFactoryCallback) {
    return S_OK;
  }

  // ICoreApplicationExit implementation:

  virtual HRESULT STDMETHODCALLTYPE Exit(void) {
    return view_emulation_->Close();
  }

  virtual HRESULT STDMETHODCALLTYPE add_Exiting(
       winfoundtn::IEventHandler<IInspectable*>* handler,
      EventRegistrationToken* token) {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE remove_Exiting(
      EventRegistrationToken token) {
    return S_OK;
  }

 private:
  mswr::ComPtr<winapp::Core::IFrameworkView> app_view_;
  mswr::ComPtr<CoreApplicationViewEmulation> view_emulation_;
};


mswr::ComPtr<winapp::Core::ICoreApplication> InitWindows7() {
  HRESULT hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr))
    CHECK(false);
  return mswr::Make<CoreApplicationWin7Emulation>();
}

