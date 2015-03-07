// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <objc/runtime.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/thread_task_runner_handle.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/dip_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/screen.h"
#import "ui/views/cocoa/cocoa_mouse_capture.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/ime/null_input_method.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_aura_utils.h"

namespace {

int kWindowPropertiesKey;

float GetDeviceScaleFactorFromView(NSView* view) {
  gfx::Display display =
      gfx::Screen::GetScreenFor(view)->GetDisplayNearestWindow(view);
  DCHECK(display.is_valid());
  return display.device_scale_factor();
}

// Returns true if bounds passed to window in SetBounds should be treated as
// though they are in screen coordinates.
bool PositionWindowInScreenCoordinates(views::Widget* widget,
                                       views::Widget::InitParams::Type type) {
  // Replicate the logic in desktop_aura/desktop_screen_position_client.cc.
  if (views::GetAuraWindowTypeForWidgetType(type) == ui::wm::WINDOW_TYPE_POPUP)
    return true;

  return widget && widget->is_top_level();
}

}  // namespace

namespace views {

BridgedNativeWidget::BridgedNativeWidget(NativeWidgetMac* parent)
    : native_widget_mac_(parent),
      focus_manager_(nullptr),
      widget_type_(Widget::InitParams::TYPE_WINDOW),  // Updated in Init().
      parent_(nullptr),
      target_fullscreen_state_(false),
      in_fullscreen_transition_(false),
      window_visible_(false),
      wants_to_be_visible_(false) {
  DCHECK(parent);
  window_delegate_.reset(
      [[ViewsNSWindowDelegate alloc] initWithBridgedNativeWidget:this]);
}

BridgedNativeWidget::~BridgedNativeWidget() {
  RemoveOrDestroyChildren();
  DCHECK(child_windows_.empty());
  SetFocusManager(NULL);
  SetRootView(NULL);
  DestroyCompositor();
  if ([window_ delegate]) {
    // If the delegate is still set, it means OnWindowWillClose has not been
    // called and the window is still open. Calling -[NSWindow close] will
    // synchronously call OnWindowWillClose and notify NativeWidgetMac.
    [window_ close];
  }
  DCHECK(![window_ delegate]);
}

void BridgedNativeWidget::Init(base::scoped_nsobject<NSWindow> window,
                               const Widget::InitParams& params) {
  widget_type_ = params.type;

  DCHECK(!window_);
  window_.swap(window);
  [window_ setDelegate:window_delegate_];

  // Register for application hide notifications so that visibility can be
  // properly tracked. This is not done in the delegate so that the lifetime is
  // tied to the C++ object, rather than the delegate (which may be reference
  // counted). This is required since the application hides do not send an
  // orderOut: to individual windows. Unhide, however, does send an order
  // message.
  [[NSNotificationCenter defaultCenter]
      addObserver:window_delegate_
         selector:@selector(onWindowOrderChanged:)
             name:NSApplicationDidHideNotification
           object:nil];

  // Validate the window's initial state, otherwise the bridge's initial
  // tracking state will be incorrect.
  DCHECK(![window_ isVisible]);
  DCHECK_EQ(0u, [window_ styleMask] & NSFullScreenWindowMask);

  if (params.parent) {
    // Disallow creating child windows of views not currently in an NSWindow.
    CHECK([params.parent window]);
    BridgedNativeWidget* parent =
        NativeWidgetMac::GetBridgeForNativeWindow([params.parent window]);
    // The parent could be an NSWindow without an associated Widget. That could
    // work by observing NSWindowWillCloseNotification, but for now it's not
    // supported, and there might not be a use-case for that.
    CHECK(parent);
    parent_ = parent;
    parent->child_windows_.push_back(this);
  }

  // Set a meaningful initial bounds. Note that except for frameless widgets
  // with no WidgetDelegate, the bounds will be set again by Widget after
  // initializing the non-client view. In the former case, if bounds were not
  // set at all, the creator of the Widget is expected to call SetBounds()
  // before calling Widget::Show() to avoid a kWindowSizeDeterminedLater-sized
  // (i.e. 1x1) window appearing.
  if (!params.bounds.IsEmpty()) {
    SetBounds(params.bounds);
  } else {
    // If a position is set, but no size, complain. Otherwise, a 1x1 window
    // would appear there, which might be unexpected.
    DCHECK(params.bounds.origin().IsOrigin())
        << "Zero-sized windows not supported on Mac.";

    // Otherwise, bounds is all zeroes. Cocoa will currently have the window at
    // the bottom left of the screen. To support a client calling SetSize() only
    // (and for consistency across platforms) put it at the top-left instead.
    // Read back the current frame: it will be a 1x1 context rect but the frame
    // size also depends on the window style.
    NSRect frame_rect = [window_ frame];
    SetBounds(gfx::Rect(gfx::Point(),
                        gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect))));
  }

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (params.type == Widget::InitParams::TYPE_CONTROL)
    SetVisibilityState(SHOW_INACTIVE);
}

