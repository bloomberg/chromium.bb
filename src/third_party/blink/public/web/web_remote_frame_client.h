// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_

#include "base/optional.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_remote_frame.h"

namespace blink {
class WebURLRequest;
struct WebRect;

class WebRemoteFrameClient {
 public:
  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // Notify the embedder that it should remove this frame from the frame tree
  // and release any resources associated with it.
  virtual void FrameDetached(DetachType) {}

  // Notifies the embedder that a postMessage was issued to a remote frame.
  virtual void ForwardPostMessage(WebLocalFrame* source_frame,
                                  WebRemoteFrame* target_frame,
                                  WebSecurityOrigin target_origin,
                                  WebDOMMessageEvent) {}

  // A remote frame was asked to start a navigation.
  virtual void Navigate(const WebURLRequest& request,
                        blink::WebLocalFrame* initiator_frame,
                        bool should_replace_current_entry,
                        bool is_opener_navigation,
                        bool initiator_frame_has_download_sandbox_flag,
                        bool blocking_downloads_in_sandbox_enabled,
                        bool initiator_frame_is_ad,
                        mojo::ScopedMessagePipeHandle blob_url_token,
                        const base::Optional<WebImpression>& impression) {}

  virtual void FrameRectsChanged(const WebRect& local_frame_rect,
                                 const WebRect& screen_space_rect) {}

  virtual void UpdateRemoteViewportIntersection(
      const ViewportIntersectionState& intersection_state) {}

  // This frame updated its opener to another frame.
  virtual void DidChangeOpener(WebFrame* opener) {}

  // Continue sequential focus navigation in this frame.  This is called when
  // the |source| frame is searching for the next focusable element (e.g., in
  // response to <tab>) and encounters a remote frame.
  virtual void AdvanceFocus(mojom::FocusType type, WebLocalFrame* source) {}

  // Returns token to be used as a frame id in the devtools protocol.
  // It is derived from the content's devtools_frame_token, is
  // defined by the browser and passed into Blink upon frame creation.
  virtual base::UnguessableToken GetDevToolsFrameToken() {
    return base::UnguessableToken::Create();
  }

  // Print out this frame.
  // |rect| is the rectangular area where this frame resides in its parent
  // frame.
  // |canvas| is the canvas we are printing on.
  // Returns the id of the placeholder content.
  virtual uint32_t Print(const WebRect& rect, cc::PaintCanvas* canvas) {
    return 0;
  }

 protected:
  virtual ~WebRemoteFrameClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
