// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_TREE_H_
#define SERVICES_WS_WINDOW_TREE_H_

#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "services/ws/focus_handler.h"
#include "services/ws/ids.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
}

namespace ws {

class ClientChangeTracker;
class ClientRoot;
class Embedding;
class FocusHandler;
class PointerWatcher;
class ServerWindow;
class TopmostWindowObserver;
class WindowService;

// WindowTree manages a client connected to the Window Service. WindowTree
// provides the implementation of mojom::WindowTree that the client talks
// to. WindowTree implements the mojom::WindowTree interface on top of aura.
// That is, changes to the aura Window hierarchy are mirrored to the
// mojom::WindowTreeClient interface. Similarly, changes from the client by
// way of the mojom::WindowTree interface modify the underlying aura
// Windows.
//
// WindowTree is created in two distinct ways:
// . when the client is embedded in a specific aura::Window, by way of
//   WindowTree::Embed(). Use InitForEmbed(). This configuration has only a
//   single ClientRoot.
// . by way of a WindowTreeFactory. In this mode the client has no initial
//   roots. To create a root the client requests a top-level window. Use
//   InitFromFactory() for this. In this configuration there is a ClientRoot
//   per top-level.
//
// Typically instances of WindowTree are owned by a WindowTreeBinding that is
// created via WindowTreeFactory, but that is not necessary (in particular tests
// often do not use WindowTreeBinding).
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowTree
    : public mojom::WindowTree,
      public aura::WindowObserver,
      public aura::client::CaptureClientObserver {
 public:
  WindowTree(WindowService* window_service,
             ClientSpecificId client_id,
             mojom::WindowTreeClient* client,
             const std::string& client_name);
  ~WindowTree() override;

  // See class description for details on Init variants.
  void InitForEmbed(aura::Window* root, mojom::WindowTreePtr window_tree_ptr);
  void InitFromFactory();

  ClientSpecificId client_id() const { return client_id_; }

  // Notifies the client than an event has been received.
  void SendEventToClient(aura::Window* window, const ui::Event& event);

  // Notifies the client that an event matching a pointer watcher has been
  // received.
  void SendPointerWatcherEventToClient(int64_t display_id,
                                       std::unique_ptr<ui::Event> event);

  // Returns true if |window| was created by the client calling
  // NewTopLevelWindow().
  bool IsTopLevel(aura::Window* window);

  // Asks the client to close |window|. |window| must be a top-level window.
  void RequestClose(ServerWindow* window);

  // Called when an Embedding is destroyed. This is only called for Embeddings
  // that do not own the WindowTree (see Embedding for more details on when this
  // happens).
  void OnEmbeddingDestroyed(Embedding* embedding);

  // Sends the topmost window information to the client. There can be multiple
  // windows, as described for OnTopmostWindowChanged() in window_tree.mojom.
  void SendTopmostWindows(const std::vector<aura::Window*>& topmosts);

  WindowService* window_service() { return window_service_; }

  // Returns the ClientWindowId for the window the client previously supplied
  // to ScheduleEmbedForExistingClient(). If the client did not call
  // ScheduleEmbedForExistingClient() with |embed_token|, an invalid
  // ClientWindowId is returned.
  ClientWindowId RemoveScheduledEmbedUsingExistingClient(
      const base::UnguessableToken& embed_token);

  // Completes a previous call to ScheduleEmbedForExistingClient().
  void CompleteScheduleEmbedForExistingClient(
      aura::Window* window,
      const ClientWindowId& id,
      const base::UnguessableToken& token);

  const std::string& client_name() const { return client_name_; }

  // Returns true if at a compositor frame sink has been created for at least
  // one of the roots.
  bool HasAtLeastOneRootWithCompositorFrameSink();

  ClientWindowId ClientWindowIdForWindow(aura::Window* window);

 private:
  friend class ClientRoot;
  // TODO(sky): WindowTree should be refactored such that it is not
  // necessary to friend this.
  friend class FocusHandler;
  friend class WindowTreeTestHelper;

  struct InFlightEvent;

  using ClientRoots = std::vector<std::unique_ptr<ClientRoot>>;

  enum class ConnectionType {
    // This client is the result of an embedding, InitForEmbed() was called.
    kEmbedding,

    // This client is not the result of an embedding. More specifically
    // InitFromFactory() was called. Generally this means the client first
    // connected to mojom::WindowTreeFactory and then called
    // mojom::WindowTreeFactory::CreateWindowTree().
    kOther,
  };

  enum class DeleteClientRootReason {
    // The window is being destroyed.
    kDeleted,

    // Another client is being embedded in the window.
    kEmbed,

    // The embedded client explicitly asked to be unembedded.
    kUnembed,

    // Called when the ClientRoot is deleted from the WindowTree destructor.
    kDestructor,
  };

  // Creates a new ClientRoot. The returned ClientRoot is owned by this.
  // |is_top_level| is true if this is called from
  // WindowTree::NewTopLevelWindow().
  ClientRoot* CreateClientRoot(aura::Window* window, bool is_top_level);
  void DeleteClientRoot(ClientRoot* client_root, DeleteClientRootReason reason);
  void DeleteClientRootWithRoot(aura::Window* window);

  aura::Window* GetWindowByClientId(const ClientWindowId& id);
  aura::Window* GetWindowByTransportId(Id transport_window_id);

  // Returns true if |this| created |window|.
  bool IsClientCreatedWindow(aura::Window* window);
  bool IsClientRootWindow(aura::Window* window);

  // Returns the window which is corresponded with the root window for the
  // specified |window| in the client.
  aura::Window* GetClientRootWindowFor(aura::Window* window);

  ClientRoots::iterator FindClientRootWithRoot(aura::Window* window);

  // Returns true if |window| has been exposed to this client. A client
  // typically only sees a limited set of windows that may exist. The set of
  // windows exposed to the client are referred to as the known windows.
  bool IsWindowKnown(aura::Window* window) const;
  bool IsWindowRootOfAnotherClient(aura::Window* window) const;

  // Called when one of the windows known to the client loses capture.
  // |lost_capture| is the window that had capture.
  void OnCaptureLost(aura::Window* lost_capture);

  // Callback from WindowServiceDelegate::RunWindowMoveLoop(). |change_id| is
  // the id at the time the move-loop was initiated. |result| is the result of
  // the move loop (true for success).
  void OnPerformWindowMoveDone(uint32_t change_id, bool result);

  // Scheduled from PerformDragDrop to run drag loop with mojo stack unwinded.
  void DoPerformDragDrop(
      uint32_t change_id,
      Id source_window_id,
      const gfx::Point& screen_location,
      const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
      const gfx::ImageSkia& drag_image,
      const gfx::Vector2d& drag_image_offset,
      uint32_t drag_operation,
      ui::mojom::PointerKind source);

  // Callback from WindowServiceDelegate::RunDragLoop(). |change_id| is
  // the id at the time the drag loop was initiated. |result| is the result of
  // the drag (the final drag operation performed, or DRAG_NONE when the drag
  // fails or gets canceled).
  void OnPerformDragDropDone(uint32_t change_id, int drag_result);

  // Called for windows created by the client (including top-levels).
  aura::Window* AddClientCreatedWindow(
      const ClientWindowId& id,
      bool is_top_level,
      std::unique_ptr<aura::Window> window_ptr);

  // Adds/removes a Window from the set of windows known to the client. This
  // also adds or removes any observers that may need to be installed.
  void AddWindowToKnownWindows(aura::Window* window, const ClientWindowId& id);

  // |delete_if_owned| indicates if |window| should be deleted if this client
  // created it. |delete_if_owned| is false only if the window was externally
  // deleted.
  void RemoveWindowFromKnownWindows(aura::Window* window, bool delete_if_owned);

  // Unregisters |window| and all its descendants. This stops at windows created
  // by this client, adding to |created_windows|.
  void RemoveWindowFromKnownWindowsRecursive(
      aura::Window* window,
      std::vector<aura::Window*>* created_windows);

  // Returns true if |id| may be used for a new Window. Clients have certain
  // restrictions placed on what ids they may use (and that an id isn't
  // reused).
  bool IsValidIdForNewWindow(const ClientWindowId& id) const;

  // Returns the id to send to the client for a ClientWindowId. Windows that
  // were created by this client always have the high bits set to 0 (because
  // the client doesn't know its own id).
  Id ClientWindowIdToTransportId(const ClientWindowId& client_window_id) const;

  Id TransportIdForWindow(aura::Window* window) const;

  // Returns the ClientWindowId from a transport id. Uses |client_id_| as the
  // ClientWindowId::client_id part if invalid. This function does a straight
  // mapping, there may not be a window with the returned id.
  ClientWindowId MakeClientWindowId(Id transport_window_id) const;

  // Returns true if the local-surface id for |window| is assigned by this
  // client. A return value of false means the LocalSurfaceId is assigned by
  // either another client, or by the WindowService itself.
  bool IsLocalSurfaceIdAssignedByClient(aura::Window* window);

  std::vector<mojom::WindowDataPtr> WindowsToWindowDatas(
      const std::vector<aura::Window*>& windows);
  mojom::WindowDataPtr WindowToWindowData(aura::Window* window);

  // Returns the WindowTreeClient previously scheduled for an embed with the
  // given |token| from ScheduleEmbed(). If this client is the result of an
  // Embed() and ScheduleEmbed() was not called on this client, then this
  // recurses to WindowTrees that embedded this tree. |visited_trees| contains
  // the WindowTrees that have already been visited. Recursing enables an
  // ancestor to call ScheduleEmbed() and the ancestor to communicate the
  // token with the client.
  mojom::WindowTreeClientPtr GetAndRemoveScheduledEmbedWindowTreeClient(
      const base::UnguessableToken& token,
      std::set<WindowTree*>* visited_trees);

  // Methods with the name Impl() mirror those of mojom::WindowTree. The
  // return value indicates whether they succeeded or not. Generally failure
  // means the operation was not allowed.
  bool NewWindowImpl(
      const ClientWindowId& client_window_id,
      const std::map<std::string, std::vector<uint8_t>>& properties);
  bool DeleteWindowImpl(const ClientWindowId& window_id);
  bool SetCaptureImpl(const ClientWindowId& window_id);
  bool ReleaseCaptureImpl(const ClientWindowId& window_id);
  bool AddWindowImpl(const ClientWindowId& parent_id,
                     const ClientWindowId& child_id);
  bool RemoveWindowFromParentImpl(const ClientWindowId& client_window_id);
  bool AddTransientWindowImpl(const ClientWindowId& parent_window_id,
                              const ClientWindowId& transient_window_id);
  bool RemoveTransientWindowFromParentImpl(const ClientWindowId& transient_id);
  bool SetModalTypeImpl(const ClientWindowId& client_window_id,
                        ui::ModalType type);
  bool SetChildModalParentImpl(const ClientWindowId& child_id,
                               const ClientWindowId& parent_id);
  bool SetWindowVisibilityImpl(const ClientWindowId& window_id, bool visible);
  bool SetWindowPropertyImpl(const ClientWindowId& window_id,
                             const std::string& name,
                             const base::Optional<std::vector<uint8_t>>& value);
  bool EmbedImpl(const ClientWindowId& window_id,
                 mojom::WindowTreeClientPtr window_tree_client_ptr,
                 mojom::WindowTreeClient* window_tree_client,
                 uint32_t flags);
  bool SetWindowOpacityImpl(const ClientWindowId& window_id, float opacity);
  bool SetWindowBoundsImpl(
      const ClientWindowId& window_id,
      const gfx::Rect& bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id);
  bool ReorderWindowImpl(const ClientWindowId& window_id,
                         const ClientWindowId& relative_window_id,
                         mojom::OrderDirection direction);
  std::vector<aura::Window*> GetWindowTreeImpl(const ClientWindowId& window_id);
  bool SetFocusImpl(const ClientWindowId& window_id);
  bool SetCursorImpl(const ClientWindowId& window_id, ui::CursorData cursor);
  bool StackAboveImpl(const ClientWindowId& above_window_id,
                      const ClientWindowId& below_window_id);
  bool StackAtTopImpl(const ClientWindowId& window_id);

  void GetWindowTreeRecursive(aura::Window* window,
                              std::vector<aura::Window*>* windows);

  // Called when the client associated with an Embedding created by this object
  // disconnects. This deletes |embedding|, which deletes the embedded client.
  void OnEmbeddedClientConnectionLost(Embedding* embedding);

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // mojom::WindowTree:
  void NewWindow(
      uint32_t change_id,
      Id transport_window_id,
      const base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>&
          transport_properties) override;
  void NewTopLevelWindow(
      uint32_t change_id,
      Id transport_window_id,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties)
      override;
  void DeleteWindow(uint32_t change_id, Id transport_window_id) override;
  void SetCapture(uint32_t change_id, Id transport_window_id) override;
  void ReleaseCapture(uint32_t change_id, Id transport_window_id) override;
  void StartPointerWatcher(bool want_moves) override;
  void StopPointerWatcher() override;
  void SetWindowBounds(
      uint32_t change_id,
      Id window_id,
      const gfx::Rect& bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void SetWindowTransform(uint32_t change_id,
                          Id window_id,
                          const gfx::Transform& transform) override;
  void SetClientArea(Id transport_window_id,
                     const gfx::Insets& insets,
                     const base::Optional<std::vector<gfx::Rect>>&
                         additional_client_areas) override;
  void SetHitTestMask(Id transport_window_id,
                      const base::Optional<gfx::Rect>& mask) override;
  void SetCanAcceptDrops(Id window_id, bool accepts_drops) override;
  void SetWindowVisibility(uint32_t change_id,
                           Id transport_window_id,
                           bool visible) override;
  void SetWindowProperty(
      uint32_t change_id,
      Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& value) override;
  void SetWindowOpacity(uint32_t change_id,
                        Id transport_window_id,
                        float opacity) override;
  void AttachCompositorFrameSink(
      Id transport_window_id,
      viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
      viz::mojom::CompositorFrameSinkClientPtr client) override;
  void AddWindow(uint32_t change_id, Id parent_id, Id child_id) override;
  void RemoveWindowFromParent(uint32_t change_id, Id window_id) override;
  void AddTransientWindow(uint32_t change_id,
                          Id window_id,
                          Id transient_window_id) override;
  void RemoveTransientWindowFromParent(uint32_t change_id,
                                       Id transient_window_id) override;
  void SetModalType(uint32_t change_id,
                    Id window_id,
                    ui::ModalType type) override;
  void SetChildModalParent(uint32_t change_id,
                           Id window_id,
                           Id parent_window_id) override;
  void ReorderWindow(uint32_t change_id,
                     Id transport_window_id,
                     Id transport_relative_window_id,
                     mojom::OrderDirection direction) override;
  void GetWindowTree(Id window_id, GetWindowTreeCallback callback) override;
  void Embed(Id transport_window_id,
             mojom::WindowTreeClientPtr client_ptr,
             uint32_t embed_flags,
             EmbedCallback callback) override;
  void ScheduleEmbed(mojom::WindowTreeClientPtr client,
                     ScheduleEmbedCallback callback) override;
  void ScheduleEmbedForExistingClient(
      uint32_t window_id,
      ScheduleEmbedForExistingClientCallback callback) override;
  void EmbedUsingToken(Id transport_window_id,
                       const base::UnguessableToken& token,
                       uint32_t embed_flags,
                       EmbedUsingTokenCallback callback) override;
  void SetFocus(uint32_t change_id, Id transport_window_id) override;
  void SetCanFocus(Id transport_window_id, bool can_focus) override;
  void SetCursor(uint32_t change_id,
                 Id transport_window_id,
                 ui::CursorData cursor) override;
  void SetWindowTextInputState(Id window_id,
                               ui::mojom::TextInputStatePtr state) override;
  void SetImeVisibility(Id window_id,
                        bool visible,
                        ::ui::mojom::TextInputStatePtr state) override;
  void SetEventTargetingPolicy(Id transport_window_id,
                               mojom::EventTargetingPolicy policy) override;
  void OnWindowInputEventAck(uint32_t event_id,
                             mojom::EventResult result) override;
  void DeactivateWindow(Id transport_window_id) override;
  void StackAbove(uint32_t change_id, Id above_id, Id below_id) override;
  void StackAtTop(uint32_t change_id, Id window_id) override;
  void PerformWmAction(Id window_id, const std::string& action) override;
  void GetCursorLocationMemory(
      GetCursorLocationMemoryCallback callback) override;
  void PerformWindowMove(uint32_t change_id,
                         Id transport_window_id,
                         mojom::MoveLoopSource source,
                         const gfx::Point& cursor) override;
  void CancelWindowMove(Id transport_window_id) override;
  void PerformDragDrop(
      uint32_t change_id,
      Id source_window_id,
      const gfx::Point& screen_location,
      const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
      const gfx::ImageSkia& drag_image,
      const gfx::Vector2d& drag_image_offset,
      uint32_t drag_operation,
      ui::mojom::PointerKind source) override;
  void CancelDragDrop(Id window_id) override;
  void ObserveTopmostWindow(mojom::MoveLoopSource source,
                            Id window_id) override;
  void StopObservingTopmostWindow() override;

  WindowService* window_service_;

  const ClientSpecificId client_id_;

  // Identity of the remote client. This is only valid for clients connecting
  // directly to the WindowService. Clients that were embedded do not connect
  // directly to the WindowService, in which case |client_name_| is empty.
  const std::string client_name_;

  ConnectionType connection_type_ = ConnectionType::kEmbedding;

  mojom::WindowTreeClient* window_tree_client_;

  // Controls whether the client can change the visibility of the roots.
  bool can_change_root_window_visibility_ = true;

  ClientRoots client_roots_;

  // Set of windows this client created. The values are the same as key, but
  // put inside a unique_ptr to reinforce this class owns these Windows.
  // Ideally set would be used, but sets have some painful restrictions
  // (c++17's set::extract() may make it possible to use a set again).
  std::unordered_map<aura::Window*, std::unique_ptr<aura::Window>>
      client_created_windows_;

  // These contain mappings for known windows. At a minimum this contains the
  // windows in |client_created_windows_|. It will also contain any windows
  // that are exposed (known) to this client for various reasons. For example,
  // if this client is the result of an embedding then the window at the embed
  // point (the root window of the ClientRoot) was not created by this client,
  // but is known and in these mappings.
  std::map<aura::Window*, ClientWindowId> window_to_client_window_id_map_;
  std::unordered_map<ClientWindowId, aura::Window*, ClientWindowIdHash>
      client_window_id_to_window_map_;

  // Used to track the active change from the client.
  std::unique_ptr<ClientChangeTracker> property_change_tracker_;

  // If non-null the client has requested pointer events the client would not
  // normally get.
  std::unique_ptr<PointerWatcher> pointer_watcher_;

  FocusHandler focus_handler_{this};

  // Holds WindowTreeClients passed to ScheduleEmbed(). Entries are removed
  // when EmbedUsingToken() is called.
  using ScheduledEmbeds =
      base::flat_map<base::UnguessableToken, mojom::WindowTreeClientPtr>;
  ScheduledEmbeds scheduled_embeds_;

  // Calls to ScheduleEmbedForExistingClient() add an entry here. The value is
  // the value supplied to ScheduleEmbedForExistingClient(). When a matching
  // EmbedUsingToken() is received, the value in the map is used as the id for
  // the window in this client (specifically |ClientWindowId.sink_id|).
  using ScheduledEmbedsForExistingClient =
      base::flat_map<base::UnguessableToken, ClientSpecificId>;
  ScheduledEmbedsForExistingClient scheduled_embeds_for_existing_client_;

  // Used to track events sent to the client. Key events are tracked separately,
  // as their acks may be delayed when the client uses async IME handling.
  std::queue<std::unique_ptr<InFlightEvent>> in_flight_key_events_;
  std::queue<std::unique_ptr<InFlightEvent>> in_flight_other_events_;

  // Set while a window move loop is in progress.
  aura::Window* window_moving_ = nullptr;

  // Set while a window move loop is in progress to track the window move
  // information.
  std::unique_ptr<TopmostWindowObserver> topmost_window_observer_;

  // Set while a drag loop is in progress.
  Id pending_drag_source_window_id_ = kInvalidTransportId;

  base::WeakPtrFactory<WindowTree> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowTree);
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_TREE_H_
