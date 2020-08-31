// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/render_accessibility.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class RenderFrameImpl;
class RenderAccessibilityImpl;

// This class implements the RenderAccessibility mojo interface that the browser
// process uses to communicate with the renderer for any accessibility-related
// operations such as enabling/disabling accessibility support in the renderer,
// requesting snapshots of the accessibility tree or performing actions.
//
// Instances of this class will be owned by one RenderFrameImpl object, which
// will keep it alive for its entire lifetime. In addition, this class will own
// an instance of the RenderAccessibilityImpl class, which will only be alive
// while accessibility support in the renderer is enabled (i.e whenever the
// ui::AXMode set from the browser includes the |kWebContents| mode).
//
// Thus, when renderer accessibility is enabled, |render_accessibility_| will
// point to a valid RenderAccessibilityImpl object which will be used to enable
// bi-directional communication between the RenderFrameHostImpl object in the
// browser process and Blink.
class CONTENT_EXPORT RenderAccessibilityManager
    : public mojom::RenderAccessibility {
 public:
  RenderAccessibilityManager(RenderFrameImpl* const render_frame);
  ~RenderAccessibilityManager() override;

  // Binds the |receiver| to process mojo messages. This method is expected to
  // be called only while |receiver_| is in an unbound state.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::RenderAccessibility> receiver);

  // Returns a pointer to the RenderAccessibilityImpl object owned by this
  // class. Can return nullptr if accessibility is not enabled in the renderer.
  RenderAccessibilityImpl* GetRenderAccessibilityImpl();

  // Returns the current accessibility mode for the associated RenderFrameImpl.
  ui::AXMode GetAccessibilityMode() const;

  // mojom::RenderAccessibility implementation.
  void SetMode(uint32_t ax_mode) override;
  void FatalError() override;
  void HitTest(const ui::AXActionData& action_data,
               mojom::RenderAccessibility::HitTestCallback callback) override;
  void PerformAction(const ui::AXActionData& data) override;
  void Reset(int32_t reset_token) override;

  // Communication with the browser process.
  void HandleAccessibilityEvents(
      const std::vector<AXContentTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events,
      int32_t reset_token,
      mojom::RenderAccessibilityHost::HandleAXEventsCallback callback);
  void HandleLocationChanges(std::vector<mojom::LocationChangesPtr> changes);

 private:
  // Returns the associated remote used to send messages to the browser process,
  // lazily initializing it the first time it's used.
  mojo::AssociatedRemote<mojom::RenderAccessibilityHost>&
  GetOrCreateRemoteRenderAccessibilityHost();

  // The RenderFrameImpl that owns us.
  RenderFrameImpl* render_frame_;

  // Valid only while an accessibility mode including kWebContents is set.
  std::unique_ptr<RenderAccessibilityImpl> render_accessibility_;

  // Endpoint to receive and handle messages from the browser process.
  mojo::AssociatedReceiver<mojom::RenderAccessibility> receiver_{this};

  // Endpoint to send messages to the browser process.
  mojo::AssociatedRemote<mojom::RenderAccessibilityHost>
      render_accessibility_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_MANAGER_H_
