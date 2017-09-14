// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskType_h
#define TaskType_h

namespace blink {

// A list of task sources known to Blink according to the spec.
enum class TaskType : unsigned {
  // Speced tasks and related internal tasks should be posted to one of
  // the following task runners. These task runners may be throttled.

  // https://html.spec.whatwg.org/multipage/webappapis.html#generic-task-sources
  //
  // This task source is used for features that react to DOM manipulations, such
  // as things that happen in a non-blocking fashion when an element is inserted
  // into the document.
  kDOMManipulation,
  // This task source is used for features that react to user interaction, for
  // example keyboard or mouse input. Events sent in response to user input
  // (e.g. click events) must be fired using tasks queued with the user
  // interaction task source.
  kUserInteraction,
  // This task source is used for features that trigger in response to network
  // activity.
  kNetworking,
  // This task source is used for control messages between kNetworking tasks.
  kNetworkingControl,
  // This task source is used to queue calls to history.back() and similar APIs.
  kHistoryTraversal,

  // https://html.spec.whatwg.org/multipage/embedded-content.html#the-embed-element
  // This task source is used for the embed element setup steps.
  kEmbed,

  // https://html.spec.whatwg.org/multipage/embedded-content.html#media-elements
  // This task source is used for all tasks queued in the [Media elements]
  // section and subsections of the spec unless explicitly specified otherwise.
  kMediaElementEvent,

  // https://html.spec.whatwg.org/multipage/scripting.html#the-canvas-element
  // This task source is used to invoke the result callback of
  // HTMLCanvasElement.toBlob().
  kCanvasBlobSerialization,

  // https://html.spec.whatwg.org/multipage/webappapis.html#event-loop-processing-model
  // This task source is used when an algorithm requires a microtask to be
  // queued.
  kMicrotask,

  // https://html.spec.whatwg.org/multipage/webappapis.html#timers
  // This task source is used to queue tasks queued by setInterval() and similar
  // APIs.
  kJavascriptTimer,

  // https://html.spec.whatwg.org/multipage/comms.html#sse-processing-model
  // This task source is used for any tasks that are queued by EventSource
  // objects.
  kRemoteEvent,

  // https://html.spec.whatwg.org/multipage/comms.html#feedback-from-the-protocol
  // The task source for all tasks queued in the [WebSocket] section of the
  // spec.
  kWebSocket,

  // https://html.spec.whatwg.org/multipage/comms.html#web-messaging
  // This task source is used for the tasks in cross-document messaging.
  kPostedMessage,

  // https://html.spec.whatwg.org/multipage/comms.html#message-ports
  kUnshippedPortMessage,

  // https://www.w3.org/TR/FileAPI/#blobreader-task-source
  // This task source is used for all tasks queued in the FileAPI spec to read
  // byte sequences associated with Blob and File objects.
  kFileReading,

  // https://www.w3.org/TR/IndexedDB/#request-api
  kDatabaseAccess,

  // https://w3c.github.io/presentation-api/#common-idioms
  // This task source is used for all tasks in the Presentation API spec.
  kPresentation,

  // https://www.w3.org/TR/2016/WD-generic-sensor-20160830/#sensor-task-source
  // This task source is used for all tasks in the Sensor API spec.
  kSensor,

  // https://w3c.github.io/performance-timeline/#performance-timeline
  kPerformanceTimeline,

  // https://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15
  // This task source is used for all tasks in the WebGL spec.
  kWebGL,

  // https://www.w3.org/TR/requestidlecallback/#start-an-event-loop-s-idle-period
  kIdleTask,

  // Use MiscPlatformAPI for a task that is defined in the spec but is not yet
  // associated with any specific task runner in the spec. MiscPlatformAPI is
  // not encouraged for stable and matured APIs. The spec should define the task
  // runner explicitly.
  // The task runner may be throttled.
  kMiscPlatformAPI,

  // Other internal tasks that cannot fit any of the above task runners
  // can be posted here, but the usage is not encouraged. The task runner
  // may be throttled.
  //
  // UnspecedLoading type should be used for all tasks associated with
  // loading page content, UnspecedTimer should be used for all other purposes.
  kUnspecedTimer,
  kUnspecedLoading,

  // Tasks that must not be throttled should be posted here, but the usage
  // should be very limited.
  kUnthrottled,
};

}  // namespace blink

#endif
