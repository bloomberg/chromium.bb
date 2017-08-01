// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AvailabilityCallbackWrapper_h
#define AvailabilityCallbackWrapper_h

#include <memory>

#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Functional.h"

namespace blink {

class RemotePlaybackAvailabilityCallback;
class RemotePlayback;

// Wraps either a WTF::Closure or RemotePlaybackAvailabilityCallback object
// to be kept in the RemotePlayback's |availability_callbacks_| map.
class AvailabilityCallbackWrapper final
    : public GarbageCollectedFinalized<AvailabilityCallbackWrapper>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(AvailabilityCallbackWrapper);

 public:
  explicit AvailabilityCallbackWrapper(RemotePlaybackAvailabilityCallback*);
  explicit AvailabilityCallbackWrapper(WTF::Closure);
  ~AvailabilityCallbackWrapper() = default;

  void Run(RemotePlayback*, bool new_availability);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  // Only one of these callbacks must be set.
  TraceWrapperMember<RemotePlaybackAvailabilityCallback> bindings_cb_;
  WTF::Closure internal_cb_;
};

}  // namespace blink

#endif  // AvailabilityCallbackWrapper_h
