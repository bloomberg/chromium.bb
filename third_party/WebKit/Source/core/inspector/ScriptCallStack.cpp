/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "core/inspector/ScriptCallStack.h"

#include "platform/TracedValue.h"

namespace blink {

PassRefPtr<ScriptCallStack> ScriptCallStack::create(Vector<ScriptCallFrame>& frames)
{
    return adoptRef(new ScriptCallStack(String(), frames, nullptr));
}

PassRefPtr<ScriptCallStack> ScriptCallStack::create(const String& description, Vector<ScriptCallFrame>& frames, PassRefPtr<ScriptCallStack> parent)
{
    return adoptRef(new ScriptCallStack(description, frames, parent));
}

ScriptCallStack::ScriptCallStack(const String& description, Vector<ScriptCallFrame>& frames, PassRefPtr<ScriptCallStack> parent)
    : m_description(description)
    , m_parent(parent)
{
    m_frames.swap(frames);
}

ScriptCallStack::~ScriptCallStack()
{
}

const ScriptCallFrame &ScriptCallStack::at(size_t index) const
{
    ASSERT(m_frames.size() > index);
    return m_frames[index];
}

void ScriptCallStack::setParent(PassRefPtr<ScriptCallStack> parent)
{
    m_parent = parent;
}

size_t ScriptCallStack::size() const
{
    return m_frames.size();
}

PassRefPtr<TypeBuilder::Runtime::StackTrace> ScriptCallStack::buildInspectorObject() const
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::CallFrame>> frames = TypeBuilder::Array<TypeBuilder::Runtime::CallFrame>::create();
    for (size_t i = 0; i < m_frames.size(); i++)
        frames->addItem(m_frames.at(i).buildInspectorObject());

    RefPtr<TypeBuilder::Runtime::StackTrace> stackTrace = TypeBuilder::Runtime::StackTrace::create()
        .setCallFrames(frames.release());
    if (!m_description.isEmpty())
        stackTrace->setDescription(m_description);
    if (m_parent)
        stackTrace->setParent(m_parent->buildInspectorObject());
    return stackTrace;
}

void ScriptCallStack::toTracedValue(TracedValue* value, const char* name) const
{
    value->beginArray(name);
    for (size_t i = 0; i < m_frames.size(); i++)
        m_frames.at(i).toTracedValue(value);
    value->endArray();
}

} // namespace blink
