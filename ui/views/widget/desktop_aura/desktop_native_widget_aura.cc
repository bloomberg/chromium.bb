// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "base/bind.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/corewm/capture_controller.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/corewm/shadow_controller.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_modality_controller.h"
#include "ui/views/drag_utils.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/widget/desktop_aura/desktop_root_window_host.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/native_widget_aura_window_observer.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/window_reorderer.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(VIEWS_EXPORT,
                                      views::DesktopNativeWidgetAura*);

namespace views {

DEFINE_WINDOW_PROPERTY_KEY(DesktopNativeWidgetAura*,
                           kDesktopNativeWidgetAuraKey, NULL);

namespace {

// This class provides functionality to create a top level widget to host a
// child window.
class DesktopNativeWidgetTopLevelHandler : public aura::WindowObserver {
 public:
  // This function creates a widget with the bounds passed in which eventually
  // becomes the parent of the child window passed in.
  static aura::Window* CreateParentWindow(aura::Window* child_window,
                                          const gfx::Rect& bounds,
                                          bool full_screen) {
    // This instance will get deleted when the widget is destroyed.
    DesktopNativeWidgetTopLevelHandler* top_level_handler =
        new DesktopNativeWidgetTopLevelHandler;

    child_window->SetBounds(gfx::Rect(bounds.size()));

    Widget::InitParams init_params;
    init_params.type = full_screen ? Widget::InitParams::TYPE_WINDOW :
        Widget::InitParams::TYPE_POPUP;
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    init_params.layer_type = ui::LAYER_NOT_DRAWN;
    init_params.can_activate = full_screen;
    // This widget instance will get deleted when the window is
    // destroyed.
    top_level_handler->top_level_widget_ = new Widget();
    top_level_handler->top_level_widget_->Init(init_params);

    top_level_handler->top_level_widget_->SetFullscreen(full_screen);
    top_level_handler->top_level_widget_->Show();

    aura::Window* native_window =
        top_level_handler->top_level_widget_->GetNativeView();
    child_window->AddObserver(top_level_handler);
    native_window->AddObserver(top_level_handler);
    return native_window;
  }

  // aura::WindowObserver overrides
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    window->RemoveObserver(this);

    // If the widget is being destroyed by the OS then we should not try and
    // destroy it again.
    if (top_level_widget_ &&
        window == top_level_widget_->GetNativeView()) {
      top_level_widget_ = NULL;
      return;
    }

    if (top_level_widget_) {
      DCHECK(top_level_widget_->GetNativeView());
      top_level_widget_->GetNativeView()->RemoveObserver(this);
      // When we receive a notification that the child of the window created
      // above is being destroyed we go ahead and initiate the destruction of
      // the corresponding widget.
      top_level_widget_->Close();
      top_level_widget_ = NULL;
    }
    delete this;
  }

 private:
  DesktopNativeWidgetTopLevelHandler()
      : top_level_widget_(NULL) {}

  virtual ~DesktopNativeWidgetTopLevelHandler() {}

  Widget* top_level_widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetTopLevelHandler);
};

class DesktopNativeWidgetAuraStackingClient :
    public aura::client::StackingClient {
 public:
  explicit DesktopNativeWidgetAuraStackingClient(aura::RootWindow* root_window)
      : root_window_(root_window) {
    aura::client::SetStackingClient(root_window_, this);
  }
  virtual ~DesktopNativeWidgetAuraStackingClient() {
    aura::client::SetStackingClient(root_window_, NULL);
  }

  // Overridden from client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    bool full_screen = window->GetProperty(aura::client::kShowStateKey) ==
        ui::SHOW_STATE_FULLSCREEN;
    bool is_menu = false;
    // TODO(erg): We need to be able to spawn and deal with toplevel windows if
    // we want the popups to extend past our window
    // bounds. http://crbug.com/288988
#if !defined(OS_LINUX)
    is_menu = window->type() == aura::client::WINDOW_TYPE_MENU;
#endif
    if (full_screen || is_menu) {
      return DesktopNativeWidgetTopLevelHandler::CreateParentWindow(
          window, bounds, full_screen);
    }
    return root_window_;
  }

 private:
  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetAuraStackingClient);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, public:

