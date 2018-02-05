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

#ifndef V8GCForContextDispose_h
#define V8GCForContextDispose_h

#include "bindings/core/v8/WindowProxy.h"
#include "platform/Timer.h"

namespace blink {

class V8GCForContextDispose {
  USING_FAST_MALLOC(V8GCForContextDispose);
  WTF_MAKE_NONCOPYABLE(V8GCForContextDispose);

 public:
  void NotifyContextDisposed(bool is_main_frame, WindowProxy::FrameReuseStatus);
  void NotifyIdle();

  static V8GCForContextDispose& Instance();

 private:
  V8GCForContextDispose();  // Use instance() instead.
  void PseudoIdleTimerFired(TimerBase*);
  void Reset();

  TaskRunnerTimer<V8GCForContextDispose> pseudo_idle_timer_;
  bool did_dispose_context_for_main_frame_;
  double last_context_disposal_time_;
};

}  // namespace blink

#endif  // V8GCForContextDispose_h
