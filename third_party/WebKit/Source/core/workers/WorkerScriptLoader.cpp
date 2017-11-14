/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/WorkerScriptLoader.h"

#include <memory>
#include "core/dom/ExecutionContext.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/AllowedByNosniff.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/NetworkUtils.h"
#include "platform/network/http_names.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebAddressSpace.h"

namespace blink {

WorkerScriptLoader::WorkerScriptLoader()
    : response_address_space_(kWebAddressSpacePublic) {}

WorkerScriptLoader::~WorkerScriptLoader() {
  // If |m_threadableLoader| is still working, we have to cancel it here.
  // Otherwise WorkerScriptLoader::didFail() of the deleted |this| will be
  // called from DocumentThreadableLoader::notifyFinished() when the frame
  // will be destroyed.
  if (need_to_cancel_)
    Cancel();
}

void WorkerScriptLoader::LoadSynchronously(
    ExecutionContext& execution_context,
    const KURL& url,
    WebURLRequest::RequestContext request_context,
    WebAddressSpace creation_address_space) {
  url_ = url;
  execution_context_ = &execution_context;

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::GET);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      creation_address_space);
  request.SetRequestContext(request_context);

  SECURITY_DCHECK(execution_context.IsWorkerGlobalScope());

  ThreadableLoaderOptions options;

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.parser_disposition =
      ParserDisposition::kNotParserInserted;

  WorkerThreadableLoader::LoadResourceSynchronously(
      ToWorkerGlobalScope(execution_context), request, *this, options,
      resource_loader_options);
}

void WorkerScriptLoader::LoadAsynchronously(
    ExecutionContext& execution_context,
    const KURL& url,
    WebURLRequest::RequestContext request_context,
    network::mojom::FetchRequestMode fetch_request_mode,
    network::mojom::FetchCredentialsMode fetch_credentials_mode,
    WebAddressSpace creation_address_space,
    WTF::Closure response_callback,
    WTF::Closure finished_callback) {
  DCHECK(response_callback || finished_callback);
  response_callback_ = std::move(response_callback);
  finished_callback_ = std::move(finished_callback);
  url_ = url;
  execution_context_ = &execution_context;

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::GET);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      creation_address_space);
  request.SetRequestContext(request_context);
  request.SetFetchRequestMode(fetch_request_mode);
  request.SetFetchCredentialsMode(fetch_credentials_mode);

  ThreadableLoaderOptions options;

  ResourceLoaderOptions resource_loader_options;

  // During create, callbacks may happen which could remove the last reference
  // to this object, while some of the callchain assumes that the client and
  // loader wouldn't be deleted within callbacks.
  // (E.g. see crbug.com/524694 for why we can't easily remove this protect)
  scoped_refptr<WorkerScriptLoader> protect(this);
  need_to_cancel_ = true;
  threadable_loader_ = ThreadableLoader::Create(
      execution_context, this, options, resource_loader_options);
  threadable_loader_->Start(request);
  if (failed_)
    NotifyFinished();
}

const KURL& WorkerScriptLoader::ResponseURL() const {
  DCHECK(!Failed());
  return response_url_;
}

void WorkerScriptLoader::DidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  if (response.HttpStatusCode() / 100 != 2 && response.HttpStatusCode()) {
    NotifyError();
    return;
  }
  if (!AllowedByNosniff::MimeTypeAsScript(execution_context_, response)) {
    NotifyError();
    return;
  }
  identifier_ = identifier;
  response_url_ = response.Url();
  response_encoding_ = response.TextEncodingName();
  app_cache_id_ = response.AppCacheID();

  referrer_policy_ = response.HttpHeaderField(HTTPNames::Referrer_Policy);
  ProcessContentSecurityPolicy(response);
  origin_trial_tokens_ = OriginTrialContext::ParseHeaderValue(
      response.HttpHeaderField(HTTPNames::Origin_Trial));

  if (NetworkUtils::IsReservedIPAddress(response.RemoteIPAddress())) {
    response_address_space_ =
        SecurityOrigin::Create(response_url_)->IsLocalhost()
            ? kWebAddressSpaceLocal
            : kWebAddressSpacePrivate;
  }

  if (response_callback_)
    std::move(response_callback_).Run();
}

void WorkerScriptLoader::DidReceiveData(const char* data, unsigned len) {
  if (failed_)
    return;

  if (!decoder_) {
    if (!response_encoding_.IsEmpty()) {
      decoder_ = TextResourceDecoder::Create(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent,
          WTF::TextEncoding(response_encoding_)));
    } else {
      decoder_ = TextResourceDecoder::Create(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent, UTF8Encoding()));
    }
  }

  if (!len)
    return;

  source_text_.Append(decoder_->Decode(data, len));
}

void WorkerScriptLoader::DidReceiveCachedMetadata(const char* data, int size) {
  cached_metadata_ = std::make_unique<Vector<char>>(size);
  memcpy(cached_metadata_->data(), data, size);
}

void WorkerScriptLoader::DidFinishLoading(unsigned long identifier, double) {
  need_to_cancel_ = false;
  if (!failed_ && decoder_)
    source_text_.Append(decoder_->Flush());

  NotifyFinished();
}

void WorkerScriptLoader::DidFail(const ResourceError& error) {
  need_to_cancel_ = false;
  canceled_ = error.IsCancellation();
  NotifyError();
}

void WorkerScriptLoader::DidFailRedirectCheck() {
  // When didFailRedirectCheck() is called, the ResourceLoader for the script
  // is not canceled yet. So we don't reset |m_needToCancel| here.
  NotifyError();
}

void WorkerScriptLoader::Cancel() {
  need_to_cancel_ = false;
  if (threadable_loader_)
    threadable_loader_->Cancel();
}

String WorkerScriptLoader::SourceText() {
  return source_text_.ToString();
}

void WorkerScriptLoader::NotifyError() {
  failed_ = true;
  // notifyError() could be called before ThreadableLoader::create() returns
  // e.g. from didFail(), and in that case m_threadableLoader is not yet set
  // (i.e. still null).
  // Since the callback invocation in notifyFinished() potentially delete
  // |this| object, the callback invocation should be postponed until the
  // create() call returns. See loadAsynchronously() for the postponed call.
  if (threadable_loader_)
    NotifyFinished();
}

void WorkerScriptLoader::NotifyFinished() {
  if (!finished_callback_)
    return;

  std::move(finished_callback_).Run();
}

void WorkerScriptLoader::ProcessContentSecurityPolicy(
    const ResourceResponse& response) {
  // Per http://www.w3.org/TR/CSP2/#processing-model-workers, if the Worker's
  // URL is not a GUID, then it grabs its CSP from the response headers
  // directly.  Otherwise, the Worker inherits the policy from the parent
  // document (which is implemented in WorkerMessagingProxy, and
  // m_contentSecurityPolicy should be left as nullptr to inherit the policy).
  if (!response.Url().ProtocolIs("blob") &&
      !response.Url().ProtocolIs("file") &&
      !response.Url().ProtocolIs("filesystem")) {
    content_security_policy_ = ContentSecurityPolicy::Create();
    content_security_policy_->SetOverrideURLForSelf(response.Url());
    content_security_policy_->DidReceiveHeaders(
        ContentSecurityPolicyResponseHeaders(response));
  }
}

}  // namespace blink
