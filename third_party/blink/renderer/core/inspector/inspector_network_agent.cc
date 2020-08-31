/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/network_resources_data.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/inspector_protocol/crdtp/json.h"

using crdtp::SpanFrom;
using crdtp::json::ConvertCBORToJSON;

namespace blink {

using GetRequestPostDataCallback =
    protocol::Network::Backend::GetRequestPostDataCallback;
using GetResponseBodyCallback =
    protocol::Network::Backend::GetResponseBodyCallback;
using protocol::Response;

namespace {

#if defined(OS_ANDROID)
constexpr int kDefaultTotalBufferSize = 10 * 1000 * 1000;    // 10 MB
constexpr int kDefaultResourceBufferSize = 5 * 1000 * 1000;  // 5 MB
#else
constexpr int kDefaultTotalBufferSize = 100 * 1000 * 1000;    // 100 MB
constexpr int kDefaultResourceBufferSize = 10 * 1000 * 1000;  // 10 MB
#endif

// Pattern may contain stars ('*') which match to any (possibly empty) string.
// Stars implicitly assumed at the begin/end of pattern.
bool Matches(const String& url, const String& pattern) {
  Vector<String> parts;
  pattern.Split("*", parts);
  wtf_size_t pos = 0;
  for (const String& part : parts) {
    pos = url.Find(part, pos);
    if (pos == kNotFound)
      return false;
    pos += part.length();
  }
  return true;
}

bool LoadsFromCacheOnly(const ResourceRequest& request) {
  switch (request.GetCacheMode()) {
    case mojom::FetchCacheMode::kDefault:
    case mojom::FetchCacheMode::kNoStore:
    case mojom::FetchCacheMode::kValidateCache:
    case mojom::FetchCacheMode::kBypassCache:
    case mojom::FetchCacheMode::kForceCache:
      return false;
    case mojom::FetchCacheMode::kOnlyIfCached:
    case mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
    case mojom::FetchCacheMode::kUnspecifiedForceCacheMiss:
      return true;
  }
  NOTREACHED();
  return false;
}

protocol::Network::CertificateTransparencyCompliance
SerializeCTPolicyCompliance(
    ResourceResponse::CTPolicyCompliance ct_compliance) {
  switch (ct_compliance) {
    case ResourceResponse::kCTPolicyComplianceDetailsNotAvailable:
      return protocol::Network::CertificateTransparencyComplianceEnum::Unknown;
    case ResourceResponse::kCTPolicyComplies:
      return protocol::Network::CertificateTransparencyComplianceEnum::
          Compliant;
    case ResourceResponse::kCTPolicyDoesNotComply:
      return protocol::Network::CertificateTransparencyComplianceEnum::
          NotCompliant;
  }
  NOTREACHED();
  return protocol::Network::CertificateTransparencyComplianceEnum::Unknown;
}

static std::unique_ptr<protocol::Network::Headers> BuildObjectForHeaders(
    const HTTPHeaderMap& headers) {
  std::unique_ptr<protocol::DictionaryValue> headers_object =
      protocol::DictionaryValue::create();
  for (const auto& header : headers)
    headers_object->setString(header.key.GetString(), header.value);
  protocol::ErrorSupport errors;
  return protocol::Network::Headers::fromValue(headers_object.get(), &errors);
}

class InspectorFileReaderLoaderClient final : public FileReaderLoaderClient {
 public:
  InspectorFileReaderLoaderClient(
      scoped_refptr<BlobDataHandle> blob,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceCallback<void(scoped_refptr<SharedBuffer>)> callback)
      : blob_(std::move(blob)), callback_(std::move(callback)) {
    loader_ = std::make_unique<FileReaderLoader>(
        FileReaderLoader::kReadByClient, this, std::move(task_runner));
  }

  ~InspectorFileReaderLoaderClient() override = default;

  void Start() {
    raw_data_ = SharedBuffer::Create();
    loader_->Start(blob_);
  }

  void DidStartLoading() override {}

  void DidReceiveDataForClient(const char* data,
                               unsigned data_length) override {
    if (!data_length)
      return;
    raw_data_->Append(data, data_length);
  }

  void DidFinishLoading() override { Done(raw_data_); }

  void DidFail(FileErrorCode) override { Done(nullptr); }

 private:
  void Done(scoped_refptr<SharedBuffer> output) {
    std::move(callback_).Run(output);
    delete this;
  }

