// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_surface.h"

#include "base/memory/ptr_util.h"
#include "services/ui/public/cpp/window_surface_client.h"

namespace ui {

// static
std::unique_ptr<WindowSurface> WindowSurface::Create(
    std::unique_ptr<WindowSurfaceBinding>* surface_binding) {
  cc::mojom::MojoCompositorFrameSinkPtr surface;
  cc::mojom::MojoCompositorFrameSinkClientPtr surface_client;
  mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSinkClient>
      surface_client_request = GetProxy(&surface_client);

  surface_binding->reset(new WindowSurfaceBinding(
      GetProxy(&surface), surface_client.PassInterface()));
  return base::WrapUnique(new WindowSurface(surface.PassInterface(),
                                            std::move(surface_client_request)));
}

WindowSurface::~WindowSurface() {}

void WindowSurface::BindToThread() {
  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
  surface_.Bind(std::move(surface_info_));
  client_binding_.reset(
      new mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>(
          this, std::move(client_request_)));
}

void WindowSurface::SubmitCompositorFrame(cc::CompositorFrame frame) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!surface_)
    return;
  surface_->SubmitCompositorFrame(std::move(frame));
}

WindowSurface::WindowSurface(
    mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSink> surface_info,
    mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSinkClient>
        client_request)
    : client_(nullptr),
      surface_info_(std::move(surface_info)),
      client_request_(std::move(client_request)) {}

void WindowSurface::DidReceiveCompositorFrameAck() {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
}

void WindowSurface::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->ReclaimResources(std::move(resources));
}

WindowSurfaceBinding::~WindowSurfaceBinding() {}

WindowSurfaceBinding::WindowSurfaceBinding(
    mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> surface_request,
    mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
        surface_client)
    : surface_request_(std::move(surface_request)),
      surface_client_(std::move(surface_client)) {}

}  // namespace ui
