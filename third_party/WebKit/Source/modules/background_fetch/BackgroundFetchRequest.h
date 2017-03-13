// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchRequest_h
#define BackgroundFetchRequest_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Request;

// Base interface for providing developers with access to the Request
// information associated with a background fetch.
class BackgroundFetchRequest : public GarbageCollected<BackgroundFetchRequest>,
                               public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Web Exposed attribute defined in the IDL file.
  Request* request() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit BackgroundFetchRequest(Request*);

 private:
  Member<Request> m_request;
};

}  // namespace blink

#endif  // BackgroundFetchRequest_h
