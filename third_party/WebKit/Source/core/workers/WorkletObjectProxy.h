// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletObjectProxy_h
#define WorkletObjectProxy_h

#include "core/CoreExport.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// A proxy to talk to the Worklet object on the main thread from the worklet
// global scope that may exist in the main thread or on a different thread.
class CORE_EXPORT WorkletObjectProxy : public GarbageCollectedMixin {
 public:
  // Called when the worklet module script is fetched and evaluated. |success|
  // is true if the sequence is completed without an error.
  virtual void DidFetchAndInvokeScript(int32_t request_id, bool success) = 0;
};

}  // namespace blink

#endif  // WorkletObjectProxy_h
