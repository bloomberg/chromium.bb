// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/drag_utils.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_capture_client.h"
#include "ui/views/widget/desktop_aura/desktop_event_client.h"
#include "ui/views/widget/desktop_aura/desktop_focus_rules.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/focus_manager_event_handler.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/window_reorderer.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_controller_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/public/activation_client.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::DesktopNativeWidgetAura*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopNativeWidgetAura*,
                             kDesktopNativeWidgetAuraKey,
                             NULL)

namespace {

// This class provides functionality to create a top level widget to host a
// child window.
class DesktopNativeWidgetTopLevelHandler : public aura::WindowObserver {
 public:
  // This function creates a widget with the bounds passed in which eventually
  // becomes the parent of the child window passed in.
  static aura::Window* CreateParentWindow(aura::Window* child_window,
                                          const gfx::Rect& bounds,
                                          bool full_screen,
                                          bool is_menu,
                                          ui::ZOrderLevel root_z_order) {
    // This instance will get deleted when the widget is destroyed.
    DesktopNativeWidgetTopLevelHandler* top_level_handler =
        new DesktopNativeWidgetTopLevelHandler;

    child_window->SetBounds(gfx::Rect(bounds.size()));

    Widget::InitParams init_params;
    init_params.type = full_screen ? Widget::InitParams::TYPE_WINDOW
                                   : is_menu ? Widget::InitParams::TYPE_MENU
                                             : Widget::InitParams::TYPE_POPUP;
#if defined(OS_WIN)
    // For menus, on Windows versions that support drop shadow remove
    // the standard frame in order to keep just the shadow.
    if (::features::IsFormControlsRefreshEnabled() &&
        init_params.type == Widget::InitParams::TYPE_MENU)
      init_params.remove_standard_frame = true;
#endif
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    init_params.layer_type = ui::LAYER_NOT_DRAWN;
    init_params.activatable = full_screen ? Widget::InitParams::ACTIVATABLE_YES
                                          : Widget::InitParams::ACTIVATABLE_NO;
    init_params.z_order = root_z_order;

    // This widget instance will get deleted when the window is
    // destroyed.
    top_level_handler->top_level_widget_ = new Widget();
    // Ensure that we always use the DesktopNativeWidgetAura instance as the
    // native widget here. If we enter this code path in tests then it is
    // possible that we may end up with a NativeWidgetAura instance as the
    // native widget which breaks this code path.
    init_params.native_widget =
        new DesktopNativeWidgetAura(top_level_handler->top_level_widget_);
    top_level_handler->top_level_widget_->Init(std::move(init_params));

    top_level_handler->top_level_widget_->SetFullscreen(full_screen);
    top_level_handler->top_level_widget_->Show();

    aura::Window* native_window =
        top_level_handler->top_level_widget_->GetNativeView();
    child_window->AddObserver(top_level_handler);
    native_window->AddObserver(top_level_handler);
    top_level_handler->child_window_ = child_window;
    return native_window;
  }

  // aura::WindowObserver overrides
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);

    // If the widget is being destroyed by the OS then we should not try and
    // destroy it again.
    if (top_level_widget_ && window == top_level_widget_->GetNativeView()) {
      top_level_widget_ = nullptr;
      return;
    }

    if (top_level_widget_) {
      DCHECK(top_level_widget_->GetNativeView());
      top_level_widget_->GetNativeView()->RemoveObserver(this);
      // When we receive a notification that the child of the window created
      // above is being destroyed we go ahead and initiate the destruction of
      // the corresponding widget.
      top_level_widget_->Close();
      top_level_widget_ = nullptr;
    }
    delete this;
  }

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    // The position of the window may have changed. Hence we use SetBounds in
    // place of SetSize. We need to pass the bounds in screen coordinates to
    // the Widget::SetBounds function.
    if (top_level_widget_ && window == child_window_)
      top_level_widget_->SetBounds(window->GetBoundsInScreen());
  }

 private:
  DesktopNativeWidgetTopLevelHandler() = default;

  ~DesktopNativeWidgetTopLevelHandler() override = default;

  Widget* top_level_widget_ = nullptr;
  aura::Window* child_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetTopLevelHandler);
};

class DesktopNativeWidgetAuraWindowParentingClient
    : public aura::client::WindowParentingClient {
 public:
  explicit DesktopNativeWidgetAuraWindowParentingClient(
      aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowParentingClient(root_window_, this);
  }
  ~DesktopNativeWidgetAuraWindowParentingClient() override {
    aura::client::SetWindowParentingClient(root_window_, nullptr);
  }

  // Overridden from client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override {
    bool is_fullscreen = window->GetProperty(aura::client::kShowStateKey) ==
                         ui::SHOW_STATE_FULLSCREEN;
    bool is_menu = window->type() == aura::client::WINDOW_TYPE_MENU;

    if (is_fullscreen || is_menu) {
      ui::ZOrderLevel root_z_order = ui::ZOrderLevel::kNormal;
      internal::NativeWidgetPrivate* native_widget =
          DesktopNativeWidgetAura::ForWindow(root_window_);
      if (native_widget)
        root_z_order = native_widget->GetZOrderLevel();

      return DesktopNativeWidgetTopLevelHandler::CreateParentWindow(
          window, bounds, is_fullscreen, is_menu, root_z_order);
    }
    return root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetAuraWindowParentingClient);
};

}  // namespace

