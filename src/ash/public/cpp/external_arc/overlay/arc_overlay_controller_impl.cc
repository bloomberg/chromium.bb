// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller_impl.h"

#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/window_targeter.h"
#include "ui/views/widget/widget.h"

namespace ash {

ArcOverlayControllerImpl::ArcOverlayControllerImpl(aura::Window* host_window)
    : host_window_(host_window) {
  DCHECK(host_window_);

  VLOG(1) << "Host is " << host_window_->GetName();

  host_window_observer_.Observe(host_window_);

  overlay_container_ = new views::NativeViewHost();
  overlay_container_observer_.Observe(overlay_container_);

  auto* const widget = views::Widget::GetWidgetForNativeWindow(
      host_window_->GetToplevelWindow());
  DCHECK(widget);
  DCHECK(widget->GetContentsView());
  widget->GetContentsView()->AddChildView(overlay_container_);
}

ArcOverlayControllerImpl::~ArcOverlayControllerImpl() = default;

void ArcOverlayControllerImpl::AttachOverlay(aura::Window* overlay_window) {
  if (!overlay_container_ || !host_window_)
    return;

  DCHECK(overlay_window);
  DCHECK(!overlay_container_->native_view())
      << "An overlay is already attached";

  VLOG(1) << "Attaching overlay " << overlay_window->GetName() << " to host "
          << host_window_->GetName();

  overlay_window_ = overlay_window;
  overlay_window_observer_.Observe(overlay_window);

  overlay_container_->Attach(overlay_window_);
  overlay_container_->GetNativeViewContainer()->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());

  UpdateHostBounds();
}

void ArcOverlayControllerImpl::OnWindowDestroying(aura::Window* window) {
  if (host_window_observer_.IsObservingSource(window)) {
    host_window_ = nullptr;
    host_window_observer_.RemoveObservation();
  }
  if (overlay_window_observer_.IsObservingSource(window)) {
    overlay_window_ = nullptr;
    overlay_window_observer_.RemoveObservation();
  }
}

void ArcOverlayControllerImpl::OnViewIsDeleting(views::View* observed_view) {
  if (overlay_container_observer_.IsObservingSource(observed_view)) {
    overlay_container_ = nullptr;
    overlay_container_observer_.RemoveObservation();
  }
}

void ArcOverlayControllerImpl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (host_window_observer_.IsObservingSource(window) &&
      old_bounds.size() != new_bounds.size()) {
    UpdateHostBounds();
  }
}

void ArcOverlayControllerImpl::UpdateHostBounds() {
  if (!overlay_container_observer_.IsObserving()) {
    LOG(ERROR) << "No container to resize";
    return;
  }

  gfx::Point origin;
  gfx::Size size = host_window_->bounds().size();
  ConvertPointFromWindow(host_window_, &origin);
  overlay_container_->SetBounds(origin.x(), origin.y(), size.width(),
                                size.height());
}

void ArcOverlayControllerImpl::ConvertPointFromWindow(aura::Window* window,
                                                      gfx::Point* point) {
  views::Widget* const widget = overlay_container_->GetWidget();
  aura::Window::ConvertPointToTarget(window, widget->GetNativeWindow(), point);
  views::View::ConvertPointFromWidget(widget->GetContentsView(), point);
}

}  // namespace ash
