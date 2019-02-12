// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_
#define UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/aura/aura_export.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}

namespace viz {
class LocalSurfaceIdAllocation;
class ScopedSurfaceIdAllocator;
}  // namespace viz

namespace aura {

class ClientSurfaceEmbedder;
class Window;
class WindowPortMus;
class WindowTreeClient;

class ParentAllocator;
class TopLevelAllocator;

// MusLsiAllocator is used by WindowPortMus to handle management of
// LocalSurfaceIdAllocation, and associated data.
class AURA_EXPORT MusLsiAllocator {
 public:
  virtual ~MusLsiAllocator() {}

  virtual ParentAllocator* AsParentAllocator();
  virtual TopLevelAllocator* AsTopLevelAllocator();

  // This is called when the allocator is set on the WindowPortMus and allows
  // for processing that has to happen when the allocator is set on the
  // WindowPortMus.
  virtual void OnInstalled() {}

  virtual void AllocateLocalSurfaceId() = 0;
  virtual viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) = 0;
  virtual void InvalidateLocalSurfaceId() = 0;
  virtual void OnDeviceScaleFactorChanged() = 0;
  virtual void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                                 bool from_server) = 0;
  virtual const viz::LocalSurfaceIdAllocation&
  GetLocalSurfaceIdAllocation() = 0;

 protected:
  MusLsiAllocator(WindowPortMus* window_port_mus,
                  WindowTreeClient* window_tree_client);

  Window* GetWindow();
  WindowPortMus* window_port_mus() { return window_port_mus_; }
  WindowTreeClient* window_tree_client() { return window_tree_client_; }

 private:
  WindowPortMus* window_port_mus_;
  WindowTreeClient* window_tree_client_;
};

// ParentAllocator is used for kEmbed and kLocal types of allocators. It uses
// a ParentLocalSurfaceIdAllocator to generate a LocalSurfaceIdAllocation.
// Additionally ParenAllocator may creates a ClientSurfaceEmbedder| to handle
// associating the FrameSinkId with Viz.
class AURA_EXPORT ParentAllocator : public MusLsiAllocator {
 public:
  // |is_embedder| is true if the window is the embedder side of an embedding.
  // A value of false for |is_embedder| indicates the window is not embedding
  // another window.
  ParentAllocator(WindowPortMus* window,
                  WindowTreeClient* window_tree_client,
                  bool is_embedder);
  ~ParentAllocator() override;

  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const viz::LocalSurfaceIdAllocation&
          embedded_client_local_surface_id_allocation);
  void OnFrameSinkIdChanged();

  // MusLsiAllocator:
  void OnInstalled() override;
  ParentAllocator* AsParentAllocator() override;
  void AllocateLocalSurfaceId() override;
  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) override;
  void InvalidateLocalSurfaceId() override;
  void OnDeviceScaleFactorChanged() override;
  void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                         bool from_server) override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation() override;

 private:
  friend class WindowPortMusTestHelper;

  void Update(bool in_bounds_change);

  WindowPortMus* window_port_mus_;
  WindowTreeClient* window_tree_client_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  std::unique_ptr<ClientSurfaceEmbedder> client_surface_embedder_;

  // Last size (in pixels) that a LocalSurfaceId was generated for.
  gfx::Size last_surface_size_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(ParentAllocator);
};

// TopLevelAllocator is used for TOP_LEVEL windows.
class AURA_EXPORT TopLevelAllocator : public MusLsiAllocator,
                                      public ui::CompositorObserver {
 public:
  TopLevelAllocator(WindowPortMus* window_port_mus,
                    WindowTreeClient* window_tree_client);
  ~TopLevelAllocator() override;

  void UpdateLocalSurfaceIdFromParent(
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation);

  // MusLsiAllocator:
  TopLevelAllocator* AsTopLevelAllocator() override;
  void AllocateLocalSurfaceId() override;
  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) override;
  void InvalidateLocalSurfaceId() override;
  void OnDeviceScaleFactorChanged() override;
  void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                         bool from_server) override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation() override;

 private:
  void NotifyServerOfLocalSurfaceId();

  // ui::CompositorObserver:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;
  void DidGenerateLocalSurfaceIdAllocation(
      ui::Compositor* compositor,
      const viz::LocalSurfaceIdAllocation& allocation) override;

  friend class WindowPortMusTestHelper;

  // This is null if the compositor is deleted before this.
  ui::Compositor* compositor_;
  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelAllocator);
};

// EmbeddedAllocator is used for the embedded side of an embedding. The
// expectation is the embedded side never changes the bounds, and only allocates
// ids in rare circumstances (when LayerTreeHostImpl decides to allocate an id,
// such as when a gpu crash happens).
class AURA_EXPORT EmbeddedAllocator : public MusLsiAllocator,
                                      public ui::CompositorObserver {
 public:
  EmbeddedAllocator(WindowPortMus* window_port_mus,
                    WindowTreeClient* window_tree_client);
  ~EmbeddedAllocator() override;

  // MusLsiAllocator:
  void AllocateLocalSurfaceId() override;
  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) override;
  void InvalidateLocalSurfaceId() override;
  void OnDeviceScaleFactorChanged() override;
  void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                         bool from_server) override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation() override;

  // ui::CompositorObserver:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;
  void DidGenerateLocalSurfaceIdAllocation(
      ui::Compositor* compositor,
      const viz::LocalSurfaceIdAllocation& allocation) override;

 private:
  // Notifies the server of a new LocalSurfaceIdAllocation.
  void NotifyServerOfLocalSurfaceId();

  // This is null if the compositor is deleted before this.
  ui::Compositor* compositor_;
  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedAllocator);
};

}  // namespace aura

#endif  // UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_
