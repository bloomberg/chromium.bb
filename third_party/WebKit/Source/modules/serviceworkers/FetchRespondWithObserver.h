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

  static FetchRespondWithObserver* Create(ExecutionContext*,
                                          int fetch_event_id,
                                          const KURL& request_url,
                                          WebURLRequest::FetchRequestMode,
                                          WebURLRequest::FetchRedirectMode,
                                          WebURLRequest::FrameType,
                                          WebURLRequest::RequestContext,
                                          WaitUntilObserver*);

  void OnResponseRejected(WebServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&) override;
  void OnNoResponse() override;

  virtual void Trace(blink::Visitor*);

 protected:
  FetchRespondWithObserver(ExecutionContext*,
                           int fetch_event_id,
                           const KURL& request_url,
                           WebURLRequest::FetchRequestMode,
                           WebURLRequest::FetchRedirectMode,
                           WebURLRequest::FrameType,
                           WebURLRequest::RequestContext,
                           WaitUntilObserver*);

 private:
  const KURL request_url_;
  const WebURLRequest::FetchRequestMode request_mode_;
  const WebURLRequest::FetchRedirectMode redirect_mode_;
  const WebURLRequest::FrameType frame_type_;
  const WebURLRequest::RequestContext request_context_;
};

}  // namespace blink

#endif  // FetchRespondWithObserver_h
