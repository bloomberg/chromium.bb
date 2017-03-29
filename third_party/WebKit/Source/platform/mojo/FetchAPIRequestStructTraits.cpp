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

namespace {

// Struct traits context for the FetchAPIRequest type. Since getters are invoked
// twice when serializing the type, this reduces the load for heavy members.
struct FetchAPIRequestStructTraitsContext {
  FetchAPIRequestStructTraitsContext() = default;
  ~FetchAPIRequestStructTraitsContext() = default;

  WTF::HashMap<WTF::String, WTF::String> headers;
};

}  // namespace

using blink::mojom::FetchCredentialsMode;
using blink::mojom::FetchRedirectMode;
using blink::mojom::FetchRequestMode;
using blink::mojom::RequestContextFrameType;
using blink::mojom::RequestContextType;

FetchCredentialsMode
EnumTraits<FetchCredentialsMode, blink::WebURLRequest::FetchCredentialsMode>::
    ToMojom(blink::WebURLRequest::FetchCredentialsMode input) {
  switch (input) {
    case blink::WebURLRequest::FetchCredentialsModeOmit:
      return FetchCredentialsMode::OMIT;
    case blink::WebURLRequest::FetchCredentialsModeSameOrigin:
      return FetchCredentialsMode::SAME_ORIGIN;
    case blink::WebURLRequest::FetchCredentialsModeInclude:
      return FetchCredentialsMode::INCLUDE;
    case blink::WebURLRequest::FetchCredentialsModePassword:
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
      *out = blink::WebURLRequest::FetchCredentialsModeOmit;
      return true;
    case FetchCredentialsMode::SAME_ORIGIN:
      *out = blink::WebURLRequest::FetchCredentialsModeSameOrigin;
      return true;
    case FetchCredentialsMode::INCLUDE:
      *out = blink::WebURLRequest::FetchCredentialsModeInclude;
      return true;
    case FetchCredentialsMode::PASSWORD:
      *out = blink::WebURLRequest::FetchCredentialsModePassword;
      return true;
  }

  return false;
}

FetchRedirectMode
EnumTraits<FetchRedirectMode, blink::WebURLRequest::FetchRedirectMode>::ToMojom(
    blink::WebURLRequest::FetchRedirectMode input) {
  switch (input) {
    case blink::WebURLRequest::FetchRedirectModeFollow:
      return FetchRedirectMode::FOLLOW;
    case blink::WebURLRequest::FetchRedirectModeError:
      return FetchRedirectMode::ERROR_MODE;
    case blink::WebURLRequest::FetchRedirectModeManual:
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
      *out = blink::WebURLRequest::FetchRedirectModeFollow;
      return true;
    case FetchRedirectMode::ERROR_MODE:
      *out = blink::WebURLRequest::FetchRedirectModeError;
      return true;
    case FetchRedirectMode::MANUAL:
      *out = blink::WebURLRequest::FetchRedirectModeManual;
      return true;
  }

  return false;
}

FetchRequestMode
EnumTraits<FetchRequestMode, blink::WebURLRequest::FetchRequestMode>::ToMojom(
    blink::WebURLRequest::FetchRequestMode input) {
  switch (input) {
    case blink::WebURLRequest::FetchRequestModeSameOrigin:
      return FetchRequestMode::SAME_ORIGIN;
    case blink::WebURLRequest::FetchRequestModeNoCORS:
      return FetchRequestMode::NO_CORS;
    case blink::WebURLRequest::FetchRequestModeCORS:
      return FetchRequestMode::CORS;
    case blink::WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
      return FetchRequestMode::CORS_WITH_FORCED_PREFLIGHT;
    case blink::WebURLRequest::FetchRequestModeNavigate:
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
      *out = blink::WebURLRequest::FetchRequestModeSameOrigin;
      return true;
    case FetchRequestMode::NO_CORS:
      *out = blink::WebURLRequest::FetchRequestModeNoCORS;
      return true;
    case FetchRequestMode::CORS:
      *out = blink::WebURLRequest::FetchRequestModeCORS;
      return true;
    case FetchRequestMode::CORS_WITH_FORCED_PREFLIGHT:
      *out = blink::WebURLRequest::FetchRequestModeCORSWithForcedPreflight;
      return true;
    case FetchRequestMode::NAVIGATE:
      *out = blink::WebURLRequest::FetchRequestModeNavigate;
      return true;
  }

  return false;
}

