// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClosureRunnerTask_h
#define ClosureRunnerTask_h

#include "platform/PlatformExport.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT ClosureRunnerTask : public WebThread::Task {
    WTF_MAKE_NONCOPYABLE(ClosureRunnerTask);

public:
    explicit ClosureRunnerTask(PassOwnPtr<Closure> closure)
        : m_closure(closure)
    {
    }

    ~ClosureRunnerTask() override { }

    // WebThread::Task implementation.
    void run() override;

private:
    OwnPtr<Closure> m_closure;
};

} // namespace blink

#endif // ClosureRunnerTask_h
