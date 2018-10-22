// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resolve_proxy_msg_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

ResolveProxyMsgHelper::ResolveProxyMsgHelper(int render_process_host_id)
    : BrowserMessageFilter(ViewMsgStart),
      render_process_host_id_(render_process_host_id),
      binding_(this) {}

void ResolveProxyMsgHelper::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == ViewHostMsg_ResolveProxy::ID)
    *thread = BrowserThread::UI;
}

bool ResolveProxyMsgHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ResolveProxyMsgHelper, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ResolveProxyMsgHelper::OnResolveProxy(const GURL& url,
                                           IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enqueue the pending request.
  pending_requests_.push_back(PendingRequest(url, reply_msg));

  // If nothing is in progress, start.
  if (!binding_.is_bound()) {
    DCHECK_EQ(1u, pending_requests_.size());
    StartPendingRequest();
  }
}

ResolveProxyMsgHelper::~ResolveProxyMsgHelper() {
  DCHECK(!owned_self_);
  DCHECK(!binding_.is_bound());
}

void ResolveProxyMsgHelper::StartPendingRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!binding_.is_bound());
  DCHECK(!pending_requests_.empty());

  // Start the request.
  network::mojom::ProxyLookupClientPtr proxy_lookup_client;
  binding_.Bind(mojo::MakeRequest(&proxy_lookup_client));
  binding_.set_connection_error_handler(
      base::BindOnce(&ResolveProxyMsgHelper::OnProxyLookupComplete,
                     base::Unretained(this), base::nullopt));
  owned_self_ = this;
  if (!SendRequestToNetworkService(pending_requests_.front().url,
                                   std::move(proxy_lookup_client))) {
    OnProxyLookupComplete(base::nullopt);
  }
}

bool ResolveProxyMsgHelper::SendRequestToNetworkService(
    const GURL& url,
    network::mojom::ProxyLookupClientPtr proxy_lookup_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_host_id_);
  // Fail the request if there's no such RenderProcessHost;
  if (!render_process_host)
    return false;
  render_process_host->GetStoragePartition()
      ->GetNetworkContext()
      ->LookUpProxyForURL(url, std::move(proxy_lookup_client));
  return true;
}

void ResolveProxyMsgHelper::OnProxyLookupComplete(
    const base::Optional<net::ProxyInfo>& proxy_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!pending_requests_.empty());

  binding_.Close();

  // If all references except |owned_self_| have been released, just release
  // the last reference, without doing anything.
  if (HasOneRef()) {
    scoped_refptr<ResolveProxyMsgHelper> self = std::move(owned_self_);
    return;
  }

  owned_self_ = nullptr;

  // Clear the current (completed) request.
  PendingRequest completed_req = std::move(pending_requests_.front());
  pending_requests_.pop_front();

  ViewHostMsg_ResolveProxy::WriteReplyParams(
      completed_req.reply_msg.get(), !!proxy_info,
      proxy_info ? proxy_info->ToPacString() : std::string());
  Send(completed_req.reply_msg.release());

  // Start the next request.
  if (!pending_requests_.empty())
    StartPendingRequest();
}

ResolveProxyMsgHelper::PendingRequest::PendingRequest(const GURL& url,
                                                      IPC::Message* reply_msg)
    : url(url), reply_msg(reply_msg) {}

ResolveProxyMsgHelper::PendingRequest::PendingRequest(
    PendingRequest&& pending_request) noexcept = default;

ResolveProxyMsgHelper::PendingRequest::~PendingRequest() noexcept = default;

ResolveProxyMsgHelper::PendingRequest& ResolveProxyMsgHelper::PendingRequest::
operator=(PendingRequest&& pending_request) noexcept = default;

}  // namespace content
