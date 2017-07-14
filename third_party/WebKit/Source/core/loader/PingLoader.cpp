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

#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/fileapi/File.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/FormData.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebURLRequest.h"

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
  request.SetKeepalive(true);
  request.SetRequestContext(WebURLRequest::kRequestContextBeacon);
  beacon.Serialize(request);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name = FetchInitiatorTypeNames::beacon;
  params.SetCrossOriginAccessControl(
      frame->GetDocument()->GetSecurityOrigin(),
      WebURLRequest::kFetchCredentialsModeInclude);

  Resource* resource =
      RawResource::Fetch(params, frame->GetDocument()->Fetcher());
  if (resource && resource->GetStatus() != ResourceStatus::kLoadError) {
    frame->Client()->DidDispatchPingLoader(request.Url());
    return true;
  }

  return false;
}

}  // namespace

void PingLoader::LoadImage(LocalFrame* frame, const KURL& url) {
  ResourceRequest request(url);
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  request.SetKeepalive(true);
  request.SetRequestContext(WebURLRequest::kRequestContextPing);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name = FetchInitiatorTypeNames::ping;
  // TODO(mkwst): Reevaluate this.
  params.SetContentSecurityCheck(kDoNotCheckContentSecurityPolicy);

  Resource* resource =
      RawResource::Fetch(params, frame->GetDocument()->Fetcher());
  if (resource && resource->GetStatus() != ResourceStatus::kLoadError)
    frame->Client()->DidDispatchPingLoader(request.Url());
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::SendLinkAuditPing(LocalFrame* frame,
                                   const KURL& ping_url,
                                   const KURL& destination_url) {
  if (!ping_url.ProtocolIsInHTTPFamily())
    return;

  ResourceRequest request(ping_url);
  request.SetHTTPMethod(HTTPNames::POST);
  request.SetHTTPContentType("text/ping");
  request.SetHTTPBody(EncodedFormData::Create("PING"));
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  request.SetHTTPHeaderField(HTTPNames::Ping_To,
                             AtomicString(destination_url.GetString()));
  RefPtr<SecurityOrigin> ping_origin = SecurityOrigin::Create(ping_url);
  if (ProtocolIs(frame->GetDocument()->Url().GetString(), "http") ||
      frame->GetDocument()->GetSecurityOrigin()->CanAccess(ping_origin.Get())) {
    request.SetHTTPHeaderField(
        HTTPNames::Ping_From,
        AtomicString(frame->GetDocument()->Url().GetString()));
  }

  request.SetKeepalive(true);
  request.SetHTTPReferrer(
      Referrer(Referrer::NoReferrer(), kReferrerPolicyNever));
  request.SetRequestContext(WebURLRequest::kRequestContextPing);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name = FetchInitiatorTypeNames::ping;

  Resource* resource =
      RawResource::Fetch(params, frame->GetDocument()->Fetcher());
  if (resource && resource->GetStatus() != ResourceStatus::kLoadError)
    frame->Client()->DidDispatchPingLoader(request.Url());
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
  request.SetKeepalive(true);
  request.SetHTTPBody(std::move(report));
  request.SetFetchCredentialsMode(
      WebURLRequest::kFetchCredentialsModeSameOrigin);
  request.SetRequestContext(WebURLRequest::kRequestContextCSPReport);
  request.SetFetchRedirectMode(WebURLRequest::kFetchRedirectModeError);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name =
      FetchInitiatorTypeNames::violationreport;
  params.MutableOptions().security_origin =
      frame->GetDocument()->GetSecurityOrigin();

  Resource* resource =
      RawResource::Fetch(params, frame->GetDocument()->Fetcher());
  if (resource && resource->GetStatus() != ResourceStatus::kLoadError)
    frame->Client()->DidDispatchPingLoader(request.Url());
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
