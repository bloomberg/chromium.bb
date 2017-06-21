/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/loader/PingLoader.h"

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/fileapi/File.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/FormData.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/WebFrameScheduler.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

namespace {

class Beacon {
  STACK_ALLOCATED();

 public:
  virtual void Serialize(ResourceRequest&) const = 0;
  virtual unsigned long long size() const = 0;
  virtual const AtomicString GetContentType() const = 0;
};

class BeaconString final : public Beacon {
 public:
  explicit BeaconString(const String& data) : data_(data) {}

  unsigned long long size() const override {
    return data_.CharactersSizeInBytes();
  }

  void Serialize(ResourceRequest& request) const override {
    RefPtr<EncodedFormData> entity_body = EncodedFormData::Create(data_.Utf8());
    request.SetHTTPBody(entity_body);
    request.SetHTTPContentType(GetContentType());
  }

  const AtomicString GetContentType() const {
    return AtomicString("text/plain;charset=UTF-8");
  }

 private:
  const String data_;
};

class BeaconBlob final : public Beacon {
 public:
  explicit BeaconBlob(Blob* data) : data_(data) {
    const String& blob_type = data_->type();
    if (!blob_type.IsEmpty() && ParsedContentType(blob_type).IsValid())
      content_type_ = AtomicString(blob_type);
  }

  unsigned long long size() const override { return data_->size(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    RefPtr<EncodedFormData> entity_body = EncodedFormData::Create();
    if (data_->HasBackingFile())
      entity_body->AppendFile(ToFile(data_)->GetPath());
    else
      entity_body->AppendBlob(data_->Uuid(), data_->GetBlobDataHandle());

    request.SetHTTPBody(std::move(entity_body));

    if (!content_type_.IsEmpty())
      request.SetHTTPContentType(content_type_);
  }

  const AtomicString GetContentType() const { return content_type_; }

 private:
  const Member<Blob> data_;
  AtomicString content_type_;
};

class BeaconDOMArrayBufferView final : public Beacon {
 public:
  explicit BeaconDOMArrayBufferView(DOMArrayBufferView* data) : data_(data) {}

  unsigned long long size() const override { return data_->byteLength(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    RefPtr<EncodedFormData> entity_body =
        EncodedFormData::Create(data_->BaseAddress(), data_->byteLength());
    request.SetHTTPBody(std::move(entity_body));

    // FIXME: a reasonable choice, but not in the spec; should it give a
    // default?
    request.SetHTTPContentType(AtomicString("application/octet-stream"));
  }

  const AtomicString GetContentType() const { return g_null_atom; }

 private:
  const Member<DOMArrayBufferView> data_;
};

class BeaconFormData final : public Beacon {
 public:
  explicit BeaconFormData(FormData* data)
      : data_(data), entity_body_(data_->EncodeMultiPartFormData()) {
    content_type_ = AtomicString("multipart/form-data; boundary=") +
                    entity_body_->Boundary().data();
  }

  unsigned long long size() const override {
    return entity_body_->SizeInBytes();
  }

  void Serialize(ResourceRequest& request) const override {
    request.SetHTTPBody(entity_body_.Get());
    request.SetHTTPContentType(content_type_);
  }

  const AtomicString GetContentType() const { return content_type_; }

 private:
  const Member<FormData> data_;
  RefPtr<EncodedFormData> entity_body_;
  AtomicString content_type_;
};

class PingLoaderImpl : public GarbageCollectedFinalized<PingLoaderImpl>,
                       public ContextClient,
                       private WebURLLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(PingLoaderImpl);
  WTF_MAKE_NONCOPYABLE(PingLoaderImpl);

 public:
  PingLoaderImpl(LocalFrame*, ResourceRequest&, const AtomicString&);
  ~PingLoaderImpl() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  void Dispose();

  // WebURLLoaderClient
  bool WillFollowRedirect(WebURLRequest&, const WebURLResponse&) override;
  void DidReceiveResponse(const WebURLResponse&) final;
  void DidReceiveData(const char*, int) final;
  void DidFinishLoading(double,
                        int64_t encoded_data_length,
                        int64_t encoded_body_length,
                        int64_t decoded_body_length) final;
  void DidFail(const WebURLError&,
               int64_t encoded_data_length,
               int64_t encoded_body_length,
               int64_t decoded_body_length) final;

