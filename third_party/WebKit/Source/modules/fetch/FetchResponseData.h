// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchResponseData_h
#define FetchResponseData_h

#include <memory>
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

class BodyStreamBuffer;
class FetchHeaderList;
class ScriptState;
class WebServiceWorkerResponse;

class MODULES_EXPORT FetchResponseData final
    : public GarbageCollectedFinalized<FetchResponseData> {
  WTF_MAKE_NONCOPYABLE(FetchResponseData);

 public:
  // "A response has an associated type which is one of basic, CORS, default,
  // error, opaque, and opaqueredirect. Unless stated otherwise, it is
  // default."
  enum Type {
    kBasicType,
    kCORSType,
    kDefaultType,
    kErrorType,
    kOpaqueType,
    kOpaqueRedirectType
  };
  // "A response can have an associated termination reason which is one of
  // end-user abort, fatal, and timeout."
  enum TerminationReason {
    kEndUserAbortTermination,
    kFatalTermination,
    kTimeoutTermination
  };

  static FetchResponseData* Create();
  static FetchResponseData* CreateNetworkErrorResponse();
  static FetchResponseData* CreateWithBuffer(BodyStreamBuffer*);

  FetchResponseData* CreateBasicFilteredResponse() const;
  // Creates a CORS filtered response, settings the response's cors exposed
  // header names list to the result of parsing the
  // Access-Control-Expose-Headers header.
  FetchResponseData* CreateCORSFilteredResponse() const;
  // Creates a CORS filtered response with an explicit set of exposed header
  // names.
  FetchResponseData* CreateCORSFilteredResponse(
      const HTTPHeaderSet& exposed_headers) const;
  FetchResponseData* CreateOpaqueFilteredResponse() const;
  FetchResponseData* CreateOpaqueRedirectFilteredResponse() const;

  FetchResponseData* InternalResponse() { return internal_response_; }
  const FetchResponseData* InternalResponse() const {
    return internal_response_;
  }

  FetchResponseData* Clone(ScriptState*);

  Type GetType() const { return type_; }
  const KURL* Url() const;
  unsigned short Status() const { return status_; }
  AtomicString StatusMessage() const { return status_message_; }
  FetchHeaderList* HeaderList() const { return header_list_.Get(); }
  BodyStreamBuffer* Buffer() const { return buffer_; }
  String MimeType() const;
  // Returns the BodyStreamBuffer of |m_internalResponse| if any. Otherwise,
  // returns |m_buffer|.
  BodyStreamBuffer* InternalBuffer() const;
  String InternalMIMEType() const;
  Time ResponseTime() const { return response_time_; }
  String CacheStorageCacheName() const { return cache_storage_cache_name_; }
  const HTTPHeaderSet& CorsExposedHeaderNames() const {
    return cors_exposed_header_names_;
  }

  void SetURLList(const Vector<KURL>&);
  const Vector<KURL>& UrlList() const { return url_list_; }
  const Vector<KURL>& InternalURLList() const;

  void SetStatus(unsigned short status) { status_ = status; }
  void SetStatusMessage(AtomicString status_message) {
    status_message_ = status_message;
  }
  void SetMIMEType(const String& type) { mime_type_ = type; }
  void SetResponseTime(Time response_time) { response_time_ = response_time; }
  void SetCacheStorageCacheName(const String& cache_storage_cache_name) {
    cache_storage_cache_name_ = cache_storage_cache_name;
  }
  void SetCorsExposedHeaderNames(const HTTPHeaderSet& header_names) {
    cors_exposed_header_names_ = header_names;
  }

  // If the type is Default, replaces |m_buffer|.
  // If the type is Basic or CORS, replaces |m_buffer| and
  // |m_internalResponse->m_buffer|.
  // If the type is Error or Opaque, does nothing.
  // Call Response::refreshBody after calling this function.
  void ReplaceBodyStreamBuffer(BodyStreamBuffer*);

  // Does not call response.setBlobDataHandle().
  void PopulateWebServiceWorkerResponse(
      WebServiceWorkerResponse& /* response */);

  DECLARE_TRACE();

 private:
  FetchResponseData(Type, unsigned short, AtomicString);

  Type type_;
  std::unique_ptr<TerminationReason> termination_reason_;
  Vector<KURL> url_list_;
  unsigned short status_;
  AtomicString status_message_;
  Member<FetchHeaderList> header_list_;
  Member<FetchResponseData> internal_response_;
  Member<BodyStreamBuffer> buffer_;
  String mime_type_;
  Time response_time_;
  String cache_storage_cache_name_;
  HTTPHeaderSet cors_exposed_header_names_;
};

}  // namespace blink

#endif  // FetchResponseData_h
