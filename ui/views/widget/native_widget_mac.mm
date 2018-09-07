// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/crash/core/common/crash_key.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer_impl_mac.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/gfx/mac/nswindow_frame_controls.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_mac.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views/cocoa/cocoa_mouse_capture.h"
#import "ui/views/cocoa/drag_drop_client_mac.h"
#import "ui/views/cocoa/native_widget_mac_nswindow.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"

using views_bridge_mac::mojom::WindowVisibilityState;

namespace views {
namespace {

base::LazyInstance<ui::GestureRecognizerImplMac>::Leaky
    g_gesture_recognizer_instance = LAZY_INSTANCE_INITIALIZER;

NSInteger StyleMaskForParams(const Widget::InitParams& params) {
  // If the Widget is modal, it will be displayed as a sheet. This works best if
  // it has NSTitledWindowMask. For example, with NSBorderlessWindowMask, the
  // parent window still accepts input.
  if (params.delegate &&
      params.delegate->GetModalType() == ui::MODAL_TYPE_WINDOW)
    return NSTitledWindowMask;

  // TODO(tapted): Determine better masks when there are use cases for it.
  if (params.remove_standard_frame)
    return NSBorderlessWindowMask;

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    return NSTitledWindowMask | NSClosableWindowMask |
           NSMiniaturizableWindowMask | NSResizableWindowMask |
           NSTexturedBackgroundWindowMask;
  }
  return NSBorderlessWindowMask;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, public:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      bridge_host_(new BridgedNativeWidgetHostImpl(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {}

NativeWidgetMac::~NativeWidgetMac() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

// static
BridgedNativeWidgetImpl* NativeWidgetMac::GetBridgeImplForNativeWindow(
    gfx::NativeWindow window) {
  if (NativeWidgetMacNSWindow* widget_window =
          base::mac::ObjCCast<NativeWidgetMacNSWindow>(window)) {
    return BridgedNativeWidgetImpl::GetFromId(
        [widget_window bridgedNativeWidgetId]);
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

// static
BridgedNativeWidgetHostImpl* NativeWidgetMac::GetBridgeHostImplForNativeWindow(
    gfx::NativeWindow window) {
  if (NativeWidgetMacNSWindow* widget_window =
          base::mac::ObjCCast<NativeWidgetMacNSWindow>(window)) {
    return BridgedNativeWidgetHostImpl::GetFromId(
        [widget_window bridgedNativeWidgetId]);
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

void NativeWidgetMac::WindowDestroying() {
  OnWindowDestroying(GetNativeWindow());
  delegate_->OnNativeWidgetDestroying();
}

void NativeWidgetMac::WindowDestroyed() {
  DCHECK(bridge());
  bridge_host_.reset();
  // |OnNativeWidgetDestroyed| may delete |this| if the object does not own
  // itself.
  bool should_delete_this =
      (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  delegate_->OnNativeWidgetDestroyed();
  if (should_delete_this)
    delete this;
}

int NativeWidgetMac::SheetPositionY() {
  NSView* view = GetNativeView();
  return
      [view convertPoint:NSMakePoint(0, NSHeight([view frame])) toView:nil].y;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, internal::NativeWidgetPrivate implementation:

void NativeWidgetMac::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;
  name_ = params.name;
  base::scoped_nsobject<NativeWidgetMacNSWindow> window(
      [CreateNSWindow(params) retain]);
  bridge_impl()->SetWindow(window);
  bridge_host_->InitWindow(params);

  // Only set always-on-top here if it is true since setting it may affect how
  // the window is treated by Expose.
  if (params.keep_on_top)
    SetAlwaysOnTop(true);

  delegate_->OnNativeWidgetCreated(true);

  DCHECK(GetWidget()->GetRootView());
  bridge_host_->SetRootView(GetWidget()->GetRootView());
  bridge()->CreateContentView(GetWidget()->GetRootView()->bounds());
  if (bridge_impl())
    bridge_impl()->CreateDragDropClient(GetWidget()->GetRootView());
  if (auto* focus_manager = GetWidget()->GetFocusManager()) {
    bridge()->MakeFirstResponder();
    bridge_host_->SetFocusManager(focus_manager);
  }

  bridge_host_->CreateCompositor(params);
}

void NativeWidgetMac::OnWidgetInitDone() {
  OnSizeConstraintsChanged();
  bridge_host_->OnWidgetInitDone();
}

NonClientFrameView* NativeWidgetMac::CreateNonClientFrameView() {
  return new NativeFrameView(GetWidget());
}

bool NativeWidgetMac::ShouldUseNativeFrame() const {
  return true;
}

bool NativeWidgetMac::ShouldWindowContentsBeTransparent() const {
  // On Windows, this returns true when Aero is enabled which draws the titlebar
  // with translucency.
  return false;
}

void NativeWidgetMac::FrameTypeChanged() {
  // This is called when the Theme has changed; forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetMac::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetMac::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMac::GetNativeView() const {
  // Returns a BridgedContentView, unless there is no views::RootView set.
  return [GetNativeWindow() contentView];
}

gfx::NativeWindow NativeWidgetMac::GetNativeWindow() const {
  return bridge_impl() ? bridge_impl()->ns_window() : nil;
}

Widget* NativeWidgetMac::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : nullptr;
}

const ui::Compositor* NativeWidgetMac::GetCompositor() const {
  return bridge_host_ && bridge_host_->layer()
             ? bridge_host_->layer()->GetCompositor()
             : nullptr;
}

const ui::Layer* NativeWidgetMac::GetLayer() const {
  return bridge_host_ ? bridge_host_->layer() : nullptr;
}

void NativeWidgetMac::ReorderNativeViews() {
  if (bridge_impl())
    bridge_impl()->ReorderChildViews();
}

void NativeWidgetMac::ViewRemoved(View* view) {
  DragDropClientMac* client =
      bridge_impl() ? bridge_impl()->drag_drop_client() : nullptr;
  if (client)
    client->drop_helper()->ResetTargetViewIfEquals(view);
}

void NativeWidgetMac::SetNativeWindowProperty(const char* name, void* value) {
  if (bridge_impl())
    bridge_impl()->SetNativeWindowProperty(name, value);
}

void* NativeWidgetMac::GetNativeWindowProperty(const char* name) const {
  if (bridge_impl())
    return bridge_impl()->GetNativeWindowProperty(name);

  return nullptr;
}

TooltipManager* NativeWidgetMac::GetTooltipManager() const {
  if (bridge_host_)
    return bridge_host_->tooltip_manager();

  return nullptr;
}

void NativeWidgetMac::SetCapture() {
  if (bridge())
    bridge()->AcquireCapture();
}

void NativeWidgetMac::ReleaseCapture() {
  if (bridge())
    bridge()->ReleaseCapture();
}

bool NativeWidgetMac::HasCapture() const {
  return bridge_host_ && bridge_host_->IsMouseCaptureActive();
}

ui::InputMethod* NativeWidgetMac::GetInputMethod() {
  return bridge_host_ ? bridge_host_->GetInputMethod() : nullptr;
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  SetSize(BridgedNativeWidgetImpl::GetWindowSizeForClientSize(GetNativeWindow(),
                                                              size));
  // Note that this is not the precise center of screen, but it is the standard
  // location for windows like dialogs to appear on screen for Mac.
  // TODO(tapted): If there is a parent window, center in that instead.
  [GetNativeWindow() center];
}

void NativeWidgetMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();
  if (IsFullscreen())
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  else if (IsMinimized())
    *show_state = ui::SHOW_STATE_MINIMIZED;
  else
    *show_state = ui::SHOW_STATE_NORMAL;
}

bool NativeWidgetMac::SetWindowTitle(const base::string16& title) {
  if (!bridge_host_)
    return false;
  return bridge_host_->SetWindowTitle(title);
}

void NativeWidgetMac::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  // Per-window icons are not really a thing on Mac, so do nothing.
  // TODO(tapted): Investigate whether to use NSWindowDocumentIconButton to set
  // an icon next to the window title. See http://crbug.com/766897.
}

void NativeWidgetMac::InitModalType(ui::ModalType modal_type) {
  if (modal_type == ui::MODAL_TYPE_NONE)
    return;

  // System modal windows not implemented (or used) on Mac.
  DCHECK_NE(ui::MODAL_TYPE_SYSTEM, modal_type);

  // A peculiarity of the constrained window framework is that it permits a
  // dialog of MODAL_TYPE_WINDOW to have a null parent window; falling back to
  // a non-modal window in this case.
  DCHECK(bridge_impl()->parent() || modal_type == ui::MODAL_TYPE_WINDOW);

  // Everything happens upon show.
}

gfx::Rect NativeWidgetMac::GetWindowBoundsInScreen() const {
  return bridge_host_ ? bridge_host_->GetWindowBoundsInScreen() : gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetClientAreaBoundsInScreen() const {
  return bridge_host_ ? bridge_host_->GetContentBoundsInScreen() : gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetRestoredBounds() const {
  return bridge_host_ ? bridge_host_->GetRestoredBounds() : gfx::Rect();
}

std::string NativeWidgetMac::GetWorkspace() const {
  return std::string();
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  if (bridge_host_)
    bridge_host_->SetBounds(bounds);
}

void NativeWidgetMac::SetBoundsConstrained(const gfx::Rect& bounds) {
  if (!bridge_impl())
    return;

  gfx::Rect new_bounds(bounds);
  NativeWidgetPrivate* ancestor = nullptr;
  if (bridge_impl() && bridge_impl()->parent()) {
    ancestor =
        GetNativeWidgetForNativeWindow(bridge_impl()->parent()->GetNSWindow());
  }
  if (!ancestor) {
    new_bounds = ConstrainBoundsToDisplayWorkArea(new_bounds);
  } else {
    new_bounds.AdjustToFit(
        gfx::Rect(ancestor->GetWindowBoundsInScreen().size()));
  }
  SetBounds(new_bounds);
}

void NativeWidgetMac::SetSize(const gfx::Size& size) {
  // Ensure the top-left corner stays in-place (rather than the bottom-left,
  // which -[NSWindow setContentSize:] would do).
  SetBounds(gfx::Rect(GetWindowBoundsInScreen().origin(), size));
}

void NativeWidgetMac::StackAbove(gfx::NativeView native_view) {
  NSInteger view_parent = native_view.window.windowNumber;
  [GetNativeWindow() orderWindow:NSWindowAbove relativeTo:view_parent];
}

void NativeWidgetMac::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetShape(std::unique_ptr<Widget::ShapeRects> shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Close() {
  if (bridge())
    bridge()->CloseWindow();
}

void NativeWidgetMac::CloseNow() {
  if (bridge())
    bridge()->CloseWindowNow();
  // Note: |bridge_host_| will be deleted her, and |this| will be deleted here
  // when ownership_ == NATIVE_WIDGET_OWNS_WIDGET,
}

void NativeWidgetMac::Show(ui::WindowShowState show_state,
                           const gfx::Rect& restore_bounds) {
  if (!bridge())
    return;

  switch (show_state) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_INACTIVE:
      break;
    case ui::SHOW_STATE_MINIMIZED:
    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      NOTIMPLEMENTED();
      break;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      break;
  }
  bridge()->SetVisibilityState(
      show_state == ui::SHOW_STATE_INACTIVE
          ? WindowVisibilityState::kShowInactive
          : WindowVisibilityState::kShowAndActivateWindow);

  // Ignore the SetInitialFocus() result. BridgedContentView should get
  // firstResponder status regardless.
  delegate_->SetInitialFocus(show_state);
}

void NativeWidgetMac::Hide() {
  if (!bridge())
    return;
  bridge()->SetVisibilityState(WindowVisibilityState::kHideWindow);
}

bool NativeWidgetMac::IsVisible() const {
  return bridge_host_ && bridge_host_->IsVisible();
}

void NativeWidgetMac::Activate() {
  if (!bridge())
    return;
  bridge()->SetVisibilityState(WindowVisibilityState::kShowAndActivateWindow);
}

void NativeWidgetMac::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsActive() const {
  return bridge_host_ ? bridge_host_->IsWindowKey() : false;
}

void NativeWidgetMac::SetAlwaysOnTop(bool always_on_top) {
  gfx::SetNSWindowAlwaysOnTop(GetNativeWindow(), always_on_top);
}

bool NativeWidgetMac::IsAlwaysOnTop() const {
  return gfx::IsNSWindowAlwaysOnTop(GetNativeWindow());
}

void NativeWidgetMac::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (!bridge())
    return;
  bridge()->SetVisibleOnAllSpaces(always_visible);
}

bool NativeWidgetMac::IsVisibleOnAllWorkspaces() const {
  return false;
}

void NativeWidgetMac::Maximize() {
  NOTIMPLEMENTED();  // See IsMaximized().
}

void NativeWidgetMac::Minimize() {
  if (!bridge())
    return;
  bridge()->SetMiniaturized(true);
}

bool NativeWidgetMac::IsMaximized() const {
  // The window frame isn't altered on Mac unless going fullscreen. The green
  // "+" button just makes the window bigger. So, always false.
  return false;
}

bool NativeWidgetMac::IsMinimized() const {
  if (!bridge_host_)
    return false;
  return bridge_host_->IsMiniaturized();
}

void NativeWidgetMac::Restore() {
  if (!bridge())
    return;
  bridge()->SetFullscreen(false);
  bridge()->SetMiniaturized(false);
}

void NativeWidgetMac::SetFullscreen(bool fullscreen) {
  if (!bridge_host_)
    return;
  bridge_host_->SetFullscreen(fullscreen);
}

bool NativeWidgetMac::IsFullscreen() const {
  return bridge_host_ && bridge_host_->target_fullscreen_state();
}

void NativeWidgetMac::SetOpacity(float opacity) {
  if (!bridge())
    return;
  bridge()->SetOpacity(opacity);
}

void NativeWidgetMac::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (!bridge())
    return;
  bridge()->SetContentAspectRatio(aspect_ratio);
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::DragDropTypes::DragEventSource source) {
  bridge_impl()->drag_drop_client()->StartDragAndDrop(view, data, operation,
                                                      source);
}

void NativeWidgetMac::SchedulePaintInRect(const gfx::Rect& rect) {
  // |rect| is relative to client area of the window.
  NSWindow* window = GetNativeWindow();
  NSRect client_rect = [window contentRectForFrameRect:[window frame]];
  NSRect target_rect = rect.ToCGRect();

  // Convert to Appkit coordinate system (origin at bottom left).
  target_rect.origin.y =
      NSHeight(client_rect) - target_rect.origin.y - NSHeight(target_rect);
  [GetNativeView() setNeedsDisplayInRect:target_rect];
  if (bridge_host_ && bridge_host_->layer())
    bridge_host_->layer()->SchedulePaint(rect);
}

void NativeWidgetMac::SetCursor(gfx::NativeCursor cursor) {
  if (bridge_impl())
    bridge_impl()->SetCursor(cursor);
}

bool NativeWidgetMac::IsMouseEventsEnabled() const {
  // On platforms with touch, mouse events get disabled and calls to this method
  // can affect hover states. Since there is no touch on desktop Mac, this is
  // always true. Touch on Mac is tracked in http://crbug.com/445520.
  return true;
}

bool NativeWidgetMac::IsMouseButtonDown() const {
  return [NSEvent pressedMouseButtons] != 0;
}

void NativeWidgetMac::ClearNativeFocus() {
  // To quote DesktopWindowTreeHostX11, "This method is weird and misnamed."
  // The goal is to set focus to the content window, thereby removing focus from
  // any NSView in the window that doesn't belong to toolkit-views.
  if (!bridge())
    return;
  bridge()->MakeFirstResponder();
}

gfx::Rect NativeWidgetMac::GetWorkAreaBoundsInScreen() const {
  return bridge_host_ ? bridge_host_->GetCurrentDisplay().work_area()
                      : gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMac::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  if (!bridge_impl())
    return Widget::MOVE_LOOP_CANCELED;

  return bridge_impl()->RunMoveLoop(drag_offset);
}

void NativeWidgetMac::EndMoveLoop() {
  if (bridge_impl())
    bridge_impl()->EndMoveLoop();
}

void NativeWidgetMac::SetVisibilityChangedAnimationsEnabled(bool value) {
  if (bridge_impl())
    bridge_impl()->SetAnimationEnabled(value);
}

void NativeWidgetMac::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  if (bridge_impl())
    bridge_impl()->set_transitions_to_animate(transition);
}

bool NativeWidgetMac::IsTranslucentWindowOpacitySupported() const {
  return false;
}

ui::GestureRecognizer* NativeWidgetMac::GetGestureRecognizer() {
  return g_gesture_recognizer_instance.Pointer();
}

void NativeWidgetMac::OnSizeConstraintsChanged() {
  Widget* widget = GetWidget();
  bridge()->SetSizeConstraints(widget->GetMinimumSize(),
                               widget->GetMaximumSize(),
                               widget->widget_delegate()->CanResize(),
                               widget->widget_delegate()->CanMaximize());
}

void NativeWidgetMac::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

std::string NativeWidgetMac::GetName() const {
  return name_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, protected:

NativeWidgetMacNSWindow* NativeWidgetMac::CreateNSWindow(
    const Widget::InitParams& params) {
  return [[[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:StyleMaskForParams(params)
                  backing:NSBackingStoreBuffered
                    defer:NO] autorelease];
}

views_bridge_mac::mojom::BridgedNativeWidget* NativeWidgetMac::bridge() const {
  return bridge_host_ ? bridge_host_->bridge() : nullptr;
}

BridgedNativeWidgetImpl* NativeWidgetMac::bridge_impl() const {
  return bridge_host_ ? bridge_host_->bridge_impl() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::CloseAllSecondaryWidgets() {
  NSArray* starting_windows = [NSApp windows];  // Creates an autoreleased copy.
  for (NSWindow* window in starting_windows) {
    // Ignore any windows that couldn't have been created by NativeWidgetMac or
    // a subclass. GetNativeWidgetForNativeWindow() will later interrogate the
    // NSWindow delegate, but we can't trust that delegate to be a valid object.
    if (![window isKindOfClass:[NativeWidgetMacNSWindow class]])
      continue;

    // Record a crash key to detect when client code may destroy a
    // WidgetObserver without removing it (possibly leaking the Widget).
    // A crash can occur in generic Widget teardown paths when trying to notify.
    // See http://crbug.com/808318.
    static crash_reporter::CrashKeyString<256> window_info_key("windowInfo");
    std::string value = base::SysNSStringToUTF8(
        [NSString stringWithFormat:@"Closing %@ (%@)", [window title],
                                   [window className]]);
    crash_reporter::ScopedCrashKeyString scopedWindowKey(&window_info_key,
                                                         value);

    Widget* widget = GetWidgetForNativeWindow(window);
    if (widget && widget->is_secondary_widget())
      [window close];
  }
}

const ui::NativeTheme* Widget::GetNativeTheme() const {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetMac(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return GetNativeWidgetForNativeWindow([native_view window]);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow window) {
  if (BridgedNativeWidgetHostImpl* bridge_host_impl =
          NativeWidgetMac::GetBridgeHostImplForNativeWindow(window)) {
    return bridge_host_impl->native_widget_mac();
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  BridgedNativeWidgetImpl* bridge_impl =
      NativeWidgetMac::GetBridgeImplForNativeWindow([native_view window]);
  if (!bridge_impl)
    return nullptr;

  NativeWidgetPrivate* ancestor =
      bridge_impl->parent()
          ? GetTopLevelNativeWidget(
                [bridge_impl->parent()->GetNSWindow() contentView])
          : nullptr;
  return ancestor ? ancestor : bridge_impl->native_widget_mac();
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  BridgedNativeWidgetImpl* bridge_impl =
      NativeWidgetMac::GetBridgeImplForNativeWindow([native_view window]);
  if (!bridge_impl) {
    // The NSWindow is not itself a views::Widget, but it may have children that
    // are. Support returning Widgets that are parented to the NSWindow, except:
    // - Ignore requests for children of an NSView that is not a contentView.
    // - We do not add a Widget for |native_view| to |children| (there is none).
    if ([[native_view window] contentView] != native_view)
      return;

    // Collect -sheets and -childWindows. A window should never appear in both,
    // since that causes AppKit to glitch.
    NSArray* sheet_children = [[native_view window] sheets];
    for (NSWindow* native_child in sheet_children)
      GetAllChildWidgets([native_child contentView], children);

    for (NSWindow* native_child in [[native_view window] childWindows]) {
      DCHECK(![sheet_children containsObject:native_child]);
      GetAllChildWidgets([native_child contentView], children);
    }
    return;
  }

  // If |native_view| is a subview of the contentView, it will share an
  // NSWindow, but will itself be a native child of the Widget. That is, adding
  // bridge_impl->..->GetWidget() to |children| would be adding the _parent_ of
  // |native_view|, not the Widget for |native_view|. |native_view| doesn't have
  // a corresponding Widget of its own in this case (and so can't have Widget
  // children of its own on Mac).
  if (bridge_impl->ns_view() != native_view)
    return;

  // Code expects widget for |native_view| to be added to |children|.
  if (bridge_impl->native_widget_mac()->GetWidget())
    children->insert(bridge_impl->native_widget_mac()->GetWidget());

  // When the NSWindow *is* a Widget, only consider child_windows(). I.e. do not
  // look through -[NSWindow childWindows] as done for the (!bridge_impl) case
  // above. -childWindows does not support hidden windows, and anything in there
  // which is not in child_windows() would have been added by AppKit.
  for (BridgedNativeWidgetImpl* child : bridge_impl->child_windows())
    GetAllChildWidgets(child->ns_view(), children);
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  BridgedNativeWidgetImpl* bridge =
      NativeWidgetMac::GetBridgeImplForNativeWindow([native_view window]);
  if (!bridge) {
    GetAllChildWidgets(native_view, owned);
    return;
  }
  if (bridge->ns_view() != native_view)
    return;
  for (BridgedNativeWidgetImpl* child : bridge->child_windows())
    GetAllChildWidgets(child->ns_view(), owned);
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  DCHECK_NE(native_view, new_parent);
  if (!new_parent || [native_view superview] == new_parent) {
    NOTREACHED();
    return;
  }

  BridgedNativeWidgetImpl* bridge =
      NativeWidgetMac::GetBridgeImplForNativeWindow([native_view window]);
  BridgedNativeWidgetImpl* parent_bridge =
      NativeWidgetMac::GetBridgeImplForNativeWindow([new_parent window]);
  DCHECK(bridge);
  if (Widget::GetWidgetForNativeView(native_view)->is_top_level() &&
      bridge->parent() == parent_bridge)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (auto* child : widgets)
    child->NotifyNativeViewHierarchyWillChange();

  bridge->ReparentNativeView(native_view, new_parent);

  // And now, notify them that they have a brand new parent.
  for (auto* child : widgets)
    child->NotifyNativeViewHierarchyChanged();
}

// static
gfx::FontList NativeWidgetPrivate::GetWindowTitleFontList() {
  NOTIMPLEMENTED();
  return gfx::FontList();
}

// static
gfx::NativeView NativeWidgetPrivate::GetGlobalCapture(
    gfx::NativeView native_view) {
  return [CocoaMouseCapture::GetGlobalCaptureWindow() contentView];
}

}  // namespace internal
}  // namespace views