DesktopNativeWidgetAura::DesktopNativeWidgetAura(
    internal::NativeWidgetDelegate* delegate)
    : ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      close_widget_factory_(this),
      can_activate_(true),
      desktop_root_window_host_(NULL),
      window_(new aura::Window(this)),
      content_window_container_(NULL),
      native_widget_delegate_(delegate),
      last_drop_operation_(ui::DragDropTypes::DRAG_NONE),
      restore_focus_on_activate_(false),
      cursor_(gfx::kNullCursor),
      widget_type_(Widget::InitParams::TYPE_WINDOW) {
  window_->SetProperty(kDesktopNativeWidgetAuraKey, this);
  aura::client::SetFocusChangeObserver(window_, this);
  aura::client::SetActivationChangeObserver(window_, this);
}

DesktopNativeWidgetAura::~DesktopNativeWidgetAura() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete native_widget_delegate_;
  else
    CloseNow();
}

// static
DesktopNativeWidgetAura* DesktopNativeWidgetAura::ForWindow(
    aura::Window* window) {
  return window->GetProperty(kDesktopNativeWidgetAuraKey);
}

void DesktopNativeWidgetAura::OnHostClosed() {
  // Don't invoke Widget::OnNativeWidgetDestroying(), its done by
  // DesktopRootWindowHost.

  // The WindowModalityController is at the front of the event pretarget
  // handler list. We destroy it first to preserve order symantics.
  if (window_modality_controller_)
    window_modality_controller_.reset();

  // Make sure we don't still have capture. Otherwise CaptureController and
  // RootWindow are left referencing a deleted Window.
  {
    aura::Window* capture_window =
        capture_client_->capture_client()->GetCaptureWindow();
    if (capture_window && root_window_->Contains(capture_window))
      capture_window->ReleaseCapture();
  }

  // DesktopRootWindowHost owns the ActivationController which ShadowController
  // references. Make sure we destroy ShadowController early on.
  shadow_controller_.reset();
  tooltip_manager_.reset();
  root_window_->RemovePreTargetHandler(tooltip_controller_.get());
  aura::client::SetTooltipClient(root_window_.get(), NULL);
  tooltip_controller_.reset();

  root_window_event_filter_->RemoveHandler(input_method_event_filter_.get());

  stacking_client_.reset();  // Uses root_window_ at destruction.

  root_window_->RemoveRootWindowObserver(this);
  root_window_.reset();  // Uses input_method_event_filter_ at destruction.
  // RootWindow owns |desktop_root_window_host_|.
  desktop_root_window_host_ = NULL;
  window_ = NULL;

  native_widget_delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void DesktopNativeWidgetAura::InstallInputMethodEventFilter(
    aura::RootWindow* root) {
  DCHECK(!input_method_event_filter_.get());

  // CEF sets focus to the window the user clicks down on.
  // TODO(beng): see if we can't do this some other way. CEF seems a heavy-
  //             handed way of accomplishing focus.
  // No event filter for aura::Env. Create CompoundEvnetFilter per RootWindow.
  root_window_event_filter_ = new corewm::CompoundEventFilter;
  // Pass ownership of the filter to the root_window.
  root->SetEventFilter(root_window_event_filter_);

  input_method_event_filter_.reset(
      new corewm::InputMethodEventFilter(root->GetAcceleratedWidget()));
  input_method_event_filter_->SetInputMethodPropertyInRootWindow(root);
  root_window_event_filter_->AddHandler(input_method_event_filter_.get());
}

void DesktopNativeWidgetAura::CreateCaptureClient(aura::RootWindow* root) {
  DCHECK(!capture_client_.get());
  capture_client_.reset(new corewm::ScopedCaptureClient(root));
}

void DesktopNativeWidgetAura::HandleActivationChanged(bool active) {
  native_widget_delegate_->OnNativeWidgetActivationChanged(active);
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(root_window_.get());
  if (!activation_client)
    return;
  if (active) {
    if (GetWidget()->HasFocusManager()) {
      // This function can be called before the focus manager has had a
      // chance to set the focused view. In which case we should get the
      // last focused view.
      View* view_for_activation =
          GetWidget()->GetFocusManager()->GetFocusedView() ?
              GetWidget()->GetFocusManager()->GetFocusedView() :
                  GetWidget()->GetFocusManager()->GetStoredFocusView();
      if (!view_for_activation)
        view_for_activation = GetWidget()->GetRootView();
      activation_client->ActivateWindow(
          view_for_activation->GetWidget()->GetNativeView());
    }
  } else {
    // If we're not active we need to deactivate the corresponding
    // aura::Window. This way if a child widget is active it gets correctly
    // deactivated (child widgets don't get native desktop activation changes,
    // only aura activation changes).
    aura::Window* active_window = activation_client->GetActiveWindow();
    if (active_window)
      activation_client->DeactivateWindow(active_window);
  }
}

void DesktopNativeWidgetAura::InstallWindowModalityController(
    aura::RootWindow* root) {
  // The WindowsModalityController event filter should be at the head of the
  // pre target handlers list. This ensures that it handles input events first
  // when modal windows are at the top of the Zorder.
  if (widget_type_ == Widget::InitParams::TYPE_WINDOW)
    window_modality_controller_.reset(
        new views::corewm::WindowModalityController(root));
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, internal::NativeWidgetPrivate implementation:

void DesktopNativeWidgetAura::InitNativeWidget(
    const Widget::InitParams& params) {
  ownership_ = params.ownership;
  widget_type_ = params.type;

  NativeWidgetAura::RegisterNativeWidgetForWindow(this, window_);
  // Animations on TYPE_WINDOW are handled by the OS. Additionally if we animate
  // these windows the size of the window gets augmented, effecting restore
  // bounds and maximized windows in bad ways.
  if (params.type == Widget::InitParams::TYPE_WINDOW &&
      !params.remove_standard_frame) {
    window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
  window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  window_->Init(params.layer_type);
  corewm::SetShadowType(window_, corewm::SHADOW_TYPE_NONE);
#if defined(OS_LINUX)  // TODO(scottmg): http://crbug.com/180071
  window_->Show();
#endif

  desktop_root_window_host_ = params.desktop_root_window_host ?
      params.desktop_root_window_host :
      DesktopRootWindowHost::Create(native_widget_delegate_,
                                    this, params.bounds);
  root_window_.reset(
      desktop_root_window_host_->Init(window_, params));

  UpdateWindowTransparency();

  content_window_container_ = new aura::Window(NULL);
  content_window_container_->Init(ui::LAYER_NOT_DRAWN);
  content_window_container_->Show();
  content_window_container_->AddChild(window_);
  root_window_->AddChild(content_window_container_);
  OnRootWindowHostResized(root_window_.get());

  root_window_->AddRootWindowObserver(this);

  stacking_client_.reset(
      new DesktopNativeWidgetAuraStackingClient(root_window_.get()));
  drop_helper_.reset(new DropHelper(
      static_cast<internal::RootView*>(GetWidget()->GetRootView())));
  aura::client::SetDragDropDelegate(window_, this);

  tooltip_manager_.reset(new views::TooltipManagerAura(window_, GetWidget()));

  tooltip_controller_.reset(
      new corewm::TooltipController(
          desktop_root_window_host_->CreateTooltip()));
  aura::client::SetTooltipClient(root_window_.get(), tooltip_controller_.get());
  root_window_->AddPreTargetHandler(tooltip_controller_.get());

  if (params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW) {
    visibility_controller_.reset(new views::corewm::VisibilityController);
    aura::client::SetVisibilityClient(GetNativeView()->GetRootWindow(),
                                      visibility_controller_.get());
    views::corewm::SetChildWindowVisibilityChangesAnimated(
        GetNativeView()->GetRootWindow());
    views::corewm::SetChildWindowVisibilityChangesAnimated(
        content_window_container_);
  }

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    FocusManager* focus_manager =
        native_widget_delegate_->AsWidget()->GetFocusManager();
    root_window_->AddPreTargetHandler(focus_manager->GetEventHandler());
  }

  window_->Show();
  desktop_root_window_host_->InitFocus(window_);

  aura::client::SetActivationDelegate(window_, this);

  shadow_controller_.reset(
      new corewm::ShadowController(
          aura::client::GetActivationClient(root_window_.get())));

  window_reorderer_.reset(new WindowReorderer(window_,
      GetWidget()->GetRootView()));
}

NonClientFrameView* DesktopNativeWidgetAura::CreateNonClientFrameView() {
  return desktop_root_window_host_->CreateNonClientFrameView();
}

bool DesktopNativeWidgetAura::ShouldUseNativeFrame() const {
  return desktop_root_window_host_->ShouldUseNativeFrame();
}

void DesktopNativeWidgetAura::FrameTypeChanged() {
  desktop_root_window_host_->FrameTypeChanged();
  UpdateWindowTransparency();
}

Widget* DesktopNativeWidgetAura::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopNativeWidgetAura::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView DesktopNativeWidgetAura::GetNativeView() const {
  return window_;
}

gfx::NativeWindow DesktopNativeWidgetAura::GetNativeWindow() const {
  return window_;
}

Widget* DesktopNativeWidgetAura::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* DesktopNativeWidgetAura::GetCompositor() const {
  return window_ ? window_->layer()->GetCompositor() : NULL;
}

ui::Compositor* DesktopNativeWidgetAura::GetCompositor() {
  return const_cast<ui::Compositor*>(
      const_cast<const DesktopNativeWidgetAura*>(this)->GetCompositor());
}

ui::Layer* DesktopNativeWidgetAura::GetLayer() {
  return window_ ? window_->layer() : NULL;
}

void DesktopNativeWidgetAura::ReorderNativeViews() {
  window_reorderer_->ReorderChildWindows();
}

void DesktopNativeWidgetAura::ViewRemoved(View* view) {
}

void DesktopNativeWidgetAura::SetNativeWindowProperty(const char* name,
                                                      void* value) {
  if (window_)
    window_->SetNativeWindowProperty(name, value);
}

void* DesktopNativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return window_ ? window_->GetNativeWindowProperty(name) : NULL;
}

TooltipManager* DesktopNativeWidgetAura::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void DesktopNativeWidgetAura::SetCapture() {
  if (!window_)
    return;

  window_->SetCapture();
  // aura::Window doesn't implicitly update capture on the RootWindowHost, so
  // we have to do that manually.
  if (!desktop_root_window_host_->HasCapture())
    window_->GetRootWindow()->SetNativeCapture();
}

void DesktopNativeWidgetAura::ReleaseCapture() {
  if (!window_)
    return;

  window_->ReleaseCapture();
  // aura::Window doesn't implicitly update capture on the RootWindowHost, so
  // we have to do that manually.
  if (desktop_root_window_host_->HasCapture())
    window_->GetRootWindow()->ReleaseNativeCapture();
}

bool DesktopNativeWidgetAura::HasCapture() const {
  return window_ && window_->HasCapture() &&
      desktop_root_window_host_->HasCapture();
}

InputMethod* DesktopNativeWidgetAura::CreateInputMethod() {
  ui::InputMethod* host = input_method_event_filter_->input_method();
  return new InputMethodBridge(this, host, false);
}

internal::InputMethodDelegate*
    DesktopNativeWidgetAura::GetInputMethodDelegate() {
  return this;
}

void DesktopNativeWidgetAura::CenterWindow(const gfx::Size& size) {
  if (window_)
    desktop_root_window_host_->CenterWindow(size);
}

void DesktopNativeWidgetAura::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  if (window_)
    desktop_root_window_host_->GetWindowPlacement(bounds, maximized);
}

