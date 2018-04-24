// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws2/ids.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
namespace ws2 {

class ClientChangeTracker;
class ClientRoot;
class WindowService;
class WindowServiceClientBinding;

// WindowServiceClient manages a client connected to the Window Service.
// WindowServiceClient provides the implementation of mojom::WindowTree that
// the client talks to. WindowServiceClient implements the mojom::WindowTree
// interface on top of aura. That is, changes to the aura Window hierarchy
// are mirrored to the mojom::WindowTreeClient interface. Similarly, changes
// from the client by way of the mojom::WindowTree interface modify the
// underlying aura Windows.
//
// WindowServiceClient is created in two distinct ways:
// . when the client is embedded in a specific aura::Window, by way of
//   WindowTree::Embed(). Use InitForEmbed(). This configuration has only a
//   single ClientRoot.
// . by way of a WindowTreeFactory. In this mode the client has no initial
//   roots. To create a root the client requests a top-level window. Use
//   InitFromFactory() for this. In this configuration there is a ClientRoot
//   per top-level.
//
// Typically instances of WindowServiceClient are owned by a
// WindowServiceClientBinding that is created via WindowTreeFactory, but that
// is not necessary (in particular tests often do not use
// WindowServiceClientBinding).
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowServiceClient
    : public mojom::WindowTree,
      aura::WindowObserver {
 public:
  WindowServiceClient(WindowService* window_service,
                      ClientSpecificId client_id,
                      mojom::WindowTreeClient* client,
                      bool intercepts_events);
  ~WindowServiceClient() override;

  // See class description for details on Init variants.
  void InitForEmbed(aura::Window* root, mojom::WindowTreePtr window_tree_ptr);
  void InitFromFactory();

 private:
  friend class ClientRoot;
  friend class WindowServiceClientTestHelper;

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

    // Called when the ClientRoot is deleted from the WindowServiceClient
    // destructor.
    kDestructor,
  };

  // Creates a new ClientRoot. The returned ClientRoot is owned by this.
  ClientRoot* CreateClientRoot(aura::Window* window,
                               mojom::WindowTreePtr window_tree);
  void DeleteClientRoot(ClientRoot* client_root, DeleteClientRootReason reason);
  void DeleteClientRootWithRoot(aura::Window* window);

  aura::Window* GetWindowByClientId(const ClientWindowId& id);

  // Returns true if |this| created |window|.
  bool IsClientCreatedWindow(aura::Window* window);
  bool IsClientRootWindow(aura::Window* window);

  // Returns true if |window| was created by the client calling
  // NewTopLevelWindow().
  bool IsTopLevel(aura::Window* window);
  ClientRoots::iterator FindClientRootWithRoot(aura::Window* window);

  // Returns true if |window| has been exposed to this client. A client
  // typically only sees a limited set of windows that may exist. The set of
  // windows exposed to the client are referred to as the known windows.
  bool IsWindowKnown(aura::Window* window) const;
  bool IsWindowRootOfAnotherClient(aura::Window* window) const;

  // Called for windows created by the client (including top-levels).
  aura::Window* AddClientCreatedWindow(
      const ClientWindowId& id,
      std::unique_ptr<aura::Window> window_ptr);

  // Adds/removes a Window from the set of windows known to the client. This
  // also adds or removes any observers that may need to be installed.
  void AddWindowToKnownWindows(aura::Window* window, const ClientWindowId& id);
  void RemoveWindowFromKnownWindows(aura::Window* window);

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

  std::vector<mojom::WindowDataPtr> WindowsToWindowDatas(
      const std::vector<aura::Window*>& windows);
  mojom::WindowDataPtr WindowToWindowData(aura::Window* window);

  // Called before |window| becomes the root Window of a ClientRoot. This
  // destroys the existing ClientRoot if there is one (because a Window can
  // only be the root Window of a single ClientRoot).
  void OnWillBecomeClientRootWindow(aura::Window* window);

  // Methods with the name Impl() mirror those of mojom::WindowTree. The return
  // value indicates whether they succeeded or not. Generally failure means the
  // operation was not allowed.
  bool NewWindowImpl(
      const ClientWindowId& client_window_id,
      const std::map<std::string, std::vector<uint8_t>>& properties);
  bool DeleteWindowImpl(const ClientWindowId& window_id);
  bool AddWindowImpl(const ClientWindowId& parent_id,
                     const ClientWindowId& child_id);
  bool RemoveWindowFromParentImpl(const ClientWindowId& client_window_id);
  bool SetWindowVisibilityImpl(const ClientWindowId& window_id, bool visible);
  bool EmbedImpl(const ClientWindowId& window_id,
                 mojom::WindowTreeClientPtr window_tree_client,
                 uint32_t flags);
  bool SetWindowOpacityImpl(const ClientWindowId& window_id, float opacity);
  bool SetWindowBoundsImpl(
      const ClientWindowId& window_id,
      const gfx::Rect& bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id);
  std::vector<aura::Window*> GetWindowTreeImpl(const ClientWindowId& window_id);
  void GetWindowTreeRecursive(aura::Window* window,
                              std::vector<aura::Window*>* windows);

