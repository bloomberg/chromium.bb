// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/native_widget_mus.h"

#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/mus/platform_window_mus.h"
#include "ui/views/mus/surface_context_factory.h"
#include "ui/views/mus/window_manager_constants_converters.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/mus/window_tree_host_mus.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"

DECLARE_WINDOW_PROPERTY_TYPE(mus::Window*);

namespace views {
namespace {

DEFINE_WINDOW_PROPERTY_KEY(mus::Window*, kMusWindow, nullptr);

MUS_DEFINE_WINDOW_PROPERTY_KEY(NativeWidgetMus*, kNativeWidgetMusKey, nullptr);

// TODO: figure out what this should be.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

class ContentWindowLayoutManager : public aura::LayoutManager {
 public:
   ContentWindowLayoutManager(aura::Window* outer, aura::Window* inner)
      : outer_(outer), inner_(inner) {}
   ~ContentWindowLayoutManager() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override { inner_->SetBounds(outer_->bounds()); }
  void OnWindowAddedToLayout(aura::Window* child) override {
    OnWindowResized();
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* outer_;
  aura::Window* inner_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowLayoutManager);
};

class NativeWidgetMusWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit NativeWidgetMusWindowTreeClient(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowTreeClient(root_window_, this);
  }
  ~NativeWidgetMusWindowTreeClient() override {
    aura::client::SetWindowTreeClient(root_window_, nullptr);
  }

  // Overridden from client::WindowTreeClient:
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMusWindowTreeClient);
};

// As the window manager renderers the non-client decorations this class does
// very little but honor the client area insets from the window manager.
class ClientSideNonClientFrameView : public NonClientFrameView {
 public:
  explicit ClientSideNonClientFrameView(views::Widget* widget)
      : widget_(widget) {}
  ~ClientSideNonClientFrameView() override {}