  void Timeout(TimerBase*);

  void DidFailLoading(LocalFrame*);

  std::unique_ptr<WebURLLoader> loader_;
  Timer<PingLoaderImpl> timeout_;
  String url_;
  unsigned long identifier_;
  SelfKeepAlive<PingLoaderImpl> keep_alive_;
  AtomicString initiator_;

  RefPtr<SecurityOrigin> origin_;
  bool cors_enabled_;
};

PingLoaderImpl::PingLoaderImpl(LocalFrame* frame,
                               ResourceRequest& request,
                               const AtomicString& initiator)
    : ContextClient(frame),
      timeout_(this, &PingLoaderImpl::Timeout),
      url_(request.Url()),
      identifier_(CreateUniqueIdentifier()),
      keep_alive_(this),
      initiator_(initiator),
      origin_(frame->GetDocument()->GetSecurityOrigin()),
      cors_enabled_(true) {
  const AtomicString content_type = request.HttpContentType();
  if (!content_type.IsNull() && FetchUtils::IsCORSSafelistedHeader(
                                    AtomicString("content-type"), content_type))
    cors_enabled_ = false;

  frame->Loader().Client()->DidDispatchPingLoader(request.Url());

  FetchContext& fetch_context = frame->GetDocument()->Fetcher()->Context();

  fetch_context.PrepareRequest(request,
                               FetchContext::RedirectType::kNotForRedirect);
  fetch_context.RecordLoadingActivity(identifier_, request, Resource::kImage,
                                      initiator);

  FetchInitiatorInfo initiator_info;
  initiator_info.name = initiator;
  fetch_context.DispatchWillSendRequest(identifier_, request,
                                        ResourceResponse(), initiator_info);

  // Make sure the scheduler doesn't wait for the ping.
  if (frame->FrameScheduler())
    frame->FrameScheduler()->DidStopLoading(identifier_);

  loader_ = fetch_context.CreateURLLoader(request);
  DCHECK(loader_);

  WrappedResourceRequest wrapped_request(request);

  bool allow_stored_credentials = false;
  switch (request.GetFetchCredentialsMode()) {
    case WebURLRequest::kFetchCredentialsModeOmit:
      NOTREACHED();
      break;
    case WebURLRequest::kFetchCredentialsModeSameOrigin:
      allow_stored_credentials =
          frame->GetDocument()->GetSecurityOrigin()->CanRequestNoSuborigin(
              request.Url());
      break;
    case WebURLRequest::kFetchCredentialsModeInclude:
      allow_stored_credentials = true;
      break;
    case WebURLRequest::kFetchCredentialsModePassword:
      NOTREACHED();
      break;
  }
  request.SetAllowStoredCredentials(allow_stored_credentials);

  loader_->LoadAsynchronously(wrapped_request, this);

  // If the server never responds, FrameLoader won't be able to cancel this load
  // and we'll sit here waiting forever. Set a very generous timeout, just in
  // case.
  timeout_.StartOneShot(60000, BLINK_FROM_HERE);
}

PingLoaderImpl::~PingLoaderImpl() {
  DCHECK(!loader_);
}

void PingLoaderImpl::Dispose() {
  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }
  timeout_.Stop();
  keep_alive_.Clear();
}

bool PingLoaderImpl::WillFollowRedirect(
    WebURLRequest& passed_new_request,
    const WebURLResponse& passed_redirect_response) {
  if (cors_enabled_) {
    DCHECK(passed_new_request.AllowStoredCredentials());

    ResourceRequest& new_request(passed_new_request.ToMutableResourceRequest());
    const ResourceResponse& redirect_response(
        passed_redirect_response.ToResourceResponse());

    DCHECK(!new_request.IsNull());
    DCHECK(!redirect_response.IsNull());

    String error_description;
    ResourceLoaderOptions options;
    // TODO(tyoshino): Save updated data in options.security_origin and pass it
    // on the next time.
    if (!CrossOriginAccessControl::HandleRedirect(
            origin_, new_request, redirect_response,
            passed_new_request.GetFetchCredentialsMode(), options,
            error_description)) {
      if (GetFrame()) {
        if (GetFrame()->GetDocument()) {
          GetFrame()->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
              kJSMessageSource, kErrorMessageLevel, error_description));
        }
      }
      // Cancel the load and self destruct.
      Dispose();

      return false;
    }
  }
  // FIXME: http://crbug.com/427429 is needed to correctly propagate updates of
  // Origin: following this successful redirect.

  if (GetFrame() && GetFrame()->GetDocument()) {
    FetchInitiatorInfo initiator_info;
    initiator_info.name = initiator_;
    FetchContext& fetch_context =
        GetFrame()->GetDocument()->Fetcher()->Context();
    fetch_context.PrepareRequest(passed_new_request.ToMutableResourceRequest(),
                                 FetchContext::RedirectType::kForRedirect);
    fetch_context.DispatchWillSendRequest(
        identifier_, passed_new_request.ToMutableResourceRequest(),
        passed_redirect_response.ToResourceResponse(), initiator_info);
  }

  return true;
}

