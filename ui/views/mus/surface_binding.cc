// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_binding.h"

#include <stdint.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/views/mus/window_tree_host_mus.h"

namespace views {

// PerConnectionState ----------------------------------------------------------

// State needed per ViewManager. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerConnectionState appropriate for the ViewManager. PerConnectionState is
// stored in a thread local map. When no more refereces to a PerConnectionState
// remain the PerConnectionState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerConnectionState
    : public base::RefCounted<PerConnectionState> {
 public:
  static PerConnectionState* Get(shell::Connector* connector,
                                 mus::WindowTreeConnection* connection);

  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      mus::Window* window,
      mus::mojom::SurfaceType type);

 private:
  typedef std::map<mus::WindowTreeConnection*, PerConnectionState*>
      ConnectionToStateMap;

  friend class base::RefCounted<PerConnectionState>;

  PerConnectionState(shell::Connector* connector,
                     mus::WindowTreeConnection* connection);
  ~PerConnectionState();

  void Init();

  static base::LazyInstance<
      base::ThreadLocalPointer<ConnectionToStateMap>>::Leaky window_states;

  shell::Connector* connector_;
  mus::WindowTreeConnection* connection_;

  // Set of state needed to create an OutputSurface.
  mus::mojom::GpuPtr gpu_;

  DISALLOW_COPY_AND_ASSIGN(PerConnectionState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerConnectionState::ConnectionToStateMap>>::Leaky
    SurfaceBinding::PerConnectionState::window_states;

// static
SurfaceBinding::PerConnectionState* SurfaceBinding::PerConnectionState::Get(
    shell::Connector* connector,
    mus::WindowTreeConnection* connection) {
  ConnectionToStateMap* window_map = window_states.Pointer()->Get();
  if (!window_map) {
    window_map = new ConnectionToStateMap;
    window_states.Pointer()->Set(window_map);
  }
  if (!(*window_map)[connection]) {
    (*window_map)[connection] = new PerConnectionState(connector, connection);
    (*window_map)[connection]->Init();
  }
  return (*window_map)[connection];
}

std::unique_ptr<cc::OutputSurface>
SurfaceBinding::PerConnectionState::CreateOutputSurface(
    mus::Window* window,
    mus::mojom::SurfaceType surface_type) {
  if (gpu_.encountered_error())
    return nullptr;
  // TODO(sky): figure out lifetime here. Do I need to worry about the return
  // value outliving this?
  mus::mojom::CommandBufferPtr cb;
  gpu_->CreateOffscreenGLES2Context(GetProxy(&cb));

  scoped_refptr<cc::ContextProvider> context_provider(
      new mus::ContextProvider(cb.PassInterface().PassHandle()));
  return base::WrapUnique(new mus::OutputSurface(
      context_provider, window->RequestSurface(surface_type)));
}

SurfaceBinding::PerConnectionState::PerConnectionState(
    shell::Connector* connector,
    mus::WindowTreeConnection* connection)
    : connector_(connector), connection_(connection) {}

SurfaceBinding::PerConnectionState::~PerConnectionState() {
  ConnectionToStateMap* window_map = window_states.Pointer()->Get();
  DCHECK(window_map);
  DCHECK_EQ(this, (*window_map)[connection_]);
  window_map->erase(connection_);
  if (window_map->empty()) {
    delete window_map;
    window_states.Pointer()->Set(nullptr);
  }
}

void SurfaceBinding::PerConnectionState::Init() {
  connector_->ConnectToInterface("mojo:mus", &gpu_);

  // TODO(sad): If connection is lost (e.g. if gpu crashes), then the
  // connections need to be restored. https://crbug.com/613366
  // TODO(rockot|yzshen): It is necessary to install a connection-error handler,
  // even if the handler doesn't actually do anything. https://crbug.com/613371
  gpu_.set_connection_error_handler([]{});
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(shell::Connector* connector,
                               mus::Window* window,
                               mus::mojom::SurfaceType surface_type)
    : window_(window),
      surface_type_(surface_type),
      state_(PerConnectionState::Get(connector, window->connection())) {}

SurfaceBinding::~SurfaceBinding() {}

std::unique_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_->CreateOutputSurface(window_, surface_type_);
}

}  // namespace views