void DesktopNativeWidgetAura::SetWindowTitle(const string16& title) {
  if (window_)
    desktop_root_window_host_->SetWindowTitle(title);
}

void DesktopNativeWidgetAura::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                             const gfx::ImageSkia& app_icon) {
  if (window_)
    desktop_root_window_host_->SetWindowIcons(window_icon, app_icon);
}

void DesktopNativeWidgetAura::InitModalType(ui::ModalType modal_type) {
  // 99% of the time, we should not be asked to create a
  // DesktopNativeWidgetAura that is modal. We only support window modal
  // dialogs on the same lines as non AURA.
  desktop_root_window_host_->InitModalType(modal_type);
}

gfx::Rect DesktopNativeWidgetAura::GetWindowBoundsInScreen() const {
  return window_ ? desktop_root_window_host_->GetWindowBoundsInScreen() :
      gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetClientAreaBoundsInScreen() const {
  return window_ ? desktop_root_window_host_->GetClientAreaBoundsInScreen() :
      gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetRestoredBounds() const {
  return window_ ? desktop_root_window_host_->GetRestoredBounds() : gfx::Rect();
}

void DesktopNativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  if (!window_)
    return;
  // TODO(ananta)
  // This code by default scales the bounds rectangle by 1.
  // We could probably get rid of this and similar logic from
  // the DesktopNativeWidgetAura::OnRootWindowHostResized function.
  float scale = 1;
  aura::RootWindow* root = root_window_.get();
  if (root) {
    scale = gfx::Screen::GetScreenFor(root)->
        GetDisplayNearestWindow(root).device_scale_factor();
  }
  gfx::Rect bounds_in_pixels(
      gfx::ToCeiledPoint(gfx::ScalePoint(bounds.origin(), scale)),
      gfx::ToFlooredSize(gfx::ScaleSize(bounds.size(), scale)));
  desktop_root_window_host_->AsRootWindowHost()->SetBounds(bounds_in_pixels);
}

void DesktopNativeWidgetAura::SetSize(const gfx::Size& size) {
  if (window_)
    desktop_root_window_host_->SetSize(size);
}

void DesktopNativeWidgetAura::StackAbove(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::StackAtTop() {
}

void DesktopNativeWidgetAura::StackBelow(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::SetShape(gfx::NativeRegion shape) {
  if (window_)
    desktop_root_window_host_->SetShape(shape);
}

void DesktopNativeWidgetAura::Close() {
  if (!window_)
    return;
  desktop_root_window_host_->Close();
  window_->SuppressPaint();
}

void DesktopNativeWidgetAura::CloseNow() {
  if (window_)
    desktop_root_window_host_->CloseNow();
}

void DesktopNativeWidgetAura::Show() {
  if (!window_)
    return;
  desktop_root_window_host_->AsRootWindowHost()->Show();
  window_->Show();
}

void DesktopNativeWidgetAura::Hide() {
  if (!window_)
    return;
  desktop_root_window_host_->AsRootWindowHost()->Hide();
  window_->Hide();
}

void DesktopNativeWidgetAura::ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) {
  if (!window_)
    return;
  desktop_root_window_host_->ShowMaximizedWithBounds(restored_bounds);
  window_->Show();
}

void DesktopNativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  if (!window_)
    return;
  desktop_root_window_host_->ShowWindowWithState(state);
  window_->Show();
}

bool DesktopNativeWidgetAura::IsVisible() const {
  return window_ && desktop_root_window_host_->IsVisible();
}

void DesktopNativeWidgetAura::Activate() {
  if (window_)
    desktop_root_window_host_->Activate();
}

void DesktopNativeWidgetAura::Deactivate() {
  if (window_)
    desktop_root_window_host_->Deactivate();
}

bool DesktopNativeWidgetAura::IsActive() const {
  return window_ && desktop_root_window_host_->IsActive();
}

void DesktopNativeWidgetAura::SetAlwaysOnTop(bool always_on_top) {
  if (window_)
    desktop_root_window_host_->SetAlwaysOnTop(always_on_top);
}

bool DesktopNativeWidgetAura::IsAlwaysOnTop() const {
  return window_ && desktop_root_window_host_->IsAlwaysOnTop();
}

void DesktopNativeWidgetAura::Maximize() {
  if (window_)
    desktop_root_window_host_->Maximize();
}

void DesktopNativeWidgetAura::Minimize() {
  if (window_)
    desktop_root_window_host_->Minimize();
}

bool DesktopNativeWidgetAura::IsMaximized() const {
  return window_ && desktop_root_window_host_->IsMaximized();
}

bool DesktopNativeWidgetAura::IsMinimized() const {
  return window_ && desktop_root_window_host_->IsMinimized();
}

void DesktopNativeWidgetAura::Restore() {
  if (window_)
    desktop_root_window_host_->Restore();
}

void DesktopNativeWidgetAura::SetFullscreen(bool fullscreen) {
  if (window_)
    desktop_root_window_host_->SetFullscreen(fullscreen);
}

bool DesktopNativeWidgetAura::IsFullscreen() const {
  return window_ && desktop_root_window_host_->IsFullscreen();
}

void DesktopNativeWidgetAura::SetOpacity(unsigned char opacity) {
  if (window_)
    desktop_root_window_host_->SetOpacity(opacity);
}

void DesktopNativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
}

