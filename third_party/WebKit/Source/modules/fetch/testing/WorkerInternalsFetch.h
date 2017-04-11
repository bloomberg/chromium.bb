// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInternalsFetch_h
#define WorkerInternalsFetch_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class WorkerInternals;
class Response;

class WorkerInternalsFetch {
  STATIC_ONLY(WorkerInternalsFetch);

 public:
  static Vector<String> getInternalResponseURLList(WorkerInternals&, Response*);
};

}  // namespace blink

#endif  // WorkerInternalsFetch_h
