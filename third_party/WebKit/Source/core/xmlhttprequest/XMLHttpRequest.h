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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include <memory>
#include "bindings/core/v8/ScriptString.h"
#include "core/dom/DocumentParserClient.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SuspendableObject.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/xmlhttprequest/XMLHttpRequestEventTarget.h"
#include "core/xmlhttprequest/XMLHttpRequestProgressEventThrottle.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class
    ArrayBufferOrArrayBufferViewOrBlobOrDocumentOrStringOrFormDataOrURLSearchParams;
class Blob;
class BlobDataHandle;
class DOMArrayBuffer;
class DOMArrayBufferView;
class Document;
class DocumentParser;
class ExceptionState;
class ExecutionContext;
class FormData;
class ScriptState;
class SharedBuffer;
class TextResourceDecoder;
class ThreadableLoader;
class URLSearchParams;
class WebDataConsumerHandle;
class XMLHttpRequestUpload;

class XMLHttpRequest final : public XMLHttpRequestEventTarget,
                             private ThreadableLoaderClient,
                             public DocumentParserClient,
                             public ActiveScriptWrappable<XMLHttpRequest>,
                             public SuspendableObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XMLHttpRequest);

  // In some cases hasPendingActivity doesn't work correctly, i.e.,
  // doesn't keep |this| alive. We need to cancel the loader in such cases,
  // which is why we need this pre-finalizer.
  // TODO(yhirano): Remove this pre-finalizer when the bug is fixed.
  USING_PRE_FINALIZER(XMLHttpRequest, Dispose);

 public:
  static XMLHttpRequest* Create(ScriptState*);
  static XMLHttpRequest* Create(ExecutionContext*);
  ~XMLHttpRequest() override;

  // These exact numeric values are important because JS expects them.
  enum State {
    kUnsent = 0,
    kOpened = 1,
    kHeadersReceived = 2,
    kLoading = 3,
    kDone = 4
  };

  enum ResponseTypeCode {
    kResponseTypeDefault,
    kResponseTypeText,
    kResponseTypeJSON,
    kResponseTypeDocument,
    kResponseTypeBlob,
    kResponseTypeArrayBuffer,
  };

  // SuspendableObject
  void ContextDestroyed(ExecutionContext*) override;
  ExecutionContext* GetExecutionContext() const override;
  void Suspend() override;
  void Resume() override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // XMLHttpRequestEventTarget
  const AtomicString& InterfaceName() const override;

  // JavaScript attributes and methods
  const KURL& Url() const { return url_; }
  String statusText() const;
  int status() const;
  State readyState() const;
  bool withCredentials() const { return with_credentials_; }
  void setWithCredentials(bool, ExceptionState&);
  void open(const AtomicString& method, const String& url, ExceptionState&);
  void open(const AtomicString& method,
            const String& url,
            bool async,
            const String& username,
            const String& password,
            ExceptionState&);
  void open(const AtomicString& method,
            const KURL&,
            bool async,
            ExceptionState&);
  void send(
      const ArrayBufferOrArrayBufferViewOrBlobOrDocumentOrStringOrFormDataOrURLSearchParams&,
      ExceptionState&);
  void abort();
  void Dispose();
  void setRequestHeader(const AtomicString& name,
                        const AtomicString& value,
                        ExceptionState&);
  void overrideMimeType(const AtomicString& override, ExceptionState&);
  String getAllResponseHeaders() const;
  const AtomicString& getResponseHeader(const AtomicString&) const;
  ScriptString responseText(ExceptionState&);
  ScriptString ResponseJSONSource();
  Document* responseXML(ExceptionState&);
  Blob* ResponseBlob();
  DOMArrayBuffer* ResponseArrayBuffer();
  unsigned timeout() const { return timeout_milliseconds_; }
  void setTimeout(unsigned timeout, ExceptionState&);
  ResponseTypeCode GetResponseTypeCode() const { return response_type_code_; }
  String responseType();
  void setResponseType(const String&, ExceptionState&);
  String responseURL();

  // For Inspector.
  void SendForInspectorXHRReplay(PassRefPtr<EncodedFormData>, ExceptionState&);

  XMLHttpRequestUpload* upload();
  bool IsAsync() { return async_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  class BlobLoader;
  XMLHttpRequest(ExecutionContext*,
                 bool is_isolated_world,
                 PassRefPtr<SecurityOrigin>);

  Document* GetDocument() const;

  // Returns the SecurityOrigin of the isolated world if the XMLHttpRequest was
  // created in an isolated world. Otherwise, returns the SecurityOrigin of the
  // execution context.
  SecurityOrigin* GetSecurityOrigin() const;

  void DidSendData(unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent) override;
  void DidReceiveResponse(unsigned long identifier,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveData(const char* data, unsigned data_length) override;
  // When responseType is set to "blob", didDownloadData() is called instead
  // of didReceiveData().
  void DidDownloadData(int data_length) override;
  void DidFinishLoading(unsigned long identifier, double finish_time) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  // BlobLoader notifications.
  void DidFinishLoadingInternal();
  void DidFinishLoadingFromBlob();
  void DidFailLoadingFromBlob();

  PassRefPtr<BlobDataHandle> CreateBlobDataHandleFromResponse();

  // DocumentParserClient
  void NotifyParserStopped() override;

  void EndLoading();

  // Returns the MIME type part of mime_type_override_ if present and
  // successfully parsed, or returns one of the "Content-Type" header value
  // of the received response.
  //
  // This method is named after the term "final MIME type" defined in the
  // spec but doesn't convert the result to ASCII lowercase as specified in
  // the spec. Must be lowered later or compared using case insensitive
  // comparison functions if required.
  AtomicString FinalResponseMIMEType() const;
  // The same as finalResponseMIMEType() but fallbacks to "text/xml" if
  // finalResponseMIMEType() returns an empty string.
  AtomicString FinalResponseMIMETypeWithFallback() const;
  // Returns the "final charset" defined in
  // https://xhr.spec.whatwg.org/#final-charset.
  String FinalResponseCharset() const;
  bool ResponseIsXML() const;
  bool ResponseIsHTML() const;

  std::unique_ptr<TextResourceDecoder> CreateDecoder() const;

  void InitResponseDocument();
  void ParseDocumentChunk(const char* data, unsigned data_length);

  bool AreMethodAndURLValidForSend();

  void ThrowForLoadFailureIfNeeded(ExceptionState&, const String&);

  bool InitSend(ExceptionState&);
  void SendBytesData(const void*, size_t, ExceptionState&);
  void send(Document*, ExceptionState&);
  void send(const String&, ExceptionState&);
  void send(Blob*, ExceptionState&);
  void send(FormData*, ExceptionState&);
  void send(URLSearchParams*, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(DOMArrayBufferView*, ExceptionState&);

  bool HasContentTypeRequestHeader() const;
  void SetRequestHeaderInternal(const AtomicString& name,
                                const AtomicString& value);

  void TrackProgress(long long data_length);
  // Changes m_state and dispatches a readyStateChange event if new m_state
  // value is different from last one.
  void ChangeState(State new_state);
  void DispatchReadyStateChangeEvent();

  // Clears variables used only while the resource is being loaded.
  void ClearVariablesForLoading();
  // Returns false iff reentry happened and a new load is started.
  bool InternalAbort();
  // Clears variables holding response header and body data.
  void ClearResponse();
  void ClearRequest();

  void CreateRequest(PassRefPtr<EncodedFormData>, ExceptionState&);

  // Dispatches a response ProgressEvent.
  void DispatchProgressEvent(const AtomicString&, long long, long long);
  // Dispatches a response ProgressEvent using values sampled from
  // m_receivedLength and m_response.
  void DispatchProgressEventFromSnapshot(const AtomicString&);

  // Handles didFail() call not caused by cancellation or timeout.
  void HandleNetworkError();
  // Handles didFail() call for cancellations. For example, the
  // ResourceLoader handling the load notifies m_loader of an error
  // cancellation when the frame containing the XHR navigates away.
  void HandleDidCancel();
  // Handles didFail() call for timeout.
  void HandleDidTimeout();

  void HandleRequestError(ExceptionCode,
                          const AtomicString&,
                          long long,
                          long long);

  void UpdateContentTypeAndCharset(const AtomicString& content_type,
                                   const String& charset);

  XMLHttpRequestProgressEventThrottle& ProgressEventThrottle();

  Member<XMLHttpRequestUpload> upload_;

  KURL url_;
  AtomicString method_;
  HTTPHeaderMap request_headers_;
  // Not converted to ASCII lowercase. Must be lowered later or compared
  // using case insensitive comparison functions if needed.
  AtomicString mime_type_override_;
  unsigned long timeout_milliseconds_;
  TraceWrapperMember<Blob> response_blob_;

  Member<ThreadableLoader> loader_;
  State state_;

  ResourceResponse response_;

  std::unique_ptr<TextResourceDecoder> decoder_;

  ScriptString response_text_;
  TraceWrapperMember<Document> response_document_;
  Member<DocumentParser> response_document_parser_;

  RefPtr<SharedBuffer> binary_response_builder_;
  long long length_downloaded_to_file_;

  TraceWrapperMember<DOMArrayBuffer> response_array_buffer_;

  // Used for onprogress tracking
  long long received_length_;

  // An exception to throw in synchronous mode. It's set when failure
  // notification is received from m_loader and thrown at the end of send() if
  // any.
  ExceptionCode exception_code_;

  Member<XMLHttpRequestProgressEventThrottle> progress_event_throttle_;

  // An enum corresponding to the allowed string values for the responseType
  // attribute.
  ResponseTypeCode response_type_code_;

  // Set to true if the XMLHttpRequest was created in an isolated world.
  bool is_isolated_world_;
  // Stores the SecurityOrigin associated with the isolated world if any.
  RefPtr<SecurityOrigin> isolated_world_security_origin_;

  // This blob loader will be used if |m_downloadingToFile| is true and
  // |m_responseTypeCode| is NOT ResponseTypeBlob.
  Member<BlobLoader> blob_loader_;

  // Positive if we are dispatching events.
  // This is an integer specifying the recursion level rather than a boolean
  // because in some cases we have recursive dispatching.
  int event_dispatch_recursion_level_;

  bool async_;

  bool with_credentials_;

  // Used to skip m_responseDocument creation if it's done previously. We need
  // this separate flag since m_responseDocument can be 0 for some cases.
  bool parsed_response_;
  bool error_;
  bool upload_events_allowed_;
  bool upload_complete_;
  bool same_origin_request_;
  // True iff the ongoing resource loading is using the downloadToFile
  // option.
  bool downloading_to_file_;
  bool response_text_overflow_;
  bool send_flag_;
  bool response_array_buffer_failure_;
};

std::ostream& operator<<(std::ostream&, const XMLHttpRequest*);

}  // namespace blink

#endif  // XMLHttpRequest_h