void DesktopNativeWidgetAura::FlashFrame(bool flash_frame) {
  if (window_)
    desktop_root_window_host_->FlashFrame(flash_frame);
}

void DesktopNativeWidgetAura::RunShellDrag(
    View* view,
    const ui::OSExchangeData& data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  views::RunShellDrag(window_, data, location, operation, source);
}

void DesktopNativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void DesktopNativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

bool DesktopNativeWidgetAura::IsMouseEventsEnabled() const {
  if (!window_)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  return cursor_client ? cursor_client->IsMouseEventsEnabled() : true;
}

void DesktopNativeWidgetAura::ClearNativeFocus() {
  desktop_root_window_host_->ClearNativeFocus();

  if (ShouldActivate()) {
    aura::client::GetFocusClient(window_)->
        ResetFocusWithinActiveWindow(window_);
  }
}

gfx::Rect DesktopNativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return desktop_root_window_host_ ?
      desktop_root_window_host_->GetWorkAreaBoundsInScreen() : gfx::Rect();
}

void DesktopNativeWidgetAura::SetInactiveRenderingDisabled(bool value) {
  if (!window_)
    return;

  if (!value) {
    active_window_observer_.reset();
  } else {
    active_window_observer_.reset(
        new NativeWidgetAuraWindowObserver(window_, native_widget_delegate_));
  }
}

