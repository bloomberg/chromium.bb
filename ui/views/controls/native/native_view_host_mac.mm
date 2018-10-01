// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"
#import "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

void EnsureNativeViewHasNoChildWidgets(NSView* native_view) {
  DCHECK(native_view);
  // Mac's NativeViewHost has no support for hosting its own child widgets.
  // This check is probably overly restrictive, since the Widget containing the
  // NativeViewHost _is_ allowed child Widgets. However, we don't know yet
  // whether those child Widgets need to be distinguished from Widgets that code
  // might want to associate with the hosted NSView instead.
  {
    Widget::Widgets child_widgets;
    Widget::GetAllChildWidgets(native_view, &child_widgets);
    CHECK_GE(1u, child_widgets.size());  // 1 (itself) or 0 if detached.
  }
}

AXPlatformNodeCocoa* ClosestPlatformAncestorNode(views::View* view) {
  do {
    gfx::NativeViewAccessible accessible = view->GetNativeViewAccessible();
    if ([accessible isKindOfClass:[AXPlatformNodeCocoa class]]) {
      return NSAccessibilityUnignoredAncestor(accessible);
    }
    view = view->parent();
  } while (view);
  return nil;
}

}  // namespace

NativeViewHostMac::NativeViewHostMac(NativeViewHost* host) : host_(host) {
  // Ensure that |host_| have its own ui::Layer and that it draw nothing.
  host_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
}

NativeViewHostMac::~NativeViewHostMac() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostMac, ViewsHostableView::Host implementation:

ui::Layer* NativeViewHostMac::GetUiLayer() const {
  return host_->layer();
}

id NativeViewHostMac::GetAccessibilityElement() const {
  // Find the closest ancestor view that participates in the views toolkit
  // accessibility hierarchy and set its element as the native view's parent.
  // This is necessary because a closer ancestor might already be attaching
  // to the NSView/content hierarchy.
  // For example, web content is currently embedded into the views hierarchy
  // roughly like this:
  // BrowserView (views)
  // |_  WebView (views)
  //   |_  NativeViewHost (views)
  //     |_  WebContentView (Cocoa, is |native_view_| in this scenario,
  //         |               accessibility ignored).
  //         |_ RenderWidgetHostView (Cocoa)
  // WebView specifies either the RenderWidgetHostView or the native view as
  // its accessibility element. That means that if we were to set it as
  // |native_view_|'s parent, the RenderWidgetHostView would be its own
  // accessibility parent! Instead, we want to find the browser view and
  // attach to its node.
  return ClosestPlatformAncestorNode(host_->parent());
}

void NativeViewHostMac::OnHostableViewDestroying() {
  DCHECK(native_view_hostable_);
  host_->NativeViewDestroyed();
  DCHECK(!native_view_hostable_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostMac, NativeViewHostWrapper implementation:

void NativeViewHostMac::AttachNativeView() {
  DCHECK(host_->native_view());
  DCHECK(!native_view_);
  native_view_.reset([host_->native_view() retain]);
  EnsureNativeViewHasNoChildWidgets(native_view_);
  BridgedNativeWidgetHostImpl* bridge_host =
      BridgedNativeWidgetHostImpl::GetFromNativeWindow(
          host_->GetWidget()->GetNativeWindow());
  DCHECK(bridge_host);
  bridge_host->SetAssociationForView(host_, native_view_);

  if ([native_view_ conformsToProtocol:@protocol(ViewsHostable)]) {
    id hostable = native_view_;
    native_view_hostable_ = [hostable viewsHostableView];
    if (native_view_hostable_)
      native_view_hostable_->OnViewsHostableAttached(this);
  }
}

void NativeViewHostMac::NativeViewDetaching(bool destroyed) {
  // |destroyed| is only true if this class calls host_->NativeViewDestroyed(),
  // which is called if a hosted WebContentsView about to be destroyed (note
  // that its corresponding NSView may still exist).

  // |native_view_| can be nil here if RemovedFromWidget() is called before
  // NativeViewHost::Detach().
  if (!native_view_) {
    DCHECK(![host_->native_view() superview]);
    return;
  }

  DCHECK(native_view_ == host_->native_view());
  [host_->native_view() setHidden:YES];
  [host_->native_view() removeFromSuperview];

  if (native_view_hostable_) {
    native_view_hostable_->OnViewsHostableDetached();
    native_view_hostable_ = nullptr;
  }

  EnsureNativeViewHasNoChildWidgets(host_->native_view());
  BridgedNativeWidgetHostImpl* bridge_host =
      BridgedNativeWidgetHostImpl::GetFromNativeWindow(
          host_->GetWidget()->GetNativeWindow());
  // BridgedNativeWidgetImpl can be null when Widget is closing.
  if (bridge_host)
    bridge_host->ClearAssociationForView(host_);

  native_view_.reset();
}

void NativeViewHostMac::AddedToWidget() {
  if (!host_->native_view())
    return;

  AttachNativeView();
  host_->Layout();
}

void NativeViewHostMac::RemovedFromWidget() {
  if (!host_->native_view())
    return;

  NativeViewDetaching(false);
}

bool NativeViewHostMac::SetCustomMask(std::unique_ptr<ui::LayerOwner> mask) {
  NOTIMPLEMENTED();
  return false;
}

void NativeViewHostMac::InstallClip(int x, int y, int w, int h) {
  NOTIMPLEMENTED();
}

bool NativeViewHostMac::HasInstalledClip() {
  return false;
}

void NativeViewHostMac::UninstallClip() {
  NOTIMPLEMENTED();
}

void NativeViewHostMac::ShowWidget(int x,
                                   int y,
                                   int w,
                                   int h,
                                   int native_w,
                                   int native_h) {
  if (host_->fast_resize())
    NOTIMPLEMENTED();

  // Coordinates will be from the top left of the parent Widget. The NativeView
  // is already in the same NSWindow, so just flip to get Cooca coordinates and
  // then convert to the containing view.
  NSRect window_rect = NSMakeRect(
      x,
      host_->GetWidget()->GetClientAreaBoundsInScreen().height() - y - h,
      w,
      h);

  // Convert window coordinates to the hosted view's superview, since that's how
  // coordinates of the hosted view's frame is based.
  NSRect container_rect =
      [[host_->native_view() superview] convertRect:window_rect fromView:nil];
  [host_->native_view() setFrame:container_rect];
  [host_->native_view() setHidden:NO];
}

void NativeViewHostMac::HideWidget() {
  [host_->native_view() setHidden:YES];
}

void NativeViewHostMac::SetFocus() {
  if ([host_->native_view() acceptsFirstResponder])
    [[host_->native_view() window] makeFirstResponder:host_->native_view()];
}

gfx::NativeView NativeViewHostMac::GetNativeViewContainer() const {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeViewAccessible NativeViewHostMac::GetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeCursor NativeViewHostMac::GetCursor(int x, int y) {
  // Intentionally not implemented: Not required on non-aura Mac because OSX
  // will query the native view for the cursor directly. For NativeViewHostMac
  // in practice, OSX will retrieve the cursor that was last set by
  // -[RenderWidgetHostViewCocoa updateCursor:] whenever the pointer is over the
  // hosted view. With some plumbing, NativeViewHostMac could return that same
  // cursor here, but it doesn't achieve anything. The implications of returning
  // null simply mean that the "fallback" cursor on the window itself will be
  // cleared (see -[NativeWidgetMacNSWindow cursorUpdate:]). However, while the
  // pointer is over a RenderWidgetHostViewCocoa, OSX won't ask for the fallback
  // cursor.
  return gfx::kNullCursor;
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostMac(host);
}

}  // namespace views
