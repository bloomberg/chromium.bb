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

#include "platform/wtf/WTF.h"

#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/StackUtil.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringStatics.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"

namespace WTF {

extern void InitializeThreading();

bool g_initialized;
void (*g_call_on_main_thread_function)(MainThreadFunction, void*);
ThreadIdentifier g_main_thread_identifier;

namespace internal {

void CallOnMainThread(MainThreadFunction* function, void* context) {
  (*g_call_on_main_thread_function)(function, context);
}

}  // namespace internal

bool IsMainThread() {
  return CurrentThread() == g_main_thread_identifier;
}

void Initialize(void (*call_on_main_thread_function)(MainThreadFunction,
                                                     void*)) {
  // WTF, and Blink in general, cannot handle being re-initialized.
  // Make that explicit here.
  CHECK(!g_initialized);
  g_initialized = true;
  InitializeCurrentThread();
  g_main_thread_identifier = CurrentThread();

  InitializeThreading();

  g_call_on_main_thread_function = call_on_main_thread_function;
  internal::InitializeMainThreadStackEstimate();
  AtomicString::Init();
  StringStatics::Init();
}

}  // namespace WTF
