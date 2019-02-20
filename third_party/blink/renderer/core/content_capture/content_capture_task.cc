// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_task.h"

#include "base/auto_reset.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_client.h"
#include "third_party/blink/renderer/core/content_capture/content_holder.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ContentCaptureTask::ContentCaptureTask(Document& document, Delegate& delegate)
    : document_(&document), delegate_(&delegate) {}

ContentCaptureTask::~ContentCaptureTask() {}

void ContentCaptureTask::Shutdown() {
  DCHECK(document_);
  document_ = nullptr;
  delegate_ = nullptr;
}

void ContentCaptureTask::OnNodeDetached(const Node& node) {
  if (!session_) {
    session_ = std::make_unique<Session>();
  }
  // TODO(michaelbai): might limit the size of detached_nodes.
  session_->detached_nodes.push_back(reinterpret_cast<int64_t>(&node));
}

bool ContentCaptureTask::CaptureContent(std::vector<cc::NodeHolder>& data) {
  // Because this is called from a different task, the frame may be in any
  // lifecycle step so we need to early-out in many cases.
  // TODO(michaelbai): runs task in main frame, and sends the captured content
  // for each document separately.
  if (const auto* frame = document_->GetFrame()) {
    if (const auto* root_frame_view = frame->LocalFrameRoot().View()) {
      if (const auto* cc_layer = root_frame_view->RootCcLayer()) {
        if (auto* layer_tree_host = cc_layer->layer_tree_host())
          return layer_tree_host->CaptureContent(&data);
      }
    }
  }
  return false;
}

bool ContentCaptureTask::CaptureContent() {
  DCHECK(session_);
  bool success = CaptureContent(session_->captured_content);
  session_->unsent = session_->captured_content.begin();
  return success;
}

void ContentCaptureTask::SendContent() {
  DCHECK(session_);
  std::vector<scoped_refptr<ContentHolder>> content_batch;
  content_batch.reserve(kBatchSize);
  for (; session_->unsent != session_->captured_content.end() &&
         content_batch.size() < kBatchSize;
       ++session_->unsent) {
    scoped_refptr<ContentHolder> content_holder;
    if (session_->unsent->type == cc::NodeHolder::Type::kID) {
      Node* node = DOMNodeIds::NodeForId(session_->unsent->id);
      if (node && node->GetLayoutObject() && !delegate_->HasSent(*node)) {
        content_holder = base::MakeRefCounted<ContentHolder>(*node);
        delegate_->OnSent(*node);
        content_batch.push_back(content_holder);
      }
    } else if (session_->unsent->type == cc::NodeHolder::Type::kTextHolder &&
               session_->unsent->text_holder) {
      content_holder = scoped_refptr<ContentHolder>(
          static_cast<ContentHolder*>(session_->unsent->text_holder.get()));
      if (content_holder && content_holder->IsValid() &&
          !content_holder->HasSent()) {
        content_holder->SetHasSent();
        content_batch.push_back(content_holder);
      }
    }
  }
  if (!content_batch.empty()) {
    GetContentCaptureClient()->DidCaptureContent(content_batch,
                                                 !has_first_data_sent_);
    has_first_data_sent_ = true;
  }
  if (session_->unsent == session_->captured_content.end())
    session_->captured_content.clear();
}

ContentCaptureClient* ContentCaptureTask::GetContentCaptureClient() {
  // TODO(michaelbai): Enable this after integrate with document.
  // return document_->GetFrame()->Client()->GetContentCaptureClient();
  return nullptr;
}

bool ContentCaptureTask::ProcessSession() {
  DCHECK(session_);
  while (!session_->captured_content.empty()) {
    SendContent();
    if (ShouldPause()) {
      return session_->captured_content.empty() &&
             session_->detached_nodes.empty();
    }
  }
  // Sent the detached nodes.
  if (!session_->detached_nodes.empty()) {
    GetContentCaptureClient()->DidRemoveContent(session_->detached_nodes);
    session_->detached_nodes.clear();
  }
  session_.reset();
  return true;
}

bool ContentCaptureTask::RunInternal() {
  base::AutoReset<TaskState> state(&task_state_, TaskState::kProcessRetryTask);
  // Already shutdown.
  if (!document_ || !GetContentCaptureClient())
    return true;

  do {
    switch (task_state_) {
      case TaskState::kProcessRetryTask:
        if (session_) {
          if (!ProcessSession())
            return false;
        }
        task_state_ = TaskState::kCaptureContent;
        break;
      case TaskState::kCaptureContent:
        if (!has_content_change_)
          return true;
        session_ = std::make_unique<Session>();
        if (!CaptureContent()) {
          // Don't schedule task again in this case.
          return true;
        }
        has_content_change_ = false;
        if (session_->captured_content.empty())
          return true;

        task_state_ = TaskState::kProcessCurrentSession;
        break;
      case TaskState::kProcessCurrentSession:
        return ProcessSession();
        break;
      default:
        return true;
    }
  } while (!ShouldPause());
  return false;
}

void ContentCaptureTask::Run(TimerBase*) {
  TRACE_EVENT0("blink", "CaptureContentTask::Run");
  is_scheduled_ = false;
  bool success = RunInternal();
  if (success) {
    session_.reset();
  } else {
    ScheduleInternal(ScheduleReason::kRetryTask);
  }
}

void ContentCaptureTask::ScheduleInternal(ScheduleReason reason) {
  DCHECK(document_);
  if (is_scheduled_)
    return;

  int delay_ms = 0;
  switch (reason) {
    case ScheduleReason::kFirstContentChange:
    case ScheduleReason::kScrolling:
    case ScheduleReason::kRetryTask:
      delay_ms = kTaskShortDelayInMS;
      break;
    case ScheduleReason::kContentChange:
      delay_ms = kTaskLongDelayInMS;
      break;
  }

  if (!delay_task_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        document_->GetTaskRunner(TaskType::kInternalContentCapture);
    delay_task_ = std::make_unique<TaskRunnerTimer<ContentCaptureTask>>(
        task_runner, this, &ContentCaptureTask::Run);
  }

  delay_task_->StartOneShot(base::TimeDelta::FromMilliseconds(delay_ms),
                            FROM_HERE);
  is_scheduled_ = true;
}

void ContentCaptureTask::Schedule(ScheduleReason reason) {
  DCHECK(document_);
  has_content_change_ = true;
  ScheduleInternal(reason);
}

bool ContentCaptureTask::ShouldPause() {
  return ThreadScheduler::Current()->ShouldYieldForHighPriorityWork();
}

}  // namespace blink
