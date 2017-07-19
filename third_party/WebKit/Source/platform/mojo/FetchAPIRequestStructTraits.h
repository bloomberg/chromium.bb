// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchAPIRequestStructTraits_h
#define FetchAPIRequestStructTraits_h

#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/fetch/fetch_api_request.mojom-blink.h"

namespace blink {
class KURL;
}

namespace mojo {

template <>
struct EnumTraits<blink::mojom::FetchCredentialsMode,
                  blink::WebURLRequest::FetchCredentialsMode> {
  static blink::mojom::FetchCredentialsMode ToMojom(
      blink::WebURLRequest::FetchCredentialsMode input);

  static bool FromMojom(blink::mojom::FetchCredentialsMode input,
                        blink::WebURLRequest::FetchCredentialsMode* out);
};

template <>
struct EnumTraits<blink::mojom::FetchRedirectMode,
                  blink::WebURLRequest::FetchRedirectMode> {
  static blink::mojom::FetchRedirectMode ToMojom(
      blink::WebURLRequest::FetchRedirectMode input);

  static bool FromMojom(blink::mojom::FetchRedirectMode input,
                        blink::WebURLRequest::FetchRedirectMode* out);
};

template <>
struct EnumTraits<blink::mojom::FetchRequestMode,
                  blink::WebURLRequest::FetchRequestMode> {
  static blink::mojom::FetchRequestMode ToMojom(
      blink::WebURLRequest::FetchRequestMode input);

  static bool FromMojom(blink::mojom::FetchRequestMode input,
                        blink::WebURLRequest::FetchRequestMode* out);
};

template <>
struct EnumTraits<blink::mojom::RequestContextFrameType,
                  blink::WebURLRequest::FrameType> {
  static blink::mojom::RequestContextFrameType ToMojom(
      blink::WebURLRequest::FrameType input);

  static bool FromMojom(blink::mojom::RequestContextFrameType input,
                        blink::WebURLRequest::FrameType* out);
};

template <>
struct EnumTraits<blink::mojom::RequestContextType,
                  blink::WebURLRequest::RequestContext> {
  static blink::mojom::RequestContextType ToMojom(
      blink::WebURLRequest::RequestContext input);

  static bool FromMojom(blink::mojom::RequestContextType input,
                        blink::WebURLRequest::RequestContext* out);
};

template <>
struct StructTraits<blink::mojom::FetchAPIRequestDataView,
                    blink::WebServiceWorkerRequest> {
  static void* SetUpContext(const blink::WebServiceWorkerRequest&);
  static void TearDownContext(const blink::WebServiceWorkerRequest&,
                              void* context);

  static blink::WebURLRequest::FetchRequestMode mode(
      const blink::WebServiceWorkerRequest& request) {
    return request.Mode();
  }

  static bool is_main_resource_load(
      const blink::WebServiceWorkerRequest& request) {
    return request.IsMainResourceLoad();
  }

  static blink::WebURLRequest::RequestContext request_context_type(
      const blink::WebServiceWorkerRequest& request) {
    return request.GetRequestContext();
  }

  static blink::WebURLRequest::FrameType frame_type(
      const blink::WebServiceWorkerRequest& request) {
    return request.GetFrameType();
  }

  static blink::KURL url(const blink::WebServiceWorkerRequest&);

  static WTF::String method(const blink::WebServiceWorkerRequest&);

  static const WTF::HashMap<WTF::String, WTF::String>& headers(
      const blink::WebServiceWorkerRequest&,
      void* context);

  static WTF::String blob_uuid(const blink::WebServiceWorkerRequest&);

  static uint64_t blob_size(const blink::WebServiceWorkerRequest&);

  static const blink::Referrer& referrer(const blink::WebServiceWorkerRequest&);

  static blink::WebURLRequest::FetchCredentialsMode credentials_mode(
      const blink::WebServiceWorkerRequest& request) {
    return request.CredentialsMode();
  }

  static blink::WebURLRequest::FetchRedirectMode redirect_mode(
      const blink::WebServiceWorkerRequest& request) {
    return request.RedirectMode();
  }

  static WTF::String integrity(const blink::WebServiceWorkerRequest&);
  static WTF::String client_id(const blink::WebServiceWorkerRequest&);

  static bool is_reload(const blink::WebServiceWorkerRequest& request) {
    return request.IsReload();
  }

  // The |fetch_type| is not used by Blink yet.
  static blink::mojom::blink::ServiceWorkerFetchType fetch_type(
      const blink::WebServiceWorkerRequest& request) {
    return blink::mojom::blink::ServiceWorkerFetchType::FETCH;
  }

  static bool Read(blink::mojom::FetchAPIRequestDataView,
                   blink::WebServiceWorkerRequest* output);
};

}  // namespace mojo

#endif  // FetchAPIRequestStructTraits_h
