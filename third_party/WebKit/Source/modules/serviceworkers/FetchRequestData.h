// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchRequestData_h
#define FetchRequestData_h

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class FetchHeaderList;
struct ResourceLoaderOptions;
class ResourceRequest;
class SecurityOrigin;
struct ThreadableLoaderOptions;
class WebServiceWorkerRequest;

class FetchRequestData FINAL : public RefCountedWillBeGarbageCollectedFinalized<FetchRequestData> {
    WTF_MAKE_NONCOPYABLE(FetchRequestData);
public:
    enum Mode { SameOriginMode, NoCORSMode, CORSMode, CORSWithForcedPreflight };
    enum Credentials { OmitCredentials, SameOriginCredentials, IncludeCredentials };
    enum Context { ChildContext, ConnectContext, DownloadContext, FontContext, FormContext, ImageContext, ManifestContext, MediaContext, NavigateContext, ObjectContext, PingContext, PopupContext, PrefetchContext, ScriptContext, ServiceWorkerContext, SharedWorkerContext, StyleContext, WorkerContext, NullContext };
    enum Tainting { BasicTainting, CORSTainting, OpaqueTainting };

    class Referrer FINAL {
    public:
        Referrer() : m_type(ClientReferrer) { }
        bool isNone() const { return m_type == NoneReferrer; }
        bool isClient() const { return m_type == ClientReferrer; }
        bool isURL() const { return m_type == URLReferrer; }
        void setNone()
        {
            m_referrer = blink::Referrer();
            m_type = NoneReferrer;
        }
        void setClient(const blink::Referrer& referrer)
        {
            m_referrer = referrer;
            m_type = ClientReferrer;
        }
        void setURL(const blink::Referrer& referrer)
        {
            m_referrer = referrer;
            m_type = URLReferrer;
        }
        blink::Referrer referrer() const { return m_referrer; }
    private:
        enum Type { NoneReferrer, ClientReferrer, URLReferrer };
        Type m_type;
        blink::Referrer m_referrer;
    };

    static PassRefPtrWillBeRawPtr<FetchRequestData> create(ExecutionContext*);
    static PassRefPtrWillBeRawPtr<FetchRequestData> create(const blink::WebServiceWorkerRequest&);
    PassRefPtrWillBeRawPtr<FetchRequestData> createRestrictedCopy(ExecutionContext*, PassRefPtr<SecurityOrigin>) const;
    PassRefPtrWillBeRawPtr<FetchRequestData> createCopy() const;
    ~FetchRequestData();

    void setMethod(AtomicString method) { m_method = method; }
    const AtomicString method() const { return m_method; }
    void setURL(const KURL& url) { m_url = url; }
    const KURL& url() const { return m_url; }
    bool unsafeRequestFlag() const { return m_unsafeRequestFlag; }
    PassRefPtr<SecurityOrigin> origin() { return m_origin; }
    bool sameOriginDataURLFlag() { return m_sameOriginDataURLFlag; }
    const Referrer& referrer() const { return m_referrer; }
    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }
    void setCredentials(Credentials credentials) { m_credentials = credentials; }
    Credentials credentials() const { return m_credentials; }
    void setResponseTainting(Tainting tainting) { m_responseTainting = tainting; }
    Tainting tainting() const { return m_responseTainting; }
    FetchHeaderList* headerList() { return m_headerList.get(); }

    void trace(Visitor*);

private:
    FetchRequestData();

    static PassRefPtrWillBeRawPtr<FetchRequestData> create();

    AtomicString m_method;
    KURL m_url;
    RefPtrWillBeMember<FetchHeaderList> m_headerList;
    bool m_unsafeRequestFlag;
    // FIXME: Support body.
    // FIXME: Support m_skipServiceWorkerFlag;
    Context m_context;
    RefPtr<SecurityOrigin> m_origin;
    // FIXME: Support m_forceOriginHeaderFlag;
    bool m_sameOriginDataURLFlag;
    Referrer m_referrer;
    // FIXME: Support m_authenticationFlag;
    // FIXME: Support m_synchronousFlag;
    Mode m_mode;
    Credentials m_credentials;
    // FIXME: Support m_useURLCredentialsFlag;
    // FIXME: Support m_manualRedirectFlag;
    // FIXME: Support m_redirectCount;
    Tainting m_responseTainting;
};

} // namespace blink

#endif // FetchRequestData_h
