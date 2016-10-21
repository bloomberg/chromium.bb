// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "services/ui/public/cpp/window_surface_client.h"

namespace ui {

class WindowSurfaceBinding;
class WindowMojoCompositorFrameSinkClient;
class Window;

// A WindowSurface is wrapper to simplify submitting CompositorFrames to
// Windows, and receiving ReturnedResources.
class WindowSurface : public cc::mojom::MojoCompositorFrameSinkClient {
 public:
  // static
  static std::unique_ptr<WindowSurface> Create(
      std::unique_ptr<WindowSurfaceBinding>* surface_binding);

  ~WindowSurface() override;

  // Called to indicate that the current thread has assumed control of this
  // object.
  void BindToThread();

  void SubmitCompositorFrame(cc::CompositorFrame frame);

  void set_client(WindowSurfaceClient* client) { client_ = client; }

 private:
  friend class Window;

  WindowSurface(
      mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSink> surface_info,
      mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSinkClient>
          client_request);

  // MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  WindowSurfaceClient* client_;
  mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSink> surface_info_;
  mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSinkClient>
      client_request_;
  cc::mojom::MojoCompositorFrameSinkPtr surface_;
  std::unique_ptr<mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>>
      client_binding_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(WindowSurface);
};

// A WindowSurfaceBinding is a bundle of mojo interfaces that are to be used by
// or implemented by the Mus window server when passed into
// Window::AttachSurface. WindowSurfaceBinding has no standalone functionality.
class WindowSurfaceBinding {
 public:
  ~WindowSurfaceBinding();

 private:
  friend class WindowSurface;
  friend class Window;

  WindowSurfaceBinding(
      mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink>
          surface_request,
      mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
          surface_client);

  mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> surface_request_;
  mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
      surface_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowSurfaceBinding);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_SURFACE_H_
