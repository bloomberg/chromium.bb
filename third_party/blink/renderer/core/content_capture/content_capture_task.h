// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_

#include <memory>
#include <vector>

#include "cc/paint/node_holder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ContentCaptureClient;
class Document;
class Node;

// This class is used to capture the on-screen content and send them out
// through ContentCaptureClient.
class CORE_EXPORT ContentCaptureTask : public RefCounted<ContentCaptureTask> {
  USING_FAST_MALLOC(ContentCaptureTask);

 public:
  // This class is used for DOMNodeIds.
  class Delegate {
   public:
    // Return if the give |node| has been sent out.
    virtual bool HasSent(const Node& node) = 0;
    // Notify the |node| has been sent.
    virtual void OnSent(const Node& node) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  enum class ScheduleReason {
    kFirstContentChange,
    kContentChange,
    kScrolling,
    kRetryTask,
  };

  enum class TaskState {
    kProcessRetryTask,
    kCaptureContent,
    kProcessCurrentSession,
    kStop,
  };

  ContentCaptureTask(Document& document, Delegate& delegate);
  virtual ~ContentCaptureTask();

  // Schedule the task if it hasn't been done.
  void Schedule(ScheduleReason reason);
  void Shutdown();

  // Invoked when the |node| is detached from LayoutTree.
  void OnNodeDetached(const Node& node);

  // Make those const public for testing purpose.
  static constexpr size_t kBatchSize = 5;

  // Schedules the task with short delay for kFirstContentChange, kScrolling and
  // kRetryTask, with long delay for kContentChange.
  static constexpr int kTaskShortDelayInMS = 500;
  static constexpr int kTaskLongDelayInMS = 5000;

  TaskState GetTaskStateForTesting() const { return task_state_; }

 protected:
  // All protected data and methods are for testing purpose.
  // Return true if the task should pause.
  virtual bool ShouldPause();
  virtual bool CaptureContent(std::vector<cc::NodeHolder>& data);
  virtual ContentCaptureClient* GetContentCaptureClient();

 private:
  struct Session {
    // The list of the captured content.
    std::vector<cc::NodeHolder> captured_content;
    // The first NodeHolder in |captured_content| hasn't been sent.
    std::vector<cc::NodeHolder>::iterator unsent = captured_content.end();
    // The list of content id of node that has been detached from LayoutTree
    // since the last running.
    std::vector<int64_t> detached_nodes;
  };

  // Callback method of delay_task_, runs the content capture task and
  // reschedule it if it necessary.
  void Run(TimerBase*);

  // The actual run method, return if the task completed.
  bool RunInternal();

  // Runs the sub task to capture content.
  bool CaptureContent();

  // Runs the sub task to process the captured content and the detached nodes.
  bool ProcessSession();

  // Sends the captured content in batch.
  void SendContent();

  void ScheduleInternal(ScheduleReason reason);

  std::unique_ptr<Session> session_;
  bool is_scheduled_ = false;

  // Indicates if there is content change since last run.
  bool has_content_change_ = false;

  // Indicates if first data has been sent out.
  bool has_first_data_sent_ = false;
  UntracedMember<Document> document_;
  Delegate* delegate_;
  std::unique_ptr<TaskRunnerTimer<ContentCaptureTask>> delay_task_;
  TaskState task_state_ = TaskState::kStop;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_