RequestContextFrameType
EnumTraits<RequestContextFrameType, blink::WebURLRequest::FrameType>::ToMojom(
    blink::WebURLRequest::FrameType input) {
  switch (input) {
    case blink::WebURLRequest::FrameTypeAuxiliary:
      return RequestContextFrameType::AUXILIARY;
    case blink::WebURLRequest::FrameTypeNested:
      return RequestContextFrameType::NESTED;
    case blink::WebURLRequest::FrameTypeNone:
      return RequestContextFrameType::NONE;
    case blink::WebURLRequest::FrameTypeTopLevel:
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
      *out = blink::WebURLRequest::FrameTypeAuxiliary;
      return true;
    case RequestContextFrameType::NESTED:
      *out = blink::WebURLRequest::FrameTypeNested;
      return true;
    case RequestContextFrameType::NONE:
      *out = blink::WebURLRequest::FrameTypeNone;
      return true;
    case RequestContextFrameType::TOP_LEVEL:
      *out = blink::WebURLRequest::FrameTypeTopLevel;
      return true;
  }

  return false;
}

RequestContextType
EnumTraits<RequestContextType, blink::WebURLRequest::RequestContext>::ToMojom(
    blink::WebURLRequest::RequestContext input) {
  switch (input) {
    case blink::WebURLRequest::RequestContextUnspecified:
      return RequestContextType::UNSPECIFIED;
    case blink::WebURLRequest::RequestContextAudio:
      return RequestContextType::AUDIO;
    case blink::WebURLRequest::RequestContextBeacon:
      return RequestContextType::BEACON;
    case blink::WebURLRequest::RequestContextCSPReport:
      return RequestContextType::CSP_REPORT;
    case blink::WebURLRequest::RequestContextDownload:
      return RequestContextType::DOWNLOAD;
    case blink::WebURLRequest::RequestContextEmbed:
      return RequestContextType::EMBED;
    case blink::WebURLRequest::RequestContextEventSource:
      return RequestContextType::EVENT_SOURCE;
    case blink::WebURLRequest::RequestContextFavicon:
      return RequestContextType::FAVICON;
    case blink::WebURLRequest::RequestContextFetch:
      return RequestContextType::FETCH;
    case blink::WebURLRequest::RequestContextFont:
      return RequestContextType::FONT;
    case blink::WebURLRequest::RequestContextForm:
      return RequestContextType::FORM;
    case blink::WebURLRequest::RequestContextFrame:
      return RequestContextType::FRAME;
    case blink::WebURLRequest::RequestContextHyperlink:
      return RequestContextType::HYPERLINK;
    case blink::WebURLRequest::RequestContextIframe:
      return RequestContextType::IFRAME;
    case blink::WebURLRequest::RequestContextImage:
      return RequestContextType::IMAGE;
    case blink::WebURLRequest::RequestContextImageSet:
      return RequestContextType::IMAGE_SET;
    case blink::WebURLRequest::RequestContextImport:
      return RequestContextType::IMPORT;
    case blink::WebURLRequest::RequestContextInternal:
      return RequestContextType::INTERNAL;
    case blink::WebURLRequest::RequestContextLocation:
      return RequestContextType::LOCATION;
    case blink::WebURLRequest::RequestContextManifest:
      return RequestContextType::MANIFEST;
    case blink::WebURLRequest::RequestContextObject:
      return RequestContextType::OBJECT;
    case blink::WebURLRequest::RequestContextPing:
      return RequestContextType::PING;
    case blink::WebURLRequest::RequestContextPlugin:
      return RequestContextType::PLUGIN;
    case blink::WebURLRequest::RequestContextPrefetch:
      return RequestContextType::PREFETCH;
    case blink::WebURLRequest::RequestContextScript:
      return RequestContextType::SCRIPT;
    case blink::WebURLRequest::RequestContextServiceWorker:
      return RequestContextType::SERVICE_WORKER;
    case blink::WebURLRequest::RequestContextSharedWorker:
      return RequestContextType::SHARED_WORKER;
    case blink::WebURLRequest::RequestContextSubresource:
      return RequestContextType::SUBRESOURCE;
    case blink::WebURLRequest::RequestContextStyle:
      return RequestContextType::STYLE;
    case blink::WebURLRequest::RequestContextTrack:
      return RequestContextType::TRACK;
    case blink::WebURLRequest::RequestContextVideo:
      return RequestContextType::VIDEO;
    case blink::WebURLRequest::RequestContextWorker:
      return RequestContextType::WORKER;
    case blink::WebURLRequest::RequestContextXMLHttpRequest:
      return RequestContextType::XML_HTTP_REQUEST;
    case blink::WebURLRequest::RequestContextXSLT:
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
      *out = blink::WebURLRequest::RequestContextUnspecified;
      return true;
    case RequestContextType::AUDIO:
      *out = blink::WebURLRequest::RequestContextAudio;
      return true;
    case RequestContextType::BEACON:
      *out = blink::WebURLRequest::RequestContextBeacon;
      return true;
    case RequestContextType::CSP_REPORT:
      *out = blink::WebURLRequest::RequestContextCSPReport;
      return true;
    case RequestContextType::DOWNLOAD:
      *out = blink::WebURLRequest::RequestContextDownload;
      return true;
    case RequestContextType::EMBED:
      *out = blink::WebURLRequest::RequestContextEmbed;
      return true;
    case RequestContextType::EVENT_SOURCE:
      *out = blink::WebURLRequest::RequestContextEventSource;
      return true;
    case RequestContextType::FAVICON:
      *out = blink::WebURLRequest::RequestContextFavicon;
      return true;
    case RequestContextType::FETCH:
      *out = blink::WebURLRequest::RequestContextFetch;
      return true;
    case RequestContextType::FONT:
      *out = blink::WebURLRequest::RequestContextFont;
      return true;
    case RequestContextType::FORM:
      *out = blink::WebURLRequest::RequestContextForm;
      return true;
    case RequestContextType::FRAME:
      *out = blink::WebURLRequest::RequestContextFrame;
      return true;
    case RequestContextType::HYPERLINK:
      *out = blink::WebURLRequest::RequestContextHyperlink;
      return true;
    case RequestContextType::IFRAME:
      *out = blink::WebURLRequest::RequestContextIframe;
      return true;
    case RequestContextType::IMAGE:
      *out = blink::WebURLRequest::RequestContextImage;
      return true;
    case RequestContextType::IMAGE_SET:
      *out = blink::WebURLRequest::RequestContextImageSet;
      return true;
    case RequestContextType::IMPORT:
      *out = blink::WebURLRequest::RequestContextImport;
      return true;
    case RequestContextType::INTERNAL:
      *out = blink::WebURLRequest::RequestContextInternal;
      return true;
    case RequestContextType::LOCATION:
      *out = blink::WebURLRequest::RequestContextLocation;
      return true;
    case RequestContextType::MANIFEST:
      *out = blink::WebURLRequest::RequestContextManifest;
      return true;
    case RequestContextType::OBJECT:
      *out = blink::WebURLRequest::RequestContextObject;
      return true;
    case RequestContextType::PING:
      *out = blink::WebURLRequest::RequestContextPing;
      return true;
    case RequestContextType::PLUGIN:
      *out = blink::WebURLRequest::RequestContextPlugin;
      return true;
    case RequestContextType::PREFETCH:
      *out = blink::WebURLRequest::RequestContextPrefetch;
      return true;
    case RequestContextType::SCRIPT:
      *out = blink::WebURLRequest::RequestContextScript;
      return true;
    case RequestContextType::SERVICE_WORKER:
      *out = blink::WebURLRequest::RequestContextServiceWorker;
      return true;
    case RequestContextType::SHARED_WORKER:
      *out = blink::WebURLRequest::RequestContextSharedWorker;
      return true;
    case RequestContextType::SUBRESOURCE:
      *out = blink::WebURLRequest::RequestContextSubresource;
      return true;
    case RequestContextType::STYLE:
      *out = blink::WebURLRequest::RequestContextStyle;
      return true;
    case RequestContextType::TRACK:
      *out = blink::WebURLRequest::RequestContextTrack;
      return true;
    case RequestContextType::VIDEO:
      *out = blink::WebURLRequest::RequestContextVideo;
      return true;
    case RequestContextType::WORKER:
      *out = blink::WebURLRequest::RequestContextWorker;
      return true;
    case RequestContextType::XML_HTTP_REQUEST:
      *out = blink::WebURLRequest::RequestContextXMLHttpRequest;
      return true;
    case RequestContextType::XSLT:
      *out = blink::WebURLRequest::RequestContextXSLT;
      return true;
  }

  return false;
}

