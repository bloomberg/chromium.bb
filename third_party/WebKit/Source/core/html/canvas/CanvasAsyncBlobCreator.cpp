// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "build/build_config.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/fileapi/Blob.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

const double kSlackBeforeDeadline =
    0.001;  // a small slack period between deadline and current time for safety

// The encoding task is highly likely to switch from idle task to alternative
// code path when the startTimeoutDelay is set to be below 150ms. As we want the
// majority of encoding tasks to take the usual async idle task, we set a
// lenient limit -- 200ms here. This limit still needs to be short enough for
// the latency to be negligible to the user.
const double kIdleTaskStartTimeoutDelay = 200.0;
// We should be more lenient on completion timeout delay to ensure that the
// switch from idle to main thread only happens to a minority of toBlob calls
#if !defined(OS_ANDROID)
// Png image encoding on 4k by 4k canvas on Mac HDD takes 5.7+ seconds
const double kIdleTaskCompleteTimeoutDelay = 6700.0;
#else
// Png image encoding on 4k by 4k canvas on Android One takes 9.0+ seconds
const double kIdleTaskCompleteTimeoutDelay = 10000.0;
#endif

bool IsDeadlineNearOrPassed(double deadline_seconds) {
  return (deadline_seconds - kSlackBeforeDeadline -
              MonotonicallyIncreasingTime() <=
          0);
}

String ConvertMimeTypeEnumToString(
    CanvasAsyncBlobCreator::MimeType mime_type_enum) {
  switch (mime_type_enum) {
    case CanvasAsyncBlobCreator::kMimeTypePng:
      return "image/png";
    case CanvasAsyncBlobCreator::kMimeTypeJpeg:
      return "image/jpeg";
    case CanvasAsyncBlobCreator::kMimeTypeWebp:
      return "image/webp";
    default:
      return "image/unknown";
  }
}

CanvasAsyncBlobCreator::MimeType ConvertMimeTypeStringToEnum(
    const String& mime_type) {
  CanvasAsyncBlobCreator::MimeType mime_type_enum;
  if (mime_type == "image/png") {
    mime_type_enum = CanvasAsyncBlobCreator::kMimeTypePng;
  } else if (mime_type == "image/jpeg") {
    mime_type_enum = CanvasAsyncBlobCreator::kMimeTypeJpeg;
  } else if (mime_type == "image/webp") {
    mime_type_enum = CanvasAsyncBlobCreator::kMimeTypeWebp;
  } else {
    mime_type_enum = CanvasAsyncBlobCreator::kNumberOfMimeTypeSupported;
  }
  return mime_type_enum;
}

void RecordIdleTaskStatusHistogram(
    CanvasAsyncBlobCreator::IdleTaskStatus status) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram,
                                  to_blob_idle_task_status,
                                  ("Blink.Canvas.ToBlob.IdleTaskStatus",
                                   CanvasAsyncBlobCreator::kIdleTaskCount));
  to_blob_idle_task_status.Count(status);
}

// This enum is used in histogram and any more types should be appended at the
// end of the list.
enum ElapsedTimeHistogramType {
  kInitiateEncodingDelay,
  kIdleEncodeDuration,
  kToBlobDuration,
  kNumberOfElapsedTimeHistogramTypes
};