  scoped_refptr<BlobDataHandle> blob_;
  String mime_type_;
  String text_encoding_name_;
  base::OnceCallback<void(scoped_refptr<SharedBuffer>)> callback_;
  std::unique_ptr<FileReaderLoader> loader_;
  scoped_refptr<SharedBuffer> raw_data_;
  DISALLOW_COPY_AND_ASSIGN(InspectorFileReaderLoaderClient);
};

static void ResponseBodyFileReaderLoaderDone(
    const String& mime_type,
    const String& text_encoding_name,
    std::unique_ptr<GetResponseBodyCallback> callback,
    scoped_refptr<SharedBuffer> raw_data) {
  if (!raw_data) {
    callback->sendFailure(Response::ServerError("Couldn't read BLOB"));
    return;
  }
  String result;
  bool base64_encoded;
  if (InspectorPageAgent::SharedBufferContent(
          raw_data, mime_type, text_encoding_name, &result, &base64_encoded)) {
    callback->sendSuccess(result, base64_encoded);
  } else {
    callback->sendFailure(Response::ServerError("Couldn't encode data"));
  }
}

class InspectorPostBodyParser
    : public WTF::RefCounted<InspectorPostBodyParser> {
 public:
  InspectorPostBodyParser(
      std::unique_ptr<GetRequestPostDataCallback> callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : callback_(std::move(callback)),
        task_runner_(std::move(task_runner)),
        error_(false) {}

  void Parse(EncodedFormData* request_body) {
    if (!request_body || request_body->IsEmpty())
      return;

    parts_.Grow(request_body->Elements().size());
    for (wtf_size_t i = 0; i < request_body->Elements().size(); i++) {
      const FormDataElement& data = request_body->Elements()[i];
      switch (data.type_) {
        case FormDataElement::kData:
          parts_[i] = String::FromUTF8WithLatin1Fallback(data.data_.data(),
                                                         data.data_.size());
          break;
        case FormDataElement::kEncodedBlob:
          ReadDataBlob(data.optional_blob_data_handle_, &parts_[i]);
          break;
        case FormDataElement::kEncodedFile:
        case FormDataElement::kDataPipe:
          // Do nothing, not supported
          break;
      }
    }
  }

 private:
  friend class WTF::RefCounted<InspectorPostBodyParser>;

  ~InspectorPostBodyParser() {
    if (error_)
      return;
    StringBuilder result;
    for (const auto& part : parts_)
      result.Append(part);
    callback_->sendSuccess(result.ToString());
  }

  void BlobReadCallback(String* destination,
                        scoped_refptr<SharedBuffer> raw_data) {
    if (raw_data) {
      *destination = String::FromUTF8WithLatin1Fallback(raw_data->Data(),
                                                        raw_data->size());
    } else {
      error_ = true;
    }
  }

  void ReadDataBlob(scoped_refptr<blink::BlobDataHandle> blob_handle,
                    String* destination) {
    if (!blob_handle)
      return;
    auto* reader = new InspectorFileReaderLoaderClient(
        blob_handle, task_runner_,
        WTF::Bind(&InspectorPostBodyParser::BlobReadCallback,
                  WTF::RetainedRef(this), WTF::Unretained(destination)));
    reader->Start();
  }

  std::unique_ptr<GetRequestPostDataCallback> callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool error_;
  Vector<String> parts_;
  DISALLOW_COPY_AND_ASSIGN(InspectorPostBodyParser);
};

KURL UrlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.RemoveFragmentIdentifier();
  return result;
}

String MixedContentTypeForContextType(WebMixedContentContextType context_type) {
  switch (context_type) {
    case WebMixedContentContextType::kNotMixedContent:
      return protocol::Security::MixedContentTypeEnum::None;
    case WebMixedContentContextType::kBlockable:
      return protocol::Security::MixedContentTypeEnum::Blockable;
    case WebMixedContentContextType::kOptionallyBlockable:
    case WebMixedContentContextType::kShouldBeBlockable:
      return protocol::Security::MixedContentTypeEnum::OptionallyBlockable;
  }

  return protocol::Security::MixedContentTypeEnum::None;
}

String ResourcePriorityJSON(ResourceLoadPriority priority) {
  switch (priority) {
    case ResourceLoadPriority::kVeryLow:
      return protocol::Network::ResourcePriorityEnum::VeryLow;
    case ResourceLoadPriority::kLow:
      return protocol::Network::ResourcePriorityEnum::Low;
    case ResourceLoadPriority::kMedium:
      return protocol::Network::ResourcePriorityEnum::Medium;
    case ResourceLoadPriority::kHigh:
      return protocol::Network::ResourcePriorityEnum::High;
    case ResourceLoadPriority::kVeryHigh:
      return protocol::Network::ResourcePriorityEnum::VeryHigh;
    case ResourceLoadPriority::kUnresolved:
      break;
  }
  NOTREACHED();
  return protocol::Network::ResourcePriorityEnum::Medium;
}

String BuildBlockedReason(ResourceRequestBlockedReason reason) {
  switch (reason) {
    case ResourceRequestBlockedReason::kCSP:
      return protocol::Network::BlockedReasonEnum::Csp;
    case ResourceRequestBlockedReason::kMixedContent:
      return protocol::Network::BlockedReasonEnum::MixedContent;
    case ResourceRequestBlockedReason::kOrigin:
      return protocol::Network::BlockedReasonEnum::Origin;
    case ResourceRequestBlockedReason::kInspector:
      return protocol::Network::BlockedReasonEnum::Inspector;
    case ResourceRequestBlockedReason::kSubresourceFilter:
      return protocol::Network::BlockedReasonEnum::SubresourceFilter;
    case ResourceRequestBlockedReason::kContentType:
      return protocol::Network::BlockedReasonEnum::ContentType;
    case ResourceRequestBlockedReason::kOther:
      return protocol::Network::BlockedReasonEnum::Other;
    case ResourceRequestBlockedReason::kCollapsedByClient:
      return protocol::Network::BlockedReasonEnum::CollapsedByClient;
    case blink::ResourceRequestBlockedReason::kCoepFrameResourceNeedsCoepHeader:
      return protocol::Network::BlockedReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case blink::ResourceRequestBlockedReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Network::BlockedReasonEnum::
          CoopSandboxedIframeCannotNavigateToCoopPage;
    case blink::ResourceRequestBlockedReason::kCorpNotSameOrigin:
      return protocol::Network::BlockedReasonEnum::CorpNotSameOrigin;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case blink::ResourceRequestBlockedReason::kCorpNotSameSite:
      return protocol::Network::BlockedReasonEnum::CorpNotSameSite;
  }
  NOTREACHED();
  return protocol::Network::BlockedReasonEnum::Other;
}

WebConnectionType ToWebConnectionType(const String& connection_type) {
  if (connection_type == protocol::Network::ConnectionTypeEnum::None)
    return kWebConnectionTypeNone;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular2g)
    return kWebConnectionTypeCellular2G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular3g)
    return kWebConnectionTypeCellular3G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular4g)
    return kWebConnectionTypeCellular4G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Bluetooth)
    return kWebConnectionTypeBluetooth;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Ethernet)
    return kWebConnectionTypeEthernet;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Wifi)
    return kWebConnectionTypeWifi;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Wimax)
    return kWebConnectionTypeWimax;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Other)
    return kWebConnectionTypeOther;
  return kWebConnectionTypeUnknown;
}

String GetReferrerPolicy(network::mojom::ReferrerPolicy policy) {
  switch (policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return protocol::Network::Request::ReferrerPolicyEnum::UnsafeUrl;
    case network::mojom::ReferrerPolicy::kDefault:
      if (RuntimeEnabledFeatures::ReducedReferrerGranularityEnabled()) {
        return protocol::Network::Request::ReferrerPolicyEnum::
            StrictOriginWhenCrossOrigin;
      } else {
        return protocol::Network::Request::ReferrerPolicyEnum::
            NoReferrerWhenDowngrade;
      }
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return protocol::Network::Request::ReferrerPolicyEnum::
          NoReferrerWhenDowngrade;
    case network::mojom::ReferrerPolicy::kNever:
      return protocol::Network::Request::ReferrerPolicyEnum::NoReferrer;
    case network::mojom::ReferrerPolicy::kOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::Origin;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          OriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::SameOrigin;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::StrictOrigin;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          StrictOriginWhenCrossOrigin;
  }

  return protocol::Network::Request::ReferrerPolicyEnum::
      NoReferrerWhenDowngrade;
}

std::unique_ptr<protocol::Network::WebSocketFrame> WebSocketMessageToProtocol(
    int op_code,
    bool masked,
    const char* payload,
    size_t payload_length) {
  return protocol::Network::WebSocketFrame::create()
      .setOpcode(op_code)
      .setMask(masked)
      // Only interpret the payload as UTF-8 when it's a text message
      .setPayloadData(
          op_code == 1
              ? String::FromUTF8WithLatin1Fallback(payload, payload_length)
              : Base64Encode(
                    base::as_bytes(base::make_span(payload, payload_length))))
      .build();
}

}  // namespace

void InspectorNetworkAgent::Restore() {
  if (enabled_.Get())
    Enable();
}

static std::unique_ptr<protocol::Network::ResourceTiming> BuildObjectForTiming(
    const ResourceLoadTiming& timing) {
  return protocol::Network::ResourceTiming::create()
      .setRequestTime(timing.RequestTime().since_origin().InSecondsF())
      .setProxyStart(timing.CalculateMillisecondDelta(timing.ProxyStart()))
      .setProxyEnd(timing.CalculateMillisecondDelta(timing.ProxyEnd()))
      .setDnsStart(timing.CalculateMillisecondDelta(timing.DnsStart()))
      .setDnsEnd(timing.CalculateMillisecondDelta(timing.DnsEnd()))
      .setConnectStart(timing.CalculateMillisecondDelta(timing.ConnectStart()))
      .setConnectEnd(timing.CalculateMillisecondDelta(timing.ConnectEnd()))
      .setSslStart(timing.CalculateMillisecondDelta(timing.SslStart()))
      .setSslEnd(timing.CalculateMillisecondDelta(timing.SslEnd()))
      .setWorkerStart(timing.CalculateMillisecondDelta(timing.WorkerStart()))
      .setWorkerReady(timing.CalculateMillisecondDelta(timing.WorkerReady()))
      .setSendStart(timing.CalculateMillisecondDelta(timing.SendStart()))
      .setSendEnd(timing.CalculateMillisecondDelta(timing.SendEnd()))
      .setReceiveHeadersEnd(
          timing.CalculateMillisecondDelta(timing.ReceiveHeadersEnd()))
      .setPushStart(timing.PushStart().since_origin().InSecondsF())
      .setPushEnd(timing.PushEnd().since_origin().InSecondsF())
      .build();
}

static bool FormDataToString(scoped_refptr<EncodedFormData> body,
                             size_t max_body_size,
                             String* content) {
  *content = "";
  if (!body || body->IsEmpty())
    return false;

  // SizeInBytes below doesn't support all element types, so first check if all
  // the body elements are of the right type.
  for (const auto& element : body->Elements()) {
    if (element.type_ != FormDataElement::kData)
      return true;
  }

  if (max_body_size != 0 && body->SizeInBytes() > max_body_size)
    return true;

  Vector<char> bytes;
  body->Flatten(bytes);
  *content = String::FromUTF8WithLatin1Fallback(bytes.data(), bytes.size());
  return true;
}