Widget::MoveLoopResult DesktopNativeWidgetAura::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  if (!window_)
    return Widget::MOVE_LOOP_CANCELED;
  return desktop_root_window_host_->RunMoveLoop(drag_offset, source,
                                                escape_behavior);
}

void DesktopNativeWidgetAura::EndMoveLoop() {
  if (window_)
    desktop_root_window_host_->EndMoveLoop();
}

void DesktopNativeWidgetAura::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  if (window_)
    desktop_root_window_host_->SetVisibilityChangedAnimationsEnabled(value);
}

ui::NativeTheme* DesktopNativeWidgetAura::GetNativeTheme() const {
  return DesktopRootWindowHost::GetNativeTheme(window_);
}

void DesktopNativeWidgetAura::OnRootViewLayout() const {
  if (window_)
    desktop_root_window_host_->OnRootViewLayout();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size DesktopNativeWidgetAura::GetMinimumSize() const {
  return native_widget_delegate_->GetMinimumSize();
}

gfx::Size DesktopNativeWidgetAura::GetMaximumSize() const {
  return native_widget_delegate_->GetMaximumSize();
}

gfx::NativeCursor DesktopNativeWidgetAura::GetCursor(const gfx::Point& point) {
  return cursor_;
}

int DesktopNativeWidgetAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

bool DesktopNativeWidgetAura::ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) {
  views::WidgetDelegate* widget_delegate = GetWidget()->widget_delegate();
  return !widget_delegate ||
      widget_delegate->ShouldDescendIntoChildForEventHandling(child, location);
}

