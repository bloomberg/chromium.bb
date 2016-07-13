// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskRunnerHelper_h
#define TaskRunnerHelper_h

#include "wtf/Allocator.h"

namespace blink {

class Document;
class LocalFrame;
class WebTaskRunner;

class TaskRunnerHelper final {
    STATIC_ONLY(TaskRunnerHelper);
public:
    static WebTaskRunner* getUnthrottledTaskRunner(LocalFrame*);
    static WebTaskRunner* getTimerTaskRunner(LocalFrame*);
    static WebTaskRunner* getLoadingTaskRunner(LocalFrame*);
    static WebTaskRunner* getUnthrottledTaskRunner(Document*);
    static WebTaskRunner* getTimerTaskRunner(Document*);
    static WebTaskRunner* getLoadingTaskRunner(Document*);
};

} // namespace blink

#endif
