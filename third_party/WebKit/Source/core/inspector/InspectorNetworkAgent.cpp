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

#include "core/inspector/InspectorNetworkAgent.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/NetworkResourcesData.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "platform/blob/BlobData.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoadTiming.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/Base64.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebMixedContentContextType.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

using GetResponseBodyCallback =
    protocol::Network::Backend::GetResponseBodyCallback;
using protocol::Response;

namespace NetworkAgentState {
static const char kNetworkAgentEnabled[] = "networkAgentEnabled";
static const char kExtraRequestHeaders[] = "extraRequestHeaders";
static const char kCacheDisabled[] = "cacheDisabled";
static const char kBypassServiceWorker[] = "bypassServiceWorker";
static const char kUserAgentOverride[] = "userAgentOverride";
static const char kBlockedURLs[] = "blockedURLs";
static const char kTotalBufferSize[] = "totalBufferSize";
static const char kResourceBufferSize[] = "resourceBufferSize";
}

namespace {
// 100MB
static size_t g_maximum_total_buffer_size = 100 * 1000 * 1000;

// 10MB
static size_t g_maximum_resource_buffer_size = 10 * 1000 * 1000;

// Pattern may contain stars ('*') which match to any (possibly empty) string.
// Stars implicitly assumed at the begin/end of pattern.
bool Matches(const String& url, const String& pattern) {
  Vector<String> parts;
  pattern.Split("*", parts);
  size_t pos = 0;
  for (const String& part : parts) {
    pos = url.Find(part, pos);
    if (pos == kNotFound)
      return false;
    pos += part.length();
  }
  return true;
}

bool LoadsFromCacheOnly(const ResourceRequest& request) {
  switch (request.GetCachePolicy()) {
    case WebCachePolicy::kUseProtocolCachePolicy:
    case WebCachePolicy::kValidatingCacheData:
    case WebCachePolicy::kBypassingCache:
    case WebCachePolicy::kReturnCacheDataElseLoad:
      return false;
    case WebCachePolicy::kReturnCacheDataDontLoad:
    case WebCachePolicy::kReturnCacheDataIfValid:
    case WebCachePolicy::kBypassCacheLoadOnlyFromCache:
      return true;
  }
  NOTREACHED();
  return false;
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
  WTF_MAKE_NONCOPYABLE(InspectorFileReaderLoaderClient);

 public:
  InspectorFileReaderLoaderClient(
      scoped_refptr<BlobDataHandle> blob,
      const String& mime_type,
      const String& text_encoding_name,
      std::unique_ptr<GetResponseBodyCallback> callback)
      : blob_(std::move(blob)),
        mime_type_(mime_type),
        text_encoding_name_(text_encoding_name),
        callback_(std::move(callback)) {
    loader_ = FileReaderLoader::Create(FileReaderLoader::kReadByClient, this);
  }

  ~InspectorFileReaderLoaderClient() override {}

  void Start(ExecutionContext* execution_context) {
    raw_data_ = SharedBuffer::Create();
    loader_->Start(execution_context, blob_);
  }

  virtual void DidStartLoading() {}

  virtual void DidReceiveDataForClient(const char* data, unsigned data_length) {
    if (!data_length)
      return;
    raw_data_->Append(data, data_length);
  }

  virtual void DidFinishLoading() {
    String result;
    bool base64_encoded;
    if (InspectorPageAgent::SharedBufferContent(raw_data_, mime_type_,
                                                text_encoding_name_, &result,
                                                &base64_encoded))
      callback_->sendSuccess(result, base64_encoded);
    else
      callback_->sendFailure(Response::Error("Couldn't encode data"));
    Dispose();
  }

  virtual void DidFail(FileError::ErrorCode) {
    callback_->sendFailure(Response::Error("Couldn't read BLOB"));
    Dispose();
  }

 private:
  void Dispose() {
    raw_data_ = nullptr;
    delete this;
  }

  scoped_refptr<BlobDataHandle> blob_;
  String mime_type_;
  String text_encoding_name_;
  std::unique_ptr<GetResponseBodyCallback> callback_;
  std::unique_ptr<FileReaderLoader> loader_;
  scoped_refptr<SharedBuffer> raw_data_;
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
    case kResourceLoadPriorityVeryLow:
      return protocol::Network::ResourcePriorityEnum::VeryLow;
    case kResourceLoadPriorityLow:
      return protocol::Network::ResourcePriorityEnum::Low;
    case kResourceLoadPriorityMedium:
      return protocol::Network::ResourcePriorityEnum::Medium;
    case kResourceLoadPriorityHigh:
      return protocol::Network::ResourcePriorityEnum::High;
    case kResourceLoadPriorityVeryHigh:
      return protocol::Network::ResourcePriorityEnum::VeryHigh;
    case kResourceLoadPriorityUnresolved:
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
    case ResourceRequestBlockedReason::kOther:
      return protocol::Network::BlockedReasonEnum::Other;
    case ResourceRequestBlockedReason::kNone:
    default:
      NOTREACHED();
      return protocol::Network::BlockedReasonEnum::Other;
  }
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

String GetReferrerPolicy(ReferrerPolicy policy) {
  switch (policy) {
    case kReferrerPolicyAlways:
      return protocol::Network::Request::ReferrerPolicyEnum::UnsafeUrl;
    case kReferrerPolicyDefault:
      if (RuntimeEnabledFeatures::ReducedReferrerGranularityEnabled()) {
        return protocol::Network::Request::ReferrerPolicyEnum::
            StrictOriginWhenCrossOrigin;
      } else {
        return protocol::Network::Request::ReferrerPolicyEnum::
            NoReferrerWhenDowngrade;
      }
    case kReferrerPolicyNoReferrerWhenDowngrade:
      return protocol::Network::Request::ReferrerPolicyEnum::
          NoReferrerWhenDowngrade;
    case kReferrerPolicyNever:
      return protocol::Network::Request::ReferrerPolicyEnum::NoReferrer;
    case kReferrerPolicyOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::Origin;
    case kReferrerPolicyOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          OriginWhenCrossOrigin;
    case kReferrerPolicySameOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::SameOrigin;
    case kReferrerPolicyStrictOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::StrictOrigin;
    case kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          StrictOriginWhenCrossOrigin;
  }

  return protocol::Network::Request::ReferrerPolicyEnum::
      NoReferrerWhenDowngrade;
}

}  // namespace

