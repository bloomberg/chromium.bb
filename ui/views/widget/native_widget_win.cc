// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_win.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>

#include "base/bind.h"
#include "base/string_util.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/base/event.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/theme_provider.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_paint.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/accessibility/native_view_accessibility_win.h"
#include "ui/views/controls/native_control_win.h"
#include "ui/views/controls/textfield/native_textfield_views.h"
#include "ui/views/drag_utils.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/ime/input_method_win.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/aero_tooltip_manager.h"
#include "ui/views/widget/child_window_message_processor.h"
#include "ui/views/widget/drop_target_win.h"
#include "ui/views/widget/monitor_win.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/views/window/native_frame_view.h"

#if !defined(USE_AURA)
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#endif

#pragma comment(lib, "dwmapi.lib")

using ui::ViewProp;

namespace views {

namespace {

// Get the source HWND of the specified message. Depending on the message, the
// source HWND is encoded in either the WPARAM or the LPARAM value.
HWND GetControlHWNDForMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  // Each of the following messages can be sent by a child HWND and must be
  // forwarded to its associated NativeControlWin for handling.
  switch (message) {
    case WM_NOTIFY:
      return reinterpret_cast<NMHDR*>(l_param)->hwndFrom;
    case WM_COMMAND:
      return reinterpret_cast<HWND>(l_param);
    case WM_CONTEXTMENU:
      return reinterpret_cast<HWND>(w_param);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
      return reinterpret_cast<HWND>(l_param);
  }
  return NULL;
}

// Some messages may be sent to us by a child HWND. If this is the case, this
// function will forward those messages on to the object associated with the
// source HWND and return true, in which case the window procedure must not do
// any further processing of the message. If there is no associated
// ChildWindowMessageProcessor, the return value will be false and the WndProc
// can continue processing the message normally.  |l_result| contains the result
// of the message processing by the control and must be returned by the WndProc
// if the return value is true.
bool ProcessChildWindowMessage(UINT message,
                               WPARAM w_param,
                               LPARAM l_param,
                               LRESULT* l_result) {
  *l_result = 0;

  HWND control_hwnd = GetControlHWNDForMessage(message, w_param, l_param);
  if (IsWindow(control_hwnd)) {
    ChildWindowMessageProcessor* processor =
        ChildWindowMessageProcessor::Get(control_hwnd);
    if (processor)
      return processor->ProcessMessage(message, w_param, l_param, l_result);
  }

  return false;
}

// Enumeration callback for NativeWidget::GetAllChildWidgets(). Called for each
// child HWND beneath the original HWND.
BOOL CALLBACK EnumerateChildWindowsForNativeWidgets(HWND hwnd, LPARAM l_param) {
  Widget* widget = Widget::GetWidgetForNativeView(hwnd);
  if (widget) {
    Widget::Widgets* widgets = reinterpret_cast<Widget::Widgets*>(l_param);
    widgets->insert(widget);
  }
  return TRUE;
}

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

// Links the HWND to its NativeWidget.
const char* const kNativeWidgetKey = "__VIEWS_NATIVE_WIDGET__";

const int kDragFrameWindowAlpha = 200;

}  // namespace

// static
bool NativeWidgetWin::screen_reader_active_ = false;

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, public:

NativeWidgetWin::NativeWidgetWin(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      restore_focus_when_enabled_(false),
      accessibility_view_events_index_(-1),
      accessibility_view_events_(kMaxAccessibilityViewEvents),
      restored_enabled_(false),
      has_non_client_view_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          message_handler_(new HWNDMessageHandler(this))) {
}

NativeWidgetWin::~NativeWidgetWin() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

// static
gfx::Font NativeWidgetWin::GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

void NativeWidgetWin::Show(int show_state) {
  ShowWindow(show_state);
  // When launched from certain programs like bash and Windows Live Messenger,
  // show_state is set to SW_HIDE, so we need to correct that condition. We
  // don't just change show_state to SW_SHOWNORMAL because MSDN says we must
  // always first call ShowWindow with the specified value from STARTUPINFO,
  // otherwise all future ShowWindow calls will be ignored (!!#@@#!). Instead,
  // we call ShowWindow again in this case.
  if (show_state == SW_HIDE) {
    show_state = SW_SHOWNORMAL;
    ShowWindow(show_state);
  }

  // We need to explicitly activate the window if we've been shown with a state
  // that should activate, because if we're opened from a desktop shortcut while
  // an existing window is already running it doesn't seem to be enough to use
  // one of these flags to activate the window.
  if (show_state == SW_SHOWNORMAL || show_state == SW_SHOWMAXIMIZED)
    Activate();

  SetInitialFocus();
}

View* NativeWidgetWin::GetAccessibilityViewEventAt(int id) {
  // Convert from MSAA child id.
  id = -(id + 1);
  DCHECK(id >= 0 && id < kMaxAccessibilityViewEvents);
  return accessibility_view_events_[id];
}

int NativeWidgetWin::AddAccessibilityViewEvent(View* view) {
  accessibility_view_events_index_ =
      (accessibility_view_events_index_ + 1) % kMaxAccessibilityViewEvents;
  accessibility_view_events_[accessibility_view_events_index_] = view;

  // Convert to MSAA child id.
  return -(accessibility_view_events_index_ + 1);
}

