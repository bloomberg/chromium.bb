// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchFetch_h
#define BackgroundFetchFetch_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Request;

// Base interface for providing developers with access to the fetch
// information associated with a background fetch.
class BackgroundFetchFetch : public GarbageCollected<BackgroundFetchFetch>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Web Exposed attribute defined in the IDL file.
  Request* request() const;

  virtual void Trace(blink::Visitor*);

 protected:
  explicit BackgroundFetchFetch(Request*);

 private:
  Member<Request> request_;
};

}  // namespace blink

#endif  // BackgroundFetchFetch_h