class RootWindowDestructionObserver : public aura::WindowObserver {
 public:
  explicit RootWindowDestructionObserver(DesktopNativeWidgetAura* parent)
      : parent_(parent) {}
  ~RootWindowDestructionObserver() override = default;

 private:
  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    parent_->RootWindowDestroyed();
    window->RemoveObserver(this);
    delete this;
  }

  DesktopNativeWidgetAura* parent_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowDestructionObserver);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, public:

int DesktopNativeWidgetAura::cursor_reference_count_ = 0;
DesktopNativeCursorManager* DesktopNativeWidgetAura::native_cursor_manager_ =
    nullptr;
wm::CursorManager* DesktopNativeWidgetAura::cursor_manager_ = nullptr;

DesktopNativeWidgetAura::DesktopNativeWidgetAura(
    internal::NativeWidgetDelegate* delegate)
    : desktop_window_tree_host_(nullptr),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      content_window_(new aura::Window(this)),
      native_widget_delegate_(delegate),
      last_drop_operation_(ui::DragDropTypes::DRAG_NONE),
      restore_focus_on_activate_(false),
      cursor_(gfx::kNullCursor),
      widget_type_(Widget::InitParams::TYPE_WINDOW) {
  aura::client::SetFocusChangeObserver(content_window_, this);
  wm::SetActivationChangeObserver(content_window_, this);
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

void DesktopNativeWidgetAura::SetDesktopWindowTreeHost(
    std::unique_ptr<DesktopWindowTreeHost> desktop_window_tree_host) {
  DCHECK(!desktop_window_tree_host_);
  desktop_window_tree_host_ = desktop_window_tree_host.get();
  host_.reset(desktop_window_tree_host.release()->AsWindowTreeHost());
}

void DesktopNativeWidgetAura::OnHostClosed() {
  // Don't invoke Widget::OnNativeWidgetDestroying(), its done by
  // DesktopWindowTreeHost.

  // The WindowModalityController is at the front of the event pretarget
  // handler list. We destroy it first to preserve order symantics.
  if (window_modality_controller_)
    window_modality_controller_.reset();

  // Make sure we don't have capture. Otherwise CaptureController and
  // WindowEventDispatcher are left referencing a deleted Window.
  {
    aura::Window* capture_window = capture_client_->GetCaptureWindow();
    if (capture_window && host_->window()->Contains(capture_window))
      capture_window->ReleaseCapture();
  }

  // DesktopWindowTreeHost owns the ActivationController which ShadowController
  // references. Make sure we destroy ShadowController early on.
  shadow_controller_.reset();
  tooltip_manager_.reset();
  if (tooltip_controller_.get()) {
    host_->window()->RemovePreTargetHandler(tooltip_controller_.get());
    wm::SetTooltipClient(host_->window(), nullptr);
    tooltip_controller_.reset();
  }

  window_parenting_client_.reset();  // Uses host_->dispatcher() at destruction.

  capture_client_.reset();  // Uses host_->dispatcher() at destruction.

  focus_manager_event_handler_.reset();

  // FocusController uses |content_window_|. Destroy it now so that we don't
  // have to worry about the possibility of FocusController attempting to use
  // |content_window_| after it's been destroyed but before all child windows
  // have been destroyed.
  host_->window()->RemovePreTargetHandler(focus_client_.get());
  aura::client::SetFocusClient(host_->window(), nullptr);
  wm::SetActivationClient(host_->window(), nullptr);
  focus_client_.reset();

  host_->window()->RemovePreTargetHandler(root_window_event_filter_.get());

  host_->RemoveObserver(this);
  host_.reset();
  // WindowEventDispatcher owns |desktop_window_tree_host_|.
  desktop_window_tree_host_ = nullptr;
  content_window_ = nullptr;

  // |OnNativeWidgetDestroyed| may delete |this| if the object does not own
  // itself.
  bool should_delete_this =
      (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  native_widget_delegate_->OnNativeWidgetDestroyed();
  if (should_delete_this)
    delete this;
}

void DesktopNativeWidgetAura::OnDesktopWindowTreeHostDestroyed(
    aura::WindowTreeHost* host) {
  if (use_desktop_native_cursor_manager_) {
    // We explicitly do NOT clear the cursor client property. Since the cursor
    // manager is a singleton, it can outlive any window hierarchy, and it's
    // important that objects attached to this destroying window hierarchy have
    // an opportunity to deregister their observers from the cursor manager.
    // They may want to do this when they are notified that they're being
    // removed from the window hierarchy, which happens soon after this
    // function when DesktopWindowTreeHost* calls DestroyDispatcher().
    native_cursor_manager_->RemoveHost(host);
  }

  aura::client::SetScreenPositionClient(host->window(), nullptr);
  position_client_.reset();

  aura::client::SetDragDropClient(host->window(), nullptr);
  drag_drop_client_.reset();

  aura::client::SetEventClient(host->window(), nullptr);
  event_client_.reset();
}

void DesktopNativeWidgetAura::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  if (!GetWidget() || !GetWidget()->GetRootView())
    return;
  GetWidget()->GetRootView()->NotifyAccessibilityEvent(event_type, true);
}

void DesktopNativeWidgetAura::HandleActivationChanged(bool active) {
  if (!native_widget_delegate_->OnNativeWidgetActivationChanged(active))
    return;
  wm::ActivationClient* activation_client =
      wm::GetActivationClient(host_->window());
  if (!activation_client)
    return;
  if (active) {
    // TODO(nektar): We need to harmonize the firing of accessibility
    // events between platforms.
    // https://crbug.com/897177
    NotifyAccessibilityEvent(ax::mojom::Event::kWindowActivated);

    if (GetWidget()->HasFocusManager()) {
      // This function can be called before the focus manager has had a
      // chance to set the focused view. In which case we should get the
      // last focused view.
      views::FocusManager* focus_manager = GetWidget()->GetFocusManager();
      View* view_for_activation = focus_manager->GetFocusedView()
                                      ? focus_manager->GetFocusedView()
                                      : focus_manager->GetStoredFocusView();
      if (!view_for_activation || !view_for_activation->GetWidget()) {
        view_for_activation = GetWidget()->GetRootView();
      } else if (view_for_activation == focus_manager->GetStoredFocusView()) {
        // When desktop native widget has modal transient child, we don't
        // restore focused view here, as the modal transient child window will
        // get activated and focused. Thus, we are not left with multiple
        // focuses. For aura child widgets, since their views are managed by
        // |focus_manager|, we then allow restoring focused view.
        if (!wm::GetModalTransient(GetWidget()->GetNativeView())) {
          focus_manager->RestoreFocusedView();
          // Set to false if desktop native widget has activated activation
          // change, so that aura window activation change focus restore
          // operation can be ignored.
          restore_focus_on_activate_ = false;
        }
      }
      activation_client->ActivateWindow(
          view_for_activation->GetWidget()->GetNativeView());
      // Refreshes the focus info to IMF in case that IMF cached the old info
      // about focused text input client when it was "inactive".
      GetInputMethod()->OnFocus();
    }
  } else {
    // TODO(nektar): We need to harmonize the firing of accessibility
    // events between platforms.
    // https://crbug.com/897177
    NotifyAccessibilityEvent(ax::mojom::Event::kWindowDeactivated);

    // If we're not active we need to deactivate the corresponding
    // aura::Window. This way if a child widget is active it gets correctly
    // deactivated (child widgets don't get native desktop activation changes,
    // only aura activation changes).
    aura::Window* active_window = activation_client->GetActiveWindow();
    if (active_window) {
      activation_client->DeactivateWindow(active_window);
      GetInputMethod()->OnBlur();
    }
  }
}

gfx::NativeWindow DesktopNativeWidgetAura::GetNativeWindow() const {
  return content_window_;
}

void DesktopNativeWidgetAura::UpdateWindowTransparency() {
  if (!desktop_window_tree_host_->ShouldUpdateWindowTransparency())
    return;

  const bool transparent =
      desktop_window_tree_host_->ShouldWindowContentsBeTransparent();

  auto* window_tree_host = desktop_window_tree_host_->AsWindowTreeHost();
  window_tree_host->compositor()->SetBackgroundColor(
      transparent ? SK_ColorTRANSPARENT : SK_ColorWHITE);
  window_tree_host->window()->SetTransparent(transparent);

  content_window_->SetTransparent(transparent);
  // Regardless of transparency or not, this root content window will always
  // fill its bounds completely, so set this flag to true to avoid an
  // unecessary clear before update.
  content_window_->SetFillsBoundsCompletely(true);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, internal::NativeWidgetPrivate implementation:

void DesktopNativeWidgetAura::InitNativeWidget(Widget::InitParams params) {
  ownership_ = params.ownership;
  widget_type_ = params.type;
  name_ = params.name;

  content_window_->AcquireAllPropertiesFrom(
      std::move(params.init_properties_container));

  NativeWidgetAura::RegisterNativeWidgetForWindow(this, content_window_);
  content_window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  content_window_->Init(params.layer_type);
  wm::SetShadowElevation(content_window_, wm::kShadowElevationNone);

  if (!desktop_window_tree_host_) {
    if (params.desktop_window_tree_host) {
      desktop_window_tree_host_ = params.desktop_window_tree_host;
    } else {
      desktop_window_tree_host_ =
          DesktopWindowTreeHost::Create(native_widget_delegate_, this);
    }
    host_.reset(desktop_window_tree_host_->AsWindowTreeHost());
  }
  desktop_window_tree_host_->Init(params);

  host_->window()->AddChild(content_window_);
  host_->window()->SetProperty(kDesktopNativeWidgetAuraKey, this);

  host_->window()->AddObserver(new RootWindowDestructionObserver(this));

  // The WindowsModalityController event filter should be at the head of the
  // pre target handlers list. This ensures that it handles input events first
  // when modal windows are at the top of the Zorder.
  if (widget_type_ == Widget::InitParams::TYPE_WINDOW)
    window_modality_controller_ =
        std::make_unique<wm::WindowModalityController>(host_->window());

  // |root_window_event_filter_| must be created before
  // OnWindowTreeHostCreated() is invoked.

  // CEF sets focus to the window the user clicks down on.
  // TODO(beng): see if we can't do this some other way. CEF seems a heavy-
  //             handed way of accomplishing focus.
  // No event filter for aura::Env. Create CompoundEventFilter per
  // WindowEventDispatcher.
  root_window_event_filter_ = std::make_unique<wm::CompoundEventFilter>();
  host_->window()->AddPreTargetHandler(root_window_event_filter_.get());

  use_desktop_native_cursor_manager_ =
      desktop_window_tree_host_->ShouldUseDesktopNativeCursorManager();
  if (use_desktop_native_cursor_manager_) {
    // The host's dispatcher must be added to |native_cursor_manager_| before
    // OnNativeWidgetCreated() is called.
    cursor_reference_count_++;
    if (!native_cursor_manager_)
      native_cursor_manager_ = new DesktopNativeCursorManager();
    if (!cursor_manager_) {
      cursor_manager_ = new wm::CursorManager(
          std::unique_ptr<wm::NativeCursorManager>(native_cursor_manager_));
    }
    native_cursor_manager_->AddHost(host());
    aura::client::SetCursorClient(host_->window(), cursor_manager_);
  }

  host_->window()->SetName(params.name);
  content_window_->SetName("DesktopNativeWidgetAura - content window");
  desktop_window_tree_host_->OnNativeWidgetCreated(params);

  UpdateWindowTransparency();

  capture_client_ = std::make_unique<DesktopCaptureClient>(host_->window());

  wm::FocusController* focus_controller =
      new wm::FocusController(new DesktopFocusRules(content_window_));
  focus_client_.reset(focus_controller);
  aura::client::SetFocusClient(host_->window(), focus_controller);
  wm::SetActivationClient(host_->window(), focus_controller);
  host_->window()->AddPreTargetHandler(focus_controller);

  position_client_ = desktop_window_tree_host_->CreateScreenPositionClient();

  drag_drop_client_ =
      desktop_window_tree_host_->CreateDragDropClient(native_cursor_manager_);
  // Mus returns null from CreateDragDropClient().
  if (drag_drop_client_)
    aura::client::SetDragDropClient(host_->window(), drag_drop_client_.get());

  wm::SetActivationDelegate(content_window_, this);
  aura::client::GetFocusClient(content_window_)->FocusWindow(content_window_);

  OnHostResized(host());

  host_->AddObserver(this);

  window_parenting_client_ =
      std::make_unique<DesktopNativeWidgetAuraWindowParentingClient>(
          host_->window());
  drop_helper_ = std::make_unique<DropHelper>(GetWidget()->GetRootView());
  aura::client::SetDragDropDelegate(content_window_, this);

  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    tooltip_manager_ = std::make_unique<TooltipManagerAura>(GetWidget());
    tooltip_controller_ = std::make_unique<corewm::TooltipController>(

        desktop_window_tree_host_->CreateTooltip());
    wm::SetTooltipClient(host_->window(), tooltip_controller_.get());
    host_->window()->AddPreTargetHandler(tooltip_controller_.get());
  }

  if (params.opacity == Widget::InitParams::WindowOpacity::kTranslucent &&
      desktop_window_tree_host_->ShouldCreateVisibilityController()) {
    visibility_controller_ = std::make_unique<wm::VisibilityController>();
    aura::client::SetVisibilityClient(host_->window(),
                                      visibility_controller_.get());
    wm::SetChildWindowVisibilityChangesAnimated(host_->window());
  }

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    focus_manager_event_handler_ = std::make_unique<FocusManagerEventHandler>(
        GetWidget(), host_->window());
  }

  event_client_ = std::make_unique<DesktopEventClient>();
  aura::client::SetEventClient(host_->window(), event_client_.get());

  shadow_controller_ = std::make_unique<wm::ShadowController>(
      wm::GetActivationClient(host_->window()), nullptr);

  OnSizeConstraintsChanged();

  window_reorderer_ = std::make_unique<WindowReorderer>(
      content_window_, GetWidget()->GetRootView());
}

