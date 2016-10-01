// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

namespace blink {

void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<CrossThreadClosure> task) {
  toSingleThreadTaskRunner()->PostTask(location,
                                       convertToBaseCallback(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<CrossThreadClosure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, convertToBaseCallback(std::move(task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<WTF::Closure> task) {
  toSingleThreadTaskRunner()->PostTask(location,
                                       convertToBaseCallback(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<WTF::Closure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, convertToBaseCallback(std::move(task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

}  // namespace blink
