// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/accessibility/ax_tree_id.h"

namespace content {

class RenderFrameHostImpl;

// Handles accessibility messages sent from the renderer to the browser possibly
// off the main UI thread and then forwards them to the RenderFrameHostImpl on
// the browser process main thread, so that the potentially large
// deserialization does not block the main thread.
//
// This endpoint is bound on the ThreadPool for *performance reasons*, so its
// messages can be reordered w.r.t. navigation. To mitigate this:
//
// * The renderer explicitly destroys this endpoint when committing a new
//   document in the existing RenderFrame, so all messages send on behalf of a
//   new document will go through new document's BrowserInterfaceBroker.
//
// * Each message contains the per-document tree_id and the message will be
//   ignored if the tree_id doesn't match the current tree_id passed at
//   construction time. (It will also be checked again on the UI thread).
class RenderAccessibilityHost : public mojom::RenderAccessibilityHost {
 public:
  RenderAccessibilityHost(
      base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl,
      mojo::PendingReceiver<mojom::RenderAccessibilityHost> receiver,
      ui::AXTreeID tree_id);

  RenderAccessibilityHost(const RenderAccessibilityHost&) = delete;
  RenderAccessibilityHost& operator=(const RenderAccessibilityHost&) = delete;

  ~RenderAccessibilityHost() override;

  void HandleAXEvents(mojom::AXUpdatesAndEventsPtr updates_and_events,
                      int32_t reset_token,
                      HandleAXEventsCallback callback) override;

  void HandleAXLocationChanges(
      std::vector<mojom::LocationChangesPtr> changes) override;

 private:
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl_;
  mojo::Receiver<mojom::RenderAccessibilityHost> receiver_;
  const ui::AXTreeID tree_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_
