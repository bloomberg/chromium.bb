// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/request_or_usv_string.h"
#include "core/CoreExport.h"
#include "core/fetch/Body.h"
#include "core/fetch/FetchRequestData.h"
#include "core/fetch/Headers.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/interfaces/fetch_api.mojom-shared.h"

namespace blink {

class BodyStreamBuffer;
class RequestInit;
class WebServiceWorkerRequest;

using RequestInfo = RequestOrUSVString;

class CORE_EXPORT Request final : public Body {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(Request);

 public:
  // These "create" function must be called with entering an appropriate
  // V8 context.
  // From Request.idl:
  static Request* Create(ScriptState*,
                         const RequestInfo&,
                         const Dictionary&,
                         ExceptionState&);

  static Request* Create(ScriptState*, const String&, ExceptionState&);
  static Request* Create(ScriptState*,
                         const String&,
                         const Dictionary&,
                         ExceptionState&);
  static Request* Create(ScriptState*, Request*, ExceptionState&);
  static Request* Create(ScriptState*,
                         Request*,
                         const Dictionary&,
                         ExceptionState&);
  static Request* Create(ScriptState*, FetchRequestData*);
  static Request* Create(ScriptState*, const WebServiceWorkerRequest&);

  // Returns false if |credentials_mode| doesn't represent a valid credentials
  // mode.
  static bool ParseCredentialsMode(const String& credentials_mode,
                                   network::mojom::FetchCredentialsMode*);

  // From Request.idl:
  String method() const;
  KURL url() const;
  Headers* getHeaders() const { return headers_; }
  String destination() const;
  String referrer() const;
  String getReferrerPolicy() const;
  String mode() const;
  String credentials() const;
  String cache() const;
  String redirect() const;
  String integrity() const;
  bool keepalive() const;

  // From Request.idl:
  // This function must be called with entering an appropriate V8 context.
  Request* clone(ScriptState*, ExceptionState&);

  FetchRequestData* PassRequestData(ScriptState*);
  void PopulateWebServiceWorkerRequest(WebServiceWorkerRequest&) const;
  bool HasBody() const;
  BodyStreamBuffer* BodyBuffer() override { return request_->Buffer(); }
  const BodyStreamBuffer* BodyBuffer() const override {
    return request_->Buffer();
  }

  void Trace(blink::Visitor*) override;

 private:
  Request(ScriptState*, FetchRequestData*, Headers*);
  Request(ScriptState*, FetchRequestData*);

  const FetchRequestData* GetRequest() const { return request_; }
  static Request* CreateRequestWithRequestOrString(ScriptState*,
                                                   Request*,
                                                   const String&,
                                                   RequestInit&,
                                                   ExceptionState&);

  String ContentType() const override;
  String MimeType() const override;
  void RefreshBody(ScriptState*);

  const Member<FetchRequestData> request_;
  const Member<Headers> headers_;
};

}  // namespace blink

#endif  // Request_h