void DesktopNativeWidgetAura::OnWidgetInitDone() {
  desktop_window_tree_host_->OnWidgetInitDone();
}

NonClientFrameView* DesktopNativeWidgetAura::CreateNonClientFrameView() {
  return desktop_window_tree_host_->CreateNonClientFrameView();
}

bool DesktopNativeWidgetAura::ShouldUseNativeFrame() const {
  return desktop_window_tree_host_->ShouldUseNativeFrame();
}

bool DesktopNativeWidgetAura::ShouldWindowContentsBeTransparent() const {
  return desktop_window_tree_host_->ShouldWindowContentsBeTransparent();
}

void DesktopNativeWidgetAura::FrameTypeChanged() {
  desktop_window_tree_host_->FrameTypeChanged();
  UpdateWindowTransparency();
}

Widget* DesktopNativeWidgetAura::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopNativeWidgetAura::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView DesktopNativeWidgetAura::GetNativeView() const {
  return content_window_;
}

Widget* DesktopNativeWidgetAura::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* DesktopNativeWidgetAura::GetCompositor() const {
  return content_window_ ? content_window_->layer()->GetCompositor() : nullptr;
}

const ui::Layer* DesktopNativeWidgetAura::GetLayer() const {
  return content_window_ ? content_window_->layer() : nullptr;
}

