// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/views/cocoa/native_widget_mac_nswindow.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/window/native_frame_view.h"

namespace views {
namespace {

NSInteger StyleMaskForParams(const Widget::InitParams& params) {
  // TODO(tapted): Determine better masks when there are use cases for it.
  if (params.remove_standard_frame)
    return NSBorderlessWindowMask;

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    return NSTitledWindowMask | NSClosableWindowMask |
           NSMiniaturizableWindowMask | NSResizableWindowMask;
  }
  return NSBorderlessWindowMask;
}

gfx::Size WindowSizeForClientAreaSize(NSWindow* window, const gfx::Size& size) {
  NSRect content_rect = NSMakeRect(0, 0, size.width(), size.height());
  NSRect frame_rect = [window frameRectForContentRect:content_rect];
  return gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, public:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      bridge_(new BridgedNativeWidget(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
}

NativeWidgetMac::~NativeWidgetMac() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

// static
BridgedNativeWidget* NativeWidgetMac::GetBridgeForNativeWindow(
    gfx::NativeWindow window) {
  id<NSWindowDelegate> window_delegate = [window delegate];
  if ([window_delegate respondsToSelector:@selector(nativeWidgetMac)]) {
    ViewsNSWindowDelegate* delegate =
        base::mac::ObjCCastStrict<ViewsNSWindowDelegate>(window_delegate);
    return [delegate nativeWidgetMac]->bridge_.get();
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

void NativeWidgetMac::OnWindowWillClose() {
  delegate_->OnNativeWidgetDestroying();
  // Note: If closed via CloseNow(), |bridge_| will already be reset. If closed
  // by the user, or via Close() and a RunLoop, this will reset it.
  bridge_.reset();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, internal::NativeWidgetPrivate implementation:

void NativeWidgetMac::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;
  base::scoped_nsobject<NSWindow> window([CreateNSWindow(params) retain]);
  [window setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
  bridge_->Init(window, params);

  delegate_->OnNativeWidgetCreated(true);

  bridge_->SetFocusManager(GetWidget()->GetFocusManager());

  DCHECK(GetWidget()->GetRootView());
  bridge_->SetRootView(GetWidget()->GetRootView());

  // "Infer" must be handled by ViewsDelegate::OnBeforeWidgetInit().
  DCHECK_NE(Widget::InitParams::INFER_OPACITY, params.opacity);
  bool translucent = params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;
  switch (params.layer_type) {
    case aura::WINDOW_LAYER_NONE:
      break;
    case aura::WINDOW_LAYER_TEXTURED:
      bridge_->CreateLayer(ui::LAYER_TEXTURED, translucent);
      break;
    case aura::WINDOW_LAYER_NOT_DRAWN:
      bridge_->CreateLayer(ui::LAYER_NOT_DRAWN, translucent);
      break;
    case aura::WINDOW_LAYER_SOLID_COLOR:
      bridge_->CreateLayer(ui::LAYER_SOLID_COLOR, translucent);
      break;
  }
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
  NOTIMPLEMENTED();
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
  return bridge_ ? bridge_->ns_window() : nil;
}

Widget* NativeWidgetMac::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetMac::GetCompositor() const {
  return bridge_ && bridge_->layer() ? bridge_->layer()->GetCompositor()
                                     : nullptr;
}

const ui::Layer* NativeWidgetMac::GetLayer() const {
  return bridge_ ? bridge_->layer() : nullptr;
}

void NativeWidgetMac::ReorderNativeViews() {
  if (bridge_)
    bridge_->SetRootView(GetWidget()->GetRootView());
}

void NativeWidgetMac::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetNativeWindowProperty(const char* name, void* value) {
  if (bridge_)
    bridge_->SetNativeWindowProperty(name, value);
}

void* NativeWidgetMac::GetNativeWindowProperty(const char* name) const {
  if (bridge_)
    return bridge_->GetNativeWindowProperty(name);

  return nullptr;
}

TooltipManager* NativeWidgetMac::GetTooltipManager() const {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetMac::SetCapture() {
  if (bridge_ && !bridge_->HasCapture())
    bridge_->AcquireCapture();
}

void NativeWidgetMac::ReleaseCapture() {
  if (bridge_)
    bridge_->ReleaseCapture();
}

bool NativeWidgetMac::HasCapture() const {
  return bridge_ && bridge_->HasCapture();
}

InputMethod* NativeWidgetMac::CreateInputMethod() {
  return bridge_ ? bridge_->CreateInputMethod() : NULL;
}

internal::InputMethodDelegate* NativeWidgetMac::GetInputMethodDelegate() {
  return bridge_.get();
}

ui::InputMethod* NativeWidgetMac::GetHostInputMethod() {
  return bridge_ ? bridge_->GetHostInputMethod() : NULL;
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  SetSize(WindowSizeForClientAreaSize(GetNativeWindow(), size));
  // Note that this is not the precise center of screen, but it is the standard
  // location for windows like dialogs to appear on screen for Mac.
  // TODO(tapted): If there is a parent window, center in that instead.
  [GetNativeWindow() center];
}

void NativeWidgetMac::GetWindowPlacement(gfx::Rect* bounds,
                                         ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::SetWindowTitle(const base::string16& title) {
  NSWindow* window = GetNativeWindow();
  NSString* current_title = [window title];
  NSString* new_title = SysUTF16ToNSString(title);
  if ([current_title isEqualToString:new_title])
    return false;

  [window setTitle:new_title];
  return true;
}

void NativeWidgetMac::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::InitModalType(ui::ModalType modal_type) {
  if (modal_type == ui::MODAL_TYPE_NONE)
    return;

  // System modal windows not implemented (or used) on Mac.
  DCHECK_NE(ui::MODAL_TYPE_SYSTEM, modal_type);
  DCHECK(bridge_->parent());
  // Everyhing happens upon show.
}

gfx::Rect NativeWidgetMac::GetWindowBoundsInScreen() const {
  return gfx::ScreenRectFromNSRect([GetNativeWindow() frame]);
}

gfx::Rect NativeWidgetMac::GetClientAreaBoundsInScreen() const {
  NSWindow* window = GetNativeWindow();
  return gfx::ScreenRectFromNSRect(
      [window contentRectForFrameRect:[window frame]]);
}

gfx::Rect NativeWidgetMac::GetRestoredBounds() const {
  return bridge_ ? bridge_->GetRestoredBounds() : gfx::Rect();
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  if (bridge_)
    bridge_->SetBounds(bounds);
}

void NativeWidgetMac::SetSize(const gfx::Size& size) {
  // Ensure the top-left corner stays in-place (rather than the bottom-left,
  // which -[NSWindow setContentSize:] would do).
  SetBounds(gfx::Rect(GetWindowBoundsInScreen().origin(), size));
}

void NativeWidgetMac::StackAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackBelow(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetShape(gfx::NativeRegion shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Close() {
  if (!bridge_)
    return;

  if (delegate_->IsModal()) {
    // Sheets can't be closed normally. This starts the sheet closing. Once the
    // sheet has finished animating, it will call sheetDidEnd: on the parent
    // window's delegate. Note it still needs to be asynchronous, since code
    // calling Widget::Close() doesn't expect things to be deleted upon return.
    [NSApp performSelector:@selector(endSheet:)
                withObject:GetNativeWindow()
                afterDelay:0];
    return;
  }

  // Clear the view early to suppress repaints.
  bridge_->SetRootView(NULL);

  NSWindow* window = GetNativeWindow();
  // Calling performClose: will momentarily highlight the close button, but
  // AppKit will reject it if there is no close button.
  SEL close_selector = ([window styleMask] & NSClosableWindowMask)
                           ? @selector(performClose:)
                           : @selector(close);
  [window performSelector:close_selector withObject:nil afterDelay:0];
}

void NativeWidgetMac::CloseNow() {
  // Reset |bridge_| to NULL before destroying it.
  scoped_ptr<BridgedNativeWidget> bridge(bridge_.Pass());
}

void NativeWidgetMac::Show() {
  ShowWithWindowState(ui::SHOW_STATE_NORMAL);
}

void NativeWidgetMac::Hide() {
  if (!bridge_)
    return;

  bridge_->SetVisibilityState(BridgedNativeWidget::HIDE_WINDOW);
}

void NativeWidgetMac::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ShowWithWindowState(ui::WindowShowState state) {
  if (!bridge_)
    return;

  switch (state) {
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
  bridge_->SetVisibilityState(state == ui::SHOW_STATE_INACTIVE
      ? BridgedNativeWidget::SHOW_INACTIVE
      : BridgedNativeWidget::SHOW_AND_ACTIVATE_WINDOW);
}

bool NativeWidgetMac::IsVisible() const {
  return bridge_ && bridge_->window_visible();
}

void NativeWidgetMac::Activate() {
  if (!bridge_)
    return;

  bridge_->SetVisibilityState(BridgedNativeWidget::SHOW_AND_ACTIVATE_WINDOW);
}

void NativeWidgetMac::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsActive() const {
  return [GetNativeWindow() isKeyWindow];
}

void NativeWidgetMac::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsAlwaysOnTop() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Maximize() {
  NOTIMPLEMENTED();  // See IsMaximized().
}

void NativeWidgetMac::Minimize() {
  NSWindow* window = GetNativeWindow();
  // Calling performMiniaturize: will momentarily highlight the button, but
  // AppKit will reject it if there is no miniaturize button.
  if ([window styleMask] & NSMiniaturizableWindowMask)
    [window performMiniaturize:nil];
  else
    [window miniaturize:nil];
}

bool NativeWidgetMac::IsMaximized() const {
  // The window frame isn't altered on Mac unless going fullscreen. The green
  // "+" button just makes the window bigger. So, always false.
  return false;
}

bool NativeWidgetMac::IsMinimized() const {
  return [GetNativeWindow() isMiniaturized];
}

void NativeWidgetMac::Restore() {
  [GetNativeWindow() deminiaturize:nil];
}

void NativeWidgetMac::SetFullscreen(bool fullscreen) {
  if (!bridge_ || fullscreen == IsFullscreen())
    return;

  bridge_->ToggleDesiredFullscreenState();
}

bool NativeWidgetMac::IsFullscreen() const {
  return bridge_ && bridge_->target_fullscreen_state();
}

void NativeWidgetMac::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::DragDropTypes::DragEventSource source) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SchedulePaintInRect(const gfx::Rect& rect) {
  // TODO(tapted): This should use setNeedsDisplayInRect:, once the coordinate
  // system of |rect| has been converted.
  [GetNativeView() setNeedsDisplay:YES];
  if (bridge_ && bridge_->layer())
    bridge_->layer()->SchedulePaint(rect);
}

void NativeWidgetMac::SetCursor(gfx::NativeCursor cursor) {
  if (bridge_)
    bridge_->SetCursor(cursor);
}

bool NativeWidgetMac::IsMouseEventsEnabled() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::ClearNativeFocus() {
  // To quote DesktopWindowTreeHostX11, "This method is weird and misnamed."
  // The goal is to set focus to the content window, thereby removing focus from
  // any NSView in the window that doesn't belong to toolkit-views.
  [GetNativeWindow() makeFirstResponder:GetNativeView()];
}

gfx::Rect NativeWidgetMac::GetWorkAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMac::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMac::EndMoveLoop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityChangedAnimationsEnabled(bool value) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMac::GetNativeTheme() const {
  return ui::NativeTheme::instance();
}

void NativeWidgetMac::OnRootViewLayout() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsTranslucentWindowOpacitySupported() const {
  return false;
}

void NativeWidgetMac::OnSizeConstraintsChanged() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, protected:

NSWindow* NativeWidgetMac::CreateNSWindow(const Widget::InitParams& params) {
  return [[[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:StyleMaskForParams(params)
                  backing:NSBackingStoreBuffered
                    defer:YES] autorelease];
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
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
    gfx::NativeWindow native_window) {
  id<NSWindowDelegate> window_delegate = [native_window delegate];
  if ([window_delegate respondsToSelector:@selector(nativeWidgetMac)]) {
    ViewsNSWindowDelegate* delegate =
        base::mac::ObjCCastStrict<ViewsNSWindowDelegate>(window_delegate);
    return [delegate nativeWidgetMac];
  }
  return NULL;  // Not created by NativeWidgetMac.
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  BridgedNativeWidget* bridge =
      NativeWidgetMac::GetBridgeForNativeWindow([native_view window]);
  if (!bridge)
    return NULL;

  for (BridgedNativeWidget* parent;
       (parent = bridge->parent());
       bridge = parent) {
  }
  return bridge->native_widget_mac();
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  BridgedNativeWidget* bridge =
      NativeWidgetMac::GetBridgeForNativeWindow([native_view window]);
  if (!bridge)
    return;

  // If |native_view| is a subview of the contentView, it will share an
  // NSWindow, but will itself be a native child of the Widget. That is, adding
  // bridge->..->GetWidget() to |children| would be adding the _parent_ of
  // |native_view|, not the Widget for |native_view|. |native_view| doesn't have
  // a corresponding Widget of its own in this case (and so can't have Widget
  // children of its own on Mac).
  if (bridge->ns_view() != native_view)
    return;

  // Code expects widget for |native_view| to be added to |children|.
  if (bridge->native_widget_mac()->GetWidget())
    children->insert(bridge->native_widget_mac()->GetWidget());

  for (BridgedNativeWidget* child : bridge->child_windows())
    GetAllChildWidgets(child->ns_view(), children);
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  NOTIMPLEMENTED();
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  NOTIMPLEMENTED();
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  return [NSEvent pressedMouseButtons] != 0;
}

// static
gfx::FontList NativeWidgetPrivate::GetWindowTitleFontList() {
  NOTIMPLEMENTED();
  return gfx::FontList();
}

}  // namespace internal
}  // namespace views
