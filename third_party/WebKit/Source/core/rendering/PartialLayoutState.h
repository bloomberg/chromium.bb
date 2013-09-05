/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef PartialLayoutState_h
#define PartialLayoutState_h

#include "core/rendering/RenderObject.h"

namespace WebCore {

class PartialLayoutState {
    friend class PartialLayoutDisabler;
public:
    PartialLayoutState()
        : m_shouldStop(false)
        , m_stopAtRenderer(0)
        , m_disableCount(0)
    {
    }

    // True if we plan to do a partial layout, or are in the process of stopping a partial layout.
    bool isPartialLayout() const { return m_stopAtRenderer || m_shouldStop; }

    bool isStopping() const { return m_shouldStop; }
    bool checkPartialLayoutComplete(const RenderObject*);
    void setStopAtRenderer(const RenderObject* renderer) { m_stopAtRenderer = renderer; }
    void reset() { m_shouldStop = false; m_stopAtRenderer = 0; }

private:
    void disable() { ASSERT(!m_shouldStop); m_disableCount++; }
    void enable() { ASSERT(m_disableCount > 0); m_disableCount--; }
    const RenderObject* stopAtRenderer() const { return m_disableCount > 0 ? 0 : m_stopAtRenderer; }

    bool m_shouldStop;
    const RenderObject* m_stopAtRenderer;
    int m_disableCount;
};

inline bool PartialLayoutState::checkPartialLayoutComplete(const RenderObject* renderer)
{
    if (m_shouldStop)
        return true;

    if (renderer == stopAtRenderer()) {
        m_shouldStop = true;
        m_stopAtRenderer = 0;
        return true;
    }

    return false;
}

class PartialLayoutDisabler {
    WTF_MAKE_NONCOPYABLE(PartialLayoutDisabler);
public:
    PartialLayoutDisabler(PartialLayoutState& partialLayout, bool disable = true)
        : m_partialLayout(partialLayout)
        , m_disable(disable)
    {
        if (m_disable)
            m_partialLayout.disable();
    }

    ~PartialLayoutDisabler()
    {
        if (m_disable)
            m_partialLayout.enable();
    }
private:
    PartialLayoutState& m_partialLayout;
    bool m_disable;
};

} // namespace WebCore

#endif // PartialLayoutState_h