static std::unique_ptr<protocol::Network::Request>
BuildObjectForResourceRequest(const ResourceRequest& request,
                              scoped_refptr<EncodedFormData> post_data,
                              size_t max_body_size) {
  String postData;
  bool hasPostData = FormDataToString(post_data, max_body_size, &postData);
  KURL url = request.Url();
  // protocol::Network::Request doesn't have a separate referrer string member
  // like blink::ResourceRequest, so here we add ResourceRequest's referrer
  // string to the protocol request's headers manually.
  auto headers = request.HttpHeaderFields();

  // The request's referrer must be generated at this point.
  DCHECK_NE(request.ReferrerString(), Referrer::ClientReferrerString());
  headers.Set(http_names::kReferer, AtomicString(request.ReferrerString()));

  std::unique_ptr<protocol::Network::Request> result =
      protocol::Network::Request::create()
          .setUrl(UrlWithoutFragment(url).GetString())
          .setMethod(request.HttpMethod())
          .setHeaders(BuildObjectForHeaders(headers))
          .setInitialPriority(ResourcePriorityJSON(request.Priority()))
          .setReferrerPolicy(GetReferrerPolicy(request.GetReferrerPolicy()))
          .build();
  if (url.FragmentIdentifier())
    result->setUrlFragment("#" + url.FragmentIdentifier());
  if (!postData.IsEmpty())
    result->setPostData(postData);
  if (hasPostData)
    result->setHasPostData(true);
  return result;
}

static std::unique_ptr<protocol::Network::Response>
BuildObjectForResourceResponse(const ResourceResponse& response,
                               const Resource* cached_resource = nullptr,
                               bool* is_empty = nullptr) {
  if (response.IsNull())
    return nullptr;

  int status;
  String status_text;
  if (response.GetResourceLoadInfo() &&
      response.GetResourceLoadInfo()->http_status_code) {
    status = response.GetResourceLoadInfo()->http_status_code;
    status_text = response.GetResourceLoadInfo()->http_status_text;
  } else {
    status = response.HttpStatusCode();
    status_text = response.HttpStatusText();
  }
  HTTPHeaderMap headers_map;
  if (response.GetResourceLoadInfo() &&
      response.GetResourceLoadInfo()->response_headers.size())
    headers_map = response.GetResourceLoadInfo()->response_headers;
  else
    headers_map = response.HttpHeaderFields();

  int64_t encoded_data_length = response.EncodedDataLength();

  String security_state = protocol::Security::SecurityStateEnum::Unknown;
  switch (response.GetSecurityStyle()) {
    case SecurityStyle::kUnknown:
      security_state = protocol::Security::SecurityStateEnum::Unknown;
      break;
    case SecurityStyle::kNeutral:
      security_state = protocol::Security::SecurityStateEnum::Neutral;
      break;
    case SecurityStyle::kInsecure:
      security_state = protocol::Security::SecurityStateEnum::Insecure;
      break;
    case SecurityStyle::kSecure:
      security_state = protocol::Security::SecurityStateEnum::Secure;
      break;
    case SecurityStyle::kInsecureBroken:
      security_state = protocol::Security::SecurityStateEnum::InsecureBroken;
      break;
  }

  // Use mime type from cached resource in case the one in response is empty.
  String mime_type = response.MimeType();
  if (mime_type.IsEmpty() && cached_resource)
    mime_type = cached_resource->GetResponse().MimeType();

  if (is_empty)
    *is_empty = !status && mime_type.IsEmpty() && !headers_map.size();

  std::unique_ptr<protocol::Network::Response> response_object =
      protocol::Network::Response::create()
          .setUrl(UrlWithoutFragment(response.CurrentRequestUrl()).GetString())
          .setStatus(status)
          .setStatusText(status_text)
          .setHeaders(BuildObjectForHeaders(headers_map))
          .setMimeType(mime_type)
          .setConnectionReused(response.ConnectionReused())
          .setConnectionId(response.ConnectionID())
          .setEncodedDataLength(encoded_data_length)
          .setSecurityState(security_state)
          .build();

  response_object->setFromDiskCache(response.WasCached());
  response_object->setFromServiceWorker(response.WasFetchedViaServiceWorker());
  response_object->setFromPrefetchCache(response.WasInPrefetchCache());
  if (response.GetResourceLoadTiming())
    response_object->setTiming(
        BuildObjectForTiming(*response.GetResourceLoadTiming()));

  if (response.GetResourceLoadInfo()) {
    if (!response.GetResourceLoadInfo()->response_headers_text.IsEmpty()) {
      response_object->setHeadersText(
          response.GetResourceLoadInfo()->response_headers_text);
    }
    if (response.GetResourceLoadInfo()->request_headers.size()) {
      response_object->setRequestHeaders(BuildObjectForHeaders(
          response.GetResourceLoadInfo()->request_headers));
    }
    if (!response.GetResourceLoadInfo()->request_headers_text.IsEmpty()) {
      response_object->setRequestHeadersText(
          response.GetResourceLoadInfo()->request_headers_text);
    }
  }

  String remote_ip_address = response.RemoteIPAddress();
  if (!remote_ip_address.IsEmpty()) {
    response_object->setRemoteIPAddress(remote_ip_address);
    response_object->setRemotePort(response.RemotePort());
  }

  String protocol = response.AlpnNegotiatedProtocol();
  if (protocol.IsEmpty() || protocol == "unknown") {
    if (response.WasFetchedViaSPDY()) {
      protocol = "h2";
    } else if (response.IsHTTP()) {
      protocol = "http";
      if (response.HttpVersion() ==
          ResourceResponse::HTTPVersion::kHTTPVersion_0_9)
        protocol = "http/0.9";
      else if (response.HttpVersion() ==
               ResourceResponse::HTTPVersion::kHTTPVersion_1_0)
        protocol = "http/1.0";
      else if (response.HttpVersion() ==
               ResourceResponse::HTTPVersion::kHTTPVersion_1_1)
        protocol = "http/1.1";
    } else {
      protocol = response.CurrentRequestUrl().Protocol();
    }
  }
  response_object->setProtocol(protocol);

  const base::Optional<ResourceResponse::SecurityDetails>&
      response_security_details = response.GetSecurityDetails();
  if (response_security_details.has_value()) {
    auto san_list = std::make_unique<protocol::Array<String>>(
        response_security_details->san_list.begin(),
        response_security_details->san_list.end());

    auto signed_certificate_timestamp_list = std::make_unique<
        protocol::Array<protocol::Network::SignedCertificateTimestamp>>();
    for (auto const& sct : response_security_details->sct_list) {
      std::unique_ptr<protocol::Network::SignedCertificateTimestamp>
          signed_certificate_timestamp =
              protocol::Network::SignedCertificateTimestamp::create()
                  .setStatus(sct.status_)
                  .setOrigin(sct.origin_)
                  .setLogDescription(sct.log_description_)
                  .setLogId(sct.log_id_)
                  .setTimestamp(sct.timestamp_)
                  .setHashAlgorithm(sct.hash_algorithm_)
                  .setSignatureAlgorithm(sct.signature_algorithm_)
                  .setSignatureData(sct.signature_data_)
                  .build();
      signed_certificate_timestamp_list->emplace_back(
          std::move(signed_certificate_timestamp));
    }

    std::unique_ptr<protocol::Network::SecurityDetails> security_details =
        protocol::Network::SecurityDetails::create()
            .setProtocol(response_security_details->protocol)
            .setKeyExchange(response_security_details->key_exchange)
            .setCipher(response_security_details->cipher)
            .setSubjectName(response_security_details->subject_name)
            .setSanList(std::move(san_list))
            .setIssuer(response_security_details->issuer)
            .setValidFrom(response_security_details->valid_from)
            .setValidTo(response_security_details->valid_to)
            .setCertificateId(0)  // Keep this in protocol for compatability.
            .setSignedCertificateTimestampList(
                std::move(signed_certificate_timestamp_list))
            .setCertificateTransparencyCompliance(
                SerializeCTPolicyCompliance(response.GetCTPolicyCompliance()))
            .build();
    if (response_security_details->key_exchange_group.length() > 0)
      security_details->setKeyExchangeGroup(
          response_security_details->key_exchange_group);
    if (response_security_details->mac.length() > 0)
      security_details->setMac(response_security_details->mac);

    response_object->setSecurityDetails(std::move(security_details));
  }

  return response_object;
}

