// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_TEST_WINDOW_TREE_H_
#define UI_AURA_TEST_MUS_TEST_WINDOW_TREE_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace aura {

enum class WindowTreeChangeType {
  BOUNDS,
  // Used for both set and release capture.
  CAPTURE,
  FOCUS,
  MODAL,
  NEW_TOP_LEVEL,
  NEW_WINDOW,
  PROPERTY,
  VISIBLE,

  // This covers all cases that aren't used in tests.
  OTHER,
};

// WindowTree implementation for tests. TestWindowTree maintains a list of all
// calls that take a change_id and are expected to be acked back to the client.
// Various functions are provided to respond to the changes.
class TestWindowTree : public ui::mojom::WindowTree {
 public:
  TestWindowTree();
  ~TestWindowTree() override;

  void set_client(ui::mojom::WindowTreeClient* client) { client_ = client; }

  uint32_t window_id() const { return window_id_; }

  bool WasEventAcked(uint32_t event_id) const;

  mojo::Array<uint8_t> GetLastPropertyValue();

  mojo::Map<mojo::String, mojo::Array<uint8_t>> GetLastNewWindowProperties();

  // True if at least one function has been called that takes a change id.
  bool has_change() const { return !changes_.empty(); }

  // Acks all changes with a value of true.
  void AckAllChanges();

  // Returns false if there are no, or more than one, changes of the specified
  // type. If there is only one of the matching type it is acked with a result
  // of |result| and true is returned.
  bool AckSingleChangeOfType(WindowTreeChangeType type, bool result);

  // Same as AckSingleChangeOfType(), but doesn't fail if there is more than
  // one change of the specified type.
  bool AckFirstChangeOfType(WindowTreeChangeType type, bool result);

  void AckAllChangesOfType(WindowTreeChangeType type, bool result);

  bool GetAndRemoveFirstChangeOfType(WindowTreeChangeType type,
                                     uint32_t* change_id);

 private:
  struct Change {
    WindowTreeChangeType type;
    uint32_t id;
  };

  void OnChangeReceived(
      uint32_t change_id,
      WindowTreeChangeType type = WindowTreeChangeType::OTHER);

  // ui::mojom::WindowTree:
  void NewWindow(
      uint32_t change_id,
      uint32_t window_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void NewTopLevelWindow(
      uint32_t change_id,
      uint32_t window_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void DeleteWindow(uint32_t change_id, uint32_t window_id) override;
  void SetWindowBounds(uint32_t change_id,
                       uint32_t window_id,
                       const gfx::Rect& bounds) override;
  void SetClientArea(uint32_t window_id,
                     const gfx::Insets& insets,
                     mojo::Array<gfx::Rect> additional_client_areas) override;
  void SetHitTestMask(uint32_t window_id,
                      const base::Optional<gfx::Rect>& mask) override;
  void SetCanAcceptDrops(uint32_t window_id, bool accepts_drags) override;
  void SetWindowVisibility(uint32_t change_id,
                           uint32_t window_id,
                           bool visible) override;
  void SetWindowProperty(uint32_t change_id,
                         uint32_t window_id,
                         const mojo::String& name,
                         mojo::Array<uint8_t> value) override;
  void SetWindowOpacity(uint32_t change_id,
                        uint32_t window_id,
                        float opacity) override;
  void AttachCompositorFrameSink(
      uint32_t window_id,
      ui::mojom::CompositorFrameSinkType type,
      mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> surface,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override;
  void OnWindowSurfaceDetached(uint32_t window_id,
                               const cc::SurfaceSequence& sequence) override;
  void AddWindow(uint32_t change_id, uint32_t parent, uint32_t child) override;
  void RemoveWindowFromParent(uint32_t change_id, uint32_t window_id) override;
  void AddTransientWindow(uint32_t change_id,
                          uint32_t window_id,
                          uint32_t transient_window_id) override;
  void RemoveTransientWindowFromParent(uint32_t change_id,
                                       uint32_t window_id) override;
  void SetModal(uint32_t change_id, uint32_t window_id) override;
  void ReorderWindow(uint32_t change_id,
                     uint32_t window_id,
                     uint32_t relative_window_id,
                     ui::mojom::OrderDirection direction) override;
  void GetWindowTree(uint32_t window_id,
                     const GetWindowTreeCallback& callback) override;
  void SetCapture(uint32_t change_id, uint32_t window_id) override;
  void ReleaseCapture(uint32_t change_id, uint32_t window_id) override;
  void StartPointerWatcher(bool want_moves) override;
  void StopPointerWatcher() override;
  void Embed(uint32_t window_id,
             ui::mojom::WindowTreeClientPtr client,
             uint32_t flags,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t change_id, uint32_t window_id) override;
  void SetCanFocus(uint32_t window_id, bool can_focus) override;
  void SetCanAcceptEvents(uint32_t window_id, bool can_accept_events) override;
  void SetPredefinedCursor(uint32_t change_id,
                           uint32_t window_id,
                           ui::mojom::Cursor cursor_id) override;
  void SetWindowTextInputState(uint32_t window_id,
                               mojo::TextInputStatePtr state) override;
  void SetImeVisibility(uint32_t window_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;
  void OnWindowInputEventAck(uint32_t event_id,
                             ui::mojom::EventResult result) override;
  void GetWindowManagerClient(
      mojo::AssociatedInterfaceRequest<ui::mojom::WindowManagerClient> internal)
      override;
  void GetCursorLocationMemory(
      const GetCursorLocationMemoryCallback& callback) override;
  void PerformDragDrop(uint32_t change_id,
                       uint32_t source_window_id,
                       mojo::Map<mojo::String, mojo::Array<uint8_t>> drag_data,
                       uint32_t drag_operation) override;
  void CancelDragDrop(uint32_t window_id) override;
  void PerformWindowMove(uint32_t change_id,
                         uint32_t window_id,
                         ui::mojom::MoveLoopSource source,
                         const gfx::Point& cursor_location) override;
  void CancelWindowMove(uint32_t window_id) override;

  std::set<uint32_t> acked_events_;
  uint32_t window_id_ = 0u;

  mojo::Array<uint8_t> last_property_value_;

  std::vector<Change> changes_;

  ui::mojom::WindowTreeClient* client_;

  mojo::Map<mojo::String, mojo::Array<uint8_t>> last_new_window_properties_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTree);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_TEST_WINDOW_TREE_H_
