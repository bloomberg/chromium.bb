// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorWebPerfAgent_h
#define InspectorWebPerfAgent_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class InspectedFrames;

// Inspector Agent for Web Performance APIs
class CORE_EXPORT InspectorWebPerfAgent final : public GarbageCollectedFinalized<InspectorWebPerfAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorWebPerfAgent);
public:
    InspectorWebPerfAgent(InspectedFrames*);
    ~InspectorWebPerfAgent();
    DECLARE_VIRTUAL_TRACE();

    void willExecuteScript(ExecutionContext*);
    void didExecuteScript();

private:
    Member<InspectedFrames> m_inspectedFrames;
};

} // namespace blink

#endif // InspectorWebPerfAgent_h