void PingLoaderImpl::DidReceiveResponse(const WebURLResponse& response) {
  if (GetFrame()) {
    const ResourceResponse& resource_response = response.ToResourceResponse();
    probe::didReceiveResourceResponse(GetFrame(), identifier_, 0,
                                      resource_response, 0);
    DidFailLoading(GetFrame());
  }
  Dispose();
}

void PingLoaderImpl::DidReceiveData(const char*, int data_length) {
  if (GetFrame())
    DidFailLoading(GetFrame());
  Dispose();
}

void PingLoaderImpl::DidFinishLoading(double,
                                      int64_t encoded_data_length,
                                      int64_t encoded_body_length,
                                      int64_t decoded_body_length) {
  if (GetFrame())
    DidFailLoading(GetFrame());
  Dispose();
}

void PingLoaderImpl::DidFail(const WebURLError& resource_error,
                             int64_t encoded_data_length,
                             int64_t encoded_body_length,
                             int64_t decoded_body_length) {
  if (GetFrame())
    DidFailLoading(GetFrame());
  Dispose();
}

void PingLoaderImpl::Timeout(TimerBase*) {
  if (GetFrame())
    DidFailLoading(GetFrame());
  Dispose();
}

void PingLoaderImpl::DidFailLoading(LocalFrame* frame) {
  probe::didFailLoading(frame, identifier_,
                        ResourceError::CancelledError(url_));
  frame->Console().DidFailLoading(identifier_,
                                  ResourceError::CancelledError(url_));
}

DEFINE_TRACE(PingLoaderImpl) {
  ContextClient::Trace(visitor);
}

void FinishPingRequestInitialization(
    ResourceRequest& request,
    LocalFrame* frame,
    WebURLRequest::RequestContext request_context) {
  request.SetRequestContext(request_context);
  FetchContext& fetch_context = frame->GetDocument()->Fetcher()->Context();
  fetch_context.AddAdditionalRequestHeaders(request, kFetchSubresource);
  // TODO(tyoshino): Call populateResourceRequest() if appropriate.
  fetch_context.SetFirstPartyCookieAndRequestorOrigin(request);
}

bool SendPingCommon(LocalFrame* frame,
                    ResourceRequest& request,
                    const AtomicString& initiator) {
  request.SetKeepalive(true);
  if (MixedContentChecker::ShouldBlockFetch(frame, request, request.Url()))
    return false;

  // The loader keeps itself alive until it receives a response and disposes
  // itself.
  PingLoaderImpl* loader = new PingLoaderImpl(frame, request, initiator);
  DCHECK(loader);

  return true;
}

// Decide if a beacon with the given size is allowed to go ahead
// given some overall allowance limit.
bool AllowBeaconWithSize(int allowance, unsigned long long size) {
  // If a negative allowance is supplied, no size constraint is imposed.
  if (allowance < 0)
    return true;

  if (static_cast<unsigned long long>(allowance) < size)
    return false;

  return true;
}

