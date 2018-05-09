// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_CLIENT_WINDOW_H_
#define SERVICES_UI_WS2_CLIENT_WINDOW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/ui/ws2/ids.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"

namespace aura {
class Window;
}

namespace ui {
namespace ws2 {

class WindowHostFrameSinkClient;
class WindowServiceClient;

// Tracks any state associated with an aura::Window for the WindowService.
// ClientWindow is created for every window created at the request of a client,
// including the root window of ClientRoots.
class COMPONENT_EXPORT(WINDOW_SERVICE) ClientWindow {
 public:
  ~ClientWindow();

  // Creates a new ClientWindow. The lifetime of the ClientWindow is tied to
  // that of the Window (the Window ends up owning the ClientWindow).
  static ClientWindow* Create(aura::Window* window,
                              WindowServiceClient* client,
                              const viz::FrameSinkId& frame_sink_id);

  // Returns the ClientWindow associated with a window, null if not created yet.
  static ClientWindow* GetMayBeNull(aura::Window* window) {
    return const_cast<ClientWindow*>(
        GetMayBeNull(const_cast<const aura::Window*>(window)));
  }
  static const ClientWindow* GetMayBeNull(const aura::Window* window);

  WindowServiceClient* owning_window_service_client() {
    return owning_window_service_client_;
  }
  const WindowServiceClient* owning_window_service_client() const {
    return owning_window_service_client_;
  }

  void set_embedded_window_service_client(WindowServiceClient* client) {
    embedded_window_service_client_ = client;
  }
  WindowServiceClient* embedded_window_service_client() {
    return embedded_window_service_client_;
  }
  const WindowServiceClient* embedded_window_service_client() const {
    return embedded_window_service_client_;
  }

  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  void AttachCompositorFrameSink(
      viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
      viz::mojom::CompositorFrameSinkClientPtr client);

 private:
  ClientWindow(aura::Window*,
               WindowServiceClient* client,
               const viz::FrameSinkId& frame_sink_id);

  aura::Window* window_;

  // Client that created the window. Null if the window was not created at the
  // request of a client. Generally this is null for first level embedding,
  // otherwise non-null. A first level embedding is one where local code
  // calls InitForEmbed() on a Window not associated with any other clients.
  WindowServiceClient* owning_window_service_client_;

  // This is the WindowServiceClient that has the Window as a ClientRoot. This
  // is only set at embed points. For example, if a client creates a new window,
  // then |embedded_window_service_client_| is initially null for that window.
  // If the client embeds another client in it, then
  // |embedded_window_service_client_| is set appropriately.
  WindowServiceClient* embedded_window_service_client_ = nullptr;

  // TODO(sky): wire this up, see ServerWindow::UpdateFrameSinkId(). This is
  // initially the id supplied by the client (for locally created windows it is
  // kWindowServerClientId for the high part and low part an ever increasing
  // number). If the window is used as the embed root, then it changes to high
  // part = id of client being embedded in and low part 0. If used as a
  // top-level, it's changed to the id passed by the client requesting the
  // top-level.
  // TODO(sky): this likely needs to plug into values in aura::Window.
  viz::FrameSinkId frame_sink_id_;

  // viz::HostFrameSinkClient registered with the HostFrameSinkManager for the
  // window.
  std::unique_ptr<WindowHostFrameSinkClient> window_host_frame_sink_client_;

  DISALLOW_COPY_AND_ASSIGN(ClientWindow);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_CLIENT_WINDOW_H_