void DesktopNativeWidgetAura::ReorderNativeViews() {
  if (!content_window_)
    return;

  // Reordering native views causes multiple changes to the window tree.
  // Instantiate a ScopedPause to recompute occlusion once at the end of this
  // scope rather than after each individual change.
  // https://crbug.com/829918
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
  window_reorderer_->ReorderChildWindows();
}

void DesktopNativeWidgetAura::ViewRemoved(View* view) {
  DCHECK(drop_helper_.get() != nullptr);
  drop_helper_->ResetTargetViewIfEquals(view);
}

void DesktopNativeWidgetAura::SetNativeWindowProperty(const char* name,
                                                      void* value) {
  if (content_window_)
    content_window_->SetNativeWindowProperty(name, value);
}

void* DesktopNativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return content_window_ ? content_window_->GetNativeWindowProperty(name)
                         : nullptr;
}

TooltipManager* DesktopNativeWidgetAura::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void DesktopNativeWidgetAura::SetCapture() {
  if (!content_window_)
    return;

  content_window_->SetCapture();
}

void DesktopNativeWidgetAura::ReleaseCapture() {
  if (!content_window_)
    return;

  content_window_->ReleaseCapture();
}

bool DesktopNativeWidgetAura::HasCapture() const {
  return content_window_ && content_window_->HasCapture() &&
         desktop_window_tree_host_->HasCapture();
}

