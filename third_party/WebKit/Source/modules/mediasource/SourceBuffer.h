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

#ifndef SourceBuffer_h
#define SourceBuffer_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/EventTarget.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/SourceBufferPrivate.h"
#include "weborigin/KURL.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class FileReaderLoader;
class GenericEventQueue;
class MediaSource;
class Stream;
class TimeRanges;

class SourceBuffer : public RefCounted<SourceBuffer>, public ActiveDOMObject, public EventTarget, public ScriptWrappable, public FileReaderLoaderClient {
public:
    static PassRefPtr<SourceBuffer> create(PassOwnPtr<SourceBufferPrivate>, MediaSource*, GenericEventQueue*);

    virtual ~SourceBuffer();

    // SourceBuffer.idl methods
    bool updating() const { return m_updating; }
    PassRefPtr<TimeRanges> buffered(ExceptionState&) const;
    double timestampOffset() const;
    void setTimestampOffset(double, ExceptionState&);
    void appendBuffer(PassRefPtr<ArrayBuffer> data, ExceptionState&);
    void appendBuffer(PassRefPtr<ArrayBufferView> data, ExceptionState&);
    void appendStream(PassRefPtr<Stream>, ExceptionState&);
    void appendStream(PassRefPtr<Stream>, unsigned long long maxSize, ExceptionState&);
    void abort(ExceptionState&);
    void remove(double start, double end, ExceptionState&);
    double appendWindowStart() const;
    void setAppendWindowStart(double, ExceptionState&);
    double appendWindowEnd() const;
    void setAppendWindowEnd(double, ExceptionState&);

    void abortIfUpdating();
    void removedFromMediaSource();

    // ActiveDOMObject interface
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    // EventTarget interface
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;
    virtual const AtomicString& interfaceName() const OVERRIDE;

    using RefCounted<SourceBuffer>::ref;
    using RefCounted<SourceBuffer>::deref;

protected:
    // EventTarget interface
    virtual EventTargetData* eventTargetData() OVERRIDE;
    virtual EventTargetData* ensureEventTargetData() OVERRIDE;
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

private:
    SourceBuffer(PassOwnPtr<SourceBufferPrivate>, MediaSource*, GenericEventQueue*);

    bool isRemoved() const;
    void scheduleEvent(const AtomicString& eventName);

    void appendBufferInternal(const unsigned char*, unsigned, ExceptionState&);
    void appendBufferTimerFired(Timer<SourceBuffer>*);

    void removeTimerFired(Timer<SourceBuffer>*);

    void appendStreamInternal(PassRefPtr<Stream>, ExceptionState&);
    void appendStreamTimerFired(Timer<SourceBuffer>*);
    void appendStreamDone(bool success);
    void clearAppendStreamState();

    // FileReaderLoaderClient interface
    virtual void didStartLoading() OVERRIDE;
    virtual void didReceiveDataForClient(const char* data, unsigned dataLength) OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

    OwnPtr<SourceBufferPrivate> m_private;
    MediaSource* m_source;
    GenericEventQueue* m_asyncEventQueue;
    EventTargetData m_eventTargetData;

    bool m_updating;
    double m_timestampOffset;
    double m_appendWindowStart;
    double m_appendWindowEnd;

    Vector<unsigned char> m_pendingAppendData;
    Timer<SourceBuffer> m_appendBufferTimer;

    double m_pendingRemoveStart;
    double m_pendingRemoveEnd;
    Timer<SourceBuffer> m_removeTimer;

    bool m_streamMaxSizeValid;
    unsigned long long m_streamMaxSize;
    Timer<SourceBuffer> m_appendStreamTimer;
    RefPtr<Stream> m_stream;
    OwnPtr<FileReaderLoader> m_loader;
};

} // namespace WebCore

#endif
