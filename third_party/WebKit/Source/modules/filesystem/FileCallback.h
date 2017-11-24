// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FileCallback_h
#define FileCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class File;

class FileCallback : public GarbageCollectedFinalized<FileCallback> {
 public:
  virtual ~FileCallback() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void handleEvent(File*) = 0;
};

}  // namespace blink

#endif  // FileCallback_h
