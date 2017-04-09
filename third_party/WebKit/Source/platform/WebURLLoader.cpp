// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebURLLoader.h"

#include "platform/WebTaskRunner.h"

namespace blink {

void WebURLLoader::SetLoadingTaskRunner(WebTaskRunner* task_runner) {
  SetLoadingTaskRunner(task_runner->ToSingleThreadTaskRunner());
}

}  // namespace blink