 private:
  // Returns the default values of client area insets from the window manager.
  static gfx::Insets GetDefaultWindowManagerInsets(bool is_maximized) {
    const WindowManagerFrameValues& values =
        WindowManagerFrameValues::instance();
    return is_maximized ? values.maximized_insets : values.normal_insets;
  }

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override {
    gfx::Rect result(GetLocalBounds());
    result.Inset(GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return result;
  }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    const gfx::Insets insets(
        GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return gfx::Rect(client_bounds.x() - insets.left(),
                     client_bounds.y() - insets.top(),
                     client_bounds.width() + insets.width(),
                     client_bounds.height() + insets.height());
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {
    // The window manager provides the shape; do nothing.
  }
  void ResetWindowControls() override {
    // TODO(sky): push to wm?
  }
  void UpdateWindowIcon() override {
    // NOTIMPLEMENTED();
  }
  void UpdateWindowTitle() override {
    // NOTIMPLEMENTED();
  }
  void SizeConstraintsChanged() override {
    // NOTIMPLEMENTED();
  }

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideNonClientFrameView);
};

int ResizeBehaviorFromDelegate(WidgetDelegate* delegate) {
  int32_t behavior = mus::mojom::kResizeBehaviorNone;
  if (delegate->CanResize())
    behavior |= mus::mojom::kResizeBehaviorCanResize;
  if (delegate->CanMaximize())
    behavior |= mus::mojom::kResizeBehaviorCanMaximize;
  if (delegate->CanMinimize())
    behavior |= mus::mojom::kResizeBehaviorCanMinimize;
  return behavior;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, public:

NativeWidgetMus::NativeWidgetMus(internal::NativeWidgetDelegate* delegate,
                                 mojo::shell::mojom::Shell* shell,
                                 mus::Window* window,
                                 mus::mojom::SurfaceType surface_type)
    : window_(window),
      shell_(shell),
      native_widget_delegate_(delegate),
      surface_type_(surface_type),
      show_state_before_fullscreen_(ui::PLATFORM_WINDOW_STATE_UNKNOWN),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      content_(new aura::Window(this)),
      close_widget_factory_(this) {
  // TODO(fsamuel): Figure out lifetime of |window_|.
  aura::SetMusWindow(content_, window_);

  window->SetLocalProperty(kNativeWidgetMusKey, this);
}

NativeWidgetMus::~NativeWidgetMus() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete native_widget_delegate_;
  else
    CloseNow();
}

// static
void NativeWidgetMus::NotifyFrameChanged(
    mus::WindowTreeConnection* connection) {
  for (mus::Window* window : connection->GetRoots()) {
    NativeWidgetMus* native_widget =
        window->GetLocalProperty(kNativeWidgetMusKey);
    if (native_widget && native_widget->GetWidget()->non_client_view()) {
      native_widget->GetWidget()->non_client_view()->Layout();
      native_widget->GetWidget()->non_client_view()->SchedulePaint();
      native_widget->UpdateClientArea();
    }
  }
}

void NativeWidgetMus::OnPlatformWindowClosed() {
  native_widget_delegate_->OnNativeWidgetDestroying();

  window_tree_client_.reset();  // Uses |content_|.
  capture_client_.reset();      // Uses |content_|.

  window_tree_host_->RemoveObserver(this);
  window_tree_host_.reset();

  window_ = nullptr;
  content_ = nullptr;

  native_widget_delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetMus::OnActivationChanged(bool active) {
  if (!native_widget_delegate_)
    return;
  if (active) {
    native_widget_delegate_->OnNativeFocus();
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else {
    native_widget_delegate_->OnNativeBlur();
    GetWidget()->GetFocusManager()->StoreFocusedView(true);
  }
}

void NativeWidgetMus::UpdateClientArea() {
  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (!non_client_view || !non_client_view->client_view())
    return;

  const gfx::Rect client_area_rect(non_client_view->client_view()->bounds());
  window_->SetClientArea(gfx::Insets(
      client_area_rect.y(), client_area_rect.x(),
      non_client_view->bounds().height() - client_area_rect.bottom(),
      non_client_view->bounds().width() - client_area_rect.right()));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, private:

// static
void NativeWidgetMus::ConfigurePropertiesForNewWindow(
    const Widget::InitParams& init_params,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  if (!init_params.bounds.IsEmpty()) {
    (*properties)[mus::mojom::WindowManager::kUserSetBounds_Property] =
        mojo::TypeConverter<const std::vector<uint8_t>, gfx::Rect>::Convert(
            init_params.bounds);
  }

  if (!Widget::RequiresNonClientView(init_params.type))
    return;

  (*properties)[mus::mojom::WindowManager::kWindowType_Property] =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
          static_cast<int32_t>(
              mojo::ConvertTo<mus::mojom::WindowType>(init_params.type)));
  (*properties)[mus::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
          ResizeBehaviorFromDelegate(init_params.delegate));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, internal::NativeWidgetPrivate implementation:

NonClientFrameView* NativeWidgetMus::CreateNonClientFrameView() {
  return new ClientSideNonClientFrameView(GetWidget());
}

void NativeWidgetMus::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;
  window_->SetCanFocus(params.activatable ==
                       Widget::InitParams::ACTIVATABLE_YES);

  // WindowTreeHost creates the compositor using the ContextFactory from
  // aura::Env. Install |context_factory_| there so that |context_factory_| is
  // picked up.
  ui::ContextFactory* default_context_factory =
      aura::Env::GetInstance()->context_factory();
  // For Chrome, we need the GpuProcessTransportFactory so that renderer and
  // browser pixels are composited into a single backing
  // SoftwareOutputDeviceMus.
  if (!default_context_factory) {
    if (!context_factory_) {
      context_factory_.reset(new SurfaceContextFactory(shell_, window_,
                                                       surface_type_));
    }
    aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  }
  window_tree_host_.reset(new WindowTreeHostMus(shell_, this, window_));
  window_tree_host_->AddObserver(this);
  window_tree_host_->InitHost();
  aura::Env::GetInstance()->set_context_factory(default_context_factory);
  window_tree_host_->window()->SetProperty(kMusWindow, window_);

  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));

  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  window_tree_client_.reset(
      new NativeWidgetMusWindowTreeClient(window_tree_host_->window()));
  window_tree_host_->window()->AddPreTargetHandler(focus_client_.get());
  window_tree_host_->window()->SetLayoutManager(
      new ContentWindowLayoutManager(window_tree_host_->window(), content_));
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  content_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  content_->Init(ui::LAYER_TEXTURED);
  content_->Show();
  content_->SetTransparent(true);
  content_->SetFillsBoundsCompletely(false);
  window_tree_host_->window()->AddChild(content_);

  // Set-up transiency if appropriate.
  if (params.parent && !params.child) {
    aura::Window* parent_root = params.parent->GetRootWindow();
    mus::Window* parent_mus = parent_root->GetProperty(kMusWindow);
    if (parent_mus)
      parent_mus->AddTransientWindow(window_);
  }

  // TODO(beng): much else, see [Desktop]NativeWidgetAura.

  native_widget_delegate_->OnNativeWidgetCreated(false);
}

void NativeWidgetMus::OnWidgetInitDone() {
  // The client area is calculated from the NonClientView. During
  // InitNativeWidget() the NonClientView has not been created. When this
  // function is called the NonClientView has been created, so that we can
  // correctly calculate the client area and push it to the mus::Window.
  UpdateClientArea();
}

bool NativeWidgetMus::ShouldUseNativeFrame() const {
  // NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMus::ShouldWindowContentsBeTransparent() const {
  // NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::FrameTypeChanged() {
  // NOTIMPLEMENTED();
}

Widget* NativeWidgetMus::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* NativeWidgetMus::GetWidget() const {
  // NOTIMPLEMENTED();
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMus::GetNativeView() const {
  return content_;
}

gfx::NativeWindow NativeWidgetMus::GetNativeWindow() const {
  return content_;
}

Widget* NativeWidgetMus::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* NativeWidgetMus::GetCompositor() const {
  return window_tree_host_->window()->layer()->GetCompositor();
}

const ui::Layer* NativeWidgetMus::GetLayer() const {
  return window_tree_host_->window()->layer();
}

void NativeWidgetMus::ReorderNativeViews() {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::ViewRemoved(View* view) {
  // NOTIMPLEMENTED();
}

// These methods are wrong in mojo. They're not usually used to associate
// data with a window; they are used exclusively in chrome/ to unsafely pass
// raw pointers around. I can only find two places where we do the "safe"
// thing (and even that requires casting an integer to a void*). They can't be
// used safely in a world where we separate things with mojo. They should be
// removed; not ported.
void NativeWidgetMus::SetNativeWindowProperty(const char* name, void* value) {
  // TODO(beng): push properties to mus::Window.
  // NOTIMPLEMENTED();
}

void* NativeWidgetMus::GetNativeWindowProperty(const char* name) const {
  // TODO(beng): pull properties to mus::Window.
  // NOTIMPLEMENTED();
  return nullptr;
}

TooltipManager* NativeWidgetMus::GetTooltipManager() const {
  // NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMus::SetCapture() {
  content_->SetCapture();
}

void NativeWidgetMus::ReleaseCapture() {
  content_->ReleaseCapture();
}

bool NativeWidgetMus::HasCapture() const {
  return content_->HasCapture();
}

ui::InputMethod* NativeWidgetMus::GetInputMethod() {
  return window_tree_host_ ? window_tree_host_->GetInputMethod() : nullptr;
}

void NativeWidgetMus::CenterWindow(const gfx::Size& size) {
  // TODO(beng): clear user-placed property and set preferred size property.
  window_->SetSharedProperty<gfx::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, size);
}

void NativeWidgetMus::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  // NOTIMPLEMENTED();
}

bool NativeWidgetMus::SetWindowTitle(const base::string16& title) {
  window_->SetSharedProperty<base::string16>(
      mus::mojom::WindowManager::kWindowTitle_Property, title);
  return true;
}

void NativeWidgetMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::InitModalType(ui::ModalType modal_type) {
  // NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMus::GetWindowBoundsInScreen() const {
  return window_ ? window_->GetBoundsInRoot() : gfx::Rect();
}

gfx::Rect NativeWidgetMus::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return GetWindowBoundsInScreen();
}

