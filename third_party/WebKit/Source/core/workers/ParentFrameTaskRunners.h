// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParentFrameTaskRunners_h
#define ParentFrameTaskRunners_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class Document;
class WebTaskRunner;

// Represents a set of task runners of the parent (or associated) document's
// frame. This could be accessed from worker thread(s) and must be initialized
// on the parent context thread (i.e. MainThread) on construction time, rather
// than being done lazily.
class CORE_EXPORT ParentFrameTaskRunners final {
    WTF_MAKE_NONCOPYABLE(ParentFrameTaskRunners);
    USING_FAST_MALLOC(ParentFrameTaskRunners);
public:
    static std::unique_ptr<ParentFrameTaskRunners> create(Document* document)
    {
        return wrapUnique(new ParentFrameTaskRunners(document));
    }

    ~ParentFrameTaskRunners();

    WebTaskRunner* getUnthrottledTaskRunner();
    WebTaskRunner* getTimerTaskRunner();
    WebTaskRunner* getLoadingTaskRunner();

private:
    ParentFrameTaskRunners(Document*);

    std::unique_ptr<WebTaskRunner> m_unthrottledTaskRunner;
    std::unique_ptr<WebTaskRunner> m_timerTaskRunner;
    std::unique_ptr<WebTaskRunner> m_loadingTaskRunner;
};

} // namespace blink

#endif // ParentFrameTaskRunners_h
