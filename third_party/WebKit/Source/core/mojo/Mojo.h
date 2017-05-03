// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Mojo_h
#define Mojo_h

#include "mojo/public/cpp/system/core.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class MojoCreateDataPipeOptions;
class MojoCreateDataPipeResult;
class MojoCreateMessagePipeResult;
class MojoCreateSharedBufferResult;

class Mojo final : public GarbageCollected<Mojo>, public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // MojoResult
  static const MojoResult kResultOk = MOJO_RESULT_OK;
  static const MojoResult kResultCancelled = MOJO_RESULT_CANCELLED;
  static const MojoResult kResultUnknown = MOJO_RESULT_UNKNOWN;
  static const MojoResult kResultInvalidArgument = MOJO_RESULT_INVALID_ARGUMENT;
  static const MojoResult kResultDeadlineExceeded =
      MOJO_RESULT_DEADLINE_EXCEEDED;
  static const MojoResult kResultNotFound = MOJO_RESULT_NOT_FOUND;
  static const MojoResult kResultAlreadyExists = MOJO_RESULT_ALREADY_EXISTS;
  static const MojoResult kResultPermissionDenied =
      MOJO_RESULT_PERMISSION_DENIED;
  static const MojoResult kResultResourceExhausted =
      MOJO_RESULT_RESOURCE_EXHAUSTED;
  static const MojoResult kResultFailedPrecondition =
      MOJO_RESULT_FAILED_PRECONDITION;
  static const MojoResult kResultAborted = MOJO_RESULT_ABORTED;
  static const MojoResult kResultOutOfRange = MOJO_RESULT_OUT_OF_RANGE;
  static const MojoResult kResultUnimplemented = MOJO_RESULT_UNIMPLEMENTED;
  static const MojoResult kResultInternal = MOJO_RESULT_INTERNAL;
  static const MojoResult kResultUnavailable = MOJO_RESULT_UNAVAILABLE;
  static const MojoResult kResultDataLoss = MOJO_RESULT_DATA_LOSS;
  static const MojoResult kResultBusy = MOJO_RESULT_BUSY;
  static const MojoResult kResultShouldWait = MOJO_RESULT_SHOULD_WAIT;

  static void createMessagePipe(MojoCreateMessagePipeResult&);
  static void createDataPipe(const MojoCreateDataPipeOptions&,
                             MojoCreateDataPipeResult&);
  static void createSharedBuffer(unsigned num_bytes,
                                 MojoCreateSharedBufferResult&);

  DEFINE_INLINE_TRACE() {}
};

}  // namespace blink

#endif  // Mojo_h
