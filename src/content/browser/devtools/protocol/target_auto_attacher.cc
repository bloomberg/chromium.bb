// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_auto_attacher.h"

#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace protocol {

TargetAutoAttacher::TargetAutoAttacher() = default;

TargetAutoAttacher::~TargetAutoAttacher() {
  for (auto& client : clients_)
    client.AutoAttacherDestroyed(this);
}

bool TargetAutoAttacher::auto_attach() const {
  return !clients_.empty();
}

bool TargetAutoAttacher::wait_for_debugger_on_start() const {
  return !clients_requesting_wait_for_debugger_.empty();
}

DevToolsAgentHost* TargetAutoAttacher::AutoAttachToFrame(
    NavigationRequest* navigation_request,
    bool wait_for_debugger_on_start) {
  if (!auto_attach())
    return nullptr;

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  RenderFrameHostImpl* new_host = navigation_request->GetRenderFrameHost();

  // |new_host| can be nullptr for navigation that doesn't commmit
  // (e.g. download). Skip possibly detaching the old agent host so the DevTools
  // message logged via the old RFH can be seen.
  if (!new_host)
    return nullptr;

  scoped_refptr<DevToolsAgentHost> agent_host =
      RenderFrameDevToolsAgentHost::FindForDangling(frame_tree_node);

  bool is_portal_main_frame =
      frame_tree_node->IsMainFrame() &&
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(new_host))
          ->IsPortal();
  bool needs_host_attached =
      new_host->is_local_root_subframe() || is_portal_main_frame;

  if (needs_host_attached) {
    if (!agent_host) {
      agent_host = RenderFrameDevToolsAgentHost::
          CreateForLocalRootOrEmbeddedPageNavigation(navigation_request);
    }
    return DispatchAutoAttach(agent_host.get(), wait_for_debugger_on_start)
               ? agent_host.get()
               : nullptr;
  }

  if (!agent_host)
    return nullptr;

  // At this point we don't need a host, so we must auto detach if we auto
  // attached earlier.
  DispatchAutoDetach(agent_host.get());
  return nullptr;
}

void TargetAutoAttacher::UpdateAutoAttach(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TargetAutoAttacher::AddClient(Client* client,
                                   bool wait_for_debugger_on_start,
                                   base::OnceClosure callback) {
  clients_.AddObserver(client);
  if (wait_for_debugger_on_start)
    clients_requesting_wait_for_debugger_.insert(client);
  UpdateAutoAttach(std::move(callback));
}

void TargetAutoAttacher::UpdateWaitForDebuggerOnStart(
    Client* client,
    bool wait_for_debugger_on_start,
    base::OnceClosure callback) {
  DCHECK(clients_.HasObserver(client));
  if (wait_for_debugger_on_start)
    clients_requesting_wait_for_debugger_.insert(client);
  else
    clients_requesting_wait_for_debugger_.erase(client);
  UpdateAutoAttach(std::move(callback));
}

void TargetAutoAttacher::RemoveClient(Client* client) {
  clients_.RemoveObserver(client);
  clients_requesting_wait_for_debugger_.erase(client);
  DCHECK(clients_requesting_wait_for_debugger_.empty() || !clients_.empty());
  if (clients_.empty() || clients_requesting_wait_for_debugger_.empty())
    UpdateAutoAttach(base::DoNothing());
}

void TargetAutoAttacher::AppendNavigationThrottles(
    NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<NavigationThrottle>>* throttles) {
  for (auto& client : clients_) {
    std::unique_ptr<NavigationThrottle> throttle =
        client.CreateThrottleForNavigation(this, navigation_handle);
    if (throttle)
      throttles->push_back(std::move(throttle));
  }
}

bool TargetAutoAttacher::DispatchAutoAttach(DevToolsAgentHost* host,
                                            bool waiting_for_debugger) {
  bool attached = false;
  for (auto& client : clients_) {
    attached =
        client.AutoAttach(
            this, host,
            waiting_for_debugger &&
                clients_requesting_wait_for_debugger_.contains(&client)) ||
        attached;
  }
  return attached;
}

void TargetAutoAttacher::DispatchAutoDetach(DevToolsAgentHost* host) {
  for (auto& client : clients_)
    client.AutoDetach(this, host);
}

void TargetAutoAttacher::DispatchSetAttachedTargetsOfType(
    const base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
    const std::string& type) {
  for (auto& client : clients_)
    client.SetAttachedTargetsOfType(this, hosts, type);
}

RendererAutoAttacherBase::RendererAutoAttacherBase(
    DevToolsRendererChannel* renderer_channel)
    : renderer_channel_(renderer_channel) {}

RendererAutoAttacherBase::~RendererAutoAttacherBase() = default;

void RendererAutoAttacherBase::UpdateAutoAttach(base::OnceClosure callback) {
  DevToolsRendererChannel::ChildWorkerCreatedCallback report_worker_callback;
  if (auto_attach()) {
    report_worker_callback = base::BindRepeating(
        &RendererAutoAttacherBase::ChildWorkerCreated, base::Unretained(this));
  }
  renderer_channel_->SetReportChildWorkers(std::move(report_worker_callback),
                                           wait_for_debugger_on_start(),
                                           std::move(callback));
}

void RendererAutoAttacherBase::ChildWorkerCreated(
    DevToolsAgentHostImpl* agent_host,
    bool waiting_for_debugger) {
  DispatchAutoAttach(agent_host, waiting_for_debugger);
}

}  // namespace protocol
}  // namespace content
