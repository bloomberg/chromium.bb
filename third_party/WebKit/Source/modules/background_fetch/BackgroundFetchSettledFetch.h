// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchSettledFetch_h
#define BackgroundFetchSettledFetch_h

#include "modules/background_fetch/BackgroundFetchFetch.h"
#include "platform/heap/Handle.h"

namespace blink {

class Request;
class Response;

// Interface for providing developers access to the Request/Response pairs when
// a background fetch has settled, either through the `backgroundfetched` event
// in case of success, or `backgroundfetchfail` in case of failure.
class BackgroundFetchSettledFetch final : public BackgroundFetchFetch {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchSettledFetch* Create(Request* request,
                                             Response* response) {
    return new BackgroundFetchSettledFetch(request, response);
  }

  // Web Exposed attribute defined in the IDL file.
  Response* response() const;

  virtual void Trace(blink::Visitor*);

 private:
  BackgroundFetchSettledFetch(Request*, Response*);

  Member<Response> response_;
};

}  // namespace blink

#endif  // BackgroundFetchSettledFetch_h
