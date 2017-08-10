// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/FetchAPIRequestStructTraits.h"

#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "platform/blob/BlobData.h"
#include "platform/mojo/KURLStructTraits.h"
#include "platform/mojo/ReferrerStructTraits.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebReferrerPolicy.h"

namespace mojo {

using blink::mojom::FetchCredentialsMode;
using blink::mojom::FetchRedirectMode;
using blink::mojom::FetchRequestMode;
using blink::mojom::RequestContextFrameType;
using blink::mojom::RequestContextType;

FetchCredentialsMode
EnumTraits<FetchCredentialsMode, blink::WebURLRequest::FetchCredentialsMode>::
    ToMojom(blink::WebURLRequest::FetchCredentialsMode input) {
  switch (input) {
    case blink::WebURLRequest::kFetchCredentialsModeOmit:
      return FetchCredentialsMode::OMIT;
    case blink::WebURLRequest::kFetchCredentialsModeSameOrigin:
      return FetchCredentialsMode::SAME_ORIGIN;
    case blink::WebURLRequest::kFetchCredentialsModeInclude:
      return FetchCredentialsMode::INCLUDE;
    case blink::WebURLRequest::kFetchCredentialsModePassword:
      return FetchCredentialsMode::PASSWORD;
  }

  NOTREACHED();
  return FetchCredentialsMode::OMIT;
}

bool EnumTraits<FetchCredentialsMode,
                blink::WebURLRequest::FetchCredentialsMode>::
    FromMojom(FetchCredentialsMode input,
              blink::WebURLRequest::FetchCredentialsMode* out) {
  switch (input) {
    case FetchCredentialsMode::OMIT:
      *out = blink::WebURLRequest::kFetchCredentialsModeOmit;
      return true;
    case FetchCredentialsMode::SAME_ORIGIN:
      *out = blink::WebURLRequest::kFetchCredentialsModeSameOrigin;
      return true;
    case FetchCredentialsMode::INCLUDE:
      *out = blink::WebURLRequest::kFetchCredentialsModeInclude;
      return true;
    case FetchCredentialsMode::PASSWORD:
      *out = blink::WebURLRequest::kFetchCredentialsModePassword;
      return true;
  }

  return false;
}

FetchRedirectMode
EnumTraits<FetchRedirectMode, blink::WebURLRequest::FetchRedirectMode>::ToMojom(
    blink::WebURLRequest::FetchRedirectMode input) {
  switch (input) {
    case blink::WebURLRequest::kFetchRedirectModeFollow:
      return FetchRedirectMode::FOLLOW;
    case blink::WebURLRequest::kFetchRedirectModeError:
      return FetchRedirectMode::ERROR_MODE;
    case blink::WebURLRequest::kFetchRedirectModeManual:
      return FetchRedirectMode::MANUAL;
  }

  NOTREACHED();
  return FetchRedirectMode::ERROR_MODE;
}

bool EnumTraits<FetchRedirectMode, blink::WebURLRequest::FetchRedirectMode>::
    FromMojom(FetchRedirectMode input,
              blink::WebURLRequest::FetchRedirectMode* out) {
  switch (input) {
    case FetchRedirectMode::FOLLOW:
      *out = blink::WebURLRequest::kFetchRedirectModeFollow;
      return true;
    case FetchRedirectMode::ERROR_MODE:
      *out = blink::WebURLRequest::kFetchRedirectModeError;
      return true;
    case FetchRedirectMode::MANUAL:
      *out = blink::WebURLRequest::kFetchRedirectModeManual;
      return true;
  }

  return false;
}

FetchRequestMode
EnumTraits<FetchRequestMode, blink::WebURLRequest::FetchRequestMode>::ToMojom(
    blink::WebURLRequest::FetchRequestMode input) {
  switch (input) {
    case blink::WebURLRequest::kFetchRequestModeSameOrigin:
      return FetchRequestMode::SAME_ORIGIN;
    case blink::WebURLRequest::kFetchRequestModeNoCORS:
      return FetchRequestMode::NO_CORS;
    case blink::WebURLRequest::kFetchRequestModeCORS:
      return FetchRequestMode::CORS;
    case blink::WebURLRequest::kFetchRequestModeCORSWithForcedPreflight:
      return FetchRequestMode::CORS_WITH_FORCED_PREFLIGHT;
    case blink::WebURLRequest::kFetchRequestModeNavigate:
      return FetchRequestMode::NAVIGATE;
  }

  NOTREACHED();
  return FetchRequestMode::NO_CORS;
}

bool EnumTraits<FetchRequestMode, blink::WebURLRequest::FetchRequestMode>::
    FromMojom(FetchRequestMode input,
              blink::WebURLRequest::FetchRequestMode* out) {
  switch (input) {
    case FetchRequestMode::SAME_ORIGIN:
      *out = blink::WebURLRequest::kFetchRequestModeSameOrigin;
      return true;
    case FetchRequestMode::NO_CORS:
      *out = blink::WebURLRequest::kFetchRequestModeNoCORS;
      return true;
    case FetchRequestMode::CORS:
      *out = blink::WebURLRequest::kFetchRequestModeCORS;
      return true;
    case FetchRequestMode::CORS_WITH_FORCED_PREFLIGHT:
      *out = blink::WebURLRequest::kFetchRequestModeCORSWithForcedPreflight;
      return true;
    case FetchRequestMode::NAVIGATE:
      *out = blink::WebURLRequest::kFetchRequestModeNavigate;
      return true;
  }

  return false;
}

RequestContextFrameType
EnumTraits<RequestContextFrameType, blink::WebURLRequest::FrameType>::ToMojom(
    blink::WebURLRequest::FrameType input) {
  switch (input) {
    case blink::WebURLRequest::kFrameTypeAuxiliary:
      return RequestContextFrameType::AUXILIARY;
    case blink::WebURLRequest::kFrameTypeNested:
      return RequestContextFrameType::NESTED;
    case blink::WebURLRequest::kFrameTypeNone:
      return RequestContextFrameType::NONE;
    case blink::WebURLRequest::kFrameTypeTopLevel:
      return RequestContextFrameType::TOP_LEVEL;
  }

  NOTREACHED();
  return RequestContextFrameType::NONE;
}

bool EnumTraits<RequestContextFrameType, blink::WebURLRequest::FrameType>::
    FromMojom(RequestContextFrameType input,
              blink::WebURLRequest::FrameType* out) {
  switch (input) {
    case RequestContextFrameType::AUXILIARY:
      *out = blink::WebURLRequest::kFrameTypeAuxiliary;
      return true;
    case RequestContextFrameType::NESTED:
      *out = blink::WebURLRequest::kFrameTypeNested;
      return true;
    case RequestContextFrameType::NONE:
      *out = blink::WebURLRequest::kFrameTypeNone;
      return true;
    case RequestContextFrameType::TOP_LEVEL:
      *out = blink::WebURLRequest::kFrameTypeTopLevel;
      return true;
  }

  return false;
}

RequestContextType
EnumTraits<RequestContextType, blink::WebURLRequest::RequestContext>::ToMojom(
    blink::WebURLRequest::RequestContext input) {
  switch (input) {
    case blink::WebURLRequest::kRequestContextUnspecified:
      return RequestContextType::UNSPECIFIED;
    case blink::WebURLRequest::kRequestContextAudio:
      return RequestContextType::AUDIO;
    case blink::WebURLRequest::kRequestContextBeacon:
      return RequestContextType::BEACON;
    case blink::WebURLRequest::kRequestContextCSPReport:
      return RequestContextType::CSP_REPORT;
    case blink::WebURLRequest::kRequestContextDownload:
      return RequestContextType::DOWNLOAD;
    case blink::WebURLRequest::kRequestContextEmbed:
      return RequestContextType::EMBED;
    case blink::WebURLRequest::kRequestContextEventSource:
      return RequestContextType::EVENT_SOURCE;
    case blink::WebURLRequest::kRequestContextFavicon:
      return RequestContextType::FAVICON;
    case blink::WebURLRequest::kRequestContextFetch:
      return RequestContextType::FETCH;
    case blink::WebURLRequest::kRequestContextFont:
      return RequestContextType::FONT;
    case blink::WebURLRequest::kRequestContextForm:
      return RequestContextType::FORM;
    case blink::WebURLRequest::kRequestContextFrame:
      return RequestContextType::FRAME;
    case blink::WebURLRequest::kRequestContextHyperlink:
      return RequestContextType::HYPERLINK;
    case blink::WebURLRequest::kRequestContextIframe:
      return RequestContextType::IFRAME;
    case blink::WebURLRequest::kRequestContextImage:
      return RequestContextType::IMAGE;
    case blink::WebURLRequest::kRequestContextImageSet:
      return RequestContextType::IMAGE_SET;
    case blink::WebURLRequest::kRequestContextImport:
      return RequestContextType::IMPORT;
    case blink::WebURLRequest::kRequestContextInternal:
      return RequestContextType::INTERNAL;
    case blink::WebURLRequest::kRequestContextLocation:
      return RequestContextType::LOCATION;
    case blink::WebURLRequest::kRequestContextManifest:
      return RequestContextType::MANIFEST;
    case blink::WebURLRequest::kRequestContextObject:
      return RequestContextType::OBJECT;
    case blink::WebURLRequest::kRequestContextPing:
      return RequestContextType::PING;
    case blink::WebURLRequest::kRequestContextPlugin:
      return RequestContextType::PLUGIN;
    case blink::WebURLRequest::kRequestContextPrefetch:
      return RequestContextType::PREFETCH;
    case blink::WebURLRequest::kRequestContextScript:
      return RequestContextType::SCRIPT;
    case blink::WebURLRequest::kRequestContextServiceWorker:
      return RequestContextType::SERVICE_WORKER;
    case blink::WebURLRequest::kRequestContextSharedWorker:
      return RequestContextType::SHARED_WORKER;
    case blink::WebURLRequest::kRequestContextSubresource:
      return RequestContextType::SUBRESOURCE;
    case blink::WebURLRequest::kRequestContextStyle:
      return RequestContextType::STYLE;
    case blink::WebURLRequest::kRequestContextTrack:
      return RequestContextType::TRACK;
    case blink::WebURLRequest::kRequestContextVideo:
      return RequestContextType::VIDEO;
    case blink::WebURLRequest::kRequestContextWorker:
      return RequestContextType::WORKER;
    case blink::WebURLRequest::kRequestContextXMLHttpRequest:
      return RequestContextType::XML_HTTP_REQUEST;
    case blink::WebURLRequest::kRequestContextXSLT:
      return RequestContextType::XSLT;
  }

  NOTREACHED();
  return RequestContextType::UNSPECIFIED;
}

bool EnumTraits<RequestContextType, blink::WebURLRequest::RequestContext>::
    FromMojom(RequestContextType input,
              blink::WebURLRequest::RequestContext* out) {
  switch (input) {
    case RequestContextType::UNSPECIFIED:
      *out = blink::WebURLRequest::kRequestContextUnspecified;
      return true;
    case RequestContextType::AUDIO:
      *out = blink::WebURLRequest::kRequestContextAudio;
      return true;
    case RequestContextType::BEACON:
      *out = blink::WebURLRequest::kRequestContextBeacon;
      return true;
    case RequestContextType::CSP_REPORT:
      *out = blink::WebURLRequest::kRequestContextCSPReport;
      return true;
    case RequestContextType::DOWNLOAD:
      *out = blink::WebURLRequest::kRequestContextDownload;
      return true;
    case RequestContextType::EMBED:
      *out = blink::WebURLRequest::kRequestContextEmbed;
      return true;
    case RequestContextType::EVENT_SOURCE:
      *out = blink::WebURLRequest::kRequestContextEventSource;
      return true;
    case RequestContextType::FAVICON:
      *out = blink::WebURLRequest::kRequestContextFavicon;
      return true;
    case RequestContextType::FETCH:
      *out = blink::WebURLRequest::kRequestContextFetch;
      return true;
    case RequestContextType::FONT:
      *out = blink::WebURLRequest::kRequestContextFont;
      return true;
    case RequestContextType::FORM:
      *out = blink::WebURLRequest::kRequestContextForm;
      return true;
    case RequestContextType::FRAME:
      *out = blink::WebURLRequest::kRequestContextFrame;
      return true;
    case RequestContextType::HYPERLINK:
      *out = blink::WebURLRequest::kRequestContextHyperlink;
      return true;
    case RequestContextType::IFRAME:
      *out = blink::WebURLRequest::kRequestContextIframe;
      return true;
    case RequestContextType::IMAGE:
      *out = blink::WebURLRequest::kRequestContextImage;
      return true;
    case RequestContextType::IMAGE_SET:
      *out = blink::WebURLRequest::kRequestContextImageSet;
      return true;
    case RequestContextType::IMPORT:
      *out = blink::WebURLRequest::kRequestContextImport;
      return true;
    case RequestContextType::INTERNAL:
      *out = blink::WebURLRequest::kRequestContextInternal;
      return true;
    case RequestContextType::LOCATION:
      *out = blink::WebURLRequest::kRequestContextLocation;
      return true;
    case RequestContextType::MANIFEST:
      *out = blink::WebURLRequest::kRequestContextManifest;
      return true;
    case RequestContextType::OBJECT:
      *out = blink::WebURLRequest::kRequestContextObject;
      return true;
    case RequestContextType::PING:
      *out = blink::WebURLRequest::kRequestContextPing;
      return true;
    case RequestContextType::PLUGIN:
      *out = blink::WebURLRequest::kRequestContextPlugin;
      return true;
    case RequestContextType::PREFETCH:
      *out = blink::WebURLRequest::kRequestContextPrefetch;
      return true;
    case RequestContextType::SCRIPT:
      *out = blink::WebURLRequest::kRequestContextScript;
      return true;
    case RequestContextType::SERVICE_WORKER:
      *out = blink::WebURLRequest::kRequestContextServiceWorker;
      return true;
    case RequestContextType::SHARED_WORKER:
      *out = blink::WebURLRequest::kRequestContextSharedWorker;
      return true;
    case RequestContextType::SUBRESOURCE:
      *out = blink::WebURLRequest::kRequestContextSubresource;
      return true;
    case RequestContextType::STYLE:
      *out = blink::WebURLRequest::kRequestContextStyle;
      return true;
    case RequestContextType::TRACK:
      *out = blink::WebURLRequest::kRequestContextTrack;
      return true;
    case RequestContextType::VIDEO:
      *out = blink::WebURLRequest::kRequestContextVideo;
      return true;
    case RequestContextType::WORKER:
      *out = blink::WebURLRequest::kRequestContextWorker;
      return true;
    case RequestContextType::XML_HTTP_REQUEST:
      *out = blink::WebURLRequest::kRequestContextXMLHttpRequest;
      return true;
    case RequestContextType::XSLT:
      *out = blink::WebURLRequest::kRequestContextXSLT;
      return true;
  }

  return false;
}

// static
blink::KURL StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    url(const blink::WebServiceWorkerRequest& request) {
  return request.Url();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    method(const blink::WebServiceWorkerRequest& request) {
  return request.Method();
}

// static
WTF::HashMap<WTF::String, WTF::String>
StructTraits<blink::mojom::FetchAPIRequestDataView,
             blink::WebServiceWorkerRequest>::
    headers(const blink::WebServiceWorkerRequest& request) {
  WTF::HashMap<WTF::String, WTF::String> header_map;
  for (const auto& pair : request.Headers())
    header_map.insert(pair.key, pair.value);
  return header_map;
}

// static
const blink::Referrer& StructTraits<blink::mojom::FetchAPIRequestDataView,
                                    blink::WebServiceWorkerRequest>::
    referrer(const blink::WebServiceWorkerRequest& request) {
  return request.GetReferrer();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    blob_uuid(const blink::WebServiceWorkerRequest& request) {
  if (request.GetBlobDataHandle())
    return request.GetBlobDataHandle()->Uuid();

  return WTF::String();
}

// static
uint64_t StructTraits<blink::mojom::FetchAPIRequestDataView,
                      blink::WebServiceWorkerRequest>::
    blob_size(const blink::WebServiceWorkerRequest& request) {
  if (request.GetBlobDataHandle())
    return request.GetBlobDataHandle()->size();

  return 0;
}

// static
storage::mojom::blink::BlobPtr StructTraits<
    blink::mojom::FetchAPIRequestDataView,
    blink::WebServiceWorkerRequest>::blob(const blink::WebServiceWorkerRequest&
                                              request) {
  if (request.GetBlobDataHandle()) {
    storage::mojom::blink::BlobPtr result =
        request.GetBlobDataHandle()->CloneBlobPtr();
    return result;
  }

  return nullptr;
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    integrity(const blink::WebServiceWorkerRequest& request) {
  return request.Integrity();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    client_id(const blink::WebServiceWorkerRequest& request) {
  return request.ClientId();
}

// static
bool StructTraits<blink::mojom::FetchAPIRequestDataView,
                  blink::WebServiceWorkerRequest>::
    Read(blink::mojom::FetchAPIRequestDataView data,
         blink::WebServiceWorkerRequest* out) {
  blink::WebURLRequest::FetchRequestMode mode;
  blink::WebURLRequest::RequestContext requestContext;
  blink::WebURLRequest::FrameType frameType;
  blink::KURL url;
  WTF::String method;
  WTF::HashMap<WTF::String, WTF::String> headers;
  WTF::String blobUuid;
  storage::mojom::blink::BlobPtr blob;
  blink::Referrer referrer;
  blink::WebURLRequest::FetchCredentialsMode credentialsMode;
  blink::WebURLRequest::FetchRedirectMode redirectMode;
  WTF::String integrity;
  WTF::String clientId;

  if (!data.ReadMode(&mode) || !data.ReadRequestContextType(&requestContext) ||
      !data.ReadFrameType(&frameType) || !data.ReadUrl(&url) ||
      !data.ReadMethod(&method) || !data.ReadHeaders(&headers) ||
      !data.ReadBlobUuid(&blobUuid) || !data.ReadReferrer(&referrer) ||
      !data.ReadCredentialsMode(&credentialsMode) ||
      !data.ReadRedirectMode(&redirectMode) || !data.ReadClientId(&clientId) ||
      !data.ReadIntegrity(&integrity)) {
    return false;
  }

  out->SetMode(mode);
  out->SetIsMainResourceLoad(data.is_main_resource_load());
  out->SetRequestContext(requestContext);
  out->SetFrameType(frameType);
  out->SetURL(url);
  out->SetMethod(method);
  for (const auto& pair : headers)
    out->SetHeader(pair.key, pair.value);
  out->SetBlob(blobUuid, static_cast<long long>(data.blob_size()),
               data.TakeBlob<storage::mojom::blink::BlobPtr>().PassInterface());
  out->SetReferrer(referrer.referrer, static_cast<blink::WebReferrerPolicy>(
                                          referrer.referrer_policy));
  out->SetCredentialsMode(credentialsMode);
  out->SetRedirectMode(redirectMode);
  out->SetIntegrity(integrity);
  out->SetClientId(clientId);
  out->SetIsReload(data.is_reload());
  return true;
}

}  // namespace mojo