void BridgedNativeWidget::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);

  if (focus_manager)
    focus_manager->AddFocusChangeListener(this);

  focus_manager_ = focus_manager;
}

void BridgedNativeWidget::SetBounds(const gfx::Rect& new_bounds) {
  // A contentRect with zero width or height is a banned practice in ChromeMac,
  // due to unpredictable OSX treatment.
  DCHECK(!new_bounds.IsEmpty()) << "Zero-sized windows not supported on Mac";

  gfx::Rect actual_new_bounds(new_bounds);

  if (parent_ &&
      !PositionWindowInScreenCoordinates(native_widget_mac_->GetWidget(),
                                         widget_type_))
    actual_new_bounds.Offset(parent_->GetRestoredBounds().OffsetFromOrigin());

  [window_ setFrame:gfx::ScreenRectToNSRect(actual_new_bounds)
            display:YES
            animate:NO];
}

void BridgedNativeWidget::SetRootView(views::View* view) {
  if (view == [bridged_view_ hostedView])
    return;

  // If this is ever false, the compositor will need to be properly torn down
  // and replaced, pointing at the new view.
  DCHECK(!view || !compositor_widget_);

  [bridged_view_ clearView];
  bridged_view_.reset();
  // Note that there can still be references to the old |bridged_view_|
  // floating around in Cocoa libraries at this point. However, references to
  // the old views::View will be gone, so any method calls will become no-ops.

  if (view) {
    bridged_view_.reset([[BridgedContentView alloc] initWithView:view]);
    // Objective C initializers can return nil. However, if |view| is non-NULL
    // this should be treated as an error and caught early.
    CHECK(bridged_view_);
  }
  [window_ setContentView:bridged_view_];
}