gfx::Rect NativeWidgetMus::GetRestoredBounds() const {
  // NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeWidgetMus::SetBounds(const gfx::Rect& bounds) {
  window_tree_host_->SetBounds(bounds);
}

void NativeWidgetMus::SetSize(const gfx::Size& size) {
  gfx::Rect bounds = window_tree_host_->GetBounds();
  SetBounds(gfx::Rect(bounds.origin(), size));
}

void NativeWidgetMus::StackAbove(gfx::NativeView native_view) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::StackAtTop() {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::StackBelow(gfx::NativeView native_view) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SetShape(SkRegion* shape) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::Close() {
  Hide();
  if (!close_widget_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&NativeWidgetMus::CloseNow,
                              close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetMus::CloseNow() {
  window_->Destroy();
}

void NativeWidgetMus::Show() {
  ShowWithWindowState(ui::SHOW_STATE_NORMAL);
}

void NativeWidgetMus::Hide() {
  window_tree_host_->Hide();
  GetNativeWindow()->Hide();
}

void NativeWidgetMus::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::ShowWithWindowState(ui::WindowShowState state) {
  window_tree_host_->Show();
  GetNativeWindow()->Show();
  if (state != ui::SHOW_STATE_INACTIVE)
    Activate();
  GetWidget()->SetInitialFocus(state);
}

bool NativeWidgetMus::IsVisible() const {
  // TODO(beng): this should probably be wired thru PlatformWindow.
  return window_->visible();
}

void NativeWidgetMus::Activate() {
  window_tree_host_->platform_window()->Activate();
}

void NativeWidgetMus::Deactivate() {
  // NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsActive() const {
  // NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::SetAlwaysOnTop(bool always_on_top) {
  // NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsAlwaysOnTop() const {
  // NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMus::SetVisibleOnAllWorkspaces(bool always_visible) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::Maximize() {
  window_tree_host_->platform_window()->Maximize();
}

void NativeWidgetMus::Minimize() {
  window_tree_host_->platform_window()->Minimize();
}

bool NativeWidgetMus::IsMaximized() const {
  return window_tree_host_->show_state() == ui::PLATFORM_WINDOW_STATE_MAXIMIZED;
}

bool NativeWidgetMus::IsMinimized() const {
  return window_tree_host_->show_state() == ui::PLATFORM_WINDOW_STATE_MINIMIZED;
}

void NativeWidgetMus::Restore() {
  window_tree_host_->platform_window()->Restore();
}

void NativeWidgetMus::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;
  if (fullscreen) {
    show_state_before_fullscreen_ = window_tree_host_->show_state();
    window_tree_host_->platform_window()->ToggleFullscreen();
  } else {
    switch (show_state_before_fullscreen_) {
      case ui::PLATFORM_WINDOW_STATE_MAXIMIZED:
        Maximize();
        break;
      case ui::PLATFORM_WINDOW_STATE_MINIMIZED:
        Minimize();
        break;
      case ui::PLATFORM_WINDOW_STATE_UNKNOWN:
      case ui::PLATFORM_WINDOW_STATE_NORMAL:
      case ui::PLATFORM_WINDOW_STATE_FULLSCREEN:
        // TODO(sad): This may not be sufficient.
        Restore();
        break;
    }
  }
}

bool NativeWidgetMus::IsFullscreen() const {
  return window_tree_host_->show_state() ==
      ui::PLATFORM_WINDOW_STATE_FULLSCREEN;
}

void NativeWidgetMus::SetOpacity(unsigned char opacity) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SetUseDragFrame(bool use_drag_frame) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::FlashFrame(bool flash_frame) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::RunShellDrag(
    View* view,
    const ui::OSExchangeData& data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SchedulePaintInRect(const gfx::Rect& rect) {
  if (content_)
    content_->SchedulePaintInRect(rect);
}

void NativeWidgetMus::SetCursor(gfx::NativeCursor cursor) {
  // TODO(erg): In aura, our incoming cursor is really two
  // parts. cursor.native_type() is an integer for standard cursors and is all
  // we support right now. If native_type() == kCursorCustom, than we should
  // also send an image, but as the cursor code is currently written, the image
  // is in a platform native format that's already uploaded to the window
  // server.
  window_tree_host_->platform_window()->SetCursorById(
      mus::mojom::Cursor(cursor.native_type()));
}

bool NativeWidgetMus::IsMouseEventsEnabled() const {
  // NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::ClearNativeFocus() {
  // NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMus::GetWorkAreaBoundsInScreen() const {
  // NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMus::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  // NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMus::EndMoveLoop() {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityChangedAnimationsEnabled(bool value) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  // NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  // NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMus::GetNativeTheme() const {
  return ui::NativeThemeAura::instance();
}

void NativeWidgetMus::OnRootViewLayout() {
  // NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsTranslucentWindowOpacitySupported() const {
  // NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::OnSizeConstraintsChanged() {
  window_->SetSharedProperty<int32_t>(
      mus::mojom::WindowManager::kResizeBehavior_Property,
      ResizeBehaviorFromDelegate(GetWidget()->widget_delegate()));
}

void NativeWidgetMus::RepostNativeEvent(gfx::NativeEvent native_event) {
  // NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, aura::WindowDelegate implementation:

gfx::Size NativeWidgetMus::GetMinimumSize() const {
  return native_widget_delegate_->GetMinimumSize();
}

gfx::Size NativeWidgetMus::GetMaximumSize() const {
  return native_widget_delegate_->GetMaximumSize();
}

void NativeWidgetMus::OnBoundsChanged(const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  // Assume that if the old bounds was completely empty a move happened. This
  // handles the case of a maximize animation acquiring the layer (acquiring a
  // layer results in clearing the bounds).
  if (old_bounds.origin() != new_bounds.origin() ||
      (old_bounds == gfx::Rect(0, 0, 0, 0) && !new_bounds.IsEmpty())) {
    native_widget_delegate_->OnNativeWidgetMove();
  }
  if (old_bounds.size() != new_bounds.size()) {
    native_widget_delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
    UpdateClientArea();
  }
}

gfx::NativeCursor NativeWidgetMus::GetCursor(const gfx::Point& point) {
  return gfx::NativeCursor();
}

int NativeWidgetMus::GetNonClientComponent(const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

bool NativeWidgetMus::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  views::WidgetDelegate* widget_delegate = GetWidget()->widget_delegate();
  return !widget_delegate ||
      widget_delegate->ShouldDescendIntoChildForEventHandling(child, location);
}

bool NativeWidgetMus::CanFocus() {
  return true;
}

void NativeWidgetMus::OnCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void NativeWidgetMus::OnPaint(const ui::PaintContext& context) {
  native_widget_delegate_->OnNativeWidgetPaint(context);
}

void NativeWidgetMus::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

void NativeWidgetMus::OnWindowDestroying(aura::Window* window) {
}

void NativeWidgetMus::OnWindowDestroyed(aura::Window* window) {
  // Cleanup happens in OnPlatformWindowClosed().
}

void NativeWidgetMus::OnWindowTargetVisibilityChanged(bool visible) {
}

bool NativeWidgetMus::HasHitTestMask() const {
  return native_widget_delegate_->HasHitTestMask();
}

void NativeWidgetMus::GetHitTestMask(gfx::Path* mask) const {
  native_widget_delegate_->GetHitTestMask(mask);
}

void NativeWidgetMus::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attached to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return;
  }
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!content_->IsVisible())
    return;

  native_widget_delegate_->OnKeyEvent(event);
  if (event->handled())
    return;

  if (GetWidget()->HasFocusManager() &&
      !GetWidget()->GetFocusManager()->OnKeyEvent(*event))
    event->SetHandled();
}

void NativeWidgetMus::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(sky): forward to tooltipmanager. See NativeWidgetDesktopAura.
  DCHECK(content_->IsVisible());
  native_widget_delegate_->OnMouseEvent(event);
  // WARNING: we may have been deleted.
}

void NativeWidgetMus::OnScrollEvent(ui::ScrollEvent* event) {
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

void NativeWidgetMus::OnGestureEvent(ui::GestureEvent* event) {
  native_widget_delegate_->OnGestureEvent(event);
}

void NativeWidgetMus::OnHostCloseRequested(const aura::WindowTreeHost* host) {
  GetWidget()->Close();
}

}  // namespace views
