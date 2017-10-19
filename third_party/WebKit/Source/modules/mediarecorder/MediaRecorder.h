// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRecorder_h
#define MediaRecorder_h

#include <memory>
#include "core/dom/SuspendableObject.h"
#include "core/dom/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediarecorder/MediaRecorderOptions.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "public/platform/WebMediaRecorderHandler.h"
#include "public/platform/WebMediaRecorderHandlerClient.h"

namespace blink {

class Blob;
class BlobData;
class ExceptionState;

class MODULES_EXPORT MediaRecorder final
    : public EventTargetWithInlineData,
      public WebMediaRecorderHandlerClient,
      public ActiveScriptWrappable<MediaRecorder>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(MediaRecorder);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class State { kInactive = 0, kRecording, kPaused };

  static MediaRecorder* Create(ExecutionContext*,
                               MediaStream*,
                               ExceptionState&);
  static MediaRecorder* Create(ExecutionContext*,
                               MediaStream*,
                               const MediaRecorderOptions&,
                               ExceptionState&);

  virtual ~MediaRecorder() {}

  MediaStream* stream() const { return stream_.Get(); }
  const String& mimeType() const { return mime_type_; }
  String state() const;
  unsigned long videoBitsPerSecond() const { return video_bits_per_second_; }
  unsigned long audioBitsPerSecond() const { return audio_bits_per_second_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(start);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(stop);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dataavailable);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pause);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resume);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void start(ExceptionState&);
  void start(int time_slice, ExceptionState&);
  void stop(ExceptionState&);
  void pause(ExceptionState&);
  void resume(ExceptionState&);
  void requestData(ExceptionState&);

  static bool isTypeSupported(const String& type);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // SuspendableObject
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  bool HasPendingActivity() const final { return !stopped_; }

  // WebMediaRecorderHandlerClient
  void WriteData(const char* data,
                 size_t length,
                 bool last_in_slice,
                 double timecode) override;
  void OnError(const WebString& message) override;

  virtual void Trace(blink::Visitor*);

 private:
  MediaRecorder(ExecutionContext*,
                MediaStream*,
                const MediaRecorderOptions&,
                ExceptionState&);

  void CreateBlobEvent(Blob*, double);

  void StopRecording();
  void ScheduleDispatchEvent(Event*);
  void DispatchScheduledEvent();

  Member<MediaStream> stream_;
  String mime_type_;
  bool stopped_;
  int audio_bits_per_second_;
  int video_bits_per_second_;

  State state_;

  std::unique_ptr<BlobData> blob_data_;

  std::unique_ptr<WebMediaRecorderHandler> recorder_handler_;

  Member<AsyncMethodRunner<MediaRecorder>> dispatch_scheduled_event_runner_;
  HeapVector<Member<Event>> scheduled_events_;
};

}  // namespace blink

#endif  // MediaRecorder_h