void NativeWidgetWin::ClearAccessibilityViewEvent(View* view) {
  for (std::vector<View*>::iterator it = accessibility_view_events_.begin();
      it != accessibility_view_events_.end();
      ++it) {
    if (*it == view)
      *it = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, CompositorDelegate implementation:

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, NativeWidget implementation:

void NativeWidgetWin::InitNativeWidget(const Widget::InitParams& params) {
  SetInitParams(params);

  message_handler_->Init(params.bounds);

  // Create the window.
  WindowImpl::Init(params.GetParent(), params.bounds);
}

NonClientFrameView* NativeWidgetWin::CreateNonClientFrameView() {
  return GetWidget()->ShouldUseNativeFrame() ?
      new NativeFrameView(GetWidget()) : NULL;
}

void NativeWidgetWin::UpdateFrameAfterFrameChange() {
  // We've either gained or lost a custom window region, so reset it now.
  message_handler_->ResetWindowRegion(true);
}

bool NativeWidgetWin::ShouldUseNativeFrame() const {
  return ui::win::IsAeroGlassEnabled();
}

void NativeWidgetWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
}

Widget* NativeWidgetWin::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetWin::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetWin::GetNativeView() const {
  return WindowImpl::hwnd();
}

gfx::NativeWindow NativeWidgetWin::GetNativeWindow() const {
  return WindowImpl::hwnd();
}

Widget* NativeWidgetWin::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetWin::GetCompositor() const {
  return NULL;
}

ui::Compositor* NativeWidgetWin::GetCompositor() {
  return NULL;
}

void NativeWidgetWin::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
}

void NativeWidgetWin::ViewRemoved(View* view) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(view);

  ClearAccessibilityViewEvent(view);
}

void NativeWidgetWin::SetNativeWindowProperty(const char* name, void* value) {
  // Remove the existing property (if any).
  for (ViewProps::iterator i = props_.begin(); i != props_.end(); ++i) {
    if ((*i)->Key() == name) {
      props_.erase(i);
      break;
    }
  }

  if (value)
    props_.push_back(new ViewProp(hwnd(), name, value));
}

void* NativeWidgetWin::GetNativeWindowProperty(const char* name) const {
  return ViewProp::GetValue(hwnd(), name);
}

TooltipManager* NativeWidgetWin::GetTooltipManager() const {
  return tooltip_manager_.get();
}

bool NativeWidgetWin::IsScreenReaderActive() const {
  return screen_reader_active_;
}

void NativeWidgetWin::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  int child_id = AddAccessibilityViewEvent(view);
  message_handler_->SendNativeAccessibilityEvent(child_id, event_type);
}

void NativeWidgetWin::SetCapture() {
  message_handler_->SetCapture();
}

void NativeWidgetWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

bool NativeWidgetWin::HasCapture() const {
  return message_handler_->HasCapture();
}

InputMethod* NativeWidgetWin::CreateInputMethod() {
  return message_handler_->CreateInputMethod();
}

internal::InputMethodDelegate* NativeWidgetWin::GetInputMethodDelegate() {
  return message_handler_.get();
}

void NativeWidgetWin::CenterWindow(const gfx::Size& size) {
  message_handler_->CenterWindow(size);
}

void NativeWidgetWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  message_handler_->GetWindowPlacement(bounds, show_state);
}

void NativeWidgetWin::SetWindowTitle(const string16& title) {
  SetWindowText(GetNativeView(), title.c_str());
  SetAccessibleName(title);
}

void NativeWidgetWin::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  if (!window_icon.isNull()) {
    HICON windows_icon = IconUtil::CreateHICONFromSkBitmap(window_icon);
    // We need to make sure to destroy the previous icon, otherwise we'll leak
    // these GDI objects until we crash!
    HICON old_icon = reinterpret_cast<HICON>(
        SendMessage(GetNativeView(), WM_SETICON, ICON_SMALL,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
  }
  if (!app_icon.isNull()) {
    HICON windows_icon = IconUtil::CreateHICONFromSkBitmap(app_icon);
    HICON old_icon = reinterpret_cast<HICON>(
        SendMessage(GetNativeView(), WM_SETICON, ICON_BIG,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
  }
}

void NativeWidgetWin::SetAccessibleName(const string16& name) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr))
    hr = pAccPropServices->SetHwndPropStr(GetNativeView(), OBJID_CLIENT,
                                          CHILDID_SELF, PROPID_ACC_NAME,
                                          name.c_str());
}

void NativeWidgetWin::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    if (role) {
      var.vt = VT_I4;
      var.lVal = NativeViewAccessibilityWin::MSAARole(role);
      hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                         CHILDID_SELF, PROPID_ACC_ROLE, var);
    }
  }
}

void NativeWidgetWin::SetAccessibleState(ui::AccessibilityTypes::State state) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    if (state) {
      var.vt = VT_I4;
      var.lVal = NativeViewAccessibilityWin::MSAAState(state);
      hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                         CHILDID_SELF, PROPID_ACC_STATE, var);
    }
  }
}

void NativeWidgetWin::InitModalType(ui::ModalType modal_type) {
  message_handler_->InitModalType(modal_type);
}

gfx::Rect NativeWidgetWin::GetWindowBoundsInScreen() const {
  return message_handler_->GetWindowBoundsInScreen();
}

gfx::Rect NativeWidgetWin::GetClientAreaBoundsInScreen() const {
  return message_handler_->GetClientAreaBoundsInScreen();
}

gfx::Rect NativeWidgetWin::GetRestoredBounds() const {
  return message_handler_->GetRestoredBounds();
}

void NativeWidgetWin::SetBounds(const gfx::Rect& bounds) {
  message_handler_->SetBounds(bounds);
}