  // Called when the WindowTreeClient of a WindowServiceClientBinding created
  // by this object disconnects. This removes |binding| from
  // |embedded_client_bindings_|, which results in deleting |binding|.
  void OnChildBindingConnectionLost(WindowServiceClientBinding* binding);

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

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
  void SetCapture(uint32_t change_id, Id window_id) override;
  void ReleaseCapture(uint32_t change_id, Id window_id) override;
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
  void SetClientArea(Id window_id,
                     const gfx::Insets& insets,
                     const base::Optional<std::vector<gfx::Rect>>&
                         additional_client_areas) override;
  void SetHitTestMask(Id window_id,
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
      ::viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
      ::viz::mojom::CompositorFrameSinkClientPtr client) override;
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
                     Id window_id,
                     Id relative_window_id,
                     ::ui::mojom::OrderDirection direction) override;
  void GetWindowTree(Id window_id, GetWindowTreeCallback callback) override;
  void Embed(Id transport_window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t embed_flags,
             EmbedCallback callback) override;
  void ScheduleEmbed(mojom::WindowTreeClientPtr client,
                     ScheduleEmbedCallback callback) override;
  void ScheduleEmbedForExistingClient(
      uint32_t window_id,
      ScheduleEmbedForExistingClientCallback callback) override;
  void EmbedUsingToken(Id window_id,
                       const base::UnguessableToken& token,
                       uint32_t embed_flags,
                       EmbedUsingTokenCallback callback) override;
  void SetFocus(uint32_t change_id, Id window_id) override;
  void SetCanFocus(Id window_id, bool can_focus) override;
  void SetCursor(uint32_t change_id,
                 Id window_id,
                 ui::CursorData cursor) override;
  void SetWindowTextInputState(Id window_id,
                               ::ui::mojom::TextInputStatePtr state) override;
  void SetImeVisibility(Id window_id,
                        bool visible,
                        ::ui::mojom::TextInputStatePtr state) override;
  void SetEventTargetingPolicy(
      Id window_id,
      ::ui::mojom::EventTargetingPolicy policy) override;
  void OnWindowInputEventAck(uint32_t event_id,
                             ::ui::mojom::EventResult result) override;
  void DeactivateWindow(Id window_id) override;
  void StackAbove(uint32_t change_id, Id above_id, Id below_id) override;
  void StackAtTop(uint32_t change_id, Id window_id) override;
  void PerformWmAction(Id window_id, const std::string& action) override;
  void GetWindowManagerClient(
      ::ui::mojom::WindowManagerClientAssociatedRequest internal) override;
  void GetCursorLocationMemory(
      GetCursorLocationMemoryCallback callback) override;
  void PerformWindowMove(uint32_t change_id,
                         Id window_id,
                         ::ui::mojom::MoveLoopSource source,
                         const gfx::Point& cursor) override;
  void CancelWindowMove(Id window_id) override;
  void PerformDragDrop(
      uint32_t change_id,
      Id source_window_id,
      const gfx::Point& screen_location,
      const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
      const SkBitmap& drag_image,
      const gfx::Vector2d& drag_image_offset,
      uint32_t drag_operation,
      ::ui::mojom::PointerKind source) override;
  void CancelDragDrop(Id window_id) override;

  WindowService* window_service_;

  const ClientSpecificId client_id_;

  ConnectionType connection_type_ = ConnectionType::kEmbedding;

  mojom::WindowTreeClient* window_tree_client_;

  // If true the client sees all the decendants of windows with embeddings
  // in them that were created by this client, and additionally any events
  // normally targeted at a descendant are targeted at the first ancestor Window
  // created by this client. This is done to allow a client to intercept events
  // normally targeted at descendants and dispatch them using some other means.
  const bool intercepts_events_;

  // Controls whether the client can change the visibility of the roots.
  bool can_change_root_window_visibility_ = true;

  ClientRoots client_roots_;

  // The set of windows created by this client.
  std::set<std::unique_ptr<aura::Window>, base::UniquePtrComparator>
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

  // If non-null the window the client requested to delete.
  aura::Window* window_deleting_ = nullptr;

  // WindowServiceClientBindings created by way of Embed().
  std::vector<std::unique_ptr<WindowServiceClientBinding>>
      embedded_client_bindings_;

  // Used to track the active change from the client.
  std::unique_ptr<ClientChangeTracker> property_change_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceClient);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_H_
