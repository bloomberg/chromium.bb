/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AsyncMethodRunner_h
#define AsyncMethodRunner_h

#include "platform/Timer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

template <typename TargetClass>
class AsyncMethodRunner final
    : public GarbageCollectedFinalized<AsyncMethodRunner<TargetClass>> {
  WTF_MAKE_NONCOPYABLE(AsyncMethodRunner);

 public:
  typedef void (TargetClass::*TargetMethod)();

  static AsyncMethodRunner* Create(TargetClass* object, TargetMethod method) {
    return new AsyncMethodRunner(object, method);
  }

  ~AsyncMethodRunner() {}

  // Schedules to run the method asynchronously. Do nothing if it's already
  // scheduled. If it's suspended, remember to schedule to run the method when
  // resume() is called.
  void RunAsync() {
    if (suspended_) {
      DCHECK(!timer_.IsActive());
      run_when_resumed_ = true;
      return;
    }

    // FIXME: runAsync should take a TraceLocation and pass it to timer here.
    if (!timer_.IsActive())
      timer_.StartOneShot(0, BLINK_FROM_HERE);
  }

  // If it's scheduled to run the method, cancel it and remember to schedule
  // it again when resume() is called. Mainly for implementing
  // SuspendableObject::suspend().
  void Suspend() {
    if (suspended_)
      return;
    suspended_ = true;

    if (!timer_.IsActive())
      return;

    timer_.Stop();
    run_when_resumed_ = true;
  }

  // Resumes pending method run.
  void Resume() {
    if (!suspended_)
      return;
    suspended_ = false;

    if (!run_when_resumed_)
      return;

    run_when_resumed_ = false;
    // FIXME: resume should take a TraceLocation and pass it to timer here.
    timer_.StartOneShot(0, BLINK_FROM_HERE);
  }

  void Stop() {
    if (suspended_) {
      DCHECK(!timer_.IsActive());
      run_when_resumed_ = false;
      suspended_ = false;
      return;
    }

    DCHECK(!run_when_resumed_);
    timer_.Stop();
  }

  bool IsActive() const { return timer_.IsActive(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(object_); }

 private:
  AsyncMethodRunner(TargetClass* object, TargetMethod method)
      : timer_(this, &AsyncMethodRunner<TargetClass>::Fired),
        object_(object),
        method_(method),
        suspended_(false),
        run_when_resumed_(false) {}

  void Fired(TimerBase*) { (object_->*method_)(); }

  Timer<AsyncMethodRunner<TargetClass>> timer_;

  Member<TargetClass> object_;
  TargetMethod method_;

  bool suspended_;
  bool run_when_resumed_;
};

}  // namespace blink

#endif
