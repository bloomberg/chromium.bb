// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerResponse_h
#define WebServiceWorkerResponse_h

#include "base/time/time.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"

#if INSIDE_BLINK
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/StringHash.h"
#endif

namespace blink {

class BlobDataHandle;
class HTTPHeaderMap;
class WebHTTPHeaderVisitor;
class WebServiceWorkerResponsePrivate;

// Represents a response to a fetch operation. ServiceWorker uses this to
// respond to a FetchEvent dispatched by the browser. The plan is for the Cache
// and fetch() API to also use it.
class BLINK_PLATFORM_EXPORT WebServiceWorkerResponse {
 public:
  ~WebServiceWorkerResponse() { Reset(); }
  WebServiceWorkerResponse();
  WebServiceWorkerResponse(const WebServiceWorkerResponse& other) {
    Assign(other);
  }
  WebServiceWorkerResponse& operator=(const WebServiceWorkerResponse& other) {
    Assign(other);
    return *this;
  }

  void Reset();
  void Assign(const WebServiceWorkerResponse&);

  void SetURLList(const WebVector<WebURL>&);
  const WebVector<WebURL>& UrlList() const;

  void SetStatus(unsigned short);
  unsigned short Status() const;

  void SetStatusText(const WebString&);
  const WebString& StatusText() const;

  void SetResponseType(WebServiceWorkerResponseType);
  WebServiceWorkerResponseType ResponseType() const;

  void SetHeader(const WebString& key, const WebString& value);

  // If the key already exists, appends the value to the same key (comma
  // delimited) else creates a new entry.
  void AppendHeader(const WebString& key, const WebString& value);

  WebVector<WebString> GetHeaderKeys() const;
  WebString GetHeader(const WebString& key) const;
  void VisitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

  void SetBlob(const WebString& uuid, uint64_t size);
  WebString BlobUUID() const;
  uint64_t BlobSize() const;

  // Provides a more detailed error when status() is zero.
  void SetError(WebServiceWorkerResponseError);
  WebServiceWorkerResponseError GetError() const;

  void SetResponseTime(base::Time);
  base::Time ResponseTime() const;

  void SetCacheStorageCacheName(const WebString&);
  const WebString& CacheStorageCacheName() const;

  void SetCorsExposedHeaderNames(const WebVector<WebString>&);
  const WebVector<WebString>& CorsExposedHeaderNames() const;

#if INSIDE_BLINK
  const HTTPHeaderMap& Headers() const;

  void SetBlobDataHandle(PassRefPtr<BlobDataHandle>);
  PassRefPtr<BlobDataHandle> GetBlobDataHandle() const;
#endif

 private:
  WebPrivatePtr<WebServiceWorkerResponsePrivate> private_;
};

}  // namespace blink

#endif  // WebServiceWorkerResponse_h
