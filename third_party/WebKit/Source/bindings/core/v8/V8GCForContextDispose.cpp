/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8GCForContextDispose.h"

#include "bindings/core/v8/V8PerIsolateData.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/StdLibExtras.h"
#include "public/platform/Platform.h"
#include "v8/include/v8.h"

namespace blink {

V8GCForContextDispose::V8GCForContextDispose()
    : pseudo_idle_timer_(this, &V8GCForContextDispose::PseudoIdleTimerFired) {
  Reset();
}

void V8GCForContextDispose::NotifyContextDisposed(bool is_main_frame) {
  did_dispose_context_for_main_frame_ = is_main_frame;
  last_context_disposal_time_ = WTF::CurrentTime();
  V8PerIsolateData::MainThreadIsolate()->ContextDisposedNotification(
      !is_main_frame);
  pseudo_idle_timer_.Stop();
}

void V8GCForContextDispose::NotifyIdle() {
  double max_time_since_last_context_disposal = .2;
  if (!did_dispose_context_for_main_frame_ && !pseudo_idle_timer_.IsActive() &&
      last_context_disposal_time_ + max_time_since_last_context_disposal >=
          WTF::CurrentTime()) {
    pseudo_idle_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

V8GCForContextDispose& V8GCForContextDispose::Instance() {
  DEFINE_STATIC_LOCAL(V8GCForContextDispose, static_instance, ());
  return static_instance;
}

void V8GCForContextDispose::PseudoIdleTimerFired(TimerBase*) {
  V8PerIsolateData::MainThreadIsolate()->IdleNotificationDeadline(
      MonotonicallyIncreasingTime());
  Reset();
}

void V8GCForContextDispose::Reset() {
  did_dispose_context_for_main_frame_ = false;
  last_context_disposal_time_ = -1;
}

}  // namespace blink
