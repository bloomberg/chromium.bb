// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_MUS_H_
#define UI_AURA_MUS_WINDOW_MUS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "services/ui/public/interfaces/cursor.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/mus/mus_types.h"

namespace gfx {
class Rect;
}

namespace ui {
namespace mojom {
enum class OrderDirection;
}
}

namespace aura {

struct SurfaceInfo;
class Window;
class WindowTreeClient;

// WindowMus defines the interface used by WindowTreeClient to modify
// the underlying Window. It's defined as a separate interface to make it clear
// that any changes that WindowTreeClient makes must be propagated through
// this interface so that they don't result in bouncing back to
// WindowTreeClient. For example, if the server change the bounds care must be
// taken that when the change is applied to the Window the server isn't asked to
// change the bounds too. See WindowPortMus for details.
class AURA_EXPORT WindowMus {
 public:
  virtual ~WindowMus() {}

  // Returns the WindowMus associated with |window|.
  static WindowMus* Get(Window* window);

  Id server_id() const { return server_id_; }

  // Top level windows may have a root window with no associated server window.
  // This happens because the client creates the top level window first, and
  // then the WindowTreeHost. Each WindowTreeHost has a Window, so the
  // WindowTreeHost for top-levels has no server window.
  bool has_server_window() const { return server_id() != kInvalidServerId; }

  virtual Window* GetWindow() = 0;

  // These functions are called in response to a change from the server. The
  // expectation is that in calling these WindowTreeClient is not called
  // back. For example, SetBoundsFromServer() should not result in calling back
  // to WindowTreeClient::OnWindowMusBoundsChanged().
  virtual void AddChildFromServer(WindowMus* window) = 0;
  virtual void RemoveChildFromServer(WindowMus* child) = 0;
  virtual void ReorderFromServer(WindowMus* child,
                                 WindowMus* relative,
                                 ui::mojom::OrderDirection) = 0;
  virtual void SetBoundsFromServer(const gfx::Rect& bounds) = 0;
  virtual void SetVisibleFromServer(bool visible) = 0;
  virtual void SetOpacityFromServer(float opacity) = 0;
  virtual void SetPredefinedCursorFromServer(ui::mojom::Cursor cursor) = 0;
  virtual void SetPropertyFromServer(const std::string& property_name,
                                     const std::vector<uint8_t>* data) = 0;
  virtual void SetSurfaceIdFromServer(
      std::unique_ptr<SurfaceInfo> surface_info) = 0;

  virtual void NotifyEmbeddedAppDisconnected() = 0;

 private:
  // Just for set_server_id(), which other places should not call.
  friend class WindowTreeClient;

  void set_server_id(Id id) { server_id_ = id; }

  Id server_id_ = kInvalidServerId;
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_MUS_H_