void NativeWidgetWin::SetSize(const gfx::Size& size) {
  message_handler_->SetSize(size);
}

void NativeWidgetWin::StackAbove(gfx::NativeView native_view) {
  message_handler_->StackAbove(native_view);
}

void NativeWidgetWin::StackAtTop() {
  message_handler_->StackAtTop();
}

void NativeWidgetWin::StackBelow(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetWin::SetShape(gfx::NativeRegion region) {
  message_handler_->SetRegion(region);
}

void NativeWidgetWin::Close() {
  if (!IsWindow())
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  // Modal dialog windows disable their owner windows; re-enable them now so
  // they can activate as foreground windows upon this window's destruction.
  RestoreEnabledIfNecessary();

  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetWin::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetWin::CloseNow() {
  message_handler_->CloseNow();
}

void NativeWidgetWin::Show() {
  if (!IsWindow())
    return;

  ShowWindow(SW_SHOWNOACTIVATE);
  SetInitialFocus();
}

void NativeWidgetWin::Hide() {
  message_handler_->Hide();
}

void NativeWidgetWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  message_handler_->ShowMaximizedWithBounds(restored_bounds);
}

void NativeWidgetWin::ShowWithWindowState(ui::WindowShowState show_state) {
  DWORD native_show_state;
  switch (show_state) {
    case ui::SHOW_STATE_INACTIVE:
      native_show_state = SW_SHOWNOACTIVATE;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      native_show_state = SW_SHOWMAXIMIZED;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      native_show_state = SW_SHOWMINIMIZED;
      break;
    default:
      native_show_state = GetShowState();
      break;
  }
  Show(native_show_state);
}

bool NativeWidgetWin::IsVisible() const {
  return message_handler_->IsVisible();
}

void NativeWidgetWin::Activate() {
  message_handler_->Activate();
}

void NativeWidgetWin::Deactivate() {
  message_handler_->Deactivate();
}

bool NativeWidgetWin::IsActive() const {
  return message_handler_->IsActive();
}

void NativeWidgetWin::SetAlwaysOnTop(bool on_top) {
  message_handler_->SetAlwaysOnTop(on_top);
}

void NativeWidgetWin::Maximize() {
  message_handler_->Maximize();
}

void NativeWidgetWin::Minimize() {
  message_handler_->Minimize();
}

bool NativeWidgetWin::IsMaximized() const {
  return message_handler_->IsMaximized();
}

bool NativeWidgetWin::IsMinimized() const {
  return message_handler_->IsMinimized();
}

void NativeWidgetWin::Restore() {
  message_handler_->Restore();
}

void NativeWidgetWin::SetFullscreen(bool fullscreen) {
  message_handler_->fullscreen_handler()->SetFullscreen(fullscreen);
}

void NativeWidgetWin::SetMetroSnapFullscreen(bool metro_snap) {
  message_handler_->fullscreen_handler()->SetMetroSnap(metro_snap);
}

bool NativeWidgetWin::IsFullscreen() const {
  return message_handler_->fullscreen_handler()->fullscreen();
}

bool NativeWidgetWin::IsInMetroSnapMode() const {
  return message_handler_->fullscreen_handler()->metro_snap();
}

void NativeWidgetWin::SetCanUpdateLayeredWindow(bool can_update) {
  message_handler_->set_can_update_layered_window(can_update);
}

void NativeWidgetWin::SetOpacity(unsigned char opacity) {
  message_handler_->SetOpacity(static_cast<BYTE>(opacity));
  GetWidget()->GetRootView()->SchedulePaint();
}

void NativeWidgetWin::SetUseDragFrame(bool use_drag_frame) {
  if (use_drag_frame) {
    // Make the frame slightly transparent during the drag operation.
    drag_frame_saved_window_style_ = GetWindowLong(GWL_STYLE);
    drag_frame_saved_window_ex_style_ = GetWindowLong(GWL_EXSTYLE);
    SetWindowLong(GWL_EXSTYLE,
                  drag_frame_saved_window_ex_style_ | WS_EX_LAYERED);
    // Remove the captions tyle so the window doesn't have window controls for a
    // more "transparent" look.
    SetWindowLong(GWL_STYLE, drag_frame_saved_window_style_ & ~WS_CAPTION);
    SetLayeredWindowAttributes(GetNativeWindow(), RGB(0xFF, 0xFF, 0xFF),
                               kDragFrameWindowAlpha, LWA_ALPHA);
  } else {
    SetWindowLong(GWL_STYLE, drag_frame_saved_window_style_);
    SetWindowLong(GWL_EXSTYLE, drag_frame_saved_window_ex_style_);
  }
}

void NativeWidgetWin::FlashFrame(bool flash) {
  message_handler_->FlashFrame(flash);
}

bool NativeWidgetWin::IsAccessibleWidget() const {
  return screen_reader_active_;
}

void NativeWidgetWin::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int operation) {
  views::RunShellDrag(NULL, data, location, operation);
}

void NativeWidgetWin::SchedulePaintInRect(const gfx::Rect& rect) {
  message_handler_->SchedulePaintInRect(rect);
}

void NativeWidgetWin::SetCursor(gfx::NativeCursor cursor) {
  message_handler_->SetCursor(cursor);
}

void NativeWidgetWin::ClearNativeFocus() {
  message_handler_->ClearNativeFocus();
}

void NativeWidgetWin::FocusNativeView(gfx::NativeView native_view) {
  message_handler_->FocusHWND(native_view);
}