ui::InputMethod* DesktopNativeWidgetAura::GetInputMethod() {
  return host() ? host()->GetInputMethod() : nullptr;
}

void DesktopNativeWidgetAura::CenterWindow(const gfx::Size& size) {
  if (content_window_)
    desktop_window_tree_host_->CenterWindow(size);
}

void DesktopNativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* maximized) const {
  if (content_window_)
    desktop_window_tree_host_->GetWindowPlacement(bounds, maximized);
}

bool DesktopNativeWidgetAura::SetWindowTitle(const base::string16& title) {
  if (!content_window_)
    return false;
  return desktop_window_tree_host_->SetWindowTitle(title);
}

void DesktopNativeWidgetAura::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                             const gfx::ImageSkia& app_icon) {
  if (content_window_)
    desktop_window_tree_host_->SetWindowIcons(window_icon, app_icon);

  NativeWidgetAura::AssignIconToAuraWindow(content_window_, window_icon,
                                           app_icon);
}

void DesktopNativeWidgetAura::InitModalType(ui::ModalType modal_type) {
  // 99% of the time, we should not be asked to create a
  // DesktopNativeWidgetAura that is modal. We only support window modal
  // dialogs on the same lines as non AURA.
  desktop_window_tree_host_->InitModalType(modal_type);
}

