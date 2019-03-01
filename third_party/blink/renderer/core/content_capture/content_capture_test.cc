// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"

#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_content_holder.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class WebContentCaptureClientTestHelper : public WebContentCaptureClient {
 public:
  WebContentCaptureClientTestHelper(NodeHolder::Type type)
      : node_holder_type_(type) {}
  ~WebContentCaptureClientTestHelper() override = default;

  NodeHolder::Type GetNodeHolderType() const override {
    return node_holder_type_;
  }

  void DidCaptureContent(
      const std::vector<scoped_refptr<WebContentHolder>>& data,
      bool first_data) override {
    data_ = data;
    first_data_ = first_data;
  }

  void DidRemoveContent(const std::vector<int64_t>& data) override {
    removed_data_ = data;
  }

  bool FirstData() const { return first_data_; }

  const std::vector<scoped_refptr<WebContentHolder>>& Data() const {
    return data_;
  }

  const std::vector<int64_t>& RemovedData() const { return removed_data_; }

  void ResetResults() {
    first_data_ = false;
    data_.clear();
    removed_data_.clear();
  }

 private:
  bool first_data_ = false;
  std::vector<scoped_refptr<WebContentHolder>> data_;
  std::vector<int64_t> removed_data_;
  NodeHolder::Type node_holder_type_;
};

class ContentCaptureTaskTestHelper : public ContentCaptureTask {
 public:
  ContentCaptureTaskTestHelper(LocalFrame& local_frame_root,
                               TaskSession& task_session,
                               WebContentCaptureClient& content_capture_client)
      : ContentCaptureTask(local_frame_root, task_session),
        content_capture_client_(&content_capture_client) {}
  void SetTaskStopState(TaskState state) { task_stop_state_ = state; }

 protected:
  WebContentCaptureClient* GetWebContentCaptureClient(
      const Document& document) override {
    return content_capture_client_;
  }

  bool ShouldPause() override {
    return GetTaskStateForTesting() == task_stop_state_;
  }

 private:
  WebContentCaptureClient* content_capture_client_;
  TaskState task_stop_state_ = TaskState::kStop;
};

class ContentCaptureManagerTestHelper : public ContentCaptureManager {
 public:
  ContentCaptureManagerTestHelper(
      LocalFrame& local_frame_root,
      WebContentCaptureClientTestHelper& content_capture_client)
      : ContentCaptureManager(local_frame_root,
                              content_capture_client.GetNodeHolderType()) {
    content_capture_task_ = base::MakeRefCounted<ContentCaptureTaskTestHelper>(
        local_frame_root, GetTaskSessionForTesting(), content_capture_client);
  }

  scoped_refptr<ContentCaptureTaskTestHelper> GetContentCaptureTask() {
    return content_capture_task_;
  }

 protected:
  scoped_refptr<ContentCaptureTask> CreateContentCaptureTask() override {
    return content_capture_task_;
  }

 private:
  scoped_refptr<ContentCaptureTaskTestHelper> content_capture_task_;
};