InspectorNetworkAgent::~InspectorNetworkAgent() = default;

void InspectorNetworkAgent::Trace(Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(worker_global_scope_);
  visitor->Trace(resources_data_);
  visitor->Trace(replay_xhrs_);
  visitor->Trace(pending_xhr_replay_data_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorNetworkAgent::ShouldBlockRequest(const KURL& url, bool* result) {
  if (blocked_urls_.IsEmpty())
    return;

  String url_string = url.GetString();
  for (const String& blocked : blocked_urls_.Keys()) {
    if (Matches(url_string, blocked)) {
      *result = true;
      return;
    }
  }
}

void InspectorNetworkAgent::ShouldBypassServiceWorker(bool* result) {
  if (bypass_service_worker_.Get())
    *result = true;
}

void InspectorNetworkAgent::DidBlockRequest(
    const ResourceRequest& request,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const FetchInitiatorInfo& initiator_info,
    ResourceRequestBlockedReason reason,
    ResourceType resource_type) {
  uint64_t identifier = request.InspectorId();
  if (identifier == 0UL) {
    NOTREACHED();
    // Defensively create a valid identifier.
    identifier = CreateUniqueIdentifier();
  }
  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(identifier, loader, fetch_context_url, request,
                          ResourceResponse(), initiator_info, type);

  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  String protocol_reason = BuildBlockedReason(reason);
  GetFrontend()->loadingFailed(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      String(), false, protocol_reason);
}

void InspectorNetworkAgent::DidChangeResourcePriority(
    DocumentLoader* loader,
    uint64_t identifier,
    ResourceLoadPriority load_priority) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  GetFrontend()->resourceChangedPriority(
      request_id, ResourcePriorityJSON(load_priority),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

void InspectorNetworkAgent::WillSendRequestInternal(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info,
    InspectorPageAgent::ResourceType type) {
  CHECK_EQ(identifier, request.InspectorId());
  String loader_id = IdentifiersFactory::LoaderId(loader);
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  NetworkResourcesData::ResourceData const* data =
      resources_data_->Data(request_id);
  // Support for POST request redirect.
  scoped_refptr<EncodedFormData> post_data;
  if (data &&
      (redirect_response.HttpStatusCode() == net::HTTP_TEMPORARY_REDIRECT ||
       redirect_response.HttpStatusCode() == net::HTTP_PERMANENT_REDIRECT)) {
    post_data = data->PostData();
  } else if (request.HttpBody()) {
    post_data = request.HttpBody()->DeepCopy();
  }

  resources_data_->ResourceCreated(request_id, loader_id, request.Url(),
                                   post_data);
  if (initiator_info.name == fetch_initiator_type_names::kXmlhttprequest)
    type = InspectorPageAgent::kXHRResource;
  else if (initiator_info.name == fetch_initiator_type_names::kFetch)
    type = InspectorPageAgent::kFetchResource;

  if (pending_request_type_)
    type = *pending_request_type_;
  resources_data_->SetResourceType(request_id, type);

  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      BuildInitiatorObject(loader && loader->GetFrame()
                               ? loader->GetFrame()->GetDocument()
                               : nullptr,
                           initiator_info, std::numeric_limits<int>::max());

  std::unique_ptr<protocol::Network::Request> request_info(
      BuildObjectForResourceRequest(request, post_data,
                                    max_post_data_size_.Get()));

  // |loader| is null while inspecting worker.
  // TODO(horo): Refactor MixedContentChecker and set mixed content type even if
  // |loader| is null.
  if (loader) {
    request_info->setMixedContentType(MixedContentTypeForContextType(
        MixedContentChecker::ContextTypeForInspector(loader->GetFrame(),
                                                     request)));
  }

  request_info->setReferrerPolicy(
      GetReferrerPolicy(request.GetReferrerPolicy()));
  if (initiator_info.is_link_preload)
    request_info->setIsLinkPreload(true);

  String resource_type = InspectorPageAgent::ResourceTypeJson(type);
  String documentURL = loader
                           ? UrlWithoutFragment(loader->Url()).GetString()
                           : UrlWithoutFragment(fetch_context_url).GetString();
  Maybe<String> maybe_frame_id;
  if (!frame_id.IsEmpty())
    maybe_frame_id = frame_id;
  GetFrontend()->requestWillBeSent(
      request_id, loader_id, documentURL, std::move(request_info),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      base::Time::Now().ToDoubleT(), std::move(initiator_object),
      BuildObjectForResourceResponse(redirect_response), resource_type,
      std::move(maybe_frame_id), request.HasUserGesture());
  if (is_handling_sync_xhr_)
    GetFrontend()->flush();

  if (pending_xhr_replay_data_) {
    resources_data_->SetXHRReplayData(request_id,
                                      pending_xhr_replay_data_.Get());
    pending_xhr_replay_data_.Clear();
  }
  pending_request_type_ = base::nullopt;
}

void InspectorNetworkAgent::WillSendNavigationRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& url,
    const AtomicString& http_method,
    EncodedFormData* http_body) {
  String loader_id = IdentifiersFactory::LoaderId(loader);
  String request_id = loader_id;
  NetworkResourcesData::ResourceData const* data =
      resources_data_->Data(request_id);
  // Support for POST request redirect.
  scoped_refptr<EncodedFormData> post_data;
  if (data)
    post_data = data->PostData();
  else if (http_body)
    post_data = http_body->DeepCopy();
  resources_data_->ResourceCreated(request_id, loader_id, url, post_data);
  resources_data_->SetResourceType(request_id,
                                   InspectorPageAgent::kDocumentResource);
}

// This method was pulled out of PrepareRequest(), because we want to be able
// to create DevTools issues before the PrepareRequest() call. We need these
// IDs to be set, to properly create a DevTools issue.
void InspectorNetworkAgent::SetDevToolsIds(ResourceRequest& request) {
  request.SetDevToolsToken(devtools_token_);

  // The loader parameter is for generating a browser generated ID for a browser
  // initiated request. We pass it null here because we are reporting a renderer
  // generated ID for a renderer initiated request.
  request.SetDevToolsId(IdentifiersFactory::RequestId(/* loader */ nullptr,
                                                      request.InspectorId()));
}

void InspectorNetworkAgent::PrepareRequest(
    DocumentLoader* loader,
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info,
    ResourceType resource_type) {
  // Ignore the request initiated internally.
  if (initiator_info.name == fetch_initiator_type_names::kInternal)
    return;

  if (!extra_request_headers_.IsEmpty()) {
    for (const WTF::String& key : extra_request_headers_.Keys()) {
      const WTF::String& value = extra_request_headers_.Get(key);
      AtomicString header_name = AtomicString(key);
      // When overriding referrer, also override referrer policy
      // for this request to assure the request will be allowed.
      // TODO: Should we store the referrer header somewhere other than
      // |extra_request_headers_|?
      if (header_name.LowerASCII() == http_names::kReferer.LowerASCII()) {
        request.SetReferrerString(value);
        request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
      } else {
        request.SetHttpHeaderField(header_name, AtomicString(value));
      }
    }
  }

  request.SetReportRawHeaders(true);

  if (cache_disabled_.Get()) {
    if (LoadsFromCacheOnly(request) &&
        request.GetRequestContext() != mojom::RequestContextType::INTERNAL) {
      request.SetCacheMode(mojom::FetchCacheMode::kUnspecifiedForceCacheMiss);
    } else {
      request.SetCacheMode(mojom::FetchCacheMode::kBypassCache);
    }
    request.SetShouldResetAppCache(true);
  }
  if (bypass_service_worker_.Get())
    request.SetSkipServiceWorker(true);
}

void InspectorNetworkAgent::WillSendRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info,
    ResourceType resource_type) {
  // Ignore the request initiated internally.
  if (initiator_info.name == fetch_initiator_type_names::kInternal)
    return;

  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(identifier, loader, fetch_context_url, request,
                          redirect_response, initiator_info, type);
}

void InspectorNetworkAgent::MarkResourceAsCached(DocumentLoader* loader,
                                                 uint64_t identifier) {
  GetFrontend()->requestServedFromCache(
      IdentifiersFactory::RequestId(loader, identifier));
}

void InspectorNetworkAgent::DidReceiveResourceResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    const Resource* cached_resource) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  bool is_not_modified = response.HttpStatusCode() == 304;

  bool resource_is_empty = true;
  std::unique_ptr<protocol::Network::Response> resource_response =
      BuildObjectForResourceResponse(response, cached_resource,
                                     &resource_is_empty);

  InspectorPageAgent::ResourceType type =
      cached_resource
          ? InspectorPageAgent::ToResourceType(cached_resource->GetType())
          : InspectorPageAgent::kOtherResource;
  // Override with already discovered resource type.
  InspectorPageAgent::ResourceType saved_type =
      resources_data_->GetResourceType(request_id);
  if (saved_type == InspectorPageAgent::kScriptResource ||
      saved_type == InspectorPageAgent::kXHRResource ||
      saved_type == InspectorPageAgent::kDocumentResource ||
      saved_type == InspectorPageAgent::kFetchResource ||
      saved_type == InspectorPageAgent::kEventSourceResource) {
    type = saved_type;
  }

  // Resources are added to NetworkResourcesData as a WeakMember here and
  // removed in willDestroyResource() called in the prefinalizer of Resource.
  // Because NetworkResourceData retains weak references only, it
  // doesn't affect Resource lifetime.
  if (cached_resource)
    resources_data_->AddResource(request_id, cached_resource);
  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  String loader_id = IdentifiersFactory::LoaderId(loader);
  resources_data_->ResponseReceived(request_id, frame_id, response);
  resources_data_->SetResourceType(request_id, type);

  const base::Optional<ResourceResponse::SecurityDetails>&
      response_security_details = response.GetSecurityDetails();
  if (response_security_details.has_value()) {
    resources_data_->SetCertificate(request_id,
                                    response_security_details->certificate);
  }

  if (IsNavigation(loader, identifier))
    return;
  if (resource_response && !resource_is_empty) {
    Maybe<String> maybe_frame_id;
    if (!frame_id.IsEmpty())
      maybe_frame_id = frame_id;
    GetFrontend()->responseReceived(
        request_id, loader_id,
        base::TimeTicks::Now().since_origin().InSecondsF(),
        InspectorPageAgent::ResourceTypeJson(type),
        std::move(resource_response), std::move(maybe_frame_id));
  }
  // If we revalidated the resource and got Not modified, send content length
  // following didReceiveResponse as there will be no calls to didReceiveData
  // from the network stack.
  if (is_not_modified && cached_resource && cached_resource->EncodedSize()) {
    DidReceiveData(identifier, loader, nullptr,
                   static_cast<int>(cached_resource->EncodedSize()));
  }
}