bool DesktopNativeWidgetAura::CanFocus() {
  return true;
}

void DesktopNativeWidgetAura::OnCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopNativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  native_widget_delegate_->OnNativeWidgetPaint(canvas);
}

void DesktopNativeWidgetAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopNativeWidgetAura::OnWindowDestroying() {
  // Cleanup happens in OnHostClosed().
}

void DesktopNativeWidgetAura::OnWindowDestroyed() {
  // Cleanup happens in OnHostClosed(). We own |window_| (indirectly by way of
  // |root_window_|) so there should be no need to do any processing here.
}

void DesktopNativeWidgetAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool DesktopNativeWidgetAura::HasHitTestMask() const {
  return native_widget_delegate_->HasHitTestMask();
}

void DesktopNativeWidgetAura::GetHitTestMask(gfx::Path* mask) const {
  native_widget_delegate_->GetHitTestMask(mask);
}

void DesktopNativeWidgetAura::DidRecreateLayer(ui::Layer* old_layer,
                                               ui::Layer* new_layer) {}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, ui::EventHandler implementation:

void DesktopNativeWidgetAura::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attached to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return;
  }
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!window_->IsVisible())
    return;

  native_widget_delegate_->OnKeyEvent(event);
  if (event->handled())
    return;

  if (GetWidget()->HasFocusManager() &&
      !GetWidget()->GetFocusManager()->OnKeyEvent(*event))
    event->SetHandled();
}

void DesktopNativeWidgetAura::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(window_->IsVisible());
  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  native_widget_delegate_->OnMouseEvent(event);
  // WARNING: we may have been deleted.
}

void DesktopNativeWidgetAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL) {
    native_widget_delegate_->OnScrollEvent(event);
    if (event->handled())
      return;

    // Convert unprocessed scroll events into wheel events.
    ui::MouseWheelEvent mwe(*static_cast<ui::ScrollEvent*>(event));
    native_widget_delegate_->OnMouseEvent(&mwe);
    if (mwe.handled())
      event->SetHandled();
  } else {
    native_widget_delegate_->OnScrollEvent(event);
  }
}