void BridgedNativeWidget::SetVisibilityState(WindowVisibilityState new_state) {
  // Ensure that:
  //  - A window with an invisible parent is not made visible.
  //  - A parent changing visibility updates child window visibility.
  //    * But only when changed via this function - ignore changes via the
  //      NSWindow API, or changes propagating out from here.
  wants_to_be_visible_ = new_state != HIDE_WINDOW;

  if (new_state == HIDE_WINDOW) {
    [window_ orderOut:nil];
    DCHECK(!window_visible_);
    return;
  }

  DCHECK(wants_to_be_visible_);

  // If there's a hidden ancestor, return and wait for it to become visible.
  for (BridgedNativeWidget* ancestor = parent();
       ancestor;
       ancestor = ancestor->parent()) {
    if (!ancestor->window_visible_)
      return;
  }

  if (new_state == SHOW_AND_ACTIVATE_WINDOW) {
    [window_ makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  } else {
    // ui::SHOW_STATE_INACTIVE is typically used to avoid stealing focus from a
    // parent window. So, if there's a parent, order above that. Otherwise, this
    // will order above all windows at the same level.
    NSInteger parent_window_number = 0;
    if (parent())
      parent_window_number = [parent()->ns_window() windowNumber];

    [window_ orderWindow:NSWindowAbove
              relativeTo:parent_window_number];
  }
  DCHECK(window_visible_);
}

void BridgedNativeWidget::AcquireCapture() {
  DCHECK(!HasCapture());
  if (!window_visible_)
    return;  // Capture on hidden windows is disallowed.

  mouse_capture_.reset(new CocoaMouseCapture(this));

  // Initiating global event capture with addGlobalMonitorForEventsMatchingMask:
  // will reset the mouse cursor to an arrow. Asking the window for an update
  // here will restore what we want. However, it can sometimes cause the cursor
  // to flicker, once, on the initial mouseDown.
  // TOOD(tapted): Make this unnecessary by only asking for global mouse capture
  // for the cases that need it (e.g. menus, but not drag and drop).
  [window_ cursorUpdate:[NSApp currentEvent]];
}

void BridgedNativeWidget::ReleaseCapture() {
  mouse_capture_.reset();
}

bool BridgedNativeWidget::HasCapture() {
  return mouse_capture_ && mouse_capture_->IsActive();
}

void BridgedNativeWidget::SetNativeWindowProperty(const char* name,
                                                  void* value) {
  NSString* key = [NSString stringWithUTF8String:name];
  if (value) {
    [GetWindowProperties() setObject:[NSValue valueWithPointer:value]
                              forKey:key];
  } else {
    [GetWindowProperties() removeObjectForKey:key];
  }
}

void* BridgedNativeWidget::GetNativeWindowProperty(const char* name) const {
  NSString* key = [NSString stringWithUTF8String:name];
  return [[GetWindowProperties() objectForKey:key] pointerValue];
}

void BridgedNativeWidget::SetCursor(NSCursor* cursor) {
  [window_delegate_ setCursor:cursor];
}

void BridgedNativeWidget::OnWindowWillClose() {
  if (parent_)
    parent_->RemoveChildWindow(this);
  [window_ setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:window_delegate_];
  native_widget_mac_->OnWindowWillClose();
}

void BridgedNativeWidget::OnFullscreenTransitionStart(
    bool target_fullscreen_state) {
  // Note: This can fail for fullscreen changes started externally, but a user
  // shouldn't be able to do that if the window is invisible to begin with.
  DCHECK(window_visible_);

  DCHECK_NE(target_fullscreen_state, target_fullscreen_state_);
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    bounds_before_fullscreen_ = gfx::ScreenRectFromNSRect([window_ frame]);
}

void BridgedNativeWidget::OnFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;
  if (target_fullscreen_state_ == actual_fullscreen_state)
    return;

  // First update to reflect reality so that OnTargetFullscreenStateChanged()
  // expects the change.
  target_fullscreen_state_ = actual_fullscreen_state;
  ToggleDesiredFullscreenState();

  // Usually ToggleDesiredFullscreenState() sets |in_fullscreen_transition_| via
  // OnFullscreenTransitionStart(). When it does not, it means Cocoa ignored the
  // toggleFullScreen: request. This can occur when the fullscreen transition
  // fails and Cocoa is *about* to send windowDidFailToEnterFullScreen:.
  // Annoyingly, for this case, Cocoa first sends windowDidExitFullScreen:.
  if (in_fullscreen_transition_)
    DCHECK_NE(target_fullscreen_state_, actual_fullscreen_state);
}

void BridgedNativeWidget::ToggleDesiredFullscreenState() {
  // If there is currently an animation into or out of fullscreen, then AppKit
  // emits the string "not in fullscreen state" to stdio and does nothing. For
  // this case, schedule a transition back into the desired state when the
  // animation completes.
  if (in_fullscreen_transition_) {
    target_fullscreen_state_ = !target_fullscreen_state_;
    return;
  }

  // Going fullscreen implicitly makes the window visible. AppKit does this.
  // That is, -[NSWindow isVisible] is always true after a call to -[NSWindow
  // toggleFullScreen:]. Unfortunately, this change happens after AppKit calls
  // -[NSWindowDelegate windowWillEnterFullScreen:], and AppKit doesn't send an
  // orderWindow message. So intercepting the implicit change is hard.
  // Luckily, to trigger externally, the window typically needs to be visible in
  // the first place. So we can just ensure the window is visible here instead
  // of relying on AppKit to do it, and not worry that OnVisibilityChanged()
  // won't be called for externally triggered fullscreen requests.
  if (!window_visible_)
    SetVisibilityState(SHOW_INACTIVE);

  if (base::mac::IsOSSnowLeopard()) {
    NOTIMPLEMENTED();
    return;  // TODO(tapted): Implement this for Snow Leopard.
  }

  // Since fullscreen requests are ignored if the collection behavior does not
  // allow it, save the collection behavior and restore it after.
  NSWindowCollectionBehavior behavior = [window_ collectionBehavior];
  [window_ setCollectionBehavior:behavior |
                                 NSWindowCollectionBehaviorFullScreenPrimary];
  [window_ toggleFullScreen:nil];
  [window_ setCollectionBehavior:behavior];
}

void BridgedNativeWidget::OnSizeChanged() {
  gfx::Size new_size = GetClientAreaSize();
  native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(new_size);
  if (layer())
    UpdateLayerProperties();
}

void BridgedNativeWidget::OnVisibilityChanged() {
  OnVisibilityChangedTo([window_ isVisible]);
}