static bool IsErrorStatusCode(int status_code) {
  return status_code >= 400;
}

void InspectorNetworkAgent::DidReceiveData(uint64_t identifier,
                                           DocumentLoader* loader,
                                           const char* data,
                                           uint64_t data_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  if (data) {
    NetworkResourcesData::ResourceData const* resource_data =
        resources_data_->Data(request_id);
    if (resource_data &&
        (!resource_data->CachedResource() ||
         resource_data->CachedResource()->GetDataBufferingPolicy() ==
             kDoNotBufferData ||
         IsErrorStatusCode(resource_data->HttpStatusCode())))
      resources_data_->MaybeAddResourceData(request_id, data, data_length);
  }

  GetFrontend()->dataReceived(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      static_cast<int>(data_length),
      static_cast<int>(
          resources_data_->GetAndClearPendingEncodedDataLength(request_id)));
}

void InspectorNetworkAgent::DidReceiveBlob(uint64_t identifier,
                                           DocumentLoader* loader,
                                           scoped_refptr<BlobDataHandle> blob) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  resources_data_->BlobReceived(request_id, std::move(blob));
}

void InspectorNetworkAgent::DidReceiveEncodedDataLength(
    DocumentLoader* loader,
    uint64_t identifier,
    size_t encoded_data_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  resources_data_->AddPendingEncodedDataLength(request_id, encoded_data_length);
}

void InspectorNetworkAgent::DidFinishLoading(
    uint64_t identifier,
    DocumentLoader* loader,
    base::TimeTicks monotonic_finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length,
    bool should_report_corb_blocking) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);

  int pending_encoded_data_length = static_cast<int>(
      resources_data_->GetAndClearPendingEncodedDataLength(request_id));
  if (pending_encoded_data_length > 0) {
    GetFrontend()->dataReceived(
        request_id, base::TimeTicks::Now().since_origin().InSecondsF(), 0,
        pending_encoded_data_length);
  }

  if (resource_data &&
      (!resource_data->CachedResource() ||
       resource_data->CachedResource()->GetDataBufferingPolicy() ==
           kDoNotBufferData ||
       IsErrorStatusCode(resource_data->HttpStatusCode()))) {
    resources_data_->MaybeAddResourceData(request_id, "", 0);
  }

  resources_data_->MaybeDecodeDataToContent(request_id);
  if (monotonic_finish_time.is_null())
    monotonic_finish_time = base::TimeTicks::Now();

  is_handling_sync_xhr_ = false;
  // TODO(npm): Use base::TimeTicks in Network.h.
  GetFrontend()->loadingFinished(
      request_id, monotonic_finish_time.since_origin().InSecondsF(),
      encoded_data_length, should_report_corb_blocking);
}

void InspectorNetworkAgent::DidReceiveCorsRedirectResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    Resource* resource) {
  // Update the response and finish loading
  DidReceiveResourceResponse(identifier, loader, response, resource);
  DidFinishLoading(identifier, loader, base::TimeTicks(),
                   WebURLLoaderClient::kUnknownEncodedDataLength, 0, false);
}

void InspectorNetworkAgent::DidFailLoading(uint64_t identifier,
                                           DocumentLoader* loader,
                                           const ResourceError& error) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  bool canceled = error.IsCancellation();
  base::Optional<ResourceRequestBlockedReason> resource_request_blocked_reason =
      error.GetResourceRequestBlockedReason();
  protocol::Maybe<String> blocked_reason;
  if (resource_request_blocked_reason) {
    blocked_reason =
        BuildBlockedReason(resource_request_blocked_reason.value());
  }
  is_handling_sync_xhr_ = false;
  GetFrontend()->loadingFailed(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      error.LocalizedDescription(), canceled, std::move(blocked_reason));
}

void InspectorNetworkAgent::ScriptImported(uint64_t identifier,
                                           const String& source_string) {
  resources_data_->SetResourceContent(
      IdentifiersFactory::SubresourceRequestId(identifier), source_string);
}

