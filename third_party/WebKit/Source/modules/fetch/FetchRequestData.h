// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchRequestData_h
#define FetchRequestData_h

#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

class BodyStreamBuffer;
class FetchHeaderList;
class SecurityOrigin;
class ScriptState;
class WebServiceWorkerRequest;

class FetchRequestData final
    : public GarbageCollectedFinalized<FetchRequestData> {
  WTF_MAKE_NONCOPYABLE(FetchRequestData);

 public:
  enum Tainting { kBasicTainting, kCORSTainting, kOpaqueTainting };

  static FetchRequestData* Create();
  static FetchRequestData* Create(ScriptState*, const WebServiceWorkerRequest&);
  // Call Request::refreshBody() after calling clone() or pass().
  FetchRequestData* Clone(ScriptState*);
  FetchRequestData* Pass(ScriptState*);
  ~FetchRequestData();

  void SetMethod(AtomicString method) { method_ = method; }
  const AtomicString& Method() const { return method_; }
  void SetURL(const KURL& url) { url_ = url; }
  const KURL& Url() const { return url_; }
  WebURLRequest::RequestContext Context() const { return context_; }
  void SetContext(WebURLRequest::RequestContext context) { context_ = context; }
  PassRefPtr<SecurityOrigin> Origin() { return origin_; }
  void SetOrigin(PassRefPtr<SecurityOrigin> origin) {
    origin_ = std::move(origin);
  }
  bool SameOriginDataURLFlag() { return same_origin_data_url_flag_; }
  void SetSameOriginDataURLFlag(bool flag) {
    same_origin_data_url_flag_ = flag;
  }
  const Referrer& GetReferrer() const { return referrer_; }
  void SetReferrer(const Referrer& r) { referrer_ = r; }
  const AtomicString& ReferrerString() const { return referrer_.referrer; }
  void SetReferrerString(const AtomicString& s) { referrer_.referrer = s; }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_.referrer_policy; }
  void SetReferrerPolicy(ReferrerPolicy p) { referrer_.referrer_policy = p; }
  void SetMode(WebURLRequest::FetchRequestMode mode) { mode_ = mode; }
  WebURLRequest::FetchRequestMode Mode() const { return mode_; }
  void SetCredentials(WebURLRequest::FetchCredentialsMode);
  WebURLRequest::FetchCredentialsMode Credentials() const {
    return credentials_;
  }
  void SetCacheMode(WebURLRequest::FetchRequestCacheMode cache_mode) {
    cache_mode_ = cache_mode;
  }
  WebURLRequest::FetchRequestCacheMode CacheMode() const { return cache_mode_; }
  void SetRedirect(WebURLRequest::FetchRedirectMode redirect) {
    redirect_ = redirect;
  }
  WebURLRequest::FetchRedirectMode Redirect() const { return redirect_; }
  void SetResponseTainting(Tainting tainting) { response_tainting_ = tainting; }
  Tainting ResponseTainting() const { return response_tainting_; }
  FetchHeaderList* HeaderList() const { return header_list_.Get(); }
  void SetHeaderList(FetchHeaderList* header_list) {
    header_list_ = header_list;
  }
  BodyStreamBuffer* Buffer() const { return buffer_; }
  // Call Request::refreshBody() after calling setBuffer().
  void SetBuffer(BodyStreamBuffer* buffer) { buffer_ = buffer; }
  String MimeType() const { return mime_type_; }
  void SetMIMEType(const String& type) { mime_type_ = type; }
  String Integrity() const { return integrity_; }
  void SetIntegrity(const String& integrity) { integrity_ = integrity; }
  PassRefPtr<EncodedFormData> AttachedCredential() const {
    return attached_credential_;
  }
  void SetAttachedCredential(PassRefPtr<EncodedFormData> attached_credential) {
    attached_credential_ = std::move(attached_credential);
  }

  // We use these strings instead of "no-referrer" and "client" in the spec.
  static AtomicString NoReferrerString() { return AtomicString(); }
  static AtomicString ClientReferrerString() {
    return AtomicString("about:client");
  }

  DECLARE_TRACE();

 private:
  FetchRequestData();

  FetchRequestData* CloneExceptBody();

  AtomicString method_;
  KURL url_;
  Member<FetchHeaderList> header_list_;
  // FIXME: Support m_skipServiceWorkerFlag;
  WebURLRequest::RequestContext context_;
  RefPtr<SecurityOrigin> origin_;
  // FIXME: Support m_forceOriginHeaderFlag;
  bool same_origin_data_url_flag_;
  // |m_referrer| consists of referrer string and referrer policy.
  // We use |noReferrerString()| and |clientReferrerString()| as
  // "no-referrer" and "client" strings in the spec.
  Referrer referrer_;
  // FIXME: Support m_authenticationFlag;
  // FIXME: Support m_synchronousFlag;
  WebURLRequest::FetchRequestMode mode_;
  WebURLRequest::FetchCredentialsMode credentials_;
  // TODO(yiyix): |cache_mode_| is exposed but does not yet affect fetch
  // behavior. We must transfer the mode to the network layer and service
  // worker.
  WebURLRequest::FetchRequestCacheMode cache_mode_;
  WebURLRequest::FetchRedirectMode redirect_;
  // FIXME: Support m_useURLCredentialsFlag;
  // FIXME: Support m_redirectCount;
  Tainting response_tainting_;
  Member<BodyStreamBuffer> buffer_;
  String mime_type_;
  String integrity_;
  RefPtr<EncodedFormData> attached_credential_;
};

}  // namespace blink

#endif  // FetchRequestData_h