void InspectorNetworkAgent::Restore() {
  if (state_->booleanProperty(NetworkAgentState::kNetworkAgentEnabled, false)) {
    Enable(state_->integerProperty(NetworkAgentState::kTotalBufferSize,
                                   g_maximum_total_buffer_size),
           state_->integerProperty(NetworkAgentState::kResourceBufferSize,
                                   g_maximum_resource_buffer_size));
  }
}

static std::unique_ptr<protocol::Network::ResourceTiming> BuildObjectForTiming(
    const ResourceLoadTiming& timing) {
  return protocol::Network::ResourceTiming::create()
      .setRequestTime(timing.RequestTime())
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
      .setPushStart(timing.PushStart())
      .setPushEnd(timing.PushEnd())
      .build();
}

static std::unique_ptr<protocol::Network::Request>
BuildObjectForResourceRequest(const ResourceRequest& request) {
  std::unique_ptr<protocol::Network::Request> request_object =
      protocol::Network::Request::create()
          .setUrl(UrlWithoutFragment(request.Url()).GetString())
          .setMethod(request.HttpMethod())
          .setHeaders(BuildObjectForHeaders(request.HttpHeaderFields()))
          .setInitialPriority(ResourcePriorityJSON(request.Priority()))
          .setReferrerPolicy(GetReferrerPolicy(request.GetReferrerPolicy()))
          .build();
  if (request.HttpBody() && !request.HttpBody()->IsEmpty()) {
    Vector<char> bytes;
    request.HttpBody()->Flatten(bytes);
    request_object->setPostData(
        String::FromUTF8WithLatin1Fallback(bytes.data(), bytes.size()));
  }
  return request_object;
}