void BridgedNativeWidget::OnVisibilityChangedTo(bool new_visibility) {
  if (window_visible_ == new_visibility)
    return;

  window_visible_ = new_visibility;

  // If arriving via SetVisible(), |wants_to_be_visible_| should already be set.
  // If made visible externally (e.g. Cmd+H), just roll with it. Don't try (yet)
  // to distinguish being *hidden* externally from being hidden by a parent
  // window - we might not need that.
  if (window_visible_) {
    wants_to_be_visible_ = true;

    if (parent_)
      [parent_->ns_window() addChildWindow:window_ ordered:NSWindowAbove];
  } else {
    mouse_capture_.reset();  // Capture on hidden windows is not permitted.

    // When becoming invisible, remove the entry in any parent's childWindow
    // list. Cocoa's childWindow management breaks down when child windows are
    // hidden.
    if (parent_)
      [parent_->ns_window() removeChildWindow:window_];
  }

  // TODO(tapted): Investigate whether we want this for Mac. This is what Aura
  // does, and it is what tests expect. However, because layer drawing is
  // asynchronous (and things like deminiaturize in AppKit are not), it can
  // result in a CALayer appearing on screen before it has been redrawn in the
  // GPU process. This is a general problem. In content, a helper class,
  // RenderWidgetResizeHelper, blocks the UI thread in -[NSView setFrameSize:]
  // and RenderWidgetHostView::Show() until a frame is ready.
  if (layer()) {
    layer()->SetVisible(window_visible_);
    layer()->SchedulePaint(gfx::Rect(GetClientAreaSize()));
  }

  NotifyVisibilityChangeDown();

  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible_);

  // Toolkit-views suppresses redraws while not visible. To prevent Cocoa asking
  // for an "empty" draw, disable auto-display while hidden. For example, this
  // prevents Cocoa drawing just *after* a minimize, resulting in a blank window
  // represented in the deminiaturize animation.
  [window_ setAutodisplay:window_visible_];
}

void BridgedNativeWidget::OnBackingPropertiesChanged() {
  if (layer())
    UpdateLayerProperties();
}

void BridgedNativeWidget::OnWindowKeyStatusChangedTo(bool is_key) {
  Widget* widget = native_widget_mac()->GetWidget();
  widget->OnNativeWidgetActivationChanged(is_key);
  // The contentView is the BridgedContentView hosting the views::RootView. The
  // focus manager will already know if a native subview has focus.
  if ([window_ contentView] == [window_ firstResponder]) {
    if (is_key)
      widget->GetFocusManager()->RestoreFocusedView();
    else
      widget->GetFocusManager()->StoreFocusedView(true);
  }
}

InputMethod* BridgedNativeWidget::CreateInputMethod() {
  if (switches::IsTextInputFocusManagerEnabled())
    return new NullInputMethod();

  return new InputMethodBridge(this, GetHostInputMethod(), true);
}

ui::InputMethod* BridgedNativeWidget::GetHostInputMethod() {
  if (!input_method_) {
    // Delegate is NULL because Mac IME does not need DispatchKeyEventPostIME
    // callbacks.
    input_method_ = ui::CreateInputMethod(NULL, nil);
  }
  return input_method_.get();
}

gfx::Rect BridgedNativeWidget::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return bounds_before_fullscreen_;

  return gfx::ScreenRectFromNSRect([window_ frame]);
}

