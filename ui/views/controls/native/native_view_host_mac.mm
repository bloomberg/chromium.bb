// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ui/views/cocoa/bridged_content_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// Reparents |native_view| to be a child of the native content view of
// |new_parent|.
void ReparentNSView(NSView* native_view, Widget* new_parent) {
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

  if (!new_parent) {
    [native_view removeFromSuperview];
    return;
  }

  BridgedContentView* new_superview =
      base::mac::ObjCCastStrict<BridgedContentView>(
          new_parent->GetNativeView());
  DCHECK(new_superview);
  [new_superview addSubview:native_view];
}

}  // namespace

NativeViewHostMac::NativeViewHostMac(NativeViewHost* host) : host_(host) {
}

NativeViewHostMac::~NativeViewHostMac() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostMac, NativeViewHostWrapper implementation:

void NativeViewHostMac::AttachNativeView() {
  DCHECK(host_->native_view());
  DCHECK(!native_view_);
  native_view_.reset([host_->native_view() retain]);
  ReparentNSView(host_->native_view(), host_->GetWidget());
}

void NativeViewHostMac::NativeViewDetaching(bool destroyed) {
  // |destroyed| is only true if this class calls host_->NativeViewDestroyed().
  // Aura does this after observing an aura OnWindowDestroying, but NSViews
  // are reference counted so there isn't a reliable signal. Instead, a
  // reference is retained until the NativeViewHost is detached.
  DCHECK(!destroyed);
  // |native_view_| can be nil here if RemovedFromWidget() is called before
  // NativeViewHost::Detach().
  DCHECK(!native_view_ || native_view_ == host_->native_view());
  [host_->native_view() setHidden:YES];
  ReparentNSView(host_->native_view(), NULL);
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

void NativeViewHostMac::InstallClip(int x, int y, int w, int h) {
  NOTIMPLEMENTED();
}

bool NativeViewHostMac::HasInstalledClip() {
  return false;
}

void NativeViewHostMac::UninstallClip() {
  NOTIMPLEMENTED();
}

void NativeViewHostMac::ShowWidget(int x, int y, int w, int h) {
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

gfx::NativeViewAccessible NativeViewHostMac::GetNativeViewAccessible() {
  return NULL;
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