void InspectorNetworkAgent::DidReceiveScriptResponse(uint64_t identifier) {
  resources_data_->SetResourceType(
      IdentifiersFactory::SubresourceRequestId(identifier),
      InspectorPageAgent::kScriptResource);
}

// static
bool InspectorNetworkAgent::IsNavigation(DocumentLoader* loader,
                                         uint64_t identifier) {
  return loader && loader->MainResourceIdentifier() == identifier;
}

void InspectorNetworkAgent::WillLoadXHR(ExecutionContext* execution_context,
                                        const AtomicString& method,
                                        const KURL& url,
                                        bool async,
                                        const HTTPHeaderMap& headers,
                                        bool include_credentials) {
  DCHECK(!pending_request_type_);
  pending_xhr_replay_data_ = MakeGarbageCollected<XHRReplayData>(
      execution_context, method, UrlWithoutFragment(url), async,
      include_credentials);
  for (const auto& header : headers)
    pending_xhr_replay_data_->AddHeader(header.key, header.value);
  DCHECK(!is_handling_sync_xhr_);
  if (!async)
    is_handling_sync_xhr_ = true;
}

void InspectorNetworkAgent::DidFinishXHR(XMLHttpRequest* xhr) {
  replay_xhrs_.erase(xhr);
}

void InspectorNetworkAgent::WillSendEventSourceRequest() {
  DCHECK(!pending_request_type_);
  pending_request_type_ = InspectorPageAgent::kEventSourceResource;
}

void InspectorNetworkAgent::WillDispatchEventSourceEvent(
    uint64_t identifier,
    const AtomicString& event_name,
    const AtomicString& event_id,
    const String& data) {
  GetFrontend()->eventSourceMessageReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      event_name.GetString(), event_id.GetString(), data);
}

std::unique_ptr<protocol::Network::Initiator>
InspectorNetworkAgent::BuildInitiatorObject(
    Document* document,
    const FetchInitiatorInfo& initiator_info,
    int max_async_depth) {
  if (initiator_info.is_imported_module && !initiator_info.referrer.IsEmpty()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Script)
            .build();
    initiator_object->setUrl(initiator_info.referrer);
    initiator_object->setLineNumber(
        initiator_info.position.line_.ZeroBasedInt());
    return initiator_object;
  }

  bool was_requested_by_stylesheet =
      initiator_info.name == fetch_initiator_type_names::kCSS ||
      initiator_info.name == fetch_initiator_type_names::kUacss;
  if (was_requested_by_stylesheet && !initiator_info.referrer.IsEmpty()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Parser)
            .build();
    initiator_object->setUrl(initiator_info.referrer);
    return initiator_object;
  }

  // We skip stack checking for stylesheet-initiated requests as it may
  // represent the cause of a style recalculation rather than the actual
  // resources themselves. See crbug.com/918196.
  if (!was_requested_by_stylesheet) {
    std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
        current_stack_trace =
            SourceLocation::Capture(document ? document->GetExecutionContext()
                                             : nullptr)
                ->BuildInspectorObject(max_async_depth);
    if (current_stack_trace) {
      std::unique_ptr<protocol::Network::Initiator> initiator_object =
          protocol::Network::Initiator::create()
              .setType(protocol::Network::Initiator::TypeEnum::Script)
              .build();
      initiator_object->setStack(std::move(current_stack_trace));
      return initiator_object;
    }
  }

  while (document && !document->GetScriptableDocumentParser())
    document = document->LocalOwner() ? document->LocalOwner()->ownerDocument()
                                      : nullptr;
  if (document && document->GetScriptableDocumentParser()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Parser)
            .build();
    initiator_object->setUrl(UrlWithoutFragment(document->Url()).GetString());
    if (TextPosition::BelowRangePosition() != initiator_info.position)
      initiator_object->setLineNumber(
          initiator_info.position.line_.ZeroBasedInt());
    else
      initiator_object->setLineNumber(
          document->GetScriptableDocumentParser()->LineNumber().ZeroBasedInt());
    return initiator_object;
  }

  return protocol::Network::Initiator::create()
      .setType(protocol::Network::Initiator::TypeEnum::Other)
      .build();
}

void InspectorNetworkAgent::DidCreateWebSocket(
    ExecutionContext* execution_context,
    uint64_t identifier,
    const KURL& request_url,
    const String&) {
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      current_stack_trace =
          SourceLocation::Capture(execution_context)->BuildInspectorObject();
  if (!current_stack_trace) {
    GetFrontend()->webSocketCreated(
        IdentifiersFactory::SubresourceRequestId(identifier),
        UrlWithoutFragment(request_url).GetString());
    return;
  }

  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      protocol::Network::Initiator::create()
          .setType(protocol::Network::Initiator::TypeEnum::Script)
          .build();
  initiator_object->setStack(std::move(current_stack_trace));
  GetFrontend()->webSocketCreated(
      IdentifiersFactory::SubresourceRequestId(identifier),
      UrlWithoutFragment(request_url).GetString(), std::move(initiator_object));
}

void InspectorNetworkAgent::WillSendWebSocketHandshakeRequest(
    ExecutionContext*,
    uint64_t identifier,
    network::mojom::blink::WebSocketHandshakeRequest* request) {
  DCHECK(request);
  HTTPHeaderMap headers;
  for (auto& header : request->headers)
    headers.Add(AtomicString(header->name), AtomicString(header->value));
  std::unique_ptr<protocol::Network::WebSocketRequest> request_object =
      protocol::Network::WebSocketRequest::create()
          .setHeaders(BuildObjectForHeaders(headers))
          .build();
  GetFrontend()->webSocketWillSendHandshakeRequest(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      base::Time::Now().ToDoubleT(), std::move(request_object));
}

void InspectorNetworkAgent::DidReceiveWebSocketHandshakeResponse(
    ExecutionContext*,
    uint64_t identifier,
    network::mojom::blink::WebSocketHandshakeRequest* request,
    network::mojom::blink::WebSocketHandshakeResponse* response) {
  DCHECK(response);

  HTTPHeaderMap response_headers;
  for (auto& header : response->headers) {
    HTTPHeaderMap::AddResult add_result = response_headers.Add(
        AtomicString(header->name), AtomicString(header->value));
    if (!add_result.is_new_entry) {
      // Protocol expects the "\n" separated format.
      add_result.stored_value->value =
          add_result.stored_value->value + "\n" + header->value;
    }
  }

  std::unique_ptr<protocol::Network::WebSocketResponse> response_object =
      protocol::Network::WebSocketResponse::create()
          .setStatus(response->status_code)
          .setStatusText(response->status_text)
          .setHeaders(BuildObjectForHeaders(response_headers))
          .build();
  if (!response->headers_text.IsEmpty())
    response_object->setHeadersText(response->headers_text);

  if (request) {
    HTTPHeaderMap request_headers;
    for (auto& header : request->headers) {
      request_headers.Add(AtomicString(header->name),
                          AtomicString(header->value));
    }
    response_object->setRequestHeaders(BuildObjectForHeaders(request_headers));
    if (!request->headers_text.IsEmpty())
      response_object->setRequestHeadersText(request->headers_text);
  }

  GetFrontend()->webSocketHandshakeResponseReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      std::move(response_object));
}

