// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForeignFetchRespondWithObserver_h
#define ForeignFetchRespondWithObserver_h

#include "modules/serviceworkers/FetchRespondWithObserver.h"

namespace blink {

// This class observes the service worker's handling of a ForeignFetchEvent and
// notifies the client.
class MODULES_EXPORT ForeignFetchRespondWithObserver final
    : public FetchRespondWithObserver {
 public:
  static ForeignFetchRespondWithObserver* Create(
      ExecutionContext*,
      int event_id,
      const KURL& request_url,
      WebURLRequest::FetchRequestMode,
      WebURLRequest::FetchRedirectMode,
      WebURLRequest::FrameType,
      WebURLRequest::RequestContext,
      scoped_refptr<SecurityOrigin>,
      WaitUntilObserver*);

  void OnResponseFulfilled(const ScriptValue&) override;

 private:
  ForeignFetchRespondWithObserver(ExecutionContext*,
                                  int event_id,
                                  const KURL& request_url,
                                  WebURLRequest::FetchRequestMode,
                                  WebURLRequest::FetchRedirectMode,
                                  WebURLRequest::FrameType,
                                  WebURLRequest::RequestContext,
                                  scoped_refptr<SecurityOrigin>,
                                  WaitUntilObserver*);

  scoped_refptr<SecurityOrigin> request_origin_;
};

}  // namespace blink

#endif  // ForeignFetchRespondWithObserver_h