class ContentCaptureTest
    : public PageTestBase,
      public ::testing::WithParamInterface<NodeHolder::Type> {
 public:
  ContentCaptureTest() { EnablePlatform(); }

  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML(
        "<!DOCTYPE HTML>"
        "<p id='p1'>1</p>"
        "<p id='p2'>2</p>"
        "<p id='p3'>3</p>"
        "<p id='p4'>4</p>"
        "<p id='p5'>5</p>"
        "<p id='p6'>6</p>"
        "<p id='p7'>7</p>"
        "<p id='p8'>8</p>");
    platform()->SetAutoAdvanceNowToPendingTasks(false);
    // TODO(michaelbai): ContentCaptureManager should be get from LocalFrame.
    content_capture_client_ =
        std::make_unique<WebContentCaptureClientTestHelper>(GetParam());
    content_capture_manager_ =
        MakeGarbageCollected<ContentCaptureManagerTestHelper>(
            GetFrame(), *content_capture_client_);

    InitNodeHolders();
    // Setup captured content to ContentCaptureTask, it isn't necessary once
    // ContentCaptureManager is created by LocalFrame.
    content_capture_manager_->GetContentCaptureTask()
        ->SetCapturedContentForTesting(node_holders_);
  }

  ContentCaptureManagerTestHelper* GetContentCaptureManager() const {
    return content_capture_manager_;
  }

  WebContentCaptureClientTestHelper* GetWebContentCaptureClient() const {
    return content_capture_client_.get();
  }

  scoped_refptr<ContentCaptureTaskTestHelper> GetContentCaptureTask() const {
    return GetContentCaptureManager()->GetContentCaptureTask();
  }

  void RunContentCaptureTask() {
    ResetResult();
    platform()->RunForPeriod(base::TimeDelta::FromMilliseconds(
        ContentCaptureTask::kTaskShortDelayInMS));
  }

  void RunLongDelayContentCaptureTask() {
    ResetResult();
    platform()->RunForPeriod(base::TimeDelta::FromMilliseconds(
        ContentCaptureTask::kTaskLongDelayInMS));
  }

  void RemoveNode(NodeHolder node_holder, Node* node) {
    // Remove the node.
    node->remove();
    GetContentCaptureManager()->OnLayoutTextWillBeDestroyed(node_holder);
  }

  size_t GetExpectedFirstResultSize() { return ContentCaptureTask::kBatchSize; }

  size_t GetExpectedSecondResultSize() {
    return node_holders_.size() - GetExpectedFirstResultSize();
  }

  const std::vector<NodeHolder>& NodeHolders() const { return node_holders_; }
  const std::vector<Node*> Nodes() const { return nodes_; }

 private:
  void ResetResult() {
    GetWebContentCaptureClient()->ResetResults();
  }

  // TODO(michaelbai): Remove this once integrate with LayoutText.
  void InitNodeHolders() {
    std::vector<std::string> ids{"p1", "p2", "p3", "p4",
                                 "p5", "p6", "p7", "p8"};
    for (auto id : ids) {
      Node* node = GetElementById(id.c_str())->firstChild();
      CHECK(node);
      LayoutObject* layout_object = node->GetLayoutObject();
      CHECK(layout_object);
      CHECK(layout_object->IsText());
      nodes_.push_back(node);
      node_holders_.push_back(GetContentCaptureManager()->GetNodeHolder(*node));
    }
  }

  std::vector<Node*> nodes_;
  std::vector<NodeHolder> node_holders_;
  std::unique_ptr<WebContentCaptureClientTestHelper> content_capture_client_;
  Persistent<ContentCaptureManagerTestHelper> content_capture_manager_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentCaptureTest,
                         testing::Values(NodeHolder::Type::kID,
                                         NodeHolder::Type::kTextHolder));

TEST_P(ContentCaptureTest, Basic) {
  RunContentCaptureTask();
  EXPECT_EQ(ContentCaptureTask::TaskState::kStop,
            GetContentCaptureTask()->GetTaskStateForTesting());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
}

TEST_P(ContentCaptureTest, PauseAndResume) {
  // The task stops before captures content.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kCaptureContent);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());

  // The task stops before sends the captured content out.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());

  // The task should be stop at kProcessRetryTask because the captured content
  // needs to be sent with 2 batch.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->FirstData());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());

  // Run task until it stops, task will not capture content, because there is no
  // content change, so we have 3 NodeHolders.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kStop);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
}

TEST_P(ContentCaptureTest, NodeOnlySendOnce) {
  // Send all nodes
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());

  GetContentCaptureManager()->OnScrollPositionChanged();
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
}

TEST_P(ContentCaptureTest, RemoveNodeBeforeSendingOut) {
  // Capture the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Remove the node and sent the captured content out.
  RemoveNode(NodeHolders().at(0), Nodes().at(0));
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // Total 7 content returned instead of 8.
  EXPECT_EQ(GetExpectedSecondResultSize() - 1,
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // No removed node because it hasn't been sent out.
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_P(ContentCaptureTest, RemoveNodeInBetweenSendingOut) {
  // Capture the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Sends first batch.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // This depends on the DocumentSession returning the unsent nodes reversely.
  // Remove the first node and sent the captured content out.
  RemoveNode(NodeHolders().at(0), Nodes().at(0));
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  // Total 7 content returned instead of 8.
  EXPECT_EQ(GetExpectedSecondResultSize() - 1,
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // No removed node because it hasn't been sent out.
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_P(ContentCaptureTest, RemoveNodeAfterSendingOut) {
  // Captures the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Sends first batch.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // Sends second batch.
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // Remove the node.
  RemoveNode(NodeHolders().at(0), Nodes().at(0));
  RunLongDelayContentCaptureTask();
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(1u, GetWebContentCaptureClient()->RemovedData().size());
}

}  // namespace blink