// static
void* StructTraits<blink::mojom::FetchAPIRequestDataView,
                   blink::WebServiceWorkerRequest>::
    SetUpContext(const blink::WebServiceWorkerRequest& request) {
  FetchAPIRequestStructTraitsContext* context =
      new FetchAPIRequestStructTraitsContext();
  for (const auto& pair : request.headers())
    context->headers.insert(pair.key, pair.value);

  return context;
}

// static
void StructTraits<blink::mojom::FetchAPIRequestDataView,
                  blink::WebServiceWorkerRequest>::
    TearDownContext(const blink::WebServiceWorkerRequest& request,
                    void* context) {
  delete static_cast<FetchAPIRequestStructTraitsContext*>(context);
}

// static
blink::KURL StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    url(const blink::WebServiceWorkerRequest& request) {
  return request.url();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    method(const blink::WebServiceWorkerRequest& request) {
  return request.method();
}

// static
const WTF::HashMap<WTF::String, WTF::String>&
StructTraits<blink::mojom::FetchAPIRequestDataView,
             blink::WebServiceWorkerRequest>::
    headers(const blink::WebServiceWorkerRequest& request, void* context) {
  DCHECK(context);
  return static_cast<FetchAPIRequestStructTraitsContext*>(context)->headers;
}

// static
const blink::Referrer& StructTraits<blink::mojom::FetchAPIRequestDataView,
                                    blink::WebServiceWorkerRequest>::
    referrer(const blink::WebServiceWorkerRequest& request) {
  return request.referrer();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    blob_uuid(const blink::WebServiceWorkerRequest& request) {
  if (request.blobDataHandle())
    return request.blobDataHandle()->uuid();

  return WTF::String();
}

// static
uint64_t StructTraits<blink::mojom::FetchAPIRequestDataView,
                      blink::WebServiceWorkerRequest>::
    blob_size(const blink::WebServiceWorkerRequest& request) {
  if (request.blobDataHandle())
    return request.blobDataHandle()->size();

  return 0;
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    client_id(const blink::WebServiceWorkerRequest& request) {
  return request.clientId();
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
  blink::Referrer referrer;
  blink::WebURLRequest::FetchCredentialsMode credentialsMode;
  blink::WebURLRequest::FetchRedirectMode redirectMode;
  WTF::String clientId;

  if (!data.ReadMode(&mode) || !data.ReadRequestContextType(&requestContext) ||
      !data.ReadFrameType(&frameType) || !data.ReadUrl(&url) ||
      !data.ReadMethod(&method) || !data.ReadHeaders(&headers) ||
      !data.ReadBlobUuid(&blobUuid) || !data.ReadReferrer(&referrer) ||
      !data.ReadCredentialsMode(&credentialsMode) ||
      !data.ReadRedirectMode(&redirectMode) || !data.ReadClientId(&clientId)) {
    return false;
  }

  out->setMode(mode);
  out->setIsMainResourceLoad(data.is_main_resource_load());
  out->setRequestContext(requestContext);
  out->setFrameType(frameType);
  out->setURL(url);
  out->setMethod(method);
  for (const auto& pair : headers)
    out->setHeader(pair.key, pair.value);
  out->setBlob(blobUuid, static_cast<long long>(data.blob_size()));
  out->setReferrer(referrer.referrer, static_cast<blink::WebReferrerPolicy>(
                                          referrer.referrerPolicy));
  out->setCredentialsMode(credentialsMode);
  out->setRedirectMode(redirectMode);
  out->setClientId(clientId);
  out->setIsReload(data.is_reload());
  return true;
}

}  // namespace mojo
