// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_

#include <map>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

class RenderFrameHost;
class WebContents;

// Owns a set of Channel-associated interface receivers with frame context on
// message dispatch.
//
// When messages are dispatched to the implementation, the implementation can
// call GetCurrentTargetFrame() on this object (see below) to determine which
// frame sent the message.
//
// In order to expose the interface to all RenderFrames, a binder must be
// registered for the interface. Typically this is done in
// BindAssociatedReceiverFromFrame() in a ContentBrowserClient subclass.  Doing
// that will expose the interface to all remote RenderFrame objects. If the
// WebContents is destroyed at any point, the receivers will automatically
// reset and will cease to dispatch further incoming messages.
//
// Because this object uses Channel-associated interface receivers, all messages
// sent via these interfaces are ordered with respect to legacy Chrome IPC
// messages on the relevant IPC::Channel (i.e. the Channel between the browser
// and whatever render process hosts the sending frame.)
template <typename Interface>
class CONTENT_EXPORT RenderFrameHostReceiverSet : public WebContentsObserver {
 public:
  RenderFrameHostReceiverSet(WebContents* web_contents, Interface* impl)
      : WebContentsObserver(web_contents), impl_(impl) {}
  ~RenderFrameHostReceiverSet() override = default;

  RenderFrameHostReceiverSet(const RenderFrameHostReceiverSet&) = delete;
  RenderFrameHostReceiverSet& operator=(const RenderFrameHostReceiverSet&) =
      delete;

  void Bind(RenderFrameHost* render_frame_host,
            mojo::PendingAssociatedReceiver<Interface> pending_receiver) {
    mojo::ReceiverId id =
        receivers_.Add(impl_, std::move(pending_receiver), render_frame_host);
    frame_to_receivers_map_[render_frame_host].push_back(id);
  }

  RenderFrameHost* GetCurrentTargetFrame() {
    return receivers_.current_context();
  }

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    auto it = frame_to_receivers_map_.find(render_frame_host);
    if (it == frame_to_receivers_map_.end())
      return;
    for (auto id : it->second)
      receivers_.Remove(id);
    frame_to_receivers_map_.erase(it);
  }

  // Receiver set for each frame in the page. Note, bindings are reused across
  // navigations that are same-site since the RenderFrameHost is reused in that
  // case.
  mojo::AssociatedReceiverSet<Interface, RenderFrameHost*> receivers_;

  // Track which RenderFrameHosts are in the |receivers_| set so they can
  // be removed them when a RenderFrameHost is removed.
  std::map<RenderFrameHost*, std::vector<mojo::ReceiverId>>
      frame_to_receivers_map_;

  // Must outlive this class.
  Interface* const impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_