gfx::Rect DesktopNativeWidgetAura::GetWindowBoundsInScreen() const {
  return content_window_ ? desktop_window_tree_host_->GetWindowBoundsInScreen()
                         : gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetClientAreaBoundsInScreen() const {
  return content_window_
             ? desktop_window_tree_host_->GetClientAreaBoundsInScreen()
             : gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetRestoredBounds() const {
  return content_window_ ? desktop_window_tree_host_->GetRestoredBounds()
                         : gfx::Rect();
}

std::string DesktopNativeWidgetAura::GetWorkspace() const {
  return content_window_ ? desktop_window_tree_host_->GetWorkspace()
                         : std::string();
}

void DesktopNativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  if (!content_window_)
    return;
  desktop_window_tree_host_->SetBoundsInDIP(bounds);
}

void DesktopNativeWidgetAura::SetBoundsConstrained(const gfx::Rect& bounds) {
  if (!content_window_)
    return;
  SetBounds(NativeWidgetPrivate::ConstrainBoundsToDisplayWorkArea(bounds));
}

void DesktopNativeWidgetAura::SetSize(const gfx::Size& size) {
  if (content_window_)
    desktop_window_tree_host_->SetSize(size);
}

void DesktopNativeWidgetAura::StackAbove(gfx::NativeView native_view) {
  if (content_window_)
    desktop_window_tree_host_->StackAbove(native_view);
}

void DesktopNativeWidgetAura::StackAtTop() {
  if (content_window_)
    desktop_window_tree_host_->StackAtTop();
}

void DesktopNativeWidgetAura::SetShape(
    std::unique_ptr<Widget::ShapeRects> shape) {
  if (content_window_)
    desktop_window_tree_host_->SetShape(std::move(shape));
}

void DesktopNativeWidgetAura::Close() {
  if (content_window_)
    desktop_window_tree_host_->Close();
}

void DesktopNativeWidgetAura::CloseNow() {
  if (content_window_)
    desktop_window_tree_host_->CloseNow();
}

void DesktopNativeWidgetAura::Show(ui::WindowShowState show_state,
                                   const gfx::Rect& restore_bounds) {
  if (!content_window_)
    return;
  desktop_window_tree_host_->Show(show_state, restore_bounds);
}

void DesktopNativeWidgetAura::Hide() {
  if (!content_window_)
    return;
  desktop_window_tree_host_->AsWindowTreeHost()->Hide();
  content_window_->Hide();
}

bool DesktopNativeWidgetAura::IsVisible() const {
  // The objects checked here should be the same objects changed in
  // ShowWithWindowState and ShowMaximizedWithBounds. For example, MS Windows
  // platform code might show the desktop window tree host early, meaning we
  // aren't fully visible as we haven't shown the content window. Callers may
  // short-circuit a call to show this widget if they think its already visible.
  return content_window_ && content_window_->TargetVisibility() &&
         desktop_window_tree_host_->IsVisible();
}

void DesktopNativeWidgetAura::Activate() {
  if (content_window_) {
    bool was_active = IsActive();
    desktop_window_tree_host_->Activate();

    // If the whole window tree host was already active,
    // treat this as a request to focus |content_window_|.
    //
    // Note: it might make sense to always focus |content_window_|,
    // since if client code is calling Widget::Activate() they probably
    // want that particular widget to be activated, not just something
    // within that widget hierarchy.
    if (was_active && focus_client_->GetFocusedWindow() != content_window_)
      focus_client_->FocusWindow(content_window_);
  }
}

void DesktopNativeWidgetAura::Deactivate() {
  if (content_window_)
    desktop_window_tree_host_->Deactivate();
}

bool DesktopNativeWidgetAura::IsActive() const {
  return content_window_ && desktop_window_tree_host_->IsActive();
}

void DesktopNativeWidgetAura::SetZOrderLevel(ui::ZOrderLevel order) {
  if (content_window_)
    desktop_window_tree_host_->SetZOrderLevel(order);
}

ui::ZOrderLevel DesktopNativeWidgetAura::GetZOrderLevel() const {
  if (content_window_)
    return desktop_window_tree_host_->GetZOrderLevel();
  return ui::ZOrderLevel::kNormal;
}

void DesktopNativeWidgetAura::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (content_window_)
    desktop_window_tree_host_->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopNativeWidgetAura::IsVisibleOnAllWorkspaces() const {
  return content_window_ &&
         desktop_window_tree_host_->IsVisibleOnAllWorkspaces();
}

void DesktopNativeWidgetAura::Maximize() {
  if (content_window_)
    desktop_window_tree_host_->Maximize();
}

void DesktopNativeWidgetAura::Minimize() {
  if (content_window_)
    desktop_window_tree_host_->Minimize();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(GetWidget()->GetRootView());
  root_view->ResetEventHandlers();
}

bool DesktopNativeWidgetAura::IsMaximized() const {
  return content_window_ && desktop_window_tree_host_->IsMaximized();
}

bool DesktopNativeWidgetAura::IsMinimized() const {
  return content_window_ && desktop_window_tree_host_->IsMinimized();
}

void DesktopNativeWidgetAura::Restore() {
  if (content_window_)
    desktop_window_tree_host_->Restore();
}

void DesktopNativeWidgetAura::SetFullscreen(bool fullscreen) {
  if (content_window_)
    desktop_window_tree_host_->SetFullscreen(fullscreen);
}

bool DesktopNativeWidgetAura::IsFullscreen() const {
  return content_window_ && desktop_window_tree_host_->IsFullscreen();
}

void DesktopNativeWidgetAura::SetCanAppearInExistingFullscreenSpaces(
    bool can_appear_in_existing_fullscreen_spaces) {}

void DesktopNativeWidgetAura::SetOpacity(float opacity) {
  if (content_window_)
    desktop_window_tree_host_->SetOpacity(opacity);
}

void DesktopNativeWidgetAura::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (content_window_)
    desktop_window_tree_host_->SetAspectRatio(aspect_ratio);
}

void DesktopNativeWidgetAura::FlashFrame(bool flash_frame) {
  if (content_window_)
    desktop_window_tree_host_->FlashFrame(flash_frame);
}