bool SendBeaconCommon(LocalFrame* frame,
                      int allowance,
                      const KURL& url,
                      const Beacon& beacon,
                      size_t& beacon_size) {
  if (!frame->GetDocument())
    return false;

  // TODO(mkwst): CSP is not enforced on redirects, crbug.com/372197
  if (!ContentSecurityPolicy::ShouldBypassMainWorld(frame->GetDocument()) &&
      !frame->GetDocument()->GetContentSecurityPolicy()->AllowConnectToSource(
          url)) {
    // We're simulating a network failure here, so we return 'true'.
    return true;
  }

  unsigned long long size = beacon.size();
  if (!AllowBeaconWithSize(allowance, size))
    return false;

  beacon_size = size;

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::POST);
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  FinishPingRequestInitialization(request, frame,
                                  WebURLRequest::kRequestContextBeacon);

  beacon.Serialize(request);

  return SendPingCommon(frame, request, FetchInitiatorTypeNames::beacon);
}

}  // namespace

void PingLoader::LoadImage(LocalFrame* frame, const KURL& url) {
  ResourceRequest request(url);
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  FinishPingRequestInitialization(request, frame,
                                  WebURLRequest::kRequestContextPing);

  SendPingCommon(frame, request, FetchInitiatorTypeNames::ping);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::SendLinkAuditPing(LocalFrame* frame,
                                   const KURL& ping_url,
                                   const KURL& destination_url) {
  if (!ping_url.ProtocolIsInHTTPFamily())
    return;

  if (ContentSecurityPolicy* policy =
          frame->GetSecurityContext()->GetContentSecurityPolicy()) {
    if (!policy->AllowConnectToSource(ping_url))
      return;
  }

  ResourceRequest request(ping_url);
  request.SetHTTPMethod(HTTPNames::POST);
  request.SetHTTPContentType("text/ping");
  request.SetHTTPBody(EncodedFormData::Create("PING"));
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  FinishPingRequestInitialization(request, frame,
                                  WebURLRequest::kRequestContextPing);

  // addAdditionalRequestHeaders() will have added a referrer for same origin
  // requests, but the spec omits the referrer.
  request.ClearHTTPReferrer();

  request.SetHTTPHeaderField(HTTPNames::Ping_To,
                             AtomicString(destination_url.GetString()));

  RefPtr<SecurityOrigin> ping_origin = SecurityOrigin::Create(ping_url);
  if (ProtocolIs(frame->GetDocument()->Url().GetString(), "http") ||
      frame->GetDocument()->GetSecurityOrigin()->CanAccess(ping_origin.Get())) {
    request.SetHTTPHeaderField(
        HTTPNames::Ping_From,
        AtomicString(frame->GetDocument()->Url().GetString()));
  }

  SendPingCommon(frame, request, FetchInitiatorTypeNames::ping);
}

void PingLoader::SendViolationReport(LocalFrame* frame,
                                     const KURL& report_url,
                                     PassRefPtr<EncodedFormData> report,
                                     ViolationReportType type) {
  ResourceRequest request(report_url);
  request.SetHTTPMethod(HTTPNames::POST);
  switch (type) {
    case kContentSecurityPolicyViolationReport:
      request.SetHTTPContentType("application/csp-report");
      break;
    case kXSSAuditorViolationReport:
      request.SetHTTPContentType("application/xss-auditor-report");
      break;
  }
  request.SetHTTPBody(std::move(report));
  request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeSameOrigin);
  FinishPingRequestInitialization(request, frame,
                                  WebURLRequest::kRequestContextCSPReport);

  SendPingCommon(frame, request, FetchInitiatorTypeNames::violationreport);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beacon_url,
                            const String& data,
                            size_t& beacon_size) {
  BeaconString beacon(data);
  return SendBeaconCommon(frame, allowance, beacon_url, beacon, beacon_size);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beacon_url,
                            DOMArrayBufferView* data,
                            size_t& beacon_size) {
  BeaconDOMArrayBufferView beacon(data);
  return SendBeaconCommon(frame, allowance, beacon_url, beacon, beacon_size);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beacon_url,
                            FormData* data,
                            size_t& beacon_size) {
  BeaconFormData beacon(data);
  return SendBeaconCommon(frame, allowance, beacon_url, beacon, beacon_size);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beacon_url,
                            Blob* data,
                            size_t& beacon_size) {
  BeaconBlob beacon(data);
  return SendBeaconCommon(frame, allowance, beacon_url, beacon, beacon_size);
}

}  // namespace blink