static std::unique_ptr<protocol::Network::Response>
BuildObjectForResourceResponse(const ResourceResponse& response,
                               Resource* cached_resource = nullptr,
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
    case ResourceResponse::kSecurityStyleUnknown:
      security_state = protocol::Security::SecurityStateEnum::Unknown;
      break;
    case ResourceResponse::kSecurityStyleUnauthenticated:
      security_state = protocol::Security::SecurityStateEnum::Neutral;
      break;
    case ResourceResponse::kSecurityStyleAuthenticationBroken:
      security_state = protocol::Security::SecurityStateEnum::Insecure;
      break;
    case ResourceResponse::kSecurityStyleAuthenticated:
      security_state = protocol::Security::SecurityStateEnum::Secure;
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
          .setUrl(UrlWithoutFragment(response.Url()).GetString())
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
  if (response.GetResourceLoadTiming())
    response_object->setTiming(
        BuildObjectForTiming(*response.GetResourceLoadTiming()));

  if (response.GetResourceLoadInfo()) {
    if (!response.GetResourceLoadInfo()->response_headers_text.IsEmpty())
      response_object->setHeadersText(
          response.GetResourceLoadInfo()->response_headers_text);
    if (response.GetResourceLoadInfo()->request_headers.size())
      response_object->setRequestHeaders(BuildObjectForHeaders(
          response.GetResourceLoadInfo()->request_headers));
    if (!response.GetResourceLoadInfo()->request_headers_text.IsEmpty())
      response_object->setRequestHeadersText(
          response.GetResourceLoadInfo()->request_headers_text);
  }

  String remote_ip_address = response.RemoteIPAddress();
  if (!remote_ip_address.IsEmpty()) {
    response_object->setRemoteIPAddress(remote_ip_address);
    response_object->setRemotePort(response.RemotePort());
  }

  String protocol;
  if (response.GetResourceLoadInfo())
    protocol = response.GetResourceLoadInfo()->npn_negotiated_protocol;
  if (protocol.IsEmpty() || protocol == "unknown") {
    if (response.WasFetchedViaSPDY()) {
      protocol = "spdy";
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
      protocol = response.Url().Protocol();
    }
  }
  response_object->setProtocol(protocol);

  if (response.GetSecurityStyle() != ResourceResponse::kSecurityStyleUnknown &&
      response.GetSecurityStyle() !=
          ResourceResponse::kSecurityStyleUnauthenticated) {
    const ResourceResponse::SecurityDetails* response_security_details =
        response.GetSecurityDetails();

    std::unique_ptr<protocol::Array<String>> san_list =
        protocol::Array<String>::create();
    for (auto const& san : response_security_details->san_list)
      san_list->addItem(san);

    std::unique_ptr<
        protocol::Array<protocol::Network::SignedCertificateTimestamp>>
        signed_certificate_timestamp_list = protocol::Array<
            protocol::Network::SignedCertificateTimestamp>::create();
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
      signed_certificate_timestamp_list->addItem(
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

InspectorNetworkAgent::~InspectorNetworkAgent() {}

void InspectorNetworkAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(worker_global_scope_);
  visitor->Trace(resources_data_);
  visitor->Trace(replay_xhrs_);
  visitor->Trace(replay_xhrs_to_be_deleted_);
  visitor->Trace(pending_xhr_replay_data_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorNetworkAgent::ShouldBlockRequest(const KURL& url, bool* result) {
  protocol::DictionaryValue* blocked_urls =
      state_->getObject(NetworkAgentState::kBlockedURLs);
  if (!blocked_urls)
    return;

  String url_string = url.GetString();
  for (size_t i = 0; i < blocked_urls->size(); ++i) {
    auto entry = blocked_urls->at(i);
    if (Matches(url_string, entry.first)) {
      *result = true;
      return;
    }
  }
  return;
}

void InspectorNetworkAgent::ShouldBypassServiceWorker(bool* result) {
  *result =
      state_->booleanProperty(NetworkAgentState::kBypassServiceWorker, false);
}

void InspectorNetworkAgent::DidBlockRequest(
    ExecutionContext* execution_context,
    const ResourceRequest& request,
    DocumentLoader* loader,
    const FetchInitiatorInfo& initiator_info,
    ResourceRequestBlockedReason reason,
    Resource::Type resource_type) {
  unsigned long identifier = CreateUniqueIdentifier();
  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(execution_context, identifier, loader, request,
                          ResourceResponse(), initiator_info, type);

  String request_id = IdentifiersFactory::RequestId(identifier);
  String protocol_reason = BuildBlockedReason(reason);
  GetFrontend()->loadingFailed(
      request_id, MonotonicallyIncreasingTime(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      String(), false, protocol_reason);
}

void InspectorNetworkAgent::DidChangeResourcePriority(
    unsigned long identifier,
    ResourceLoadPriority load_priority) {
  String request_id = IdentifiersFactory::RequestId(identifier);
  GetFrontend()->resourceChangedPriority(request_id,
                                         ResourcePriorityJSON(load_priority),
                                         MonotonicallyIncreasingTime());
}

void InspectorNetworkAgent::WillSendRequestInternal(
    ExecutionContext* execution_context,
    unsigned long identifier,
    DocumentLoader* loader,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info,
    InspectorPageAgent::ResourceType type) {
  String request_id = IdentifiersFactory::RequestId(identifier);
  String loader_id = loader ? IdentifiersFactory::LoaderId(loader) : "";
  resources_data_->ResourceCreated(request_id, loader_id, request.Url());
  if (initiator_info.name == FetchInitiatorTypeNames::xmlhttprequest)
    type = InspectorPageAgent::kXHRResource;

  resources_data_->SetResourceType(request_id, type);

  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      BuildInitiatorObject(loader && loader->GetFrame()
                               ? loader->GetFrame()->GetDocument()
                               : nullptr,
                           initiator_info);
  if (initiator_info.name == FetchInitiatorTypeNames::document) {
    FrameNavigationInitiatorMap::iterator it =
        frame_navigation_initiator_map_.find(frame_id);
    if (it != frame_navigation_initiator_map_.end())
      initiator_object = it->value->clone();
  }

  std::unique_ptr<protocol::Network::Request> request_info(
      BuildObjectForResourceRequest(request));

  // |loader| is null while inspecting worker if off-main-thread-fetch is
  // enabled. TODO(horo): Refactor MixedContentChecker and set mixed content
  // type even if |loader| is null.
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
  String documentURL =
      loader ? UrlWithoutFragment(loader->Url()).GetString()
             : UrlWithoutFragment(execution_context->Url()).GetString();
  Maybe<String> maybe_frame_id;
  if (!frame_id.IsEmpty())
    maybe_frame_id = frame_id;
  GetFrontend()->requestWillBeSent(
      request_id, loader_id, documentURL, std::move(request_info),
      MonotonicallyIncreasingTime(), CurrentTime(), std::move(initiator_object),
      BuildObjectForResourceResponse(redirect_response), resource_type,
      std::move(maybe_frame_id));
  if (pending_xhr_replay_data_ && !pending_xhr_replay_data_->Async())
    GetFrontend()->flush();
}

void InspectorNetworkAgent::WillSendRequest(
    ExecutionContext* execution_context,
    unsigned long identifier,
    DocumentLoader* loader,
    ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info,
    Resource::Type resource_type) {
  // Ignore the request initiated internally.
  if (initiator_info.name == FetchInitiatorTypeNames::internal)
    return;

  if (initiator_info.name == FetchInitiatorTypeNames::document &&
      loader->GetSubstituteData().IsValid())
    return;

  protocol::DictionaryValue* headers =
      state_->getObject(NetworkAgentState::kExtraRequestHeaders);
  if (headers) {
    for (size_t i = 0; i < headers->size(); ++i) {
      auto header = headers->at(i);
      String value;
      if (header.second->asString(&value))
        request.SetHTTPHeaderField(AtomicString(header.first),
                                   AtomicString(value));
    }
  }

  request.SetReportRawHeaders(true);

  if (state_->booleanProperty(NetworkAgentState::kCacheDisabled, false)) {
    if (LoadsFromCacheOnly(request) &&
        request.GetRequestContext() != WebURLRequest::kRequestContextInternal) {
      request.SetCachePolicy(WebCachePolicy::kBypassCacheLoadOnlyFromCache);
    } else {
      request.SetCachePolicy(WebCachePolicy::kBypassingCache);
    }
    request.SetShouldResetAppCache(true);
  }
  if (state_->booleanProperty(NetworkAgentState::kBypassServiceWorker, false))
    request.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kNone);

  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(execution_context, identifier, loader, request,
                          redirect_response, initiator_info, type);

  if (!host_id_.IsEmpty()) {
    request.AddHTTPHeaderField(
        HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id,
        AtomicString(host_id_));
  }
}

void InspectorNetworkAgent::MarkResourceAsCached(unsigned long identifier) {
  GetFrontend()->requestServedFromCache(
      IdentifiersFactory::RequestId(identifier));
}

void InspectorNetworkAgent::DidReceiveResourceResponse(
    unsigned long identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    Resource* cached_resource) {
  String request_id = IdentifiersFactory::RequestId(identifier);
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
  if (type == InspectorPageAgent::kDocumentResource && loader &&
      loader->GetSubstituteData().IsValid())
    return;

  // Resources are added to NetworkResourcesData as a WeakMember here and
  // removed in willDestroyResource() called in the prefinalizer of Resource.
  // Because NetworkResourceData retains weak references only, it
  // doesn't affect Resource lifetime.
  if (cached_resource)
    resources_data_->AddResource(request_id, cached_resource);
  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  String loader_id = loader ? IdentifiersFactory::LoaderId(loader) : "";
  resources_data_->ResponseReceived(request_id, frame_id, response);
  resources_data_->SetResourceType(request_id, type);

  if (response.GetSecurityStyle() != ResourceResponse::kSecurityStyleUnknown &&
      response.GetSecurityStyle() !=
          ResourceResponse::kSecurityStyleUnauthenticated) {
    const ResourceResponse::SecurityDetails* response_security_details =
        response.GetSecurityDetails();
    resources_data_->SetCertificate(request_id,
                                    response_security_details->certificate);
  }

  if (resource_response && !resource_is_empty) {
    Maybe<String> maybe_frame_id;
    if (!frame_id.IsEmpty())
      maybe_frame_id = frame_id;
    GetFrontend()->responseReceived(
        request_id, loader_id, MonotonicallyIncreasingTime(),
        InspectorPageAgent::ResourceTypeJson(type),
        std::move(resource_response), std::move(maybe_frame_id));
  }
  // If we revalidated the resource and got Not modified, send content length
  // following didReceiveResponse as there will be no calls to didReceiveData
  // from the network stack.
  if (is_not_modified && cached_resource && cached_resource->EncodedSize())
    DidReceiveData(identifier, loader, nullptr, cached_resource->EncodedSize());
}

static bool IsErrorStatusCode(int status_code) {
  return status_code >= 400;
}

void InspectorNetworkAgent::DidReceiveData(unsigned long identifier,
                                           DocumentLoader* loader,
                                           const char* data,
                                           int data_length) {
  String request_id = IdentifiersFactory::RequestId(identifier);

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
      request_id, MonotonicallyIncreasingTime(), data_length,
      resources_data_->GetAndClearPendingEncodedDataLength(request_id));
}

void InspectorNetworkAgent::DidReceiveEncodedDataLength(
    unsigned long identifier,
    int encoded_data_length) {
  String request_id = IdentifiersFactory::RequestId(identifier);
  resources_data_->AddPendingEncodedDataLength(request_id, encoded_data_length);
}

void InspectorNetworkAgent::DidFinishLoading(unsigned long identifier,
                                             DocumentLoader*,
                                             double monotonic_finish_time,
                                             int64_t encoded_data_length,
                                             int64_t decoded_body_length) {
  String request_id = IdentifiersFactory::RequestId(identifier);
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);

  int pending_encoded_data_length =
      resources_data_->GetAndClearPendingEncodedDataLength(request_id);
  if (pending_encoded_data_length > 0) {
    GetFrontend()->dataReceived(request_id, MonotonicallyIncreasingTime(), 0,
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
  if (!monotonic_finish_time)
    monotonic_finish_time = MonotonicallyIncreasingTime();
  GetFrontend()->loadingFinished(request_id, monotonic_finish_time,
                                 encoded_data_length);
}

void InspectorNetworkAgent::DidReceiveCORSRedirectResponse(
    unsigned long identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    Resource* resource) {
  // Update the response and finish loading
  DidReceiveResourceResponse(identifier, loader, response, resource);
  DidFinishLoading(identifier, loader, 0,
                   WebURLLoaderClient::kUnknownEncodedDataLength, 0);
}

void InspectorNetworkAgent::DidFailLoading(unsigned long identifier,
                                           DocumentLoader*,
                                           const ResourceError& error) {
  String request_id = IdentifiersFactory::RequestId(identifier);
  bool canceled = error.IsCancellation();
  GetFrontend()->loadingFailed(
      request_id, MonotonicallyIncreasingTime(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      error.LocalizedDescription(), canceled);
}

void InspectorNetworkAgent::ScriptImported(unsigned long identifier,
                                           const String& source_string) {
  resources_data_->SetResourceContent(IdentifiersFactory::RequestId(identifier),
                                      source_string);
}

void InspectorNetworkAgent::DidReceiveScriptResponse(unsigned long identifier) {
  resources_data_->SetResourceType(IdentifiersFactory::RequestId(identifier),
                                   InspectorPageAgent::kScriptResource);
}

void InspectorNetworkAgent::ClearPendingRequestData() {
  if (pending_request_type_ == InspectorPageAgent::kXHRResource)
    pending_xhr_replay_data_.Clear();
  pending_request_ = nullptr;
}

void InspectorNetworkAgent::DocumentThreadableLoaderStartedLoadingForClient(
    unsigned long identifier,
    ThreadableLoaderClient* client) {
  if (!client)
    return;
  if (client != pending_request_) {
    DCHECK(!pending_request_);
    return;
  }

  known_request_id_map_.Set(client, identifier);
  String request_id = IdentifiersFactory::RequestId(identifier);
  resources_data_->SetResourceType(request_id, pending_request_type_);
  if (pending_request_type_ == InspectorPageAgent::kXHRResource) {
    resources_data_->SetXHRReplayData(request_id,
                                      pending_xhr_replay_data_.Get());
  }

  ClearPendingRequestData();
}

void InspectorNetworkAgent::
    DocumentThreadableLoaderFailedToStartLoadingForClient(
        ThreadableLoaderClient* client) {
  if (!client)
    return;
  if (client != pending_request_) {
    DCHECK(!pending_request_);
    return;
  }

  ClearPendingRequestData();
}

void InspectorNetworkAgent::WillLoadXHR(
    XMLHttpRequest* xhr,
    ThreadableLoaderClient* client,
    const AtomicString& method,
    const KURL& url,
    bool async,
    scoped_refptr<EncodedFormData> form_data,
    const HTTPHeaderMap& headers,
    bool include_credentials) {
  DCHECK(xhr);
  DCHECK(!pending_request_);
  pending_request_ = client;
  pending_request_type_ = InspectorPageAgent::kXHRResource;
  pending_xhr_replay_data_ = XHRReplayData::Create(
      xhr->GetExecutionContext(), method, UrlWithoutFragment(url), async,
      form_data.get(), include_credentials);
  for (const auto& header : headers)
    pending_xhr_replay_data_->AddHeader(header.key, header.value);
}

void InspectorNetworkAgent::DelayedRemoveReplayXHR(XMLHttpRequest* xhr) {
  if (!replay_xhrs_.Contains(xhr))
    return;
  replay_xhrs_to_be_deleted_.insert(xhr);
  replay_xhrs_.erase(xhr);
  remove_finished_replay_xhr_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void InspectorNetworkAgent::DidFailXHRLoading(ExecutionContext* context,
                                              XMLHttpRequest* xhr,
                                              ThreadableLoaderClient* client,
                                              const AtomicString& method,
                                              const String& url) {
  DidFinishXHRInternal(context, xhr, client, method, url, false);
}

void InspectorNetworkAgent::DidFinishXHRLoading(ExecutionContext* context,
                                                XMLHttpRequest* xhr,
                                                ThreadableLoaderClient* client,
                                                const AtomicString& method,
                                                const String& url) {
  DidFinishXHRInternal(context, xhr, client, method, url, true);
}

void InspectorNetworkAgent::DidFinishXHRInternal(ExecutionContext* context,
                                                 XMLHttpRequest* xhr,
                                                 ThreadableLoaderClient* client,
                                                 const AtomicString& method,
                                                 const String& url,
                                                 bool success) {
  ClearPendingRequestData();

  // This method will be called from the XHR.
  // We delay deleting the replay XHR, as deleting here may delete the caller.
  DelayedRemoveReplayXHR(xhr);

  ThreadableLoaderClientRequestIdMap::iterator it =
      known_request_id_map_.find(client);
  if (it == known_request_id_map_.end())
    return;
  known_request_id_map_.erase(client);
}

void InspectorNetworkAgent::WillStartFetch(ThreadableLoaderClient* client) {
  DCHECK(!pending_request_);
  pending_request_ = client;
  pending_request_type_ = InspectorPageAgent::kFetchResource;
}

void InspectorNetworkAgent::DidFailFetch(ThreadableLoaderClient* client) {
  known_request_id_map_.erase(client);
}

void InspectorNetworkAgent::DidFinishFetch(ExecutionContext* context,
                                           ThreadableLoaderClient* client,
                                           const AtomicString& method,
                                           const String& url) {
  ThreadableLoaderClientRequestIdMap::iterator it =
      known_request_id_map_.find(client);
  if (it == known_request_id_map_.end())
    return;
  known_request_id_map_.erase(client);
}

void InspectorNetworkAgent::WillSendEventSourceRequest(
    ThreadableLoaderClient* event_source) {
  DCHECK(!pending_request_);
  pending_request_ = event_source;
  pending_request_type_ = InspectorPageAgent::kEventSourceResource;
}

void InspectorNetworkAgent::WillDispatchEventSourceEvent(
    ThreadableLoaderClient* event_source,
    const AtomicString& event_name,
    const AtomicString& event_id,
    const String& data) {
  ThreadableLoaderClientRequestIdMap::iterator it =
      known_request_id_map_.find(event_source);
  if (it == known_request_id_map_.end())
    return;
  GetFrontend()->eventSourceMessageReceived(
      IdentifiersFactory::RequestId(it->value), MonotonicallyIncreasingTime(),
      event_name.GetString(), event_id.GetString(), data);
}

void InspectorNetworkAgent::DidFinishEventSourceRequest(
    ThreadableLoaderClient* event_source) {
  known_request_id_map_.erase(event_source);
  ClearPendingRequestData();
}

void InspectorNetworkAgent::DetachClientRequest(
    ThreadableLoaderClient* client) {
  // This method is called by loader clients when finalizing
  // (i.e., from their "prefinalizers".) The client reference must
  // no longer be held onto upon completion.
  if (pending_request_ == client) {
    pending_request_ = nullptr;
    if (pending_request_type_ == InspectorPageAgent::kXHRResource) {
      pending_xhr_replay_data_.Clear();
    }
  }
  known_request_id_map_.erase(client);
}

void InspectorNetworkAgent::ApplyUserAgentOverride(String* user_agent) {
  String user_agent_override;
  state_->getString(NetworkAgentState::kUserAgentOverride,
                    &user_agent_override);
  if (!user_agent_override.IsEmpty())
    *user_agent = user_agent_override;
}

std::unique_ptr<protocol::Network::Initiator>
InspectorNetworkAgent::BuildInitiatorObject(
    Document* document,
    const FetchInitiatorInfo& initiator_info) {
  if (!initiator_info.imported_module_referrer.IsEmpty()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Script)
            .build();
    initiator_object->setUrl(initiator_info.imported_module_referrer);
    initiator_object->setLineNumber(
        initiator_info.position.line_.ZeroBasedInt());
    return initiator_object;
  }

  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      current_stack_trace =
          SourceLocation::Capture(document)->BuildInspectorObject();
  if (current_stack_trace) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Script)
            .build();
    initiator_object->setStack(std::move(current_stack_trace));
    return initiator_object;
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

void InspectorNetworkAgent::DidCreateWebSocket(Document* document,
                                               unsigned long identifier,
                                               const KURL& request_url,
                                               const String&) {
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      current_stack_trace =
          SourceLocation::Capture(document)->BuildInspectorObject();
  if (!current_stack_trace) {
    GetFrontend()->webSocketCreated(
        IdentifiersFactory::RequestId(identifier),
        UrlWithoutFragment(request_url).GetString());
    return;
  }

  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      protocol::Network::Initiator::create()
          .setType(protocol::Network::Initiator::TypeEnum::Script)
          .build();
  initiator_object->setStack(std::move(current_stack_trace));
  GetFrontend()->webSocketCreated(IdentifiersFactory::RequestId(identifier),
                                  UrlWithoutFragment(request_url).GetString(),
                                  std::move(initiator_object));
}

void InspectorNetworkAgent::WillSendWebSocketHandshakeRequest(
    Document*,
    unsigned long identifier,
    const WebSocketHandshakeRequest* request) {
  DCHECK(request);
  std::unique_ptr<protocol::Network::WebSocketRequest> request_object =
      protocol::Network::WebSocketRequest::create()
          .setHeaders(BuildObjectForHeaders(request->HeaderFields()))
          .build();
  GetFrontend()->webSocketWillSendHandshakeRequest(
      IdentifiersFactory::RequestId(identifier), MonotonicallyIncreasingTime(),
      CurrentTime(), std::move(request_object));
}

void InspectorNetworkAgent::DidReceiveWebSocketHandshakeResponse(
    Document*,
    unsigned long identifier,
    const WebSocketHandshakeRequest* request,
    const WebSocketHandshakeResponse* response) {
  DCHECK(response);
  std::unique_ptr<protocol::Network::WebSocketResponse> response_object =
      protocol::Network::WebSocketResponse::create()
          .setStatus(response->StatusCode())
          .setStatusText(response->StatusText())
          .setHeaders(BuildObjectForHeaders(response->HeaderFields()))
          .build();

  if (!response->HeadersText().IsEmpty())
    response_object->setHeadersText(response->HeadersText());
  if (request) {
    response_object->setRequestHeaders(
        BuildObjectForHeaders(request->HeaderFields()));
    if (!request->HeadersText().IsEmpty())
      response_object->setRequestHeadersText(request->HeadersText());
  }
  GetFrontend()->webSocketHandshakeResponseReceived(
      IdentifiersFactory::RequestId(identifier), MonotonicallyIncreasingTime(),
      std::move(response_object));
}

void InspectorNetworkAgent::DidCloseWebSocket(Document*,
                                              unsigned long identifier) {
  GetFrontend()->webSocketClosed(IdentifiersFactory::RequestId(identifier),
                                 MonotonicallyIncreasingTime());
}

void InspectorNetworkAgent::DidReceiveWebSocketFrame(unsigned long identifier,
                                                     int op_code,
                                                     bool masked,
                                                     const char* payload,
                                                     size_t payload_length) {
  std::unique_ptr<protocol::Network::WebSocketFrame> frame_object =
      protocol::Network::WebSocketFrame::create()
          .setOpcode(op_code)
          .setMask(masked)
          .setPayloadData(
              String::FromUTF8WithLatin1Fallback(payload, payload_length))
          .build();
  GetFrontend()->webSocketFrameReceived(
      IdentifiersFactory::RequestId(identifier), MonotonicallyIncreasingTime(),
      std::move(frame_object));
}

void InspectorNetworkAgent::DidSendWebSocketFrame(unsigned long identifier,
                                                  int op_code,
                                                  bool masked,
                                                  const char* payload,
                                                  size_t payload_length) {
  std::unique_ptr<protocol::Network::WebSocketFrame> frame_object =
      protocol::Network::WebSocketFrame::create()
          .setOpcode(op_code)
          .setMask(masked)
          .setPayloadData(
              String::FromUTF8WithLatin1Fallback(payload, payload_length))
          .build();
  GetFrontend()->webSocketFrameSent(IdentifiersFactory::RequestId(identifier),
                                    MonotonicallyIncreasingTime(),
                                    std::move(frame_object));
}

void InspectorNetworkAgent::DidReceiveWebSocketFrameError(
    unsigned long identifier,
    const String& error_message) {
  GetFrontend()->webSocketFrameError(IdentifiersFactory::RequestId(identifier),
                                     MonotonicallyIncreasingTime(),
                                     error_message);
}

Response InspectorNetworkAgent::enable(Maybe<int> total_buffer_size,
                                       Maybe<int> resource_buffer_size) {
  Enable(total_buffer_size.fromMaybe(g_maximum_total_buffer_size),
         resource_buffer_size.fromMaybe(g_maximum_resource_buffer_size));
  return Response::OK();
}

void InspectorNetworkAgent::Enable(int total_buffer_size,
                                   int resource_buffer_size) {
  if (!GetFrontend())
    return;
  resources_data_->SetResourcesDataSizeLimits(total_buffer_size,
                                              resource_buffer_size);
  state_->setBoolean(NetworkAgentState::kNetworkAgentEnabled, true);
  state_->setInteger(NetworkAgentState::kTotalBufferSize, total_buffer_size);
  state_->setInteger(NetworkAgentState::kResourceBufferSize,
                     resource_buffer_size);
  instrumenting_agents_->addInspectorNetworkAgent(this);
}

Response InspectorNetworkAgent::disable() {
  DCHECK(!pending_request_);
  state_->setBoolean(NetworkAgentState::kNetworkAgentEnabled, false);
  state_->setString(NetworkAgentState::kUserAgentOverride, "");
  instrumenting_agents_->removeInspectorNetworkAgent(this);
  resources_data_->Clear();
  known_request_id_map_.clear();
  return Response::OK();
}

Response InspectorNetworkAgent::setUserAgentOverride(const String& user_agent) {
  if (user_agent.Contains('\n') || user_agent.Contains('\r') ||
      user_agent.Contains('\0')) {
    return Response::Error("Invalid characters found in userAgent");
  }
  state_->setString(NetworkAgentState::kUserAgentOverride, user_agent);
  return Response::OK();
}

Response InspectorNetworkAgent::setExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  state_->setObject(NetworkAgentState::kExtraRequestHeaders,
                    headers->toValue());
  return Response::OK();
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
  InspectorFileReaderLoaderClient* client = new InspectorFileReaderLoaderClient(
      blob, resource_data->MimeType(), resource_data->TextEncodingName(),
      std::move(callback));
  if (worker_global_scope_) {
    client->Start(worker_global_scope_);
    return;
  }
  DCHECK(inspected_frames_);
  LocalFrame* frame = IdentifiersFactory::FrameById(inspected_frames_,
                                                    resource_data->FrameId());
  Document* document = frame->GetDocument();
  client->Start(document);
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
  if (response.isSuccess()) {
    callback->sendSuccess(content, base64_encoded);
  } else {
    callback->sendFailure(response);
  }
}

Response InspectorNetworkAgent::setBlockedURLs(
    std::unique_ptr<protocol::Array<String>> urls) {
  std::unique_ptr<protocol::DictionaryValue> new_list =
      protocol::DictionaryValue::create();
  for (size_t i = 0; i < urls->length(); i++)
    new_list->setBoolean(urls->get(i), true);
  state_->setObject(NetworkAgentState::kBlockedURLs, std::move(new_list));
  return Response::OK();
}

Response InspectorNetworkAgent::replayXHR(const String& request_id) {
  String actual_request_id = request_id;

  XHRReplayData* xhr_replay_data = resources_data_->XhrReplayData(request_id);
  if (!xhr_replay_data)
    return Response::Error("Given id does not correspond to XHR");

  ExecutionContext* execution_context = xhr_replay_data->GetExecutionContext();
  if (execution_context->IsContextDestroyed()) {
    resources_data_->SetXHRReplayData(request_id, nullptr);
    return Response::Error("Document is already detached");
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
  xhr->SendForInspectorXHRReplay(xhr_replay_data->FormData(),
                                 IGNORE_EXCEPTION_FOR_TESTING);

  replay_xhrs_.insert(xhr);
  return Response::OK();
}

Response InspectorNetworkAgent::canClearBrowserCache(bool* result) {
  *result = true;
  return Response::OK();
}

Response InspectorNetworkAgent::canClearBrowserCookies(bool* result) {
  *result = true;
  return Response::OK();
}

Response InspectorNetworkAgent::emulateNetworkConditions(
    bool offline,
    double latency,
    double download_throughput,
    double upload_throughput,
    Maybe<String> connection_type) {
  if (!IsMainThread())
    return Response::Error("Not supported");

  WebConnectionType type = kWebConnectionTypeUnknown;
  if (connection_type.isJust()) {
    type = ToWebConnectionType(connection_type.fromJust());
    if (type == kWebConnectionTypeUnknown)
      return Response::Error("Unknown connection type");
  }
  // TODO(dgozman): networkStateNotifier is per-process. It would be nice to
  // have per-frame override instead.
  if (offline || latency || download_throughput || upload_throughput)
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        !offline, type, download_throughput / (1024 * 1024 / 8));
  else
    GetNetworkStateNotifier().ClearOverride();
  return Response::OK();
}

Response InspectorNetworkAgent::setCacheDisabled(bool cache_disabled) {
  // TODO(ananta)
  // We should extract network cache state into a global entity which can be
  // queried from FrameLoader and other places.
  state_->setBoolean(NetworkAgentState::kCacheDisabled, cache_disabled);
  if (cache_disabled && IsMainThread())
    GetMemoryCache()->EvictResources();
  return Response::OK();
}

Response InspectorNetworkAgent::setBypassServiceWorker(bool bypass) {
  state_->setBoolean(NetworkAgentState::kBypassServiceWorker, bypass);
  return Response::OK();
}

Response InspectorNetworkAgent::setDataSizeLimitsForTest(int max_total,
                                                         int max_resource) {
  resources_data_->SetResourcesDataSizeLimits(max_total, max_resource);
  return Response::OK();
}

Response InspectorNetworkAgent::getCertificate(
    const String& origin,
    std::unique_ptr<protocol::Array<String>>* certificate) {
  *certificate = protocol::Array<String>::create();
  scoped_refptr<SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(origin);
  for (auto& resource : resources_data_->Resources()) {
    scoped_refptr<SecurityOrigin> resource_origin =
        SecurityOrigin::Create(resource->RequestedURL());
    if (resource_origin->IsSameSchemeHostPort(security_origin.get()) &&
        resource->Certificate().size()) {
      for (auto& cert : resource->Certificate())
        certificate->get()->addItem(Base64Encode(cert.Latin1()));
      return Response::OK();
    }
  }
  return Response::OK();
}

void InspectorNetworkAgent::DidCommitLoad(LocalFrame* frame,
                                          DocumentLoader* loader) {
  DCHECK(inspected_frames_);
  DCHECK(IsMainThread());
  if (loader->GetFrame() != inspected_frames_->Root())
    return;

  if (state_->booleanProperty(NetworkAgentState::kCacheDisabled, false))
    GetMemoryCache()->EvictResources(MemoryCache::kDoNotEvictUnusedPreloads);

  resources_data_->Clear(IdentifiersFactory::LoaderId(loader));
}

void InspectorNetworkAgent::FrameScheduledNavigation(LocalFrame* frame,
                                                     ScheduledNavigation*) {
  String frame_id = IdentifiersFactory::FrameId(frame);
  frames_with_scheduled_navigation_.insert(frame_id);
  if (!frames_with_scheduled_client_navigation_.Contains(frame_id)) {
    frame_navigation_initiator_map_.Set(
        frame_id,
        BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo()));
  }
}

void InspectorNetworkAgent::FrameClearedScheduledNavigation(LocalFrame* frame) {
  String frame_id = IdentifiersFactory::FrameId(frame);
  frames_with_scheduled_navigation_.erase(frame_id);
  if (!frames_with_scheduled_client_navigation_.Contains(frame_id))
    frame_navigation_initiator_map_.erase(frame_id);
}

void InspectorNetworkAgent::FrameScheduledClientNavigation(LocalFrame* frame) {
  String frame_id = IdentifiersFactory::FrameId(frame);
  frames_with_scheduled_client_navigation_.insert(frame_id);
  if (!frames_with_scheduled_navigation_.Contains(frame_id)) {
    frame_navigation_initiator_map_.Set(
        frame_id,
        BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo()));
  }
}

void InspectorNetworkAgent::FrameClearedScheduledClientNavigation(
    LocalFrame* frame) {
  String frame_id = IdentifiersFactory::FrameId(frame);
  frames_with_scheduled_client_navigation_.erase(frame_id);
  if (!frames_with_scheduled_navigation_.Contains(frame_id))
    frame_navigation_initiator_map_.erase(frame_id);
}

void InspectorNetworkAgent::SetHostId(const String& host_id) {
  host_id_ = host_id;
}

Response InspectorNetworkAgent::GetResponseBody(const String& request_id,
                                                String* content,
                                                bool* base64_encoded) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  if (!resource_data) {
    return Response::Error("No resource with given identifier found");
  }

  if (resource_data->HasContent()) {
    *content = resource_data->Content();
    *base64_encoded = resource_data->Base64Encoded();
    return Response::OK();
  }

  if (resource_data->IsContentEvicted()) {
    return Response::Error("Request content was evicted from inspector cache");
  }

  if (resource_data->Buffer() && !resource_data->TextEncodingName().IsNull()) {
    bool success = InspectorPageAgent::SharedBufferContent(
        resource_data->Buffer(), resource_data->MimeType(),
        resource_data->TextEncodingName(), content, base64_encoded);
    DCHECK(success);
    return Response::OK();
  }

  if (resource_data->CachedResource() &&
      InspectorPageAgent::CachedResourceContent(resource_data->CachedResource(),
                                                content, base64_encoded)) {
    return Response::OK();
  }

  return Response::Error("No data found for resource with given identifier");
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

bool InspectorNetworkAgent::CacheDisabled() {
  return state_->booleanProperty(NetworkAgentState::kNetworkAgentEnabled,
                                 false) &&
         state_->booleanProperty(NetworkAgentState::kCacheDisabled, false);
}

void InspectorNetworkAgent::RemoveFinishedReplayXHRFired(TimerBase*) {
  replay_xhrs_to_be_deleted_.clear();
}

InspectorNetworkAgent::InspectorNetworkAgent(
    InspectedFrames* inspected_frames,
    WorkerGlobalScope* worker_global_scope)
    : inspected_frames_(inspected_frames),
      worker_global_scope_(worker_global_scope),
      resources_data_(
          NetworkResourcesData::Create(g_maximum_total_buffer_size,
                                       g_maximum_resource_buffer_size)),
      pending_request_(nullptr),
      remove_finished_replay_xhr_timer_(
          inspected_frames ? TaskRunnerHelper::Get(TaskType::kUnspecedLoading,
                                                   inspected_frames->Root())
                           : TaskRunnerHelper::Get(TaskType::kUnspecedLoading,
                                                   worker_global_scope),
          this,
          &InspectorNetworkAgent::RemoveFinishedReplayXHRFired) {
  DCHECK((IsMainThread() && inspected_frames_ && !worker_global_scope_) ||
         (!IsMainThread() && !inspected_frames_ && worker_global_scope_));
}

void InspectorNetworkAgent::ShouldForceCORSPreflight(bool* result) {
  if (state_->booleanProperty(NetworkAgentState::kCacheDisabled, false))
    *result = true;
}

}  // namespace blink
