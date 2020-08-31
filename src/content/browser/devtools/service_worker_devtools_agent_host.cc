// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_agent_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void TerminateServiceWorkerOnCoreThread(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->StopWorker(base::DoNothing());
  }
}

void SetDevToolsAttachedOnCoreThread(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    bool attached) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->SetDevToolsAttached(attached);
  }
}

void UpdateLoaderFactoriesOnCoreThread(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_bundle) {
  auto* version =
      context_weak ? context_weak->GetLiveVersion(version_id) : nullptr;
  if (!version)
    return;
  version->embedded_worker()->UpdateLoaderFactories(
      std::move(script_bundle), std::move(subresource_bundle));
}

}  // namespace

ServiceWorkerDevToolsAgentHost::ServiceWorkerDevToolsAgentHost(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerContextCore* context,
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    base::Optional<network::CrossOriginEmbedderPolicy>
        cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const base::UnguessableToken& devtools_worker_token)
    : DevToolsAgentHostImpl(devtools_worker_token.ToString()),
      state_(WORKER_NOT_READY),
      devtools_worker_token_(devtools_worker_token),
      worker_process_id_(worker_process_id),
      worker_route_id_(worker_route_id),
      context_(context),
      context_weak_(context_weak),
      version_id_(version_id),
      url_(url),
      scope_(scope),
      version_installed_time_(is_installed_version ? base::Time::Now()
                                                   : base::Time()),
      cross_origin_embedder_policy_(std::move(cross_origin_embedder_policy)),
      coep_reporter_(std::move(coep_reporter)) {
  NotifyCreated();
}

BrowserContext* ServiceWorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id_);
  return rph ? rph->GetBrowserContext() : nullptr;
}

std::string ServiceWorkerDevToolsAgentHost::GetType() {
  return kTypeServiceWorker;
}

std::string ServiceWorkerDevToolsAgentHost::GetTitle() {
  return "Service Worker " + url_.spec();
}

GURL ServiceWorkerDevToolsAgentHost::GetURL() {
  return url_;
}

bool ServiceWorkerDevToolsAgentHost::Activate() {
  return false;
}

void ServiceWorkerDevToolsAgentHost::Reload() {
}

bool ServiceWorkerDevToolsAgentHost::Close() {
  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindOnce(&TerminateServiceWorkerOnCoreThread,
                                       context_weak_, version_id_));
  return true;
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionInstalled() {
  version_installed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionDoomed() {
  version_doomed_time_ = base::Time::Now();
}

bool ServiceWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerContextCore* context,
    int64_t version_id) {
  return context_ == context && version_id_ == version_id;
}

ServiceWorkerDevToolsAgentHost::~ServiceWorkerDevToolsAgentHost() {
  ServiceWorkerDevToolsManager::GetInstance()->AgentHostDestroyed(this);
}

bool ServiceWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler(
      GetId(), devtools_worker_token_, GetIOContext(), base::DoNothing())));
  session->AddHandler(base::WrapUnique(new protocol::FetchHandler(
      GetIOContext(),
      base::BindRepeating(
          &ServiceWorkerDevToolsAgentHost::UpdateLoaderFactories,
          base::Unretained(this)))));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  session->AddHandler(std::make_unique<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      GetRendererChannel(), session->GetRootSession()));
  if (state_ == WORKER_READY && sessions().empty())
    UpdateIsAttached(true);
  return true;
}

void ServiceWorkerDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
  if (state_ == WORKER_READY && sessions().empty())
    UpdateIsAttached(false);
}

void ServiceWorkerDevToolsAgentHost::WorkerReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK_EQ(WORKER_NOT_READY, state_);
  state_ = WORKER_READY;
  GetRendererChannel()->SetRenderer(
      std::move(agent_remote), std::move(host_receiver), worker_process_id_);
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetReloadedAfterCrash();
  if (!sessions().empty())
    UpdateIsAttached(true);
}

void ServiceWorkerDevToolsAgentHost::UpdateCrossOriginEmbedderPolicy(
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  cross_origin_embedder_policy_ = std::move(cross_origin_embedder_policy);
  coep_reporter_.Bind(std::move(coep_reporter));
}

void ServiceWorkerDevToolsAgentHost::WorkerRestarted(int worker_process_id,
                                                     int worker_route_id) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  state_ = WORKER_NOT_READY;
  worker_process_id_ = worker_process_id;
  worker_route_id_ = worker_route_id;
}

void ServiceWorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  state_ = WORKER_TERMINATED;
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetCrashed();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  if (!sessions().empty())
    UpdateIsAttached(false);
}

void ServiceWorkerDevToolsAgentHost::UpdateIsAttached(bool attached) {
  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindOnce(&SetDevToolsAttachedOnCoreThread,
                                       context_weak_, version_id_, attached));
}

void ServiceWorkerDevToolsAgentHost::UpdateLoaderFactories(
    base::OnceClosure callback) {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id_);
  if (!rph) {
    std::move(callback).Run();
    return;
  }
  const url::Origin origin = url::Origin::Create(url_);

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_script_loader;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_subresource_loader;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_for_script_loader.InitWithNewPipeAndPassReceiver());
    coep_reporter_->Clone(
        coep_reporter_for_subresource_loader.InitWithNewPipeAndPassReceiver());
  }
  // Use the default CrossOriginEmbedderPolicy if
  // |cross_origin_embedder_policy_| is nullopt. It's acceptable because the
  // factory bundles are updated with correct COEP value before any subresource
  // requests in that case.
  auto script_bundle = EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
      rph, worker_route_id_, origin,
      cross_origin_embedder_policy_ ? cross_origin_embedder_policy_.value()
                                    : network::CrossOriginEmbedderPolicy(),
      std::move(coep_reporter_for_script_loader),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript);
  auto subresource_bundle = EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
      rph, worker_route_id_, origin,
      cross_origin_embedder_policy_ ? cross_origin_embedder_policy_.value()
                                    : network::CrossOriginEmbedderPolicy(),
      std::move(coep_reporter_for_subresource_loader),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource);

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    UpdateLoaderFactoriesOnCoreThread(context_weak_, version_id_,
                                      std::move(script_bundle),
                                      std::move(subresource_bundle));
    std::move(callback).Run();
  } else {
    base::PostTaskAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&UpdateLoaderFactoriesOnCoreThread, context_weak_,
                       version_id_, std::move(script_bundle),
                       std::move(subresource_bundle)),
        std::move(callback));
  }
}

}  // namespace content
