/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "config.h"
#include "core/rendering/LayoutRectRecorder.h"

#include "core/rendering/RenderObject.h"

namespace WebCore {

LayoutRectRecorder::LayoutRectRecorder(RenderObject& object, bool skipRecording)
    : m_object(object)
    , m_repaintContainer(0)
    , m_skipRecording(skipRecording)
{
    if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return;
    if (m_skipRecording)
        return;

    m_repaintContainer = m_object.containerForRepaint();

    // FIXME: This will do more work then needed in some cases. The isEmpty will
    // return true if width <=0 or height <=0 so if we have a 0x0 item it will
    // set the old rect each time it comes through here until it's given a size.
    if (m_object.everHadLayout() && m_object.oldRepaintRect().isEmpty()) {
        m_object.setOldRepaintRect(m_object.clippedOverflowRectForRepaint(m_repaintContainer));
    }
}

LayoutRectRecorder::~LayoutRectRecorder()
{
    if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return;
    if (m_skipRecording)
        return;

    m_object.setNewRepaintRect(m_object.clippedOverflowRectForRepaint(m_repaintContainer));
}

} // namespace WebCore