void InspectorNetworkAgent::DidCloseWebSocket(ExecutionContext*,
                                              uint64_t identifier) {
  GetFrontend()->webSocketClosed(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

void InspectorNetworkAgent::DidReceiveWebSocketMessage(
    uint64_t identifier,
    int op_code,
    bool masked,
    const Vector<base::span<const char>>& data) {
  size_t size = 0;
  for (const auto& span : data) {
    size += span.size();
  }
  Vector<char> flatten;
  flatten.ReserveCapacity(SafeCast<wtf_size_t>(size));
  for (const auto& span : data) {
    flatten.Append(span.data(), SafeCast<wtf_size_t>(span.size()));
  }
  GetFrontend()->webSocketFrameReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      WebSocketMessageToProtocol(op_code, masked, flatten.data(),
                                 flatten.size()));
}

void InspectorNetworkAgent::DidSendWebSocketMessage(uint64_t identifier,
                                                    int op_code,
                                                    bool masked,
                                                    const char* payload,
                                                    size_t payload_length) {
  GetFrontend()->webSocketFrameSent(
      IdentifiersFactory::RequestId(nullptr, identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      WebSocketMessageToProtocol(op_code, masked, payload, payload_length));
}

void InspectorNetworkAgent::DidReceiveWebSocketMessageError(
    uint64_t identifier,
    const String& error_message) {
  GetFrontend()->webSocketFrameError(
      IdentifiersFactory::RequestId(nullptr, identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(), error_message);
}

Response InspectorNetworkAgent::enable(Maybe<int> total_buffer_size,
                                       Maybe<int> resource_buffer_size,
                                       Maybe<int> max_post_data_size) {
  total_buffer_size_.Set(total_buffer_size.fromMaybe(kDefaultTotalBufferSize));
  resource_buffer_size_.Set(
      resource_buffer_size.fromMaybe(kDefaultResourceBufferSize));
  max_post_data_size_.Set(max_post_data_size.fromMaybe(0));
  Enable();
  return Response::Success();
}

void InspectorNetworkAgent::Enable() {
  if (!GetFrontend())
    return;
  enabled_.Set(true);
  resources_data_->SetResourcesDataSizeLimits(total_buffer_size_.Get(),
                                              resource_buffer_size_.Get());
  instrumenting_agents_->AddInspectorNetworkAgent(this);
}

Response InspectorNetworkAgent::disable() {
  DCHECK(!pending_request_type_);
  if (IsMainThread())
    GetNetworkStateNotifier().ClearOverride();
  instrumenting_agents_->RemoveInspectorNetworkAgent(this);
  agent_state_.ClearAllFields();
  resources_data_->Clear();
  return Response::Success();
}

Response InspectorNetworkAgent::setExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  extra_request_headers_.Clear();
  std::unique_ptr<protocol::DictionaryValue> in = headers->toValue();
  for (size_t i = 0; i < in->size(); ++i) {
    const auto& entry = in->at(i);
    String value;
    if (entry.second && entry.second->asString(&value))
      extra_request_headers_.Set(entry.first, value);
  }
  return Response::Success();
}

bool InspectorNetworkAgent::CanGetResponseBodyBlob(const String& request_id) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  BlobDataHandle* blob =
      resource_data ? resource_data->DownloadedFileBlob() : nullptr;
  if (!blob)
    return false;
  if (worker_global_scope_)
    return true;
  LocalFrame* frame = IdentifiersFactory::FrameById(inspected_frames_,
                                                    resource_data->FrameId());
  return frame && frame->GetDocument();
}

void InspectorNetworkAgent::GetResponseBodyBlob(
    const String& request_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  BlobDataHandle* blob = resource_data->DownloadedFileBlob();
  ExecutionContext* context = GetTargetExecutionContext();
  if (!context) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  InspectorFileReaderLoaderClient* client = new InspectorFileReaderLoaderClient(
      blob, context->GetTaskRunner(TaskType::kFileReading),
      WTF::Bind(ResponseBodyFileReaderLoaderDone, resource_data->MimeType(),
                resource_data->TextEncodingName(),
                WTF::Passed(std::move(callback))));
  client->Start();
}

void InspectorNetworkAgent::getResponseBody(
    const String& request_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  if (CanGetResponseBodyBlob(request_id)) {
    GetResponseBodyBlob(request_id, std::move(callback));
    return;
  }

  String content;
  bool base64_encoded;
  Response response = GetResponseBody(request_id, &content, &base64_encoded);
  if (response.IsSuccess()) {
    callback->sendSuccess(content, base64_encoded);
  } else {
    callback->sendFailure(response);
  }
}

Response InspectorNetworkAgent::setBlockedURLs(
    std::unique_ptr<protocol::Array<String>> urls) {
  blocked_urls_.Clear();
  for (const String& url : *urls)
    blocked_urls_.Set(url, true);
  return Response::Success();
}

Response InspectorNetworkAgent::replayXHR(const String& request_id) {
  String actual_request_id = request_id;

  XHRReplayData* xhr_replay_data = resources_data_->XhrReplayData(request_id);
  auto* data = resources_data_->Data(request_id);
  if (!xhr_replay_data || !data)
    return Response::ServerError("Given id does not correspond to XHR");

  ExecutionContext* execution_context = xhr_replay_data->GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed()) {
    resources_data_->SetXHRReplayData(request_id, nullptr);
    return Response::ServerError("Document is already detached");
  }

  XMLHttpRequest* xhr = XMLHttpRequest::Create(execution_context);

  execution_context->RemoveURLFromMemoryCache(xhr_replay_data->Url());

  xhr->open(xhr_replay_data->Method(), xhr_replay_data->Url(),
            xhr_replay_data->Async(), IGNORE_EXCEPTION_FOR_TESTING);
  if (xhr_replay_data->IncludeCredentials())
    xhr->setWithCredentials(true, IGNORE_EXCEPTION_FOR_TESTING);
  for (const auto& header : xhr_replay_data->Headers()) {
    xhr->setRequestHeader(header.key, header.value,
                          IGNORE_EXCEPTION_FOR_TESTING);
  }
  xhr->SendForInspectorXHRReplay(data ? data->PostData() : nullptr,
                                 IGNORE_EXCEPTION_FOR_TESTING);

  replay_xhrs_.insert(xhr);
  return Response::Success();
}

Response InspectorNetworkAgent::canClearBrowserCache(bool* result) {
  *result = true;
  return Response::Success();
}

Response InspectorNetworkAgent::canClearBrowserCookies(bool* result) {
  *result = true;
  return Response::Success();
}

Response InspectorNetworkAgent::emulateNetworkConditions(
    bool offline,
    double latency,
    double download_throughput,
    double upload_throughput,
    Maybe<String> connection_type) {
  if (!IsMainThread())
    return Response::ServerError("Not supported");

  WebConnectionType type = kWebConnectionTypeUnknown;
  if (connection_type.isJust()) {
    type = ToWebConnectionType(connection_type.fromJust());
    if (type == kWebConnectionTypeUnknown)
      return Response::ServerError("Unknown connection type");
  }
  // TODO(dgozman): networkStateNotifier is per-process. It would be nice to
  // have per-frame override instead.
  if (offline || latency || download_throughput || upload_throughput) {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        !offline, type, base::nullopt, latency,
        download_throughput / (1024 * 1024 / 8));
  } else {
    GetNetworkStateNotifier().ClearOverride();
  }
  return Response::Success();
}

Response InspectorNetworkAgent::setCacheDisabled(bool cache_disabled) {
  // TODO(ananta)
  // We should extract network cache state into a global entity which can be
  // queried from FrameLoader and other places.
  cache_disabled_.Set(cache_disabled);
  if (cache_disabled && IsMainThread())
    GetMemoryCache()->EvictResources();
  return Response::Success();
}

Response InspectorNetworkAgent::setBypassServiceWorker(bool bypass) {
  bypass_service_worker_.Set(bypass);
  return Response::Success();
}

Response InspectorNetworkAgent::setDataSizeLimitsForTest(int max_total,
                                                         int max_resource) {
  resources_data_->SetResourcesDataSizeLimits(max_total, max_resource);
  return Response::Success();
}