void RecordElapsedTimeHistogram(ElapsedTimeHistogramType type,
                                CanvasAsyncBlobCreator::MimeType mime_type,
                                double elapsed_time) {
  if (type == kInitiateEncodingDelay) {
    if (mime_type == CanvasAsyncBlobCreator::kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_initiate_encoding_counter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.PNG", 0, 10000000, 50));
      to_blob_png_initiate_encoding_counter.Count(elapsed_time * 1000000.0);
    } else if (mime_type == CanvasAsyncBlobCreator::kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_initiate_encoding_counter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.JPEG", 0, 10000000, 50));
      to_blob_jpeg_initiate_encoding_counter.Count(elapsed_time * 1000000.0);
    }
  } else if (type == kIdleEncodeDuration) {
    if (mime_type == CanvasAsyncBlobCreator::kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_idle_encode_counter,
          ("Blink.Canvas.ToBlob.IdleEncodeDuration.PNG", 0, 10000000, 50));
      to_blob_png_idle_encode_counter.Count(elapsed_time * 1000000.0);
    } else if (mime_type == CanvasAsyncBlobCreator::kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_idle_encode_counter,
          ("Blink.Canvas.ToBlob.IdleEncodeDuration.JPEG", 0, 10000000, 50));
      to_blob_jpeg_idle_encode_counter.Count(elapsed_time * 1000000.0);
    }
  } else if (type == kToBlobDuration) {
    if (mime_type == CanvasAsyncBlobCreator::kMimeTypePng) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_png_counter,
          ("Blink.Canvas.ToBlobDuration.PNG", 0, 10000000, 50));
      to_blob_png_counter.Count(elapsed_time * 1000000.0);
    } else if (mime_type == CanvasAsyncBlobCreator::kMimeTypeJpeg) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_jpeg_counter,
          ("Blink.Canvas.ToBlobDuration.JPEG", 0, 10000000, 50));
      to_blob_jpeg_counter.Count(elapsed_time * 1000000.0);
    } else if (mime_type == CanvasAsyncBlobCreator::kMimeTypeWebp) {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, to_blob_webp_counter,
          ("Blink.Canvas.ToBlobDuration.WEBP", 0, 10000000, 50));
      to_blob_webp_counter.Count(elapsed_time * 1000000.0);
    }
  }
}

}  // anonymous namespace

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::Create(
    DOMUint8ClampedArray* unpremultiplied_rgba_image_data,
    const String& mime_type,
    const IntSize& size,
    BlobCallback* callback,
    double start_time,
    ExecutionContext* context) {
  return new CanvasAsyncBlobCreator(
      unpremultiplied_rgba_image_data, ConvertMimeTypeStringToEnum(mime_type),
      size, callback, start_time, context, nullptr);
}

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::Create(
    DOMUint8ClampedArray* unpremultiplied_rgba_image_data,
    const String& mime_type,
    const IntSize& size,
    double start_time,
    ExecutionContext* context,
    ScriptPromiseResolver* resolver) {
  return new CanvasAsyncBlobCreator(
      unpremultiplied_rgba_image_data, ConvertMimeTypeStringToEnum(mime_type),
      size, nullptr, start_time, context, resolver);
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(DOMUint8ClampedArray* data,
                                               MimeType mime_type,
                                               const IntSize& size,
                                               BlobCallback* callback,
                                               double start_time,
                                               ExecutionContext* context,
                                               ScriptPromiseResolver* resolver)
    : data_(data),
      context_(context),
      mime_type_(mime_type),
      start_time_(start_time),
      elapsed_time_(0),
      callback_(callback),
      script_promise_resolver_(resolver) {
  size_t rowBytes = size.Width() * 4;
  DCHECK(data_->length() == (unsigned)(size.Height() * rowBytes));
  SkImageInfo info =
      SkImageInfo::Make(size.Width(), size.Height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType, nullptr);
  src_data_.reset(info, data_->Data(), rowBytes);
  idle_task_status_ = kIdleTaskNotSupported;
  num_rows_completed_ = 0;
  if (context->IsDocument()) {
    parent_frame_task_runner_ =
        ParentFrameTaskRunners::Create(*ToDocument(context)->GetFrame());
  }
  if (script_promise_resolver_) {
    function_type_ = kOffscreenCanvasToBlobPromise;
  } else {
    function_type_ = kHTMLCanvasToBlobCallback;
  }
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator() {}

void CanvasAsyncBlobCreator::Dispose() {
  // Eagerly let go of references to prevent retention of these
  // resources while any remaining posted tasks are queued.
  data_.Clear();
  context_.Clear();
  parent_frame_task_runner_.Clear();
  callback_.Clear();
  script_promise_resolver_.Clear();
}

void CanvasAsyncBlobCreator::ScheduleAsyncBlobCreation(const double& quality) {
  if (mime_type_ == kMimeTypeWebp) {
    if (!IsMainThread()) {
      DCHECK(function_type_ == kOffscreenCanvasToBlobPromise);
      // When OffscreenCanvas.convertToBlob() occurs on worker thread,
      // we do not need to use background task runner to reduce load on main.
      // So we just directly encode images on the worker thread.
      IntSize size(src_data_.width(), src_data_.height());
      if (!ImageDataBuffer(size, data_->Data())
               .EncodeImage("image/webp", quality, &encoded_image_)) {
        TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
            ->PostTask(
                BLINK_FROM_HERE,
                WTF::Bind(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                          WrapPersistent(this)));

        return;
      }
      TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
          ->PostTask(
              BLINK_FROM_HERE,
              WTF::Bind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                        WrapPersistent(this)));

    } else {
      BackgroundTaskRunner::PostOnBackgroundThread(
          BLINK_FROM_HERE,
          CrossThreadBind(&CanvasAsyncBlobCreator::EncodeImageOnEncoderThread,
                          WrapCrossThreadPersistent(this), quality));
    }
  } else {
    idle_task_status_ = kIdleTaskNotStarted;
    this->ScheduleInitiateEncoding(quality);

    // We post the below task to check if the above idle task isn't late.
    // There's no risk of concurrency as both tasks are on the same thread.
    this->PostDelayedTaskToCurrentThread(
        BLINK_FROM_HERE,
        WTF::Bind(&CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent,
                  WrapPersistent(this), quality),
        kIdleTaskStartTimeoutDelay);
  }
}

void CanvasAsyncBlobCreator::ScheduleInitiateEncoding(double quality) {
  schedule_initiate_start_time_ = WTF::MonotonicallyIncreasingTime();
  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      BLINK_FROM_HERE, WTF::Bind(&CanvasAsyncBlobCreator::InitiateEncoding,
                                 WrapPersistent(this), quality));
}

