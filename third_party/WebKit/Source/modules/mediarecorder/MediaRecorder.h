// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRecorder_h
#define MediaRecorder_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediarecorder/MediaRecorderOptions.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/AsyncMethodRunner.h"
#include "public/platform/WebMediaRecorderHandler.h"
#include "public/platform/WebMediaRecorderHandlerClient.h"

namespace blink {

class Blob;
class BlobData;
class ExceptionState;

class MODULES_EXPORT MediaRecorder final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<MediaRecorder>
    , public WebMediaRecorderHandlerClient
    , public ActiveDOMObject {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(MediaRecorder);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaRecorder);
    DEFINE_WRAPPERTYPEINFO();
public:
    enum class State {
        Inactive = 0,
        Recording,
        Paused
    };

    static MediaRecorder* create(ExecutionContext*, MediaStream*, ExceptionState&);
    static MediaRecorder* create(ExecutionContext*, MediaStream*, const MediaRecorderOptions&, ExceptionState&);

    virtual ~MediaRecorder() {}

    MediaStream* stream() const { return m_stream.get(); }
    const String& mimeType() const { return m_mimeType; }
    String state() const;
    bool ignoreMutedMedia() const { return m_ignoreMutedMedia; }
    void setIgnoreMutedMedia(bool ignoreMutedMedia) { m_ignoreMutedMedia = ignoreMutedMedia; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(start);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(stop);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dataavailable);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(pause);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(resume);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void start(ExceptionState&);
    void start(int timeSlice, ExceptionState&);
    void stop(ExceptionState&);
    void pause(ExceptionState&);
    void resume(ExceptionState&);
    void requestData(ExceptionState&);

    static bool isTypeSupported(const String& type);

    // EventTarget
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    // ActiveDOMObject
    void suspend() override;
    void resume() override;
    void stop() override;
    bool hasPendingActivity() const override { return !m_stopped; }

    // WebMediaRecorderHandlerClient
    void writeData(const char* data, size_t length, bool lastInSlice) override;
    void onError(const WebString& message) override;

    DECLARE_VIRTUAL_TRACE();

private:
    MediaRecorder(ExecutionContext*, MediaStream*, const MediaRecorderOptions&, ExceptionState&);

    void createBlobEvent(Blob*);

    void stopRecording();
    void scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event>);
    void dispatchScheduledEvent();

    Member<MediaStream> m_stream;
    size_t m_streamAmountOfTracks;
    String m_mimeType;
    bool m_stopped;
    bool m_ignoreMutedMedia;

    State m_state;

    OwnPtr<BlobData> m_blobData;

    OwnPtr<WebMediaRecorderHandler> m_recorderHandler;

    Member<AsyncMethodRunner<MediaRecorder>> m_dispatchScheduledEventRunner;
    WillBeHeapVector<RefPtrWillBeMember<Event>> m_scheduledEvents;
};

} // namespace blink

#endif // MediaRecorder_h
