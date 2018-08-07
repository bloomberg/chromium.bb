// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_CLIENT_H_
#define UI_AURA_MUS_WINDOW_TREE_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/screen_provider_observer.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/mus/capture_synchronizer_delegate.h"
#include "ui/aura/mus/drag_drop_controller_host.h"
#include "ui/aura/mus/focus_synchronizer_delegate.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/base/ui_base_types.h"

namespace base {
class Thread;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ContextFactory;
class Gpu;
struct PropertyData;
}

namespace aura {
class CaptureSynchronizer;
class DragDropControllerMus;
class EmbedRoot;
class EmbedRootDelegate;
class FocusSynchronizer;
class InFlightBoundsChange;
class InFlightChange;
class InFlightFocusChange;
class InFlightPropertyChange;
class InFlightVisibleChange;
class MusContextFactory;
class WindowMus;
class WindowPortMus;
class WindowTreeClientDelegate;
class WindowTreeClientPrivate;
class WindowTreeClientObserver;
class WindowTreeClientTestObserver;
class WindowTreeHostMus;

using EventResultCallback = base::OnceCallback<void(ui::mojom::EventResult)>;

// Used to enable Aura to act as the client-library for the Window Service.
//
// WindowTreeClient is created in a handful of distinct ways. See the various
// Create functions for details.
//
// Generally when the delegate gets one of OnEmbedRootDestroyed() or
// OnLostConnection() it should delete the WindowTreeClient.
//
// When WindowTreeClient is deleted all windows are deleted (and observers
// notified).
class AURA_EXPORT WindowTreeClient
    : public ui::mojom::WindowTreeClient,
      public ui::mojom::ScreenProviderObserver,
      public CaptureSynchronizerDelegate,
      public FocusSynchronizerDelegate,
      public DragDropControllerHost,
      public WindowTreeHostMusDelegate,
      public client::TransientWindowClientObserver {
 public:
  // Creates a WindowTreeClient for use in embedding.
  static std::unique_ptr<WindowTreeClient> CreateForEmbedding(
      service_manager::Connector* connector,
      WindowTreeClientDelegate* delegate,
      ui::mojom::WindowTreeClientRequest request,
      bool create_discardable_memory = true);

  // Creates a WindowTreeClient useful for creating top-level windows.
  static std::unique_ptr<WindowTreeClient> CreateForWindowTreeFactory(
      service_manager::Connector* connector,
      WindowTreeClientDelegate* delegate,
      bool create_discardable_memory = true,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr);

  // Creates a WindowTreeClient such that the Window Service creates a single
  // WindowTreeHost. This is useful for testing and examples.
  static std::unique_ptr<WindowTreeClient> CreateForWindowTreeHostFactory(
      service_manager::Connector* connector,
      WindowTreeClientDelegate* delegate,
      bool create_discardable_memory = true);

  ~WindowTreeClient() override;

  service_manager::Connector* connector() { return connector_; }
  CaptureSynchronizer* capture_synchronizer() {
    return capture_synchronizer_.get();
  }
  FocusSynchronizer* focus_synchronizer() { return focus_synchronizer_.get(); }

  bool connected() const { return tree_ != nullptr; }

  // Blocks until the initial screen configuration is received.
  bool WaitForDisplays();

  void SetCanFocus(Window* window, bool can_focus);
  void SetCanAcceptDrops(WindowMus* window, bool can_accept_drops);
  void SetEventTargetingPolicy(WindowMus* window,
                               ui::mojom::EventTargetingPolicy policy);
  void SetCursor(WindowMus* window,
                 const ui::CursorData& old_cursor,
                 const ui::CursorData& new_cursor);
  void SetWindowTextInputState(WindowMus* window,
                               ui::mojom::TextInputStatePtr state);
  void SetImeVisibility(WindowMus* window,
                        bool visible,
                        ui::mojom::TextInputStatePtr state);
  void SetHitTestMask(WindowMus* window, const base::Optional<gfx::Rect>& rect);

  // Embeds a new client in |window|. |flags| is a bitmask of the values defined
  // by kEmbedFlag*; 0 gives default behavior. |callback| is called to indicate
  // whether the embedding succeeded or failed and may be called immediately if
  // the embedding is known to fail.
  void Embed(Window* window,
             ui::mojom::WindowTreeClientPtr client,
             uint32_t flags,
             ui::mojom::WindowTree::EmbedCallback callback);
  void EmbedUsingToken(Window* window,
                       const base::UnguessableToken& token,
                       uint32_t flags,
                       ui::mojom::WindowTree::EmbedCallback callback);

  // Schedules an embed of a client. See
  // mojom::WindowTreeClient::ScheduleEmbed() for details.
  void ScheduleEmbed(
      ui::mojom::WindowTreeClientPtr client,
      base::OnceCallback<void(const base::UnguessableToken&)> callback);

  // Creates a new EmbedRoot. See EmbedRoot for details.
  std::unique_ptr<EmbedRoot> CreateEmbedRoot(EmbedRootDelegate* delegate);

  void AttachCompositorFrameSink(
      ui::Id window_id,
      viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
      viz::mojom::CompositorFrameSinkClientPtr client);

  bool IsRoot(WindowMus* window) const { return roots_.count(window) > 0; }

  // Returns the root of this connection.
  std::set<Window*> GetRoots();

  // Returns true if the specified window was created by this client.
  bool WasCreatedByThisClient(const WindowMus* window) const;

  // Returns the current location of the mouse on screen. Note: this method may
  // race the asynchronous initialization; but in that case we return (0, 0).
  gfx::Point GetCursorScreenPoint();

  // See description in window_tree.mojom. When an existing pointer watcher is
  // updated or cleared then any future events from the server for that watcher
  // will be ignored.
  void StartPointerWatcher(bool want_moves);
  void StopPointerWatcher();

  void AddObserver(WindowTreeClientObserver* observer);
  void RemoveObserver(WindowTreeClientObserver* observer);

  void AddTestObserver(WindowTreeClientTestObserver* observer);
  void RemoveTestObserver(WindowTreeClientTestObserver* observer);

 private:
  friend class EmbedRoot;
  friend class InFlightBoundsChange;
  friend class InFlightFocusChange;
  friend class InFlightPropertyChange;
  friend class InFlightTransformChange;
  friend class InFlightVisibleChange;
  friend class WindowPortMus;
  friend class WindowTreeClientPrivate;

  enum class Origin {
    CLIENT,
    SERVER,
  };

  using IdToWindowMap = std::map<ui::Id, WindowMus*>;

  // TODO(sky): this assumes change_ids never wrap, which is a bad assumption.
  using InFlightMap = std::map<uint32_t, std::unique_ptr<InFlightChange>>;

  // |create_discardable_memory| specifies whether WindowTreeClient will setup
  // the dicardable shared memory manager for this process. In some tests, more
  // than one WindowTreeClient will be created, so we need to pass false to
  // avoid setting up the discardable shared memory manager more than once.
  WindowTreeClient(
      service_manager::Connector* connector,
      WindowTreeClientDelegate* delegate,
      ui::mojom::WindowTreeClientRequest request = nullptr,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr,
      bool create_discardable_memory = true);

  void RegisterWindowMus(WindowMus* window);

  WindowMus* GetWindowByServerId(ui::Id id);

  bool IsWindowKnown(aura::Window* window);

  // Returns the oldest InFlightChange that matches |change|.
  InFlightChange* GetOldestInFlightChangeMatching(const InFlightChange& change);

  // See InFlightChange for details on how InFlightChanges are used.
  uint32_t ScheduleInFlightChange(std::unique_ptr<InFlightChange> change);

  // Returns true if there is an InFlightChange that matches |change|. If there
  // is an existing change SetRevertValueFrom() is invoked on it. Returns false
  // if there is no InFlightChange matching |change|.
  // See InFlightChange for details on how InFlightChanges are used.
  bool ApplyServerChangeToExistingInFlightChange(const InFlightChange& change);

  void BuildWindowTree(const std::vector<ui::mojom::WindowDataPtr>& windows);

  // If the window identified by |window_data| doesn't exist a new window is
  // created, otherwise the existing window is updated based on |window_data|.
  void CreateOrUpdateWindowFromWindowData(
      const ui::mojom::WindowData& window_data);

  // Creates a WindowPortMus from the server side data.
  std::unique_ptr<WindowPortMus> CreateWindowPortMus(
      const ui::mojom::WindowData& window_data,
      WindowMusType window_mus_type);

  // Sets local properties on the associated Window from the server properties.
  void SetLocalPropertiesFromServerProperties(
      WindowMus* window,
      const ui::mojom::WindowData& window_data);

  // Creates a new WindowTreeHostMus.
  std::unique_ptr<WindowTreeHostMus> CreateWindowTreeHost(
      WindowMusType window_mus_type,
      const ui::mojom::WindowData& window_data,
      int64_t display_id,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id =
          base::nullopt);

  WindowMus* NewWindowFromWindowData(WindowMus* parent,
                                     const ui::mojom::WindowData& window_data);

  // Sets the ui::mojom::WindowTree implementation.
  void SetWindowTree(ui::mojom::WindowTreePtr window_tree_ptr);

  // Called when the connection to the server is established.
  void WindowTreeConnectionEstablished(ui::mojom::WindowTree* window_tree);

  // Called when the ui::mojom::WindowTree connection is lost, deletes this.
  void OnConnectionLost();

  // Called when a Window property changes. If |key| is handled internally
  // (maps to a function on WindowTree) returns true.
  bool HandleInternalPropertyChanged(WindowMus* window,
                                     const void* key,
                                     int64_t old_value);

  // OnEmbed() calls into this. Exposed as a separate function for testing.
  void OnEmbedImpl(ui::mojom::WindowTree* window_tree,
                   ui::mojom::WindowDataPtr root_data,
                   int64_t display_id,
                   ui::Id focused_window_id,
                   bool drawn,
                   const base::Optional<viz::LocalSurfaceId>& local_surface_id);

  // Returns the EmbedRoot whose root is |window|, or null if there isn't one.
  EmbedRoot* GetEmbedRootWithRootWindow(aura::Window* window);

  // Called from EmbedRoot's destructor.
  void OnEmbedRootDestroyed(EmbedRoot* embed_root);

  EventResultCallback CreateEventResultCallback(int32_t event_id);

  void OnReceivedCursorLocationMemory(mojo::ScopedSharedBufferHandle handle);

  // Called when a property needs to change as the result of a change in the
  // server, or the server failing to accept a change.
  void SetWindowBoundsFromServer(
      WindowMus* window,
      const gfx::Rect& revert_bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id);
  void SetWindowTransformFromServer(WindowMus* window,
                                    const gfx::Transform& transform);
  void SetWindowVisibleFromServer(WindowMus* window, bool visible);

  // Called from OnWindowMusBoundsChanged() and SetRootWindowBounds().
  void ScheduleInFlightBoundsChange(WindowMus* window,
                                    const gfx::Rect& old_bounds,
                                    const gfx::Rect& new_bounds);

  // Following are called from WindowMus.
  void OnWindowMusCreated(WindowMus* window);
  void OnWindowMusDestroyed(WindowMus* window, Origin origin);
  void OnWindowMusBoundsChanged(WindowMus* window,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds);
  void OnWindowMusTransformChanged(WindowMus* window,
                                   const gfx::Transform& old_transform,
                                   const gfx::Transform& new_transform);
  void OnWindowMusAddChild(WindowMus* parent, WindowMus* child);
  void OnWindowMusRemoveChild(WindowMus* parent, WindowMus* child);
  void OnWindowMusMoveChild(WindowMus* parent,
                            size_t current_index,
                            size_t dest_index);
  void OnWindowMusSetVisible(WindowMus* window, bool visible);
  std::unique_ptr<ui::PropertyData> OnWindowMusWillChangeProperty(
      WindowMus* window,
      const void* key);
  void OnWindowMusPropertyChanged(WindowMus* window,
                                  const void* key,
                                  int64_t old_value,
                                  std::unique_ptr<ui::PropertyData> data);

  // Overridden from WindowTreeClient:
  void OnEmbed(
      ui::mojom::WindowDataPtr root,
      ui::mojom::WindowTreePtr tree,
      int64_t display_id,
      ui::Id focused_window_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbedFromToken(
      const base::UnguessableToken& token,
      ui::mojom::WindowDataPtr root,
      int64_t display_id,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbeddedAppDisconnected(ui::Id window_id) override;
  void OnUnembed(ui::Id window_id) override;
  void OnCaptureChanged(ui::Id new_capture_window_id,
                        ui::Id old_capture_window_id) override;
  void OnFrameSinkIdAllocated(ui::Id window_id,
                              const viz::FrameSinkId& frame_sink_id) override;
  void OnTopLevelCreated(
      uint32_t change_id,
      ui::mojom::WindowDataPtr data,
      int64_t display_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnWindowBoundsChanged(
      ui::Id window_id,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnWindowTransformChanged(ui::Id window_id,
                                const gfx::Transform& old_transform,
                                const gfx::Transform& new_transform) override;
  void OnTransientWindowAdded(ui::Id window_id,
                              ui::Id transient_window_id) override;
  void OnTransientWindowRemoved(ui::Id window_id,
                                ui::Id transient_window_id) override;
  void OnWindowHierarchyChanged(
      ui::Id window_id,
      ui::Id old_parent_id,
      ui::Id new_parent_id,
      std::vector<ui::mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(ui::Id window_id,
                         ui::Id relative_window_id,
                         ui::mojom::OrderDirection direction) override;
  void OnWindowDeleted(ui::Id window_id) override;
  void OnWindowVisibilityChanged(ui::Id window_id, bool visible) override;
  void OnWindowOpacityChanged(ui::Id window_id,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowParentDrawnStateChanged(ui::Id window_id, bool drawn) override;
  void OnWindowSharedPropertyChanged(
      ui::Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& transport_data) override;
  void OnWindowInputEvent(
      uint32_t event_id,
      ui::Id window_id,
      int64_t display_id,
      std::unique_ptr<ui::Event> event,
      bool matches_pointer_watcher) override;
  void OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                              ui::Id window_id,
                              int64_t display_id) override;
  void OnWindowFocused(ui::Id focused_window_id) override;
  void OnWindowCursorChanged(ui::Id window_id, ui::CursorData cursor) override;
  void OnWindowSurfaceChanged(ui::Id window_id,
                              const viz::SurfaceInfo& surface_info) override;
  void OnDragDropStart(const base::flat_map<std::string, std::vector<uint8_t>>&
                           mime_data) override;
  void OnDragEnter(ui::Id window_id,
                   uint32_t event_flags,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   OnDragEnterCallback callback) override;
  void OnDragOver(ui::Id window_id,
                  uint32_t event_flags,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  OnDragOverCallback callback) override;
  void OnDragLeave(ui::Id window_id) override;
  void OnCompleteDrop(ui::Id window_id,
                      uint32_t event_flags,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      OnCompleteDropCallback callback) override;
  void OnPerformDragDropCompleted(uint32_t change_id,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(ui::Id window_id) override;
  void GetScreenProviderObserver(
      ui::mojom::ScreenProviderObserverAssociatedRequest observer) override;

  // ui::mojom::ScreenProviderObserver:
  void OnDisplaysChanged(std::vector<ui::mojom::WsDisplayPtr> ws_displays,
                         int64_t primary_display_id,
                         int64_t internal_display_id,
                         int64_t display_id_for_new_windows) override;

  // Overriden from WindowTreeHostMusDelegate:
  void OnWindowTreeHostBoundsWillChange(
      WindowTreeHostMus* window_tree_host,
      const gfx::Rect& bounds_in_pixels) override;
  void OnWindowTreeHostClientAreaWillChange(
      WindowTreeHostMus* window_tree_host,
      const gfx::Insets& client_area,
      const std::vector<gfx::Rect>& additional_client_areas) override;
  void OnWindowTreeHostSetOpacity(WindowTreeHostMus* window_tree_host,
                                  float opacity) override;
  void OnWindowTreeHostDeactivateWindow(
      WindowTreeHostMus* window_tree_host) override;
  void OnWindowTreeHostStackAbove(WindowTreeHostMus* window_tree_host,
                                  Window* window) override;
  void OnWindowTreeHostStackAtTop(WindowTreeHostMus* window_tree_host) override;
  void OnWindowTreeHostPerformWmAction(WindowTreeHostMus* window_tree_host,
                                       const std::string& action) override;
  void OnWindowTreeHostPerformWindowMove(
      WindowTreeHostMus* window_tree_host,
      ui::mojom::MoveLoopSource mus_source,
      const gfx::Point& cursor_location,
      const base::Callback<void(bool)>& callback) override;
  void OnWindowTreeHostCancelWindowMove(
      WindowTreeHostMus* window_tree_host) override;
  std::unique_ptr<WindowPortMus> CreateWindowPortForTopLevel(
      const std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWindowTreeHostCreated(WindowTreeHostMus* window_tree_host) override;

  // Override from client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(Window* parent,
                                   Window* transient_child) override;
  void OnTransientChildWindowRemoved(Window* parent,
                                     Window* transient_child) override;

  // Overriden from DragDropControllerHost:
  uint32_t CreateChangeIdForDrag(WindowMus* window) override;

  // Overrided from CaptureSynchronizerDelegate:
  uint32_t CreateChangeIdForCapture(WindowMus* window) override;

  // Overrided from FocusSynchronizerDelegate:
  uint32_t CreateChangeIdForFocus(WindowMus* window) override;

  // The one int in |cursor_location_mapping_|. When we read from this
  // location, we must always read from it atomically.
  base::subtle::Atomic32* cursor_location_memory() {
    return reinterpret_cast<base::subtle::Atomic32*>(
        cursor_location_mapping_.get());
  }

  // This may be null in tests.
  service_manager::Connector* connector_;

  // Id assigned to the next window created.
  ui::ClientSpecificId next_window_id_;

  // Id used for the next change id supplied to the server.
  uint32_t next_change_id_;
  InFlightMap in_flight_map_;

  WindowTreeClientDelegate* delegate_;

  std::set<WindowMus*> roots_;

  base::flat_set<EmbedRoot*> embed_roots_;

  IdToWindowMap windows_;

  std::unique_ptr<CaptureSynchronizer> capture_synchronizer_;

  std::unique_ptr<FocusSynchronizer> focus_synchronizer_;

  mojo::Binding<ui::mojom::WindowTreeClient> binding_;
  ui::mojom::WindowTreePtr tree_ptr_;
  // Typically this is the value contained in |tree_ptr_|, but tests may
  // directly set this.
  ui::mojom::WindowTree* tree_;

  // Set to true if OnEmbed() was received.
  bool is_from_embed_ = false;

  bool in_destructor_;

  // A mapping to shared memory that is one 32 bit integer long. The window
  // server uses this to let us synchronously read the cursor location.
  mojo::ScopedSharedBufferMapping cursor_location_mapping_;

  base::ObserverList<WindowTreeClientObserver> observers_;

  bool has_pointer_watcher_ = false;

  // The current change id for the client.
  uint32_t current_move_loop_change_ = 0u;

  // Callback executed when a move loop initiated by PerformWindowMove() is
  // completed.
  base::Callback<void(bool)> on_current_move_finished_;

  std::unique_ptr<DragDropControllerMus> drag_drop_controller_;

  base::ObserverList<WindowTreeClientTestObserver> test_observers_;

  // IO thread for GPU and discardable shared memory IPC.
  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<ui::Gpu> gpu_;
  std::unique_ptr<MusContextFactory> compositor_context_factory_;

  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  // If |compositor_context_factory_| is installed on Env, then this is the
  // ContextFactory that was set on Env originally.
  ui::ContextFactory* initial_context_factory_ = nullptr;

  // Set to true once OnWmDisplayAdded() is called.
  bool got_initial_displays_ = false;

  bool in_shutdown_ = false;

  mojo::AssociatedBinding<ui::mojom::ScreenProviderObserver>
      screen_provider_observer_binding_{this};

  base::WeakPtrFactory<WindowTreeClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClient);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_CLIENT_H_