void CanvasAsyncBlobCreator::InitiateEncoding(double quality,
                                              double deadline_seconds) {
  RecordElapsedTimeHistogram(
      kInitiateEncodingDelay, mime_type_,
      WTF::MonotonicallyIncreasingTime() - schedule_initiate_start_time_);
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }

  DCHECK(idle_task_status_ == kIdleTaskNotStarted);
  idle_task_status_ = kIdleTaskStarted;

  if (!InitializeEncoder(quality)) {
    idle_task_status_ = kIdleTaskFailed;
    return;
  }

  this->IdleEncodeRows(deadline_seconds);
}

void CanvasAsyncBlobCreator::IdleEncodeRows(double deadline_seconds) {
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }

  double start_time = WTF::MonotonicallyIncreasingTime();
  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (IsDeadlineNearOrPassed(deadline_seconds)) {
      num_rows_completed_ = y;
      elapsed_time_ += (WTF::MonotonicallyIncreasingTime() - start_time);
      Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
          BLINK_FROM_HERE, WTF::Bind(&CanvasAsyncBlobCreator::IdleEncodeRows,
                                     WrapPersistent(this)));
      return;
    }

    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      this->CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  idle_task_status_ = kIdleTaskCompleted;
  elapsed_time_ += (WTF::MonotonicallyIncreasingTime() - start_time);
  RecordElapsedTimeHistogram(kIdleEncodeDuration, mime_type_, elapsed_time_);
  if (IsDeadlineNearOrPassed(deadline_seconds)) {
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                             WrapPersistent(this)));
  } else {
    this->CreateBlobAndReturnResult();
  }
}

void CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread() {
  DCHECK(idle_task_status_ == kIdleTaskSwitchedToImmediateTask);

  // Continue encoding from the last completed row
  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      this->CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  if (IsMainThread()) {
    this->CreateBlobAndReturnResult();
  } else {
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
        ->PostTask(
            BLINK_FROM_HERE,
            CrossThreadBind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                            WrapCrossThreadPersistent(this)));
  }

  this->SignalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::CreateBlobAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);
  RecordElapsedTimeHistogram(kToBlobDuration, mime_type_,
                             WTF::MonotonicallyIncreasingTime() - start_time_);

  Blob* result_blob = Blob::Create(encoded_image_.data(), encoded_image_.size(),
                                   ConvertMimeTypeEnumToString(mime_type_));
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
        ->PostTask(BLINK_FROM_HERE, WTF::Bind(&BlobCallback::handleEvent,
                                              WrapPersistent(callback_.Get()),
                                              WrapPersistent(result_blob)));
  } else {
    script_promise_resolver_->Resolve(result_blob);
  }
  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::CreateNullAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    DCHECK(IsMainThread());
    RecordIdleTaskStatusHistogram(idle_task_status_);
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&BlobCallback::handleEvent,
                             WrapPersistent(callback_.Get()), nullptr));
  } else {
    script_promise_resolver_->Reject(DOMException::Create(
        kEncodingError, "Encoding of the source image has failed."));
  }
  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::EncodeImageOnEncoderThread(double quality) {
  DCHECK(!IsMainThread());
  DCHECK(mime_type_ == kMimeTypeWebp);

  IntSize size(src_data_.width(), src_data_.height());
  if (!ImageDataBuffer(size, data_->Data())
           .EncodeImage("image/webp", quality, &encoded_image_)) {
    parent_frame_task_runner_->Get(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            BLINK_FROM_HERE,
            CrossThreadBind(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                            WrapCrossThreadPersistent(this)));
    return;
  }

  parent_frame_task_runner_->Get(TaskType::kCanvasBlobSerialization)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                          WrapCrossThreadPersistent(this)));
}

bool CanvasAsyncBlobCreator::InitializeEncoder(double quality) {
  if (mime_type_ == kMimeTypeJpeg) {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    options.fBlendBehavior = SkTransferFunctionBehavior::kIgnore;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  } else {
    // Progressive encoding is only applicable to png and jpeg image format,
    // and thus idle tasks scheduling can only be applied to these image
    // formats.
    // TODO(xlai): Progressive encoding on webp image formats
    // (crbug.com/571399)
    DCHECK_EQ(kMimeTypePng, mime_type_);
    SkPngEncoder::Options options;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
    options.fZLibLevel = 3;
    options.fUnpremulBehavior = SkTransferFunctionBehavior::kIgnore;
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  }

  return encoder_.get();
}

void CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent(double quality) {
  if (idle_task_status_ == kIdleTaskStarted) {
    // Even if the task started quickly, we still want to ensure completion
    this->PostDelayedTaskToCurrentThread(
        BLINK_FROM_HERE,
        WTF::Bind(&CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent,
                  WrapPersistent(this)),
        kIdleTaskCompleteTimeoutDelay);
  } else if (idle_task_status_ == kIdleTaskNotStarted) {
    // If the idle task does not start after a delay threshold, we will
    // force it to happen on main thread (even though it may cause more
    // janks) to prevent toBlob being postponed forever in extreme cases.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInStartTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    if (InitializeEncoder(quality)) {
      TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
          ->PostTask(
              BLINK_FROM_HERE,
              WTF::Bind(&CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                        WrapPersistent(this)));
    } else {
      // Failing in initialization of encoder
      this->SignalAlternativeCodePathFinishedForTesting();
    }
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    this->SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent() {
  DCHECK(idle_task_status_ != kIdleTaskNotStarted);

  if (idle_task_status_ == kIdleTaskStarted) {
    // It has taken too long to complete for the idle task.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInCompleteTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
        ->PostTask(
            BLINK_FROM_HERE,
            WTF::Bind(&CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                      WrapPersistent(this)));
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    this->SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const WebTraceLocation& location,
    WTF::Closure task,
    double delay_ms) {
  TaskRunnerHelper::Get(TaskType::kCanvasBlobSerialization, context_)
      ->PostDelayedTask(location, std::move(task),
                        TimeDelta::FromMillisecondsD(delay_ms));
}

void CanvasAsyncBlobCreator::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(data_);
  visitor->Trace(callback_);
  visitor->Trace(parent_frame_task_runner_);
  visitor->Trace(script_promise_resolver_);
}

}  // namespace blink
