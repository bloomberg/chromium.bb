// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskRunnerHelper_h
#define TaskRunnerHelper_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/HashTraits.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalFrame;
class ScriptState;
class WebTaskRunner;

enum class TaskType : unsigned {
  // Speced tasks and related internal tasks should be posted to one of
  // the following task runners. These task runners may be throttled.

  // https://html.spec.whatwg.org/multipage/webappapis.html#generic-task-sources
  //
  // This task source is used for features that react to DOM manipulations, such
  // as things that happen in a non-blocking fashion when an element is inserted
  // into the document.
  DOMManipulation,
  // This task source is used for features that react to user interaction, for
  // example keyboard or mouse input. Events sent in response to user input
  // (e.g. click events) must be fired using tasks queued with the user
  // interaction task source.
  UserInteraction,
  // This task source is used for features that trigger in response to network
  // activity.
  Networking,
  // This task source is used to queue calls to history.back() and similar APIs.
  HistoryTraversal,

  // https://html.spec.whatwg.org/multipage/embedded-content.html#the-embed-element
  // This task source is used for the embed element setup steps.
  Embed,

  // https://html.spec.whatwg.org/multipage/embedded-content.html#media-elements
  // This task source is used for all tasks queued in the [Media elements]
  // section and subsections of the spec unless explicitly specified otherwise.
  MediaElementEvent,

  // https://html.spec.whatwg.org/multipage/scripting.html#the-canvas-element
  // This task source is used to invoke the result callback of
  // HTMLCanvasElement.toBlob().
  CanvasBlobSerialization,

  // https://html.spec.whatwg.org/multipage/webappapis.html#event-loop-processing-model
  // This task source is used when an algorithm requires a microtask to be
  // queued.
  Microtask,

  // https://html.spec.whatwg.org/multipage/webappapis.html#timers
  // This task source is used to queue tasks queued by setInterval() and similar
  // APIs.
  Timer,

  // https://html.spec.whatwg.org/multipage/comms.html#sse-processing-model
  // This task source is used for any tasks that are queued by EventSource
  // objects.
  RemoteEvent,

  // https://html.spec.whatwg.org/multipage/comms.html#feedback-from-the-protocol
  // The task source for all tasks queued in the [WebSocket] section of the
  // spec.
  WebSocket,

  // https://html.spec.whatwg.org/multipage/comms.html#web-messaging
  // This task source is used for the tasks in cross-document messaging.
  PostedMessage,

  // https://html.spec.whatwg.org/multipage/comms.html#message-ports
  UnshippedPortMessage,

  // https://www.w3.org/TR/FileAPI/#blobreader-task-source
  // This task source is used for all tasks queued in the FileAPI spec to read
  // byte sequences associated with Blob and File objects.
  FileReading,

  // https://www.w3.org/TR/IndexedDB/#request-api
  DatabaseAccess,

  // https://w3c.github.io/presentation-api/#common-idioms
  // This task source is used for all tasks in the Presentation API spec.
  Presentation,

  // https://www.w3.org/TR/2016/WD-generic-sensor-20160830/#sensor-task-source
  // This task source is used for all tasks in the Sensor API spec.
  Sensor,

  // https://w3c.github.io/performance-timeline/#performance-timeline
  PerformanceTimeline,

  // https://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15
  // This task source is used for all tasks in the WebGL spec.
  WebGL,

  // Use MiscPlatformAPI for a task that is defined in the spec but is not yet
  // associated with any specific task runner in the spec. MiscPlatformAPI is
  // not encouraged for stable and matured APIs. The spec should define the task
  // runner explicitly.
  // The task runner may be throttled.
  MiscPlatformAPI,

  // Other internal tasks that cannot fit any of the above task runners
  // can be posted here, but the usage is not encouraged. The task runner
  // may be throttled.
  //
  // UnspecedLoading type should be used for all tasks associated with
  // loading page content, UnspecedTimer should be used for all other purposes.
  UnspecedTimer,
  UnspecedLoading,

  // Tasks that must not be throttled should be posted here, but the usage
  // should be very limited.
  Unthrottled,
};

// HashTraits for TaskType.
struct TaskTypeTraits : WTF::GenericHashTraits<TaskType> {
  static const bool emptyValueIsZero = false;
  static TaskType emptyValue() { return static_cast<TaskType>(-1); }
  static void constructDeletedValue(TaskType& slot, bool) {
    slot = static_cast<TaskType>(-2);
  }
  static bool isDeletedValue(TaskType value) {
    return value == static_cast<TaskType>(-2);
  }
};

// A set of helper functions to get a WebTaskRunner for TaskType and a context
// object. The posted tasks are guaranteed to run in a sequence if they have the
// same TaskType and the context objects belong to the same frame.
class CORE_EXPORT TaskRunnerHelper final {
  STATIC_ONLY(TaskRunnerHelper);

 public:
  static RefPtr<WebTaskRunner> get(TaskType, LocalFrame*);
  static RefPtr<WebTaskRunner> get(TaskType, Document*);
  static RefPtr<WebTaskRunner> get(TaskType, ExecutionContext*);
  static RefPtr<WebTaskRunner> get(TaskType, ScriptState*);
};

}  // namespace blink

#endif