gfx::Rect NativeWidgetWin::GetWorkAreaBoundsInScreen() const {
  return message_handler_->GetWorkAreaBoundsInScreen();
}

void NativeWidgetWin::SetInactiveRenderingDisabled(bool value) {
}

Widget::MoveLoopResult NativeWidgetWin::RunMoveLoop(
    const gfx::Point& drag_offset) {
  return message_handler_->RunMoveLoop(drag_offset) ?
      Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetWin::EndMoveLoop() {
  SendMessage(hwnd(), WM_CANCELMODE, 0, 0);
}

void NativeWidgetWin::SetVisibilityChangedAnimationsEnabled(bool value) {
  message_handler_->SetVisibilityChangedAnimationsEnabled(value);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, MessageLoop::Observer implementation:

base::EventStatus NativeWidgetWin::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void NativeWidgetWin::DidProcessEvent(const base::NativeEvent& event) {
  message_handler_->RedrawInvalidRect();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, WindowImpl overrides:

HICON NativeWidgetWin::GetDefaultWindowIcon() const {
  if (ViewsDelegate::views_delegate)
    return ViewsDelegate::views_delegate->GetDefaultWindowIcon();
  return NULL;
}

LRESULT NativeWidgetWin::OnWndProc(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;

  // First allow messages sent by child controls to be processed directly by
  // their associated views. If such a view is present, it will handle the
  // message *instead of* this NativeWidgetWin.
  if (ProcessChildWindowMessage(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  if (!ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY) {
    MessageLoopForUI::current()->RemoveObserver(this);
    OnFinalMessage(window);
  }

  // Only top level widget should store/restore focus.
  if (message == WM_ACTIVATE && GetWidget()->is_top_level())
    PostProcessActivateMessage(this, LOWORD(w_param));
  if (message == WM_ENABLE && restore_focus_when_enabled_) {
    // This path should be executed only for top level as
    // restore_focus_when_enabled_ is set in PostProcessActivateMessage.
    DCHECK(GetWidget()->is_top_level());
    restore_focus_when_enabled_ = false;
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, protected:

// Message handlers ------------------------------------------------------------

void NativeWidgetWin::OnActivate(UINT action, BOOL minimized, HWND window) {
  message_handler_->OnActivate(action, minimized, window);
}

void NativeWidgetWin::OnActivateApp(BOOL active, DWORD thread_id) {
  message_handler_->OnActivateApp(active, thread_id);
}

LRESULT NativeWidgetWin::OnAppCommand(HWND window,
                                      short app_command,
                                      WORD device,
                                      int keystate) {
  return message_handler_->OnAppCommand(window, app_command, device, keystate);
}

void NativeWidgetWin::OnCancelMode() {
  message_handler_->OnCancelMode();
}

void NativeWidgetWin::OnCaptureChanged(HWND hwnd) {
  message_handler_->OnCaptureChanged(hwnd);
}

void NativeWidgetWin::OnClose() {
  message_handler_->OnClose();
}

void NativeWidgetWin::OnCommand(UINT notification_code,
                                int command_id,
                                HWND window) {
  message_handler_->OnCommand(notification_code, command_id, window);
}

LRESULT NativeWidgetWin::OnCreate(CREATESTRUCT* create_struct) {
  return message_handler_->OnCreate(create_struct);
}

void NativeWidgetWin::OnDestroy() {
  message_handler_->OnDestroy();
}

void NativeWidgetWin::OnDisplayChange(UINT bits_per_pixel, CSize screen_size) {
  message_handler_->OnDisplayChange(bits_per_pixel, screen_size);
}

LRESULT NativeWidgetWin::OnDwmCompositionChanged(UINT msg,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  return message_handler_->OnDwmCompositionChanged(msg, w_param, l_param);
}

void NativeWidgetWin::OnEndSession(BOOL ending, UINT logoff) {
  message_handler_->OnEndSession(ending, logoff);
}

void NativeWidgetWin::OnEnterSizeMove() {
  message_handler_->OnEnterSizeMove();
}

LRESULT NativeWidgetWin::OnEraseBkgnd(HDC dc) {
  return message_handler_->OnEraseBkgnd(dc);
}

void NativeWidgetWin::OnExitMenuLoop(BOOL is_track_popup_menu) {
  message_handler_->OnExitMenuLoop(is_track_popup_menu);
}

void NativeWidgetWin::OnExitSizeMove() {
  message_handler_->OnExitSizeMove();
}

LRESULT NativeWidgetWin::OnGetObject(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  return message_handler_->OnGetObject(message, w_param, l_param);
}

void NativeWidgetWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  message_handler_->OnGetMinMaxInfo(minmax_info);
}

void NativeWidgetWin::OnHScroll(int scroll_type,
                                short position,
                                HWND scrollbar) {
  message_handler_->OnHScroll(scroll_type, position, scrollbar);
}

LRESULT NativeWidgetWin::OnImeMessages(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  return message_handler_->OnImeMessages(message, w_param, l_param);
}

void NativeWidgetWin::OnInitMenu(HMENU menu) {
  message_handler_->OnInitMenu(menu);
}

void NativeWidgetWin::OnInitMenuPopup(HMENU menu,
                                      UINT position,
                                      BOOL is_system_menu) {
  message_handler_->OnInitMenuPopup();
}

void NativeWidgetWin::OnInputLangChange(DWORD character_set,
                                        HKL input_language_id) {
  message_handler_->OnInputLangChange(character_set, input_language_id);
}

LRESULT NativeWidgetWin::OnKeyEvent(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {
  return message_handler_->OnKeyEvent(message, w_param, l_param);
}

void NativeWidgetWin::OnKillFocus(HWND focused_window) {
  message_handler_->OnKillFocus(focused_window);
}

LRESULT NativeWidgetWin::OnMouseActivate(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  return message_handler_->OnMouseActivate(message, w_param, l_param);
}

LRESULT NativeWidgetWin::OnMouseRange(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  return message_handler_->OnMouseRange(message, w_param, l_param);
}

void NativeWidgetWin::OnMove(const CPoint& point) {
  message_handler_->OnMove(point);
}

void NativeWidgetWin::OnMoving(UINT param, const LPRECT new_bounds) {
  message_handler_->OnMoving(param, new_bounds);
}

LRESULT NativeWidgetWin::OnNCActivate(BOOL active) {
  return message_handler_->OnNCActivate(active);
}

LRESULT NativeWidgetWin::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  return message_handler_->OnNCCalcSize(mode, l_param);
}

LRESULT NativeWidgetWin::OnNCHitTest(const CPoint& point) {
  return message_handler_->OnNCHitTest(point);
}

void NativeWidgetWin::OnNCPaint(HRGN rgn) {
  message_handler_->OnNCPaint(rgn);
}

LRESULT NativeWidgetWin::OnNCUAHDrawCaption(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  return message_handler_->OnNCUAHDrawCaption(msg, w_param, l_param);
}

LRESULT NativeWidgetWin::OnNCUAHDrawFrame(UINT msg,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  return message_handler_->OnNCUAHDrawFrame(msg, w_param, l_param);
}

LRESULT NativeWidgetWin::OnNotify(int w_param, NMHDR* l_param) {
  return message_handler_->OnNotify(w_param, l_param);
}

void NativeWidgetWin::OnPaint(HDC dc) {
  message_handler_->OnPaint(dc);
}

LRESULT NativeWidgetWin::OnPowerBroadcast(DWORD power_event, DWORD data) {
  return message_handler_->OnPowerBroadcast(power_event, data);
}

LRESULT NativeWidgetWin::OnReflectedMessage(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  return message_handler_->OnReflectedMessage(msg, w_param, l_param);
}

LRESULT NativeWidgetWin::OnSetCursor(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  return message_handler_->OnSetCursor(message, w_param, l_param);
}

void NativeWidgetWin::OnSetFocus(HWND old_focused_window) {
  message_handler_->OnSetFocus(old_focused_window);
}

LRESULT NativeWidgetWin::OnSetIcon(UINT size_type, HICON new_icon) {
  return message_handler_->OnSetIcon(size_type, new_icon);
}

LRESULT NativeWidgetWin::OnSetText(const wchar_t* text) {
  return message_handler_->OnSetText(text);
}

void NativeWidgetWin::OnSettingChange(UINT flags, const wchar_t* section) {
  message_handler_->OnSettingChange(flags, section);
}

void NativeWidgetWin::OnSize(UINT param, const CSize& size) {
  message_handler_->OnSize(param, size);
}

void NativeWidgetWin::OnSysCommand(UINT notification_code, CPoint click) {
  message_handler_->OnSysCommand(notification_code, click);
}

void NativeWidgetWin::OnThemeChanged() {
  message_handler_->OnThemeChanged();
}

LRESULT NativeWidgetWin::OnTouchEvent(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  return message_handler_->OnTouchEvent(message, w_param, l_param);
}

void NativeWidgetWin::OnVScroll(int scroll_type,
                                short position,
                                HWND scrollbar) {
  message_handler_->OnVScroll(scroll_type, position, scrollbar);
}

void NativeWidgetWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  message_handler_->OnWindowPosChanging(window_pos);
}

void NativeWidgetWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  message_handler_->OnWindowPosChanged(window_pos);
}

void NativeWidgetWin::OnFinalMessage(HWND window) {
  // We don't destroy props in WM_DESTROY as we may still get messages after
  // WM_DESTROY that assume the properties are still valid (such as WM_CLOSE).
  props_.clear();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, protected:

int NativeWidgetWin::GetShowState() const {
  return SW_SHOWNORMAL;
}

void NativeWidgetWin::OnScreenReaderDetected() {
  screen_reader_active_ = true;
}

void NativeWidgetWin::SetInitialFocus() {
  if (!GetWidget()->SetInitialFocus() &&
      !(GetWindowLong(GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
      !(GetWindowLong(GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
    // The window does not get keyboard messages unless we focus it.
    SetFocus(GetNativeView());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, HWNDMessageHandlerDelegate implementation:

bool NativeWidgetWin::IsWidgetWindow() const {
  // We don't NULL check GetWidget()->non_client_view() here because this
  // function can be called before the widget is fully constructed.
  return has_non_client_view_;
}

bool NativeWidgetWin::IsUsingCustomFrame() const {
  return !GetWidget()->ShouldUseNativeFrame();
}

void NativeWidgetWin::SchedulePaint() {
  GetWidget()->GetRootView()->SchedulePaint();
}

void NativeWidgetWin::EnableInactiveRendering() {
  delegate_->EnableInactiveRendering();
}

bool NativeWidgetWin::IsInactiveRenderingDisabled() {
  return delegate_->IsInactiveRenderingDisabled();
}

bool NativeWidgetWin::CanResize() const {
  return GetWidget()->widget_delegate()->CanResize();
}

bool NativeWidgetWin::CanMaximize() const {
  return GetWidget()->widget_delegate()->CanMaximize();
}

bool NativeWidgetWin::CanActivate() const {
  return delegate_->CanActivate();
}

bool NativeWidgetWin::WillProcessWorkAreaChange() const {
  return GetWidget()->widget_delegate()->WillProcessWorkAreaChange();
}

int NativeWidgetWin::GetNonClientComponent(const gfx::Point& point) const {
  return delegate_->GetNonClientComponent(point);
}

void NativeWidgetWin::GetWindowMask(const gfx::Size& size, gfx::Path* path) {
  if (GetWidget()->non_client_view())
    GetWidget()->non_client_view()->GetWindowMask(size, path);
}

bool NativeWidgetWin::GetClientAreaInsets(gfx::Insets* insets) const {
  return false;
}

void NativeWidgetWin::GetMinMaxSize(gfx::Size* min_size,
                                    gfx::Size* max_size) const {
  *min_size = delegate_->GetMinimumSize();
  *max_size = delegate_->GetMaximumSize();
}

gfx::Size NativeWidgetWin::GetRootViewSize() const {
  return GetWidget()->GetRootView()->size();
}

void NativeWidgetWin::ResetWindowControls() {
  GetWidget()->non_client_view()->ResetWindowControls();
}

void NativeWidgetWin::UpdateFrame() {
  GetWidget()->non_client_view()->UpdateFrame();
}

void NativeWidgetWin::PaintLayeredWindow(gfx::Canvas* canvas) {
  GetWidget()->GetRootView()->Paint(canvas);
}

InputMethod* NativeWidgetWin::GetInputMethod() {
  return GetWidget()->GetInputMethodDirect();
}

gfx::NativeViewAccessible NativeWidgetWin::GetNativeViewAccessible() {
  return GetWidget()->GetRootView()->GetNativeViewAccessible();
}

void NativeWidgetWin::HandleAppDeactivated() {
  // Another application was activated, we should reset any state that
  // disables inactive rendering now.
  delegate_->EnableInactiveRendering();
}

void NativeWidgetWin::HandleActivationChanged(bool active) {
  delegate_->OnNativeWidgetActivationChanged(active);
}

bool NativeWidgetWin::HandleAppCommand(short command) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  return GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void NativeWidgetWin::HandleCaptureLost() {
  delegate_->OnMouseCaptureLost();
}

void NativeWidgetWin::HandleClose() {
  GetWidget()->Close();
}

bool NativeWidgetWin::HandleCommand(int command) {
  return GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void NativeWidgetWin::HandleAccelerator(const ui::Accelerator& accelerator) {
  GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
}

void NativeWidgetWin::HandleCreate() {
  // TODO(beng): much of this could/should maybe move to HWNDMessageHandler.

  SetNativeWindowProperty(kNativeWidgetKey, this);
  CHECK_EQ(this, GetNativeWidgetForNativeView(hwnd()));

  props_.push_back(ui::SetWindowSupportsRerouteMouseWheel(hwnd()));

  drop_target_ = new DropTargetWin(
      static_cast<internal::RootView*>(GetWidget()->GetRootView()));

  // We need to add ourselves as a message loop observer so that we can repaint
  // aggressively if the contents of our window become invalid. Unfortunately
  // WM_PAINT messages are starved and we get flickery redrawing when resizing
  // if we do not do this.
  MessageLoopForUI::current()->AddObserver(this);

  // Windows special DWM window frame requires a special tooltip manager so
  // that window controls in Chrome windows don't flicker when you move your
  // mouse over them. See comment in aero_tooltip_manager.h.
  Widget* widget = GetWidget()->GetTopLevelWidget();
  if (widget && widget->ShouldUseNativeFrame()) {
    tooltip_manager_.reset(new AeroTooltipManager(GetWidget()));
  } else {
    tooltip_manager_.reset(new TooltipManagerWin(GetWidget()));
  }
  if (!tooltip_manager_->Init()) {
    // There was a problem creating the TooltipManager. Common error is 127.
    // See 82193 for details.
    LOG_GETLASTERROR(WARNING) << "tooltip creation failed, disabling tooltips";
    tooltip_manager_.reset();
  }

  delegate_->OnNativeWidgetCreated();
}

void NativeWidgetWin::HandleDestroy() {
  delegate_->OnNativeWidgetDestroying();
  if (drop_target_.get()) {
    RevokeDragDrop(hwnd());
    drop_target_ = NULL;
  }
}

void NativeWidgetWin::HandleDisplayChange() {
  GetWidget()->widget_delegate()->OnDisplayChanged();
}

void NativeWidgetWin::HandleGlassModeChange() {
  // For some reason, we need to hide the window while we're changing the frame
  // type only when we're changing it in response to WM_DWMCOMPOSITIONCHANGED.
  // If we don't, the client area will be filled with black. I'm suspecting
  // something skia-ey.
  // Frame type toggling caused by the user (e.g. switching theme) doesn't seem
  // to have this requirement.
  FrameTypeChanged();
}

void NativeWidgetWin::HandleBeginWMSizeMove() {
  delegate_->OnNativeWidgetBeginUserBoundsChange();
}

void NativeWidgetWin::HandleEndWMSizeMove() {
  delegate_->OnNativeWidgetEndUserBoundsChange();
}

void NativeWidgetWin::HandleMove() {
  delegate_->OnNativeWidgetMove();
}

void NativeWidgetWin::HandleWorkAreaChanged() {
  GetWidget()->widget_delegate()->OnWorkAreaChanged();
}

void NativeWidgetWin::HandleVisibilityChanged(bool visible) {
  delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void NativeWidgetWin::HandleClientSizeChanged(const gfx::Size& new_size) {
  delegate_->OnNativeWidgetSizeChanged(new_size);
}

void NativeWidgetWin::HandleNativeFocus(HWND last_focused_window) {
  delegate_->OnNativeFocus(last_focused_window);
}

void NativeWidgetWin::HandleNativeBlur(HWND focused_window) {
  delegate_->OnNativeBlur(focused_window);
}

bool NativeWidgetWin::HandleMouseEvent(const ui::MouseEvent& event) {
  return delegate_->OnMouseEvent(event);
}

bool NativeWidgetWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return delegate_->OnKeyEvent(event);
}

bool NativeWidgetWin::HandlePaintAccelerated(const gfx::Rect& invalid_rect) {
  return delegate_->OnNativeWidgetPaintAccelerated(gfx::Rect(invalid_rect));
}

void NativeWidgetWin::HandlePaint(gfx::Canvas* canvas) {
  delegate_->OnNativeWidgetPaint(canvas);
}

void NativeWidgetWin::HandleScreenReaderDetected() {
  // TODO(beng): just consolidate this with OnScreenReaderDetected.
  OnScreenReaderDetected();
}

bool NativeWidgetWin::HandleTooltipNotify(int w_param,
                                          NMHDR* l_param,
                                          LRESULT* l_result) {
  // We can be sent this message before the tooltip manager is created, if a
  // subclass overrides OnCreate and creates some kind of Windows control there
  // that sends WM_NOTIFY messages.
  if (tooltip_manager_.get()) {
    bool handled;
    *l_result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
    return handled;
  }
  return false;
}

void NativeWidgetWin::HandleTooltipMouseMove(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  if (tooltip_manager_.get())
    tooltip_manager_->OnMouse(message, w_param, l_param);
}

NativeWidgetWin* NativeWidgetWin::AsNativeWidgetWin() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, private:

// static
void NativeWidgetWin::PostProcessActivateMessage(NativeWidgetWin* widget,
                                                 int activation_state) {
  DCHECK(widget->GetWidget()->is_top_level());
  FocusManager* focus_manager = widget->GetWidget()->GetFocusManager();
  if (WA_INACTIVE == activation_state) {
    // We might get activated/inactivated without being enabled, so we need to
    // clear restore_focus_when_enabled_.
    widget->restore_focus_when_enabled_ = false;
    focus_manager->StoreFocusedView();
  } else {
    // We must restore the focus after the message has been DefProc'ed as it
    // does set the focus to the last focused HWND.
    // Note that if the window is not enabled, we cannot restore the focus as
    // calling ::SetFocus on a child of the non-enabled top-window would fail.
    // This is the case when showing a modal dialog (such as 'open file',
    // 'print'...) from a different thread.
    // In that case we delay the focus restoration to when the window is enabled
    // again.
    if (!IsWindowEnabled(widget->GetNativeView())) {
      DCHECK(!widget->restore_focus_when_enabled_);
      widget->restore_focus_when_enabled_ = true;
      return;
    }

    // Mysteriously, this only appears to be needed support restoration of focus
    // to a child hwnd when restoring its top level window from the minimized
    // state. If we don't do this, then ::SetFocus() to that child HWND returns
    // ERROR_INVALID_PARAMETER, despite both HWNDs being of the same thread.
    // See http://crbug.com/125976
    {
      // Since this is a synthetic reset, we don't need to tell anyone about it.
      AutoNativeNotificationDisabler disabler;
      focus_manager->ClearFocus();
    }

    focus_manager->RestoreFocusedView();
  }
}

void NativeWidgetWin::SetInitParams(const Widget::InitParams& params) {
  // Set non-style attributes.
  ownership_ = params.ownership;

  DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  DWORD ex_style = 0;
  DWORD class_style = CS_DBLCLKS;

  // Set type-independent style attributes.
  if (params.child)
    style |= WS_CHILD;
  if (params.show_state == ui::SHOW_STATE_MAXIMIZED)
    style |= WS_MAXIMIZE;
  if (params.show_state == ui::SHOW_STATE_MINIMIZED)
    style |= WS_MINIMIZE;
  if (!params.accept_events)
    ex_style |= WS_EX_TRANSPARENT;
  if (!params.can_activate)
    ex_style |= WS_EX_NOACTIVATE;
  if (params.keep_on_top)
    ex_style |= WS_EX_TOPMOST;
  if (params.mirror_origin_in_rtl)
    ex_style |= l10n_util::GetExtendedTooltipStyles();
  if (params.transparent)
    ex_style |= WS_EX_LAYERED;
  if (params.has_dropshadow) {
    class_style |= (base::win::GetVersion() < base::win::VERSION_XP) ?
        0 : CS_DROPSHADOW;
  }

  // Set type-dependent style attributes.
  switch (params.type) {
    case Widget::InitParams::TYPE_PANEL:
      ex_style |= WS_EX_TOPMOST;
      // No break. Fall through to TYPE_WINDOW.
    case Widget::InitParams::TYPE_WINDOW: {
      style |= WS_SYSMENU | WS_CAPTION;
      bool can_resize = GetWidget()->widget_delegate()->CanResize();
      bool can_maximize = GetWidget()->widget_delegate()->CanMaximize();
      if (can_maximize) {
        style |= WS_OVERLAPPEDWINDOW;
      } else if (can_resize) {
        style |= WS_OVERLAPPED | WS_THICKFRAME;
      }
      if (delegate_->IsDialogBox()) {
        style |= DS_MODALFRAME;
        // NOTE: Turning this off means we lose the close button, which is bad.
        // Turning it on though means the user can maximize or size the window
        // from the system menu, which is worse. We may need to provide our own
        // menu to get the close button to appear properly.
        // style &= ~WS_SYSMENU;

        // Set the WS_POPUP style for modal dialogs. This ensures that the owner
        // window is activated on destruction. This style should not be set for
        // non-modal non-top-level dialogs like constrained windows.
        style |= delegate_->IsModal() ? WS_POPUP : 0;
      }
      ex_style |= delegate_->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;
      break;
    }
    case Widget::InitParams::TYPE_CONTROL:
      style |= WS_VISIBLE;
      break;
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      style |= WS_POPUP;
      break;
    case Widget::InitParams::TYPE_BUBBLE:
      style |= WS_POPUP;
      style |= WS_CLIPCHILDREN;
      break;
    case Widget::InitParams::TYPE_POPUP:
      style |= WS_POPUP;
      ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_MENU:
      style |= WS_POPUP;
      break;
    default:
      NOTREACHED();
  }

  set_initial_class_style(class_style);
  set_window_style(window_style() | style);
  set_window_ex_style(window_ex_style() | ex_style);

  has_non_client_view_ = Widget::RequiresNonClientView(params.type);
  message_handler_->set_remove_standard_frame(params.remove_standard_frame);
}

bool NativeWidgetWin::WidgetSizeIsClientSize() const {
  const Widget* widget = GetWidget()->GetTopLevelWidget();
  return IsZoomed() || (widget && widget->ShouldUseNativeFrame());
}

void NativeWidgetWin::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = ::GetWindow(GetNativeView(), GW_OWNER);
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  NOTIMPLEMENTED();
}

namespace {
BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
  Widget* widget = Widget::GetWidgetForNativeView(hwnd);
  if (widget && widget->is_secondary_widget())
    widget->Close();
  return TRUE;
}
}  // namespace

// static
void Widget::CloseAllSecondaryWidgets() {
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, 0);
}

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  DCHECK(source);
  DCHECK(target);
  DCHECK(rect);

  HWND source_hwnd = source->GetNativeView();
  HWND target_hwnd = target->GetNativeView();
  if (source_hwnd == target_hwnd)
    return true;

  RECT win_rect = rect->ToRECT();
  if (::MapWindowPoints(source_hwnd, target_hwnd,
                        reinterpret_cast<LPPOINT>(&win_rect),
                        sizeof(RECT)/sizeof(POINT))) {
    *rect = win_rect;
    return true;
  }
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetWin(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return reinterpret_cast<NativeWidgetWin*>(
      ViewProp::GetValue(native_view, kNativeWidgetKey));
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return GetNativeWidgetForNativeView(native_window);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  if (!native_view)
    return NULL;

  // First, check if the top-level window is a Widget.
  HWND root = ::GetAncestor(native_view, GA_ROOT);
  if (!root)
    return NULL;

  NativeWidgetPrivate* widget = GetNativeWidgetForNativeView(root);
  if (widget)
    return widget;

  // Second, try to locate the last Widget window in the parent hierarchy.
  HWND parent_hwnd = native_view;
  // If we fail to find the native widget pointer for the root then it probably
  // means that the root belongs to a different process in which case we walk up
  // the native view chain looking for a parent window which corresponds to a
  // valid native widget. We only do this if we fail to find the native widget
  // for the current native view which means it is being destroyed.
  if (!widget && !GetNativeWidgetForNativeView(native_view)) {
    parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    if (!parent_hwnd)
      return NULL;
  }
  NativeWidgetPrivate* parent_widget;
  do {
    parent_widget = GetNativeWidgetForNativeView(parent_hwnd);
    if (parent_widget) {
      widget = parent_widget;
      parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    }
  } while (parent_hwnd != NULL && parent_widget != NULL);

  return widget;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  if (!native_view)
    return;

  Widget* widget = Widget::GetWidgetForNativeView(native_view);
  if (widget)
    children->insert(widget);
  EnumChildWindows(native_view, EnumerateChildWindowsForNativeWidgets,
                   reinterpret_cast<LPARAM>(children));
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  if (!native_view)
    return;

  HWND previous_parent = ::GetParent(native_view);
  if (previous_parent == new_parent)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    // TODO(beng): Rename this notification to NotifyNativeViewChanging()
    // and eliminate the bool parameter.
    (*it)->NotifyNativeViewHierarchyChanged(false, previous_parent);
  }

  ::SetParent(native_view, new_parent);

  // And now, notify them that they have a brand new parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    (*it)->NotifyNativeViewHierarchyChanged(true, new_parent);
  }
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  return (GetKeyState(VK_LBUTTON) & 0x80) ||
    (GetKeyState(VK_RBUTTON) & 0x80) ||
    (GetKeyState(VK_MBUTTON) & 0x80) ||
    (GetKeyState(VK_XBUTTON1) & 0x80) ||
    (GetKeyState(VK_XBUTTON2) & 0x80);
}

// static
bool NativeWidgetPrivate::IsTouchDown() {
  // This currently isn't necessary because we're not generating touch events on
  // windows.  When we do, this will need to be updated.
  return false;
}

}  // namespace internal

}  // namespace views
