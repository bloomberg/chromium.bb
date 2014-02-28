/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "modules/mediasource/WebKitSourceBuffer.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/TimeRanges.h"
#include "modules/mediasource/WebKitMediaSource.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/Uint8Array.h"

using blink::WebSourceBuffer;

namespace WebCore {

static bool throwExceptionIfRemoved(bool isRemoved, ExceptionState& exceptionState)
{
    if (isRemoved) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer has been removed from its parent MediaSource.");
        return true;
    }
    return false;
}

PassRefPtrWillBeRawPtr<WebKitSourceBuffer> WebKitSourceBuffer::create(PassOwnPtr<WebSourceBuffer> webSourceBuffer, PassRefPtrWillBeRawPtr<WebKitMediaSource> source)
{
    return adoptRefWillBeNoop(new WebKitSourceBuffer(webSourceBuffer, source));
}

WebKitSourceBuffer::WebKitSourceBuffer(PassOwnPtr<WebSourceBuffer> webSourceBuffer, PassRefPtrWillBeRawPtr<WebKitMediaSource> source)
    : m_webSourceBuffer(webSourceBuffer)
    , m_source(source)
    , m_timestampOffset(0)
{
    ASSERT(m_webSourceBuffer);
    ASSERT(m_source);
    ScriptWrappable::init(this);
}

WebKitSourceBuffer::~WebKitSourceBuffer()
{
}

PassRefPtr<TimeRanges> WebKitSourceBuffer::buffered(ExceptionState& exceptionState) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemoved(isRemoved(), exceptionState))
        return nullptr;

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return TimeRanges::create(m_webSourceBuffer->buffered());
}

double WebKitSourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void WebKitSourceBuffer::setTimestampOffset(double offset, ExceptionState& exceptionState)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemoved(isRemoved(), exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If this object is waiting for the end of a media segment to be appended, then throw an InvalidStateError
    // and abort these steps.
    if (!m_webSourceBuffer->setTimestampOffset(offset)) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer is waiting for the end of a media segment to be appended.");
        return;
    }

    // 6. Update the attribute to the new value.
    m_timestampOffset = offset;
}

void WebKitSourceBuffer::append(PassRefPtr<Uint8Array> data, ExceptionState& exceptionState)
{
    TRACE_EVENT0("media", "SourceBuffer::append");

    // SourceBuffer.append() steps from October 1st version of the Media Source Extensions spec.
    // https://dvcs.w3.org/hg/html-media/raw-file/7bab66368f2c/media-source/media-source.html#dom-append

    // 2. If data is null then throw an InvalidAccessError exception and abort these steps.
    if (!data) {
        exceptionState.throwDOMException(InvalidAccessError, "The data array provided is invalid.");
        return;
    }

    // 3. If this object has been removed from the sourceBuffers attribute of media source then throw
    //    an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemoved(isRemoved(), exceptionState))
        return;

    // 5. If the readyState attribute of media source is in the "ended" state then run the following steps:
    // 5.1. Set the readyState attribute of media source to "open"
    // 5.2. Queue a task to fire a simple event named sourceopen at media source.
    m_source->openIfInEndedState();

    // Steps 6 & beyond are handled by m_webSourceBuffer.

    // Use null for |timestampOffset| parameter because the prefixed API does not allow appends
    // to update timestampOffset.
    m_webSourceBuffer->append(data->data(), data->length(), 0);
}

void WebKitSourceBuffer::abort(ExceptionState& exceptionState)
{
    // Section 3.2 abort() method steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemoved(isRemoved(), exceptionState))
        return;
    if (!m_source->isOpen()) {
        exceptionState.throwDOMException(InvalidStateError, "The parent MediaSource's readyState is not 'open'.");
        return;
    }

    // 4. Run the reset parser state algorithm.
    m_webSourceBuffer->abort();
}

void WebKitSourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    m_webSourceBuffer->removedFromMediaSource();
    m_source.clear();
}

bool WebKitSourceBuffer::isRemoved() const
{
    return !m_source;
}

void WebKitSourceBuffer::trace(Visitor* visitor)
{
    visitor->trace(m_source);
}

} // namespace WebCore
