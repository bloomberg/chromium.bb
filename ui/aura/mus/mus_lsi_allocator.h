// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_
#define UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
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

enum class MusLsiAllocatorType {
  // The allocator was created by a window that has an embedding in it. This
  // is the embedder side, *not* the embedding side.
  kEmbed,

  // A local window that has a FrameSinkId associated with it.
  kLocal,

  // Used for top-level windows.
  kTopLevel,
};

class ParentAllocator;
class TopLevelAllocator;

// MusLsiAllocator is used by WindowPortMus to handle management of
// LocalSurfaceIdAllocation, and associated data.
class MusLsiAllocator {
 public:
  virtual ~MusLsiAllocator() {}

  static std::unique_ptr<MusLsiAllocator> CreateAllocator(
      MusLsiAllocatorType type,
      WindowPortMus* window,
      WindowTreeClient* window_tree_client);

  MusLsiAllocatorType type() const { return type_; }

  virtual ParentAllocator* AsParentAllocator();
  virtual TopLevelAllocator* AsTopLevelAllocator();

  virtual void AllocateLocalSurfaceId() = 0;
  virtual viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) = 0;
  virtual void InvalidateLocalSurfaceId() = 0;
  virtual void OnDeviceScaleFactorChanged() = 0;
  virtual void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                                 bool from_server) = 0;
  virtual const viz::LocalSurfaceIdAllocation&
  GetLocalSurfaceIdAllocation() = 0;
  virtual void OnFrameSinkIdChanged() = 0;

 protected:
  explicit MusLsiAllocator(MusLsiAllocatorType type) : type_(type) {}

 private:
  const MusLsiAllocatorType type_;
};

// ParentAllocator is used for kEmbed and kLocal types of allocators. It uses
// a ParentLocalSurfaceIdAllocator to generate a LocalSurfaceIdAllocation.
// Additionally ParenAllocator may creates a ClientSurfaceEmbedder| to handle
// associating the FrameSinkId with Viz.
class ParentAllocator : public MusLsiAllocator {
 public:
  ParentAllocator(MusLsiAllocatorType type,
                  WindowPortMus* window,
                  WindowTreeClient* window_tree_client);
  ~ParentAllocator() override;

  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const viz::LocalSurfaceIdAllocation&
          embedded_client_local_surface_id_allocation);

  // MusLsiAllocator:
  ParentAllocator* AsParentAllocator() override;
  void AllocateLocalSurfaceId() override;
  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceClosure allocation_task) override;
  void InvalidateLocalSurfaceId() override;
  void OnDeviceScaleFactorChanged() override;
  void OnDidChangeBounds(const gfx::Size& size_in_pixels,
                         bool from_server) override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation() override;
  void OnFrameSinkIdChanged() override;

 private:
  friend class WindowPortMusTestHelper;

  Window* GetWindow();

  void Update(bool in_bounds_change);

  WindowPortMus* window_;
  WindowTreeClient* window_tree_client_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  std::unique_ptr<ClientSurfaceEmbedder> client_surface_embedder_;

  // Last size (in pixels) that a LocalSurfaceId was generated for.
  gfx::Size last_surface_size_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(ParentAllocator);
};

// TopLevelAllocator is used for TOP_LEVEL windows.
class TopLevelAllocator : public MusLsiAllocator,
                          public ui::CompositorObserver {
 public:
  explicit TopLevelAllocator(WindowPortMus* window,
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
  void OnFrameSinkIdChanged() override;

 private:
  Window* GetWindow();
  void NotifyServerOfLocalSurfaceId();

  // ui::CompositorObserver:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;
  void DidGenerateLocalSurfaceIdAllocation(
      ui::Compositor* compositor,
      const viz::LocalSurfaceIdAllocation& allocation) override;

  friend class WindowPortMusTestHelper;

  WindowPortMus* window_;
  WindowTreeClient* window_tree_client_;
  // This is null if the compositor is deleted before this.
  ui::Compositor* compositor_;
  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelAllocator);
};

}  // namespace aura

#endif  // UI_AURA_MUS_MUS_LSI_ALLOCATOR_H_
