// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveScriptWrappable_h
#define ActiveScriptWrappable_h

#include "core/CoreExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScriptWrappable;
class ScriptWrappableVisitor;

/**
 * Classes deriving from ActiveScriptWrappable will be registered in a
 * thread-specific list. They keep their wrappers and dependant objects alive
 * as long as they have pending activity.
 */
class CORE_EXPORT ActiveScriptWrappable {
    WTF_MAKE_NONCOPYABLE(ActiveScriptWrappable);
public:
    explicit ActiveScriptWrappable(ScriptWrappable*);

    static void traceActiveScriptWrappables(ScriptWrappableVisitor*);

    virtual bool hasPendingActivity() const = 0;

    ScriptWrappable* toScriptWrappable() const;

protected:
    virtual ~ActiveScriptWrappable();

private:
    ScriptWrappable* m_scriptWrappable;
};

} // namespace blink

#endif // ActiveScriptWrappable_h
