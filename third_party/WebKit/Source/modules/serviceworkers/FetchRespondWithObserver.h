// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchRespondWithObserver_h
#define FetchRespondWithObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// This class observes the service worker's handling of a FetchEvent and
// notifies the client.
class MODULES_EXPORT FetchRespondWithObserver : public RespondWithObserver {
 public:
  ~FetchRespondWithObserver() override = default;

  static FetchRespondWithObserver* create(ExecutionContext*,
                                          int fetchEventID,
                                          const KURL& requestURL,
                                          WebURLRequest::FetchRequestMode,
                                          WebURLRequest::FetchRedirectMode,
                                          WebURLRequest::FrameType,
                                          WebURLRequest::RequestContext,
                                          WaitUntilObserver*);

  void onResponseRejected(WebServiceWorkerResponseError) override;
  void onResponseFulfilled(const ScriptValue&) override;
  void onNoResponse() override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  FetchRespondWithObserver(ExecutionContext*,
                           int fetchEventID,
                           const KURL& requestURL,
                           WebURLRequest::FetchRequestMode,
                           WebURLRequest::FetchRedirectMode,
                           WebURLRequest::FrameType,
                           WebURLRequest::RequestContext,
                           WaitUntilObserver*);

 private:
  const KURL m_requestURL;
  const WebURLRequest::FetchRequestMode m_requestMode;
  const WebURLRequest::FetchRedirectMode m_redirectMode;
  const WebURLRequest::FrameType m_frameType;
  const WebURLRequest::RequestContext m_requestContext;
};

}  // namespace blink

#endif  // FetchRespondWithObserver_h
