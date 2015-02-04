// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/ClosureRunnerTask.h"

namespace blink {

void ClosureRunnerTask::run()
{
    (*m_closure)();
}

} // namespace blink
