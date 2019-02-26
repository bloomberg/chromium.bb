// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_agent_host.h"

#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/common/child_process_host.h"

namespace content {

WorkerDevToolsAgentHost::WorkerDevToolsAgentHost(
    int process_id,
    blink::mojom::DevToolsAgentPtr agent_ptr,
    blink::mojom::DevToolsAgentHostRequest host_request,
    const GURL& url,
    const base::UnguessableToken& devtools_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      process_id_(process_id),
      url_(url),
      parent_id_(parent_id),
      destroyed_callback_(std::move(destroyed_callback)) {
  DCHECK(agent_ptr);
  DCHECK(!devtools_worker_token.is_empty());
  AddRef();  // Self keep-alive while the worker agent is alive.
  agent_ptr.set_connection_error_handler(base::BindOnce(
      &WorkerDevToolsAgentHost::Disconnected, base::Unretained(this)));
  NotifyCreated();
  GetRendererChannel()->SetRenderer(
      std::move(agent_ptr), std::move(host_request), process_id, nullptr);
}

WorkerDevToolsAgentHost::~WorkerDevToolsAgentHost() {}

void WorkerDevToolsAgentHost::Disconnected() {
  ForceDetachAllSessions();
  GetRendererChannel()->SetRenderer(
      nullptr, nullptr, ChildProcessHost::kInvalidUniqueID, nullptr);
  std::move(destroyed_callback_).Run(this);
  Release();  // Matches AddRef() in constructor.
}

BrowserContext* WorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  return process ? process->GetBrowserContext() : nullptr;
}

std::string WorkerDevToolsAgentHost::GetType() {
  return kTypeDedicatedWorker;
}

std::string WorkerDevToolsAgentHost::GetTitle() {
  return url_.spec();
}

std::string WorkerDevToolsAgentHost::GetParentId() {
  return parent_id_;
}

GURL WorkerDevToolsAgentHost::GetURL() {
  return url_;
}

bool WorkerDevToolsAgentHost::Activate() {
  return false;
}

void WorkerDevToolsAgentHost::Reload() {}

bool WorkerDevToolsAgentHost::Close() {
  return false;
}

bool WorkerDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  session->AddHandler(std::make_unique<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      GetRendererChannel(), session->GetRootSession()));
  return true;
}

void WorkerDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
}

}  // namespace content
