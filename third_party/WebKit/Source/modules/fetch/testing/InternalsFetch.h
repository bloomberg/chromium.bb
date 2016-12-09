// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsFetch_h
#define InternalsFetch_h

#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Internals;
class Response;

class InternalsFetch {
  STATIC_ONLY(InternalsFetch);

 public:
  static Vector<String> getInternalResponseURLList(Internals&, Response*);
};

}  // namespace blink

#endif  // InternalsFetch_h