void DesktopNativeWidgetAura::RunShellDrag(
    View* view,
    std::unique_ptr<ui::OSExchangeData> data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  views::RunShellDrag(content_window_, std::move(data), location, operation,
                      source);
}

void DesktopNativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (content_window_)
    content_window_->SchedulePaintInRect(rect);
}

void DesktopNativeWidgetAura::ScheduleLayout() {
  // ScheduleDraw() triggers a callback to
  // WindowDelegate::UpdateVisualState().
  if (content_window_)
    content_window_->ScheduleDraw();
}

void DesktopNativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

bool DesktopNativeWidgetAura::IsMouseEventsEnabled() const {
  // We explicitly check |host_| here because it can be null during the process
  // of widget shutdown (even if |content_window_| is not), and must be valid to
  // determine if mouse events are enabled.
  if (!content_window_ || !host_)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  return cursor_client ? cursor_client->IsMouseEventsEnabled() : true;
}

bool DesktopNativeWidgetAura::IsMouseButtonDown() const {
  return aura::Env::GetInstance()->IsMouseButtonDown();
}

void DesktopNativeWidgetAura::ClearNativeFocus() {
  desktop_window_tree_host_->ClearNativeFocus();

  if (ShouldActivate()) {
    aura::client::GetFocusClient(content_window_)
        ->ResetFocusWithinActiveWindow(content_window_);
  }
}

gfx::Rect DesktopNativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return desktop_window_tree_host_
             ? desktop_window_tree_host_->GetWorkAreaBoundsInScreen()
             : gfx::Rect();
}

Widget::MoveLoopResult DesktopNativeWidgetAura::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  if (!content_window_)
    return Widget::MOVE_LOOP_CANCELED;
  return desktop_window_tree_host_->RunMoveLoop(drag_offset, source,
                                                escape_behavior);
}

void DesktopNativeWidgetAura::EndMoveLoop() {
  if (content_window_)
    desktop_window_tree_host_->EndMoveLoop();
}

void DesktopNativeWidgetAura::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  if (content_window_)
    desktop_window_tree_host_->SetVisibilityChangedAnimationsEnabled(value);
}

void DesktopNativeWidgetAura::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  wm::SetWindowVisibilityAnimationDuration(content_window_, duration);
}

void DesktopNativeWidgetAura::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  wm::WindowVisibilityAnimationTransition wm_transition = wm::ANIMATE_NONE;
  switch (transition) {
    case Widget::ANIMATE_SHOW:
      wm_transition = wm::ANIMATE_SHOW;
      break;
    case Widget::ANIMATE_HIDE:
      wm_transition = wm::ANIMATE_HIDE;
      break;
    case Widget::ANIMATE_BOTH:
      wm_transition = wm::ANIMATE_BOTH;
      break;
    case Widget::ANIMATE_NONE:
      wm_transition = wm::ANIMATE_NONE;
      break;
  }
  wm::SetWindowVisibilityAnimationTransition(content_window_, wm_transition);
}

bool DesktopNativeWidgetAura::IsTranslucentWindowOpacitySupported() const {
  return content_window_ &&
         desktop_window_tree_host_->IsTranslucentWindowOpacitySupported();
}

ui::GestureRecognizer* DesktopNativeWidgetAura::GetGestureRecognizer() {
  return aura::Env::GetInstance()->gesture_recognizer();
}

void DesktopNativeWidgetAura::OnSizeConstraintsChanged() {
  NativeWidgetAura::SetResizeBehaviorFromDelegate(
      GetWidget()->widget_delegate(), content_window_);
  desktop_window_tree_host_->SizeConstraintsChanged();
}

void DesktopNativeWidgetAura::OnNativeViewHierarchyWillChange() {}

void DesktopNativeWidgetAura::OnNativeViewHierarchyChanged() {}

std::string DesktopNativeWidgetAura::GetName() const {
  return name_;
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
  return native_widget_delegate_->ShouldDescendIntoChildForEventHandling(
      content_window_->layer(), child, child->layer(), location);
}

bool DesktopNativeWidgetAura::CanFocus() {
  return true;
}

void DesktopNativeWidgetAura::OnCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopNativeWidgetAura::OnPaint(const ui::PaintContext& context) {
  native_widget_delegate_->OnNativeWidgetPaint(context);
}

void DesktopNativeWidgetAura::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void DesktopNativeWidgetAura::OnWindowDestroying(aura::Window* window) {
  // Cleanup happens in OnHostClosed().
}

void DesktopNativeWidgetAura::OnWindowDestroyed(aura::Window* window) {
  // Cleanup happens in OnHostClosed(). We own |content_window_| (indirectly by
  // way of |dispatcher_|) so there should be no need to do any processing
  // here.
}

void DesktopNativeWidgetAura::OnWindowTargetVisibilityChanged(bool visible) {}

bool DesktopNativeWidgetAura::HasHitTestMask() const {
  return native_widget_delegate_->HasHitTestMask();
}

void DesktopNativeWidgetAura::GetHitTestMask(SkPath* mask) const {
  native_widget_delegate_->GetHitTestMask(mask);
}

