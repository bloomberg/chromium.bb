// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WorkerShadowPage.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/web/WebSettings.h"

namespace blink {

WorkerShadowPage::WorkerShadowPage(Client* client)
    : web_view_(WebViewImpl::Create(nullptr, kWebPageVisibilityStateVisible)),
      main_frame_(WebLocalFrameImpl::CreateMainFrame(web_view_,
                                                     this,
                                                     nullptr,
                                                     nullptr,
                                                     g_empty_atom,
                                                     WebSandboxFlags::kNone)),
      client_(client) {
  DCHECK(IsMainThread());
  // TODO(http://crbug.com/363843): This needs to find a better way to
  // not create graphics layers.
  web_view_->GetSettings()->SetAcceleratedCompositingEnabled(false);

  main_frame_->SetDevToolsAgentClient(client_);

  // Create an empty InterfaceProvider because WebFrameClient subclasses are
  // required to do it even if it's not used.
  // See https://chromium-review.googlesource.com/c/576370
  service_manager::mojom::InterfaceProviderPtr provider;
  mojo::MakeRequest(&provider);
  interface_provider_.Bind(std::move(provider));
}

WorkerShadowPage::~WorkerShadowPage() {
  DCHECK(IsMainThread());
  // Detach the client before closing the view to avoid getting called back.
  main_frame_->SetClient(nullptr);
  web_view_->Close();
  main_frame_->Close();
}

void WorkerShadowPage::Initialize(const KURL& script_url) {
  DCHECK(IsMainThread());
  AdvanceState(State::kInitializing);

  // Construct substitute data source. We only need it to have same origin as
  // the worker so the loading checks work correctly.
  CString content("");
  RefPtr<SharedBuffer> buffer(
      SharedBuffer::Create(content.data(), content.length()));
  main_frame_->GetFrame()->Loader().Load(
      FrameLoadRequest(0, ResourceRequest(script_url), SubstituteData(buffer)));
}

void WorkerShadowPage::SetContentSecurityPolicyAndReferrerPolicy(
    ContentSecurityPolicyResponseHeaders csp_headers,
    String referrer_policy) {
  DCHECK(IsMainThread());
  Document* document = main_frame_->GetFrame()->GetDocument();
  ContentSecurityPolicy* content_security_policy =
      ContentSecurityPolicy::Create();
  content_security_policy->SetOverrideURLForSelf(document->Url());
  content_security_policy->DidReceiveHeaders(csp_headers);
  document->InitContentSecurityPolicy(content_security_policy);
  if (!referrer_policy.IsNull())
    document->ParseAndSetReferrerPolicy(referrer_policy);
}

void WorkerShadowPage::DidFinishDocumentLoad() {
  DCHECK(IsMainThread());
  AdvanceState(State::kInitialized);
  client_->OnShadowPageInitialized();
}

std::unique_ptr<WebApplicationCacheHost>
WorkerShadowPage::CreateApplicationCacheHost(
    WebApplicationCacheHostClient* appcache_host_client) {
  DCHECK(IsMainThread());
  return client_->CreateApplicationCacheHost(appcache_host_client);
}

void WorkerShadowPage::FrameDetached(WebLocalFrame* frame, DetachType type) {
  DCHECK(IsMainThread());
  DCHECK(type == DetachType::kRemove && frame->Parent());
  DCHECK(frame->FrameWidget());
  frame->Close();
}

service_manager::InterfaceProvider* WorkerShadowPage::GetInterfaceProvider() {
  DCHECK(IsMainThread());
  return &interface_provider_;
}

std::unique_ptr<blink::WebURLLoader> WorkerShadowPage::CreateURLLoader(
    const WebURLRequest& request,
    SingleThreadTaskRunner* task_runner) {
  DCHECK(IsMainThread());
  // TODO(yhirano): Stop using Platform::CreateURLLoader() here.
  return Platform::Current()->CreateURLLoader(request, task_runner);
}

bool WorkerShadowPage::WasInitialized() const {
  return state_ == State::kInitialized;
}

void WorkerShadowPage::AdvanceState(State new_state) {
  switch (new_state) {
    case State::kUninitialized:
      NOTREACHED();
      return;
    case State::kInitializing:
      DCHECK_EQ(State::kUninitialized, state_);
      state_ = new_state;
      return;
    case State::kInitialized:
      DCHECK_EQ(State::kInitializing, state_);
      state_ = new_state;
      return;
  }
}

}  // namespace blink
