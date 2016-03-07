// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletConsole_h
#define WorkletConsole_h

#include "core/frame/ConsoleBase.h"
#include "core/frame/ConsoleTypes.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class ConsoleMessage;
class WorkletGlobalScope;

class WorkletConsole final : public ConsoleBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    static WorkletConsole* create(WorkletGlobalScope* scope)
    {
        return new WorkletConsole(scope);
    }
    ~WorkletConsole() override;

    DECLARE_VIRTUAL_TRACE();

protected:
    ExecutionContext* context() final;
    void reportMessageToConsole(PassRefPtrWillBeRawPtr<ConsoleMessage>) final;

private:
    explicit WorkletConsole(WorkletGlobalScope*);

    RawPtrWillBeMember<WorkletGlobalScope> m_scope;
};

} // namespace blink

#endif // WorkletConsole_h
