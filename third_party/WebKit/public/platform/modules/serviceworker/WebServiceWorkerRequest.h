// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRequest_h
#define WebServiceWorkerRequest_h

#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebReferrerPolicy.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "services/network/public/interfaces/request_context_frame_type.mojom-shared.h"

#if INSIDE_BLINK
#include <utility>
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/StringHash.h"
#include "third_party/WebKit/common/blob/blob.mojom-blink.h"  // nogncheck
#endif

namespace blink {

class BlobDataHandle;
class WebHTTPHeaderVisitor;
class WebServiceWorkerRequestPrivate;

// Represents a request for a web resource.
class BLINK_PLATFORM_EXPORT WebServiceWorkerRequest {
 public:
  ~WebServiceWorkerRequest() { Reset(); }
  WebServiceWorkerRequest();
  WebServiceWorkerRequest(const WebServiceWorkerRequest& other) {
    Assign(other);
  }
  WebServiceWorkerRequest& operator=(const WebServiceWorkerRequest& other) {
    Assign(other);
    return *this;
  }

  void Reset();
  void Assign(const WebServiceWorkerRequest&);

  void SetURL(const WebURL&);
  const WebURL& Url() const;

  void SetMethod(const WebString&);
  const WebString& Method() const;

  void SetHeader(const WebString& key, const WebString& value);

  // If the key already exists, the value is appended to the existing value
  // with a comma delimiter between them.
  void AppendHeader(const WebString& key, const WebString& value);

  void VisitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

  // There are two ways of representing body: WebHTTPBody or Blob.  Only one
  // should be used.
  void SetBody(const WebHTTPBody&);
  WebHTTPBody Body() const;
  void SetBlob(const WebString& uuid,
               long long size,
               mojo::ScopedMessagePipeHandle);

  void SetReferrer(const WebString&, WebReferrerPolicy);
  WebURL ReferrerUrl() const;
  WebReferrerPolicy GetReferrerPolicy() const;

  void SetMode(network::mojom::FetchRequestMode);
  network::mojom::FetchRequestMode Mode() const;

  void SetIsMainResourceLoad(bool);
  bool IsMainResourceLoad() const;

  void SetCredentialsMode(network::mojom::FetchCredentialsMode);
  network::mojom::FetchCredentialsMode CredentialsMode() const;

  void SetIntegrity(const WebString&);
  const WebString& Integrity() const;

  void SetCacheMode(mojom::FetchCacheMode);
  mojom::FetchCacheMode CacheMode() const;

  void SetKeepalive(bool);
  bool Keepalive() const;

  void SetRedirectMode(network::mojom::FetchRedirectMode);
  network::mojom::FetchRedirectMode RedirectMode() const;

  void SetRequestContext(WebURLRequest::RequestContext);
  WebURLRequest::RequestContext GetRequestContext() const;

  void SetFrameType(network::mojom::RequestContextFrameType);
  network::mojom::RequestContextFrameType GetFrameType() const;

  void SetClientId(const WebString&);
  const WebString& ClientId() const;

  void SetIsReload(bool);
  bool IsReload() const;

#if INSIDE_BLINK
  const HTTPHeaderMap& Headers() const;
  scoped_refptr<BlobDataHandle> GetBlobDataHandle() const;
  const Referrer& GetReferrer() const;
  void SetBlob(const WebString& uuid,
               long long size,
               mojom::blink::BlobPtrInfo);
#endif

 private:
  WebPrivatePtr<WebServiceWorkerRequestPrivate> private_;
};

}  // namespace blink

#endif  // WebServiceWorkerRequest_h
