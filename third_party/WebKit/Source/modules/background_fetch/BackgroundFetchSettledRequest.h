// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchSettledRequest_h
#define BackgroundFetchSettledRequest_h

#include "modules/background_fetch/BackgroundFetchRequest.h"
#include "platform/heap/Handle.h"

namespace blink {

class Request;
class Response;

// Interface for providing developers access to the Request/Response pairs when
// a background fetch has settled, either through the `backgroundfetched` event
// in case of success, or `backgroundfetchfail` in case of failure.
class BackgroundFetchSettledRequest final : public BackgroundFetchRequest {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchSettledRequest* create(Request* request,
                                               Response* response) {
    return new BackgroundFetchSettledRequest(request, response);
  }

  // Web Exposed attribute defined in the IDL file.
  Response* response() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  BackgroundFetchSettledRequest(Request*, Response*);

  Member<Response> m_response;
};

}  // namespace blink

#endif  // BackgroundFetchSettledRequest_h
