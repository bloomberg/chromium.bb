/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "platform/wtf/Time.h"

#include "base/time/time.h"

namespace WTF {

static TimeFunction g_mock_time_function_for_testing = nullptr;

double CurrentTime() {
  if (g_mock_time_function_for_testing)
    return g_mock_time_function_for_testing();
  return base::Time::Now().ToDoubleT();
}

double MonotonicallyIncreasingTime() {
  if (g_mock_time_function_for_testing)
    return g_mock_time_function_for_testing();
  return base::TimeTicks::Now().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

TimeFunction SetTimeFunctionsForTesting(TimeFunction new_function) {
  TimeFunction old_function = g_mock_time_function_for_testing;
  g_mock_time_function_for_testing = new_function;
  return old_function;
}

TimeFunction GetTimeFunctionForTesting() {
  return g_mock_time_function_for_testing;
}

}  // namespace WTF