void DesktopNativeWidgetAura::OnTouchEvent(ui::TouchEvent* event) {
  native_widget_delegate_->OnTouchEvent(event);
}

void DesktopNativeWidgetAura::OnGestureEvent(ui::GestureEvent* event) {
  native_widget_delegate_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::ActivationDelegate implementation:

bool DesktopNativeWidgetAura::ShouldActivate() const {
  return can_activate_ && native_widget_delegate_->CanActivate();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::ActivationChangeObserver
//    implementation:

void DesktopNativeWidgetAura::OnWindowActivated(aura::Window* gained_active,
                                                aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if ((window_ == gained_active || window_ == lost_active) &&
      IsVisible() && GetWidget()->non_client_view()) {
    GetWidget()->non_client_view()->SchedulePaint();
  }
  if (gained_active == window_ && restore_focus_on_activate_) {
    restore_focus_on_activate_ = false;
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else if (lost_active == window_ && GetWidget()->HasFocusManager()) {
    DCHECK(!restore_focus_on_activate_);
    restore_focus_on_activate_ = true;
    // Pass in false so that ClearNativeFocus() isn't invoked.
    GetWidget()->GetFocusManager()->StoreFocusedView(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::FocusChangeObserver implementation:

void DesktopNativeWidgetAura::OnWindowFocused(aura::Window* gained_focus,
                                              aura::Window* lost_focus) {
  if (window_ == gained_focus) {
    desktop_root_window_host_->OnNativeWidgetFocus();
    native_widget_delegate_->OnNativeFocus(lost_focus);

    // If focus is moving from a descendant Window to |window_| then native
    // activation hasn't changed. We still need to inform the InputMethod we've
    // been focused though.
    InputMethod* input_method = GetWidget()->GetInputMethod();
    if (input_method)
      input_method->OnFocus();
  } else if (window_ == lost_focus) {
    desktop_root_window_host_->OnNativeWidgetBlur();
    native_widget_delegate_->OnNativeBlur(
        aura::client::GetFocusClient(window_)->GetFocusedWindow());
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, views::internal::InputMethodDelegate:

void DesktopNativeWidgetAura::DispatchKeyEventPostIME(const ui::KeyEvent& key) {
  FocusManager* focus_manager =
      native_widget_delegate_->AsWidget()->GetFocusManager();
  native_widget_delegate_->OnKeyEvent(const_cast<ui::KeyEvent*>(&key));
  if (key.handled() || !focus_manager)
    return;
  focus_manager->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDragDropDelegate implementation:

void DesktopNativeWidgetAura::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
}

int DesktopNativeWidgetAura::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
  return last_drop_operation_;
}

void DesktopNativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->OnDragExit();
}

int DesktopNativeWidgetAura::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  return drop_helper_->OnDrop(event.data(), event.location(),
      last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::RootWindowObserver implementation:

void DesktopNativeWidgetAura::OnRootWindowHostCloseRequested(
    const aura::RootWindow* root) {
  Close();
}

void DesktopNativeWidgetAura::OnRootWindowHostResized(
    const aura::RootWindow* root) {
  gfx::Rect new_bounds = gfx::Rect(root->bounds().size());
  // TODO(ananta)
  // This code by default scales the bounds rectangle by 1.
  // We could probably get rid of this and similar logic from
  // the DesktopNativeWidgetAura::SetBounds function.
#if defined(OS_WIN)
  gfx::Size dip_size = gfx::win::ScreenToDIPSize(new_bounds.size());
  new_bounds = gfx::Rect(dip_size);
#endif
  window_->SetBounds(new_bounds);
  // Can be NULL at start.
  if (content_window_container_)
    content_window_container_->SetBounds(new_bounds);
  native_widget_delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void DesktopNativeWidgetAura::OnRootWindowHostMoved(
    const aura::RootWindow* root,
    const gfx::Point& new_origin) {
  native_widget_delegate_->OnNativeWidgetMove();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, NativeWidget implementation:

ui::EventHandler* DesktopNativeWidgetAura::GetEventHandler() {
  return this;
}

void DesktopNativeWidgetAura::UpdateWindowTransparency() {
  window_->SetTransparent(ShouldUseNativeFrame());
}

}  // namespace views