Response InspectorNetworkAgent::getCertificate(
    const String& origin,
    std::unique_ptr<protocol::Array<String>>* certificate) {
  *certificate = std::make_unique<protocol::Array<String>>();
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(origin);
  for (auto& resource : resources_data_->Resources()) {
    scoped_refptr<const SecurityOrigin> resource_origin =
        SecurityOrigin::Create(resource->RequestedURL());
    if (resource_origin->IsSameOriginWith(security_origin.get()) &&
        resource->Certificate().size()) {
      for (auto& cert : resource->Certificate()) {
        (*certificate)
            ->emplace_back(
                Base64Encode(base::as_bytes(base::make_span(cert.Latin1()))));
      }
      return Response::Success();
    }
  }
  return Response::Success();
}

void InspectorNetworkAgent::DidCommitLoad(LocalFrame* frame,
                                          DocumentLoader* loader) {
  DCHECK(IsMainThread());
  if (loader->GetFrame() != inspected_frames_->Root())
    return;

  if (cache_disabled_.Get())
    GetMemoryCache()->EvictResources();

  resources_data_->Clear(IdentifiersFactory::LoaderId(loader));
}

void InspectorNetworkAgent::FrameScheduledNavigation(LocalFrame* frame,
                                                     const KURL&,
                                                     base::TimeDelta,
                                                     ClientNavigationReason) {
  // For navigations, we limit async stack trace to depth 1 to avoid the
  // base::Value depth limits with Mojo serialization / parsing.
  // See http://crbug.com/809996.
  frame_navigation_initiator_map_.Set(
      IdentifiersFactory::FrameId(frame),
      BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo(),
                           /*max_async_depth=*/1));
}

void InspectorNetworkAgent::FrameClearedScheduledNavigation(LocalFrame* frame) {
  frame_navigation_initiator_map_.erase(IdentifiersFactory::FrameId(frame));
}

Response InspectorNetworkAgent::GetResponseBody(const String& request_id,
                                                String* content,
                                                bool* base64_encoded) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  if (!resource_data) {
    return Response::ServerError("No resource with given identifier found");
  }

  if (resource_data->HasContent()) {
    *content = resource_data->Content();
    *base64_encoded = resource_data->Base64Encoded();
    return Response::Success();
  }

  if (resource_data->IsContentEvicted()) {
    return Response::ServerError(
        "Request content was evicted from inspector cache");
  }

  if (resource_data->Buffer() && !resource_data->TextEncodingName().IsNull()) {
    bool success = InspectorPageAgent::SharedBufferContent(
        resource_data->Buffer(), resource_data->MimeType(),
        resource_data->TextEncodingName(), content, base64_encoded);
    DCHECK(success);
    return Response::Success();
  }

  if (resource_data->CachedResource() &&
      InspectorPageAgent::CachedResourceContent(resource_data->CachedResource(),
                                                content, base64_encoded)) {
    return Response::Success();
  }

  return Response::ServerError(
      "No data found for resource with given identifier");
}

Response InspectorNetworkAgent::searchInResponseBody(
    const String& request_id,
    const String& query,
    Maybe<bool> case_sensitive,
    Maybe<bool> is_regex,
    std::unique_ptr<
        protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>*
        matches) {
  String content;
  bool base64_encoded;
  Response response = GetResponseBody(request_id, &content, &base64_encoded);
  if (!response.IsSuccess())
    return response;

  auto results = v8_session_->searchInTextByLines(
      ToV8InspectorStringView(content), ToV8InspectorStringView(query),
      case_sensitive.fromMaybe(false), is_regex.fromMaybe(false));
  *matches = std::make_unique<
      protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>(
      std::move(results));
  return Response::Success();
}

bool InspectorNetworkAgent::FetchResourceContent(Document* document,
                                                 const KURL& url,
                                                 String* content,
                                                 bool* base64_encoded) {
  DCHECK(document);
  DCHECK(IsMainThread());
  // First try to fetch content from the cached resource.
  Resource* cached_resource = document->Fetcher()->CachedResource(url);
  if (!cached_resource) {
    cached_resource = GetMemoryCache()->ResourceForURL(
        url, document->Fetcher()->GetCacheIdentifier());
  }
  if (cached_resource && InspectorPageAgent::CachedResourceContent(
                             cached_resource, content, base64_encoded))
    return true;

  // Then fall back to resource data.
  for (auto& resource : resources_data_->Resources()) {
    if (resource->RequestedURL() == url) {
      *content = resource->Content();
      *base64_encoded = resource->Base64Encoded();
      return true;
    }
  }
  return false;
}

String InspectorNetworkAgent::NavigationInitiatorInfo(LocalFrame* frame) {
  if (!enabled_.Get())
    return String();
  auto it =
      frame_navigation_initiator_map_.find(IdentifiersFactory::FrameId(frame));
  std::vector<uint8_t> cbor;
  if (it != frame_navigation_initiator_map_.end()) {
    it->value->AppendSerialized(&cbor);
  } else {
    // For navigations, we limit async stack trace to depth 1 to avoid the
    // base::Value depth limits with Mojo serialization / parsing.
    // See http://crbug.com/809996.
    BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo(),
                         /*max_async_depth=*/1)
        ->AppendSerialized(&cbor);
  }
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(cbor), &json);
  return String(reinterpret_cast<const char*>(json.data()), json.size());
}

InspectorNetworkAgent::InspectorNetworkAgent(
    InspectedFrames* inspected_frames,
    WorkerGlobalScope* worker_global_scope,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      worker_global_scope_(worker_global_scope),
      v8_session_(v8_session),
      resources_data_(MakeGarbageCollected<NetworkResourcesData>(
          kDefaultTotalBufferSize,
          kDefaultResourceBufferSize)),
      devtools_token_(worker_global_scope_
                          ? worker_global_scope_->GetParentDevToolsToken()
                          : inspected_frames->Root()->GetDevToolsFrameToken()),
      enabled_(&agent_state_, /*default_value=*/false),
      cache_disabled_(&agent_state_, /*default_value=*/false),
      bypass_service_worker_(&agent_state_, /*default_value=*/false),
      blocked_urls_(&agent_state_, /*default_value=*/false),
      extra_request_headers_(&agent_state_, /*default_value=*/WTF::String()),
      total_buffer_size_(&agent_state_,
                         /*default_value=*/kDefaultTotalBufferSize),
      resource_buffer_size_(&agent_state_,
                            /*default_value=*/kDefaultResourceBufferSize),
      max_post_data_size_(&agent_state_, /*default_value=*/0) {
  DCHECK((IsMainThread() && !worker_global_scope_) ||
         (!IsMainThread() && worker_global_scope_));
}

void InspectorNetworkAgent::ShouldForceCorsPreflight(bool* result) {
  if (cache_disabled_.Get())
    *result = true;
}

void InspectorNetworkAgent::getRequestPostData(
    const String& request_id,
    std::unique_ptr<GetRequestPostDataCallback> callback) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  if (!resource_data) {
    callback->sendFailure(
        Response::ServerError("No resource with given id was found"));
    return;
  }
  scoped_refptr<EncodedFormData> post_data = resource_data->PostData();
  if (!post_data || post_data->IsEmpty()) {
    callback->sendFailure(
        Response::ServerError("No post data available for the request"));
    return;
  }
  ExecutionContext* context = GetTargetExecutionContext();
  if (!context) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  scoped_refptr<InspectorPostBodyParser> parser =
      base::MakeRefCounted<InspectorPostBodyParser>(
          std::move(callback), context->GetTaskRunner(TaskType::kFileReading));
  // TODO(crbug.com/810554): Extend protocol to fetch body parts separately
  parser->Parse(post_data.get());
}

ExecutionContext* InspectorNetworkAgent::GetTargetExecutionContext() const {
  if (worker_global_scope_)
    return worker_global_scope_;
  DCHECK(inspected_frames_);
  return inspected_frames_->Root()->DomWindow();
}

}  // namespace blink