void BridgedNativeWidget::CreateLayer(ui::LayerType layer_type,
                                      bool translucent) {
  DCHECK(bridged_view_);
  DCHECK(!layer());

  CreateCompositor();
  DCHECK(compositor_);

  SetLayer(new ui::Layer(layer_type));
  // Note, except for controls, this will set the layer to be hidden, since it
  // is only called during Init().
  layer()->SetVisible(window_visible_);
  layer()->set_delegate(this);

  InitCompositor();

  // Transparent window support.
  layer()->GetCompositor()->SetHostHasTransparentBackground(translucent);
  layer()->SetFillsBoundsOpaquely(!translucent);
  if (translucent) {
    [window_ setOpaque:NO];
    [window_ setBackgroundColor:[NSColor clearColor]];
  }

  UpdateLayerProperties();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, internal::InputMethodDelegate:

void BridgedNativeWidget::DispatchKeyEventPostIME(const ui::KeyEvent& key) {
  DCHECK(focus_manager_);
  native_widget_mac_->GetWidget()->OnKeyEvent(const_cast<ui::KeyEvent*>(&key));
  if (!key.handled())
    focus_manager_->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, CocoaMouseCaptureDelegate:

void BridgedNativeWidget::PostCapturedEvent(NSEvent* event) {
  [bridged_view_ processCapturedMouseEvent:event];
}

void BridgedNativeWidget::OnMouseCaptureLost() {
  native_widget_mac_->GetWidget()->OnMouseCaptureLost();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, FocusChangeListener:

void BridgedNativeWidget::OnWillChangeFocus(View* focused_before,
                                            View* focused_now) {
}

void BridgedNativeWidget::OnDidChangeFocus(View* focused_before,
                                           View* focused_now) {
  ui::TextInputClient* input_client =
      focused_now ? focused_now->GetTextInputClient() : NULL;
  [bridged_view_ setTextInputClient:input_client];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, LayerDelegate:

void BridgedNativeWidget::OnPaintLayer(gfx::Canvas* canvas) {
  DCHECK(window_visible_);
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(canvas);
}

void BridgedNativeWidget::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
  NOTIMPLEMENTED();
}

void BridgedNativeWidget::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

base::Closure BridgedNativeWidget::PrepareForLayerBoundsChange() {
  NOTIMPLEMENTED();
  return base::Closure();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, AcceleratedWidgetMac:

NSView* BridgedNativeWidget::AcceleratedWidgetGetNSView() const {
  return compositor_superview_;
}

bool BridgedNativeWidget::AcceleratedWidgetShouldIgnoreBackpressure() const {
  return true;
}

void BridgedNativeWidget::AcceleratedWidgetSwapCompleted(
    const std::vector<ui::LatencyInfo>& latency_info) {
}

void BridgedNativeWidget::AcceleratedWidgetHitError() {
  compositor_->ScheduleFullRedraw();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, private:

void BridgedNativeWidget::RemoveOrDestroyChildren() {
  // TODO(tapted): Implement unowned child windows if required.
  while (!child_windows_.empty()) {
    // The NSWindow can only be destroyed after -[NSWindow close] is complete.
    // Retain the window, otherwise the reference count can reach zero when the
    // child calls back into RemoveChildWindow() via its OnWindowWillClose().
    base::scoped_nsobject<NSWindow> child(
        [child_windows_.back()->ns_window() retain]);
    [child close];
  }
}

void BridgedNativeWidget::RemoveChildWindow(BridgedNativeWidget* child) {
  auto location = std::find(
      child_windows_.begin(), child_windows_.end(), child);
  DCHECK(location != child_windows_.end());
  child_windows_.erase(location);
  child->parent_ = nullptr;

  // Note the child is sometimes removed already by AppKit. This depends on OS
  // version, and possibly some unpredictable reference counting. Removing it
  // here should be safe regardless.
  [window_ removeChildWindow:child->window_];
}

void BridgedNativeWidget::NotifyVisibilityChangeDown() {
  // Child windows sometimes like to close themselves in response to visibility
  // changes. That's supported, but only with the asynchronous Widget::Close().
  // Perform a heuristic to detect child removal that would break these loops.
  const size_t child_count = child_windows_.size();
  if (!window_visible_) {
    for (BridgedNativeWidget* child : child_windows_) {
      if (child->window_visible_)
        [child->ns_window() orderOut:nil];

      DCHECK(!child->window_visible_);
      CHECK_EQ(child_count, child_windows_.size());
    }
    // The orderOut calls above should result in a call to OnVisibilityChanged()
    // in each child. There, children will remove themselves from the NSWindow
    // childWindow list as well as propagate NotifyVisibilityChangeDown() calls
    // to any children of their own.
    DCHECK_EQ(0u, [[window_ childWindows] count]);
    return;
  }

  NSUInteger visible_children = 0;  // For a DCHECK below.
  NSInteger parent_window_number = [window_ windowNumber];
  for (BridgedNativeWidget* child: child_windows_) {
    // Note: order the child windows on top, regardless of whether or not they
    // are currently visible. They probably aren't, since the parent was hidden
    // prior to this, but they could have been made visible in other ways.
    if (child->wants_to_be_visible_) {
      ++visible_children;
      // Here -[NSWindow orderWindow:relativeTo:] is used to put the window on
      // screen. However, that by itself is insufficient to guarantee a correct
      // z-order relationship. If this function is being called from a z-order
      // change in the parent, orderWindow turns out to be unreliable (i.e. the
      // ordering doesn't always take effect). What this actually relies on is
      // the resulting call to OnVisibilityChanged() in the child, which will
      // then insert itself into -[NSWindow childWindows] to let Cocoa do its
      // internal layering magic.
      [child->ns_window() orderWindow:NSWindowAbove
                           relativeTo:parent_window_number];
      DCHECK(child->window_visible_);
    }
    CHECK_EQ(child_count, child_windows_.size());
  }
  DCHECK_EQ(visible_children, [[window_ childWindows] count]);
}

gfx::Size BridgedNativeWidget::GetClientAreaSize() const {
  NSRect content_rect = [window_ contentRectForFrameRect:[window_ frame]];
  return gfx::Size(NSWidth(content_rect), NSHeight(content_rect));
}

void BridgedNativeWidget::CreateCompositor() {
  DCHECK(!compositor_);
  DCHECK(!compositor_widget_);
  DCHECK(ViewsDelegate::views_delegate);

  ui::ContextFactory* context_factory =
      ViewsDelegate::views_delegate->GetContextFactory();
  DCHECK(context_factory);

  AddCompositorSuperview();

  // TODO(tapted): Get this value from GpuDataManagerImpl via ViewsDelegate.
  bool needs_gl_finish_workaround = false;

  compositor_widget_.reset(
      new ui::AcceleratedWidgetMac(needs_gl_finish_workaround));
  compositor_.reset(new ui::Compositor(compositor_widget_->accelerated_widget(),
                                       context_factory,
                                       base::ThreadTaskRunnerHandle::Get()));
  compositor_widget_->SetNSView(this);
}

void BridgedNativeWidget::InitCompositor() {
  DCHECK(layer());
  float scale_factor = GetDeviceScaleFactorFromView(compositor_superview_);
  gfx::Size size_in_dip = GetClientAreaSize();
  compositor_->SetScaleAndSize(scale_factor,
                               ConvertSizeToPixel(scale_factor, size_in_dip));
  compositor_->SetRootLayer(layer());
}

void BridgedNativeWidget::DestroyCompositor() {
  if (layer())
    layer()->set_delegate(nullptr);
  DestroyLayer();

  if (!compositor_widget_) {
    DCHECK(!compositor_);
    return;
  }
  compositor_widget_->ResetNSView();
  compositor_.reset();
  compositor_widget_.reset();
}

void BridgedNativeWidget::AddCompositorSuperview() {
  DCHECK(!compositor_superview_);
  compositor_superview_.reset(
      [[NSView alloc] initWithFrame:[bridged_view_ bounds]]);

  // Size and resize automatically with |bridged_view_|.
  [compositor_superview_
      setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  // Enable HiDPI backing when supported (only on 10.7+).
  if ([compositor_superview_ respondsToSelector:
      @selector(setWantsBestResolutionOpenGLSurface:)]) {
    [compositor_superview_ setWantsBestResolutionOpenGLSurface:YES];
  }

  base::scoped_nsobject<CALayer> background_layer([[CALayer alloc] init]);
  [background_layer
      setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];

  // Set the layer first to create a layer-hosting view (not layer-backed).
  [compositor_superview_ setLayer:background_layer];
  [compositor_superview_ setWantsLayer:YES];

  // The UI compositor should always be the first subview, to ensure webviews
  // are drawn on top of it.
  DCHECK_EQ(0u, [[bridged_view_ subviews] count]);
  [bridged_view_ addSubview:compositor_superview_];
}

void BridgedNativeWidget::UpdateLayerProperties() {
  DCHECK(layer());
  DCHECK(compositor_superview_);
  gfx::Size size_in_dip = GetClientAreaSize();
  layer()->SetBounds(gfx::Rect(size_in_dip));

  float scale_factor = GetDeviceScaleFactorFromView(compositor_superview_);
  compositor_->SetScaleAndSize(scale_factor,
                               ConvertSizeToPixel(scale_factor, size_in_dip));
}

NSMutableDictionary* BridgedNativeWidget::GetWindowProperties() const {
  NSMutableDictionary* properties = objc_getAssociatedObject(
      window_, &kWindowPropertiesKey);
  if (!properties) {
    properties = [NSMutableDictionary dictionary];
    objc_setAssociatedObject(window_, &kWindowPropertiesKey,
                             properties, OBJC_ASSOCIATION_RETAIN);
  }
  return properties;
}

}  // namespace views
