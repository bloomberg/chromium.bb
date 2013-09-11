/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
 *  Copyright (C) 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include "bindings/v8/ScriptString.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/EventListener.h"
#include "core/dom/EventNames.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/platform/network/FormData.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/xml/XMLHttpRequestEventTarget.h"
#include "core/xml/XMLHttpRequestProgressEventThrottle.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

class Blob;
class DOMFormData;
class Document;
class ExceptionState;
class ResourceRequest;
class SecurityOrigin;
class SharedBuffer;
class Stream;
class TextResourceDecoder;
class ThreadableLoader;

typedef int ExceptionCode;

class XMLHttpRequest : public ScriptWrappable, public RefCounted<XMLHttpRequest>, public XMLHttpRequestEventTarget, private ThreadableLoaderClient, public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<XMLHttpRequest> create(ScriptExecutionContext*, PassRefPtr<SecurityOrigin> = 0);
    ~XMLHttpRequest();

    // These exact numeric values are important because JS expects them.
    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

    enum ResponseTypeCode {
        ResponseTypeDefault,
        ResponseTypeText,
        ResponseTypeJSON,
        ResponseTypeDocument,
        ResponseTypeBlob,
        ResponseTypeArrayBuffer,
        ResponseTypeStream
    };

    enum DropProtection {
        DropProtectionSync,
        DropProtectionAsync,
    };

    virtual void contextDestroyed();
    virtual void didTimeout();
    virtual bool canSuspend() const;
    virtual void suspend(ReasonForSuspension);
    virtual void resume();
    virtual void stop();

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

    const KURL& url() const { return m_url; }
    String statusText(ExceptionState&) const;
    int status(ExceptionState&) const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    void setWithCredentials(bool, ExceptionState&);
    void open(const String& method, const KURL&, ExceptionState&);
    void open(const String& method, const KURL&, bool async, ExceptionState&);
    void open(const String& method, const KURL&, bool async, const String& user, ExceptionState&);
    void open(const String& method, const KURL&, bool async, const String& user, const String& password, ExceptionState&);
    void send(ExceptionState&);
    void send(Document*, ExceptionState&);
    void send(const String&, ExceptionState&);
    void send(Blob*, ExceptionState&);
    void send(DOMFormData*, ExceptionState&);
    void send(ArrayBuffer*, ExceptionState&);
    void send(ArrayBufferView*, ExceptionState&);
    void abort();
    void setRequestHeader(const AtomicString& name, const String& value, ExceptionState&);
    void overrideMimeType(const String& override);
    String getAllResponseHeaders(ExceptionState&) const;
    String getResponseHeader(const AtomicString& name, ExceptionState&) const;
    ScriptString responseText(ExceptionState&);
    ScriptString responseJSONSource();
    Document* responseXML(ExceptionState&);
    Blob* responseBlob();
    Stream* responseStream();
    unsigned long timeout() const { return m_timeoutMilliseconds; }
    void setTimeout(unsigned long timeout, ExceptionState&);

    void sendForInspectorXHRReplay(PassRefPtr<FormData>, ExceptionState&);

    // Expose HTTP validation methods for other untrusted requests.
    static bool isAllowedHTTPMethod(const String&);
    static String uppercaseKnownHTTPMethod(const String&);
    static bool isAllowedHTTPHeader(const String&);

    void setResponseType(const String&, ExceptionState&);
    String responseType();
    ResponseTypeCode responseTypeCode() const { return m_responseTypeCode; }

    // response attribute has custom getter.
    ArrayBuffer* responseArrayBuffer();

    void setLastSendLineNumber(unsigned lineNumber) { m_lastSendLineNumber = lineNumber; }
    void setLastSendURL(const String& url) { m_lastSendURL = url; }

    XMLHttpRequestUpload* upload();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    XMLHttpRequest(ScriptExecutionContext*, PassRefPtr<SecurityOrigin>);

    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    Document* document() const;
    SecurityOrigin* securityOrigin() const;

    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&);
    virtual void didReceiveData(const char* data, int dataLength);
    virtual void didFinishLoading(unsigned long identifier, double finishTime);
    virtual void didFail(const ResourceError&);
    virtual void didFailRedirectCheck();

    String responseMIMEType() const;
    bool responseIsXML() const;

    bool areMethodAndURLValidForSend();

    bool initSend(ExceptionState&);
    void sendBytesData(const void*, size_t, ExceptionState&);

    String getRequestHeader(const AtomicString& name) const;
    void setRequestHeaderInternal(const AtomicString& name, const String& value);

    void changeState(State newState);
    void callReadyStateChangeListener();
    void dropProtectionSoon();
    void dropProtection(Timer<XMLHttpRequest>* = 0);
    void internalAbort(DropProtection = DropProtectionSync);
    void clearResponse();
    void clearResponseBuffers();
    void clearRequest();

    void createRequest(ExceptionState&);

    void genericError();
    void networkError();
    void abortError();

    OwnPtr<XMLHttpRequestUpload> m_upload;

    KURL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async;
    bool m_includeCredentials;
    unsigned long m_timeoutMilliseconds;
    RefPtr<Blob> m_responseBlob;
    RefPtr<Stream> m_responseStream;

    RefPtr<ThreadableLoader> m_loader;
    State m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;

    ScriptString m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseDocument;

    RefPtr<SharedBuffer> m_binaryResponseBuilder;
    mutable RefPtr<ArrayBuffer> m_responseArrayBuffer;

    bool m_error;

    bool m_uploadEventsAllowed;
    bool m_uploadComplete;

    bool m_sameOriginRequest;
    bool m_allowCrossOriginRequests;

    // Used for onprogress tracking
    long long m_receivedLength;

    unsigned m_lastSendLineNumber;
    String m_lastSendURL;
    ExceptionCode m_exceptionCode;

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    // An enum corresponding to the allowed string values for the responseType attribute.
    ResponseTypeCode m_responseTypeCode;
    Timer<XMLHttpRequest> m_protectionTimer;
    RefPtr<SecurityOrigin> m_securityOrigin;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