void DesktopNativeWidgetAura::UpdateVisualState() {
  native_widget_delegate_->LayoutRootViewIfNecessary();
}

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
  if (!content_window_->IsVisible())
    return;

  native_widget_delegate_->OnKeyEvent(event);
}

void DesktopNativeWidgetAura::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(content_window_->IsVisible());
  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  TooltipManagerAura::UpdateTooltipManagerForCapture(GetWidget());
  native_widget_delegate_->OnMouseEvent(event);
  // WARNING: we may have been deleted.
}

void DesktopNativeWidgetAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL) {
    native_widget_delegate_->OnScrollEvent(event);
    if (event->handled())
      return;

    // Convert unprocessed scroll events into wheel events.
    ui::MouseWheelEvent mwe(*event->AsScrollEvent());
    native_widget_delegate_->OnMouseEvent(&mwe);
    if (mwe.handled())
      event->SetHandled();
  } else {
    native_widget_delegate_->OnScrollEvent(event);
  }
}

void DesktopNativeWidgetAura::OnGestureEvent(ui::GestureEvent* event) {
  native_widget_delegate_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, wm::ActivationDelegate implementation:

bool DesktopNativeWidgetAura::ShouldActivate() const {
  return native_widget_delegate_->CanActivate();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, wm::ActivationChangeObserver implementation:

void DesktopNativeWidgetAura::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK(content_window_ == gained_active || content_window_ == lost_active);
  if (gained_active == content_window_ && restore_focus_on_activate_) {
    restore_focus_on_activate_ = false;
    // For OS_LINUX, desktop native widget may not be activated when child
    // widgets gets aura activation changes. Only when desktop native widget is
    // active, we can rely on aura activation to restore focused view.
    if (GetWidget()->IsActive())
      GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else if (lost_active == content_window_ && GetWidget()->HasFocusManager()) {
    DCHECK(!restore_focus_on_activate_);
    restore_focus_on_activate_ = true;
    // Pass in false so that ClearNativeFocus() isn't invoked.
    GetWidget()->GetFocusManager()->StoreFocusedView(false);
  }

  // Give the native widget a chance to handle any specific changes it needs.
  desktop_window_tree_host_->OnActiveWindowChanged(content_window_ ==
                                                   gained_active);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::FocusChangeObserver implementation:

void DesktopNativeWidgetAura::OnWindowFocused(aura::Window* gained_focus,
                                              aura::Window* lost_focus) {
  if (content_window_ == gained_focus) {
    native_widget_delegate_->OnNativeFocus();
  } else if (content_window_ == lost_focus) {
    native_widget_delegate_->OnNativeBlur();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDragDropDelegate implementation:

void DesktopNativeWidgetAura::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != nullptr);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
}

int DesktopNativeWidgetAura::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != nullptr);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
  return last_drop_operation_;
}

void DesktopNativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != nullptr);
  drop_helper_->OnDragExit();
}

int DesktopNativeWidgetAura::OnPerformDrop(
    const ui::DropTargetEvent& event,
    std::unique_ptr<ui::OSExchangeData> data) {
  DCHECK(drop_helper_.get() != nullptr);
  if (ShouldActivate())
    Activate();
  return drop_helper_->OnDrop(event.data(), event.location(),
                              last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowTreeHostObserver implementation:

void DesktopNativeWidgetAura::OnHostCloseRequested(aura::WindowTreeHost* host) {
  GetWidget()->Close();
}

void DesktopNativeWidgetAura::OnHostResized(aura::WindowTreeHost* host) {
  // Don't update the bounds of the child layers when animating closed. If we
  // did it would force a paint, which we don't want. We don't want the paint
  // as we can't assume any of the children are valid.
  if (desktop_window_tree_host_->IsAnimatingClosed())
    return;

  gfx::Rect new_bounds = gfx::Rect(host->window()->bounds().size());
  content_window_->SetBounds(new_bounds);
  native_widget_delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void DesktopNativeWidgetAura::OnHostWorkspaceChanged(
    aura::WindowTreeHost* host) {
  native_widget_delegate_->OnNativeWidgetWorkspaceChanged();
}

void DesktopNativeWidgetAura::OnHostMovedInPixels(
    aura::WindowTreeHost* host,
    const gfx::Point& new_origin_in_pixels) {
  TRACE_EVENT1("views", "DesktopNativeWidgetAura::OnHostMovedInPixels",
               "new_origin_in_pixels", new_origin_in_pixels.ToString());

  native_widget_delegate_->OnNativeWidgetMove();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, private:

void DesktopNativeWidgetAura::RootWindowDestroyed() {
  cursor_reference_count_--;
  if (cursor_reference_count_ == 0) {
    // We are the last DesktopNativeWidgetAura instance, and we are responsible
    // for cleaning up |cursor_manager_|.
    delete cursor_manager_;
    native_cursor_manager_ = nullptr;
    cursor_manager_ = nullptr;
  }
}

}  // namespace views
