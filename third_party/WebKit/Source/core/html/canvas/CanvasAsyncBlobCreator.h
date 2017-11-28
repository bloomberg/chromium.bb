// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasAsyncBlobCreator_h
#define CanvasAsyncBlobCreator_h

#include <memory>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/v8_blob_callback.h"
#include "core/CoreExport.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "platform/image-encoders/ImageEncoder.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT CanvasAsyncBlobCreator
    : public GarbageCollectedFinalized<CanvasAsyncBlobCreator> {
 public:
  static CanvasAsyncBlobCreator* Create(
      DOMUint8ClampedArray* unpremultiplied_rgba_image_data,
      const String& mime_type,
      const IntSize&,
      V8BlobCallback*,
      double start_time,
      ExecutionContext*);
  static CanvasAsyncBlobCreator* Create(
      DOMUint8ClampedArray* unpremultiplied_rgba_image_data,
      const String& mime_type,
      const IntSize&,
      double start_time,
      ExecutionContext*,
      ScriptPromiseResolver*);
  void ScheduleAsyncBlobCreation(const double& quality);
  virtual ~CanvasAsyncBlobCreator();
  enum MimeType {
    kMimeTypePng,
    kMimeTypeJpeg,
    kMimeTypeWebp,
    kNumberOfMimeTypeSupported
  };
  // This enum is used to back an UMA histogram, and should therefore be treated
  // as append-only.
  enum IdleTaskStatus {
    kIdleTaskNotStarted,
    kIdleTaskStarted,
    kIdleTaskCompleted,
    kIdleTaskFailed,
    kIdleTaskSwitchedToImmediateTask,
    kIdleTaskNotSupported,  // Idle tasks are not implemented for some image
                            // types
    kIdleTaskCount,         // Should not be seen in production
  };

  enum ToBlobFunctionType {
    kHTMLCanvasToBlobCallback,
    kOffscreenCanvasToBlobPromise,
    kNumberOfToBlobFunctionTypes
  };

  // Methods are virtual for mocking in unit tests
  virtual void SignalTaskSwitchInStartTimeoutEventForTesting() {}
  virtual void SignalTaskSwitchInCompleteTimeoutEventForTesting() {}

  virtual void Trace(blink::Visitor*);

 protected:
  CanvasAsyncBlobCreator(DOMUint8ClampedArray* data,
                         MimeType,
                         const IntSize&,
                         V8BlobCallback*,
                         double,
                         ExecutionContext*,
                         ScriptPromiseResolver*);
  // Methods are virtual for unit testing
  virtual void ScheduleInitiateEncoding(double quality);
  virtual void IdleEncodeRows(double deadline_seconds);
  virtual void PostDelayedTaskToCurrentThread(const WebTraceLocation&,
                                              WTF::Closure,
                                              double delay_ms);
  virtual void SignalAlternativeCodePathFinishedForTesting() {}
  virtual void CreateBlobAndReturnResult();
  virtual void CreateNullAndReturnResult();

  void InitiateEncoding(double quality, double deadline_seconds);
  IdleTaskStatus idle_task_status_;

 private:
  friend class CanvasAsyncBlobCreatorTest;

  void Dispose();

  Member<DOMUint8ClampedArray> data_;
  std::unique_ptr<ImageEncoder> encoder_;
  Vector<unsigned char> encoded_image_;
  int num_rows_completed_;
  Member<ExecutionContext> context_;

  SkPixmap src_data_;
  const MimeType mime_type_;
  double start_time_;
  double schedule_initiate_start_time_;
  double elapsed_time_;

  ToBlobFunctionType function_type_;

  // Used when CanvasAsyncBlobCreator runs on main thread only
  Member<ParentFrameTaskRunners> parent_frame_task_runner_;

  // Used for HTMLCanvasElement only
  //
  // Note: CanvasAsyncBlobCreator is never held by other objects. As soon as
  // an instance gets created, ScheduleAsyncBlobCreation is invoked, and then
  // the instance is only held by a task runner (via PostTask). Thus the
  // instance has only limited lifetime. Hence, Persistent here is okay.
  V8BlobCallback::Persistent<V8BlobCallback> callback_;

  // Used for OffscreenCanvas only
  Member<ScriptPromiseResolver> script_promise_resolver_;

  // PNG, JPEG
  bool InitializeEncoder(double quality);
  void ForceEncodeRowsOnCurrentThread();  // Similar to IdleEncodeRows
                                          // without deadline

  // WEBP
  void EncodeImageOnEncoderThread(double quality);

  void IdleTaskStartTimeoutEvent(double quality);
  void IdleTaskCompleteTimeoutEvent();
};

}  // namespace blink

#endif  // CanvasAsyncBlobCreator_h
