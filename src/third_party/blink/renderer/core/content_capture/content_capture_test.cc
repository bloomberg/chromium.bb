// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_content_holder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class WebContentCaptureClientTestHelper : public WebContentCaptureClient {
 public:
  ~WebContentCaptureClientTestHelper() override = default;

  void GetTaskTimingParameters(base::TimeDelta& short_delay,
                               base::TimeDelta& long_delay) const override {
    short_delay = GetTaskShortDelay();
    long_delay = GetTaskLongDelay();
  }

  base::TimeDelta GetTaskLongDelay() const {
    return base::TimeDelta::FromMilliseconds(5000);
  }

  base::TimeDelta GetTaskShortDelay() const {
    return base::TimeDelta::FromMilliseconds(500);
  }

  void DidCaptureContent(const WebVector<WebContentHolder>& data,
                         bool first_data) override {
    data_ = data;
    first_data_ = first_data;
    for (auto& d : data)
      all_text_.push_back(d.GetValue().Utf8());
  }

  void DidUpdateContent(const WebVector<WebContentHolder>& data) override {
    updated_data_ = data;
    for (auto& d : data)
      updated_text_.push_back(d.GetValue().Utf8());
  }

  void DidRemoveContent(WebVector<int64_t> data) override {
    removed_data_ = data;
  }

  bool FirstData() const { return first_data_; }

  const WebVector<WebContentHolder>& Data() const { return data_; }

  const WebVector<WebContentHolder>& UpdatedData() const {
    return updated_data_;
  }

  const Vector<std::string>& AllText() const { return all_text_; }

  const Vector<std::string>& UpdatedText() const { return updated_text_; }

  const WebVector<int64_t>& RemovedData() const { return removed_data_; }

  void ResetResults() {
    first_data_ = false;
    data_.Clear();
    updated_data_.Clear();
    removed_data_.Clear();
  }

 private:
  bool first_data_ = false;
  WebVector<WebContentHolder> data_;
  WebVector<WebContentHolder> updated_data_;
  WebVector<int64_t> removed_data_;
  Vector<std::string> all_text_;
  Vector<std::string> updated_text_;
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
      : ContentCaptureManager(local_frame_root) {
    content_capture_task_ = MakeGarbageCollected<ContentCaptureTaskTestHelper>(
        local_frame_root, GetTaskSessionForTesting(), content_capture_client);
  }

  ContentCaptureTaskTestHelper* GetContentCaptureTask() {
    return content_capture_task_;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(content_capture_task_);
    ContentCaptureManager::Trace(visitor);
  }

 protected:
  ContentCaptureTask* CreateContentCaptureTask() override {
    return content_capture_task_;
  }

 private:
  Member<ContentCaptureTaskTestHelper> content_capture_task_;
};

class ContentCaptureLocalFrameClientHelper : public EmptyLocalFrameClient {
 public:
  ContentCaptureLocalFrameClientHelper(WebContentCaptureClient& client)
      : client_(client) {}

  WebContentCaptureClient* GetWebContentCaptureClient() const override {
    return &client_;
  }

 private:
  WebContentCaptureClient& client_;
};

class ContentCaptureTest : public PageTestBase {
 public:
  ContentCaptureTest() { EnablePlatform(); }

  void SetUp() override {
    content_capture_client_ =
        std::make_unique<WebContentCaptureClientTestHelper>();
    local_frame_client_ =
        MakeGarbageCollected<ContentCaptureLocalFrameClientHelper>(
            *content_capture_client_);
    SetupPageWithClients(nullptr, local_frame_client_);
    SetHtmlInnerHTML(
        "<!DOCTYPE HTML>"
        "<p id='p1'>1</p>"
        "<p id='p2'>2</p>"
        "<p id='p3'>3</p>"
        "<p id='p4'>4</p>"
        "<p id='p5'>5</p>"
        "<p id='p6'>6</p>"
        "<p id='p7'>7</p>"
        "<p id='p8'>8</p>"
        "<div id='d1'></div>");
    platform()->SetAutoAdvanceNowToPendingTasks(false);
    // TODO(michaelbai): ContentCaptureManager should be get from LocalFrame.
    content_capture_manager_ =
        MakeGarbageCollected<ContentCaptureManagerTestHelper>(
            GetFrame(), *content_capture_client_);

    InitNodeHolders();
    // Setup captured content to ContentCaptureTask, it isn't necessary once
    // ContentCaptureManager is created by LocalFrame.
    content_capture_manager_->GetContentCaptureTask()
        ->SetCapturedContentForTesting(node_ids_);
  }

  void CreateTextNodeAndNotifyManager() {
    Document& doc = GetDocument();
    Node* node = doc.createTextNode("New Text");
    Element* element = MakeGarbageCollected<Element>(html_names::kPTag, &doc);
    element->appendChild(node);
    Element* div_element = GetElementById("d1");
    div_element->appendChild(element);
    UpdateAllLifecyclePhasesForTest();
    GetContentCaptureManager()->ScheduleTaskIfNeeded();
    created_node_id_ = DOMNodeIds::IdForNode(node);
    Vector<DOMNodeId> captured_content{created_node_id_};
    content_capture_manager_->GetContentCaptureTask()
        ->SetCapturedContentForTesting(captured_content);
  }

  ContentCaptureManagerTestHelper* GetContentCaptureManager() const {
    return content_capture_manager_;
  }

  WebContentCaptureClientTestHelper* GetWebContentCaptureClient() const {
    return content_capture_client_.get();
  }

  ContentCaptureTaskTestHelper* GetContentCaptureTask() const {
    return GetContentCaptureManager()->GetContentCaptureTask();
  }

  void RunContentCaptureTask() {
    ResetResult();
    platform()->RunForPeriod(GetWebContentCaptureClient()->GetTaskShortDelay());
  }

  void RunLongDelayContentCaptureTask() {
    ResetResult();
    platform()->RunForPeriod(GetWebContentCaptureClient()->GetTaskLongDelay());
  }

  void RemoveNode(Node* node) {
    // Remove the node.
    node->remove();
    GetContentCaptureManager()->OnLayoutTextWillBeDestroyed(*node);
  }

  void RemoveUnsentNode(const WebVector<WebContentHolder>& sent_nodes) {
    // Find a node isn't in sent_nodes
    for (auto node : nodes_) {
      bool found_in_sent = false;
      for (auto& sent : sent_nodes) {
        found_in_sent = (node->nodeValue().Utf8().c_str() == sent.GetValue());
        if (found_in_sent)
          break;
      }
      if (!found_in_sent) {
        RemoveNode(node);
        return;
      }
    }
    // Didn't find unsent nodes.
    NOTREACHED();
  }

  size_t GetExpectedFirstResultSize() { return ContentCaptureTask::kBatchSize; }

  size_t GetExpectedSecondResultSize() {
    return node_ids_.size() - GetExpectedFirstResultSize();
  }

  const Vector<DOMNodeId>& NodeIds() const { return node_ids_; }
  const Vector<Persistent<Node>> Nodes() const { return nodes_; }

 private:
  void ResetResult() {
    GetWebContentCaptureClient()->ResetResults();
  }

  // TODO(michaelbai): Remove this once integrate with LayoutText.
  void InitNodeHolders() {
    Vector<std::string> ids{"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8"};
    for (auto id : ids) {
      Node* node = GetElementById(id.c_str())->firstChild();
      CHECK(node);
      LayoutObject* layout_object = node->GetLayoutObject();
      CHECK(layout_object);
      CHECK(layout_object->IsText());
      nodes_.push_back(node);
      GetContentCaptureManager()->ScheduleTaskIfNeeded();
      node_ids_.push_back(DOMNodeIds::IdForNode(node));
    }
  }

  Vector<Persistent<Node>> nodes_;
  Vector<DOMNodeId> node_ids_;
  std::unique_ptr<WebContentCaptureClientTestHelper> content_capture_client_;
  Persistent<ContentCaptureManagerTestHelper> content_capture_manager_;
  Persistent<ContentCaptureLocalFrameClientHelper> local_frame_client_;
  DOMNodeId created_node_id_ = kInvalidDOMNodeId;
};

TEST_F(ContentCaptureTest, Basic) {
  RunContentCaptureTask();
  EXPECT_EQ(ContentCaptureTask::TaskState::kStop,
            GetContentCaptureTask()->GetTaskStateForTesting());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
}

TEST_F(ContentCaptureTest, PauseAndResume) {
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

TEST_F(ContentCaptureTest, NodeOnlySendOnce) {
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

TEST_F(ContentCaptureTest, RemoveNodeBeforeSendingOut) {
  // Capture the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Remove the node and sent the captured content out.
  RemoveNode(Nodes().at(0));
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

TEST_F(ContentCaptureTest, RemoveNodeInBetweenSendingOut) {
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

  // This relies on each node to have different value.
  RemoveUnsentNode(GetWebContentCaptureClient()->Data());
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

TEST_F(ContentCaptureTest, RemoveNodeAfterSendingOut) {
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
  RemoveNode(Nodes().at(0));
  RunLongDelayContentCaptureTask();
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(1u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_F(ContentCaptureTest, TaskHistogramReporter) {
  // This performs gc for all DocumentSession, flushes the existing
  // SentContentCount and give a clean baseline for histograms.
  // We are not sure if it always work, maybe still be the source of flaky.
  ThreadState::Current()->CollectAllGarbageForTesting();
  base::HistogramTester histograms;

  // The task stops before captures content.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kCaptureContent);
  RunContentCaptureTask();
  // Verify no histogram reported yet.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // The task stops before sends the captured content out.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  // Verify has one CaptureContentTime record.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // The task stops at kProcessRetryTask because the captured content
  // needs to be sent with 2 batch.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  // Verify has one CaptureContentTime, one SendContentTime and one
  // CaptureContentDelayTime record.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // Run task until it stops, task will not capture content, because there is no
  // content change.
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kStop);
  RunContentCaptureTask();
  // Verify has two SendContentTime records.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // Create a node and run task until it stops.
  CreateTextNodeAndNotifyManager();
  GetContentCaptureTask()->SetTaskStopState(
      ContentCaptureTask::TaskState::kStop);
  RunLongDelayContentCaptureTask();
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 3u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  GetContentCaptureTask()->ClearDocumentSessionsForTesting();
  ThreadState::Current()->CollectAllGarbageForTesting();
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 3u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 1u);
  // Verify total content has been sent.
  histograms.ExpectBucketCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 9u, 1u);
}

TEST_F(ContentCaptureTest, RescheduleTask) {
  // This test assumes test runs much faster than task's long delay which is 5s.
  Persistent<ContentCaptureTaskTestHelper> task = GetContentCaptureTask();
  task->CancelTaskForTesting();
  EXPECT_TRUE(task->GetTaskNextFireIntervalForTesting().is_zero());
  task->Schedule(ContentCaptureTask::ScheduleReason::kContentChange);
  auto begin = base::TimeTicks::Now();
  base::TimeDelta interval1 = task->GetTaskNextFireIntervalForTesting();
  task->Schedule(ContentCaptureTask::ScheduleReason::kScrolling);
  base::TimeDelta interval2 = task->GetTaskNextFireIntervalForTesting();
  auto test_running_time = base::TimeTicks::Now() - begin;
  // The interval1 will be greater than interval2 even the task wasn't
  // rescheduled, removing the test_running_time from interval1 make sure
  // task rescheduled.
  EXPECT_GT(interval1 - test_running_time, interval2);
}

TEST_F(ContentCaptureTest, NotRescheduleTask) {
  // This test assumes test runs much faster than task's long delay which is 5s.
  Persistent<ContentCaptureTaskTestHelper> task = GetContentCaptureTask();
  task->CancelTaskForTesting();
  EXPECT_TRUE(task->GetTaskNextFireIntervalForTesting().is_zero());
  task->Schedule(ContentCaptureTask::ScheduleReason::kContentChange);
  auto begin = base::TimeTicks::Now();
  base::TimeDelta interval1 = task->GetTaskNextFireIntervalForTesting();
  task->Schedule(ContentCaptureTask::ScheduleReason::kContentChange);
  base::TimeDelta interval2 = task->GetTaskNextFireIntervalForTesting();
  auto test_running_time = base::TimeTicks::Now() - begin;
  EXPECT_GE(interval1, interval2);
  EXPECT_LE(interval1 - test_running_time, interval2);
}

// TODO(michaelbai): use RenderingTest instead of PageTestBase for multiple
// frame test.
class ContentCaptureSimTest : public SimTest {
 public:
  static const char* kEditableContent;

  ContentCaptureSimTest() : client_(), child_client_() {}
  void SetUp() override {
    SimTest::SetUp();
    MainFrame().SetContentCaptureClient(&client_);
    SetupPage();
  }

  void RunContentCaptureTaskUntil(ContentCaptureTask::TaskState state) {
    Client().ResetResults();
    ChildClient().ResetResults();
    GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .GetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->RunTaskForTestingUntil(state);
  }

  WebContentCaptureClientTestHelper& Client() { return client_; }
  WebContentCaptureClientTestHelper& ChildClient() { return child_client_; }

  enum class ContentType { kAll, kMainFrame, kChildFrame };
  void SetCapturedContent(ContentType type) {
    if (type == ContentType::kMainFrame) {
      SetCapturedContent(main_frame_content_);
    } else if (type == ContentType::kChildFrame) {
      SetCapturedContent(child_frame_content_);
    } else if (type == ContentType::kAll) {
      Vector<DOMNodeId> holders(main_frame_content_);
      holders.AppendRange(child_frame_content_.begin(),
                          child_frame_content_.end());
      SetCapturedContent(holders);
    }
  }

  void AddOneNodeToMainFrame() {
    AddNodeToDocument(GetDocument(), main_frame_content_);
    main_frame_expected_text_.push_back("New Text");
  }

  void AddOneNodeToChildFrame() {
    AddNodeToDocument(*child_document_, child_frame_content_);
    child_frame_expected_text_.push_back("New Text");
  }

  void InsertMainFrameEditableContent(const std::string& content,
                                      unsigned offset) {
    InsertNodeContent(GetDocument(), "editable_id", content, offset);
  }

  void DeleteMainFrameEditableContent(unsigned offset, unsigned length) {
    DeleteNodeContent(GetDocument(), "editable_id", offset, length);
  }

  const Vector<std::string>& MainFrameExpectedText() const {
    return main_frame_expected_text_;
  }

  const Vector<std::string>& ChildFrameExpectedText() const {
    return child_frame_expected_text_;
  }

  void ReplaceMainFrameExpectedText(const std::string& old_text,
                                    const std::string& new_text) {
    std::replace(main_frame_expected_text_.begin(),
                 main_frame_expected_text_.end(), old_text, new_text);
  }

 private:
  void SetupPage() {
    SimRequest main_resource("https://example.com/test.html", "text/html");
    SimRequest frame_resource("https://example.com/frame.html", "text/html");
    LoadURL("https://example.com/test.html");
    WebView().MainFrameWidget()->Resize(WebSize(800, 6000));
    main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <body style='background: white'>
      <iframe id=frame name='frame' src=frame.html></iframe>
      <p id='p1'>Hello World1</p>
      <p id='p2'>Hello World2</p>
      <p id='p3'>Hello World3</p>
      <p id='p4'>Hello World4</p>
      <p id='p5'>Hello World5</p>
      <p id='p6'>Hello World6</p>
      <p id='p7'>Hello World7</p>
      <div id='editable_id'>editable</div>
      <svg>
      <text id="s8">Hello World8</text>
      </svg>
      <div id='d1'></div>
      )HTML");
    auto frame1 = Compositor().BeginFrame();
    frame_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <p id='c1'>Hello World11</p>
      <p id='c2'>Hello World12</p>
      <div id='d1'></div>
      )HTML");

    static_cast<WebLocalFrame*>(MainFrame().FindFrameByName("frame"))
        ->SetContentCaptureClient(&child_client_);
    auto* child_frame =
        To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
    child_document_ = child_frame->contentDocument();
    child_document_->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    Compositor().BeginFrame();
    InitMainFrameNodeHolders();
    InitChildFrameNodeHolders(*child_document_);
  }

  void InitMainFrameNodeHolders() {
    Vector<std::string> ids = {"p1", "p2", "p3", "p4",         "p5",
                               "p6", "p7", "s8", "editable_id"};
    main_frame_expected_text_ = {
        "Hello World1", "Hello World2", "Hello World3",
        "Hello World4", "Hello World5", "Hello World6",
        "Hello World7", "Hello World8", kEditableContent};
    InitNodeHolders(main_frame_content_, ids, GetDocument());
    EXPECT_EQ(9u, main_frame_content_.size());
  }

  void InitChildFrameNodeHolders(const Document& doc) {
    Vector<std::string> ids = {"c1", "c2"};
    child_frame_expected_text_ = {"Hello World11", "Hello World12"};
    InitNodeHolders(child_frame_content_, ids, doc);
    EXPECT_EQ(2u, child_frame_content_.size());
  }

  void InitNodeHolders(Vector<DOMNodeId>& buffer,
                       const Vector<std::string>& ids,
                       const Document& document) {
    for (auto id : ids) {
      LayoutText* layout_text = ToLayoutText(
          document.getElementById(id.c_str())->firstChild()->GetLayoutObject());
      EXPECT_TRUE(layout_text->HasNodeId());
      buffer.push_back(layout_text->EnsureNodeId());
    }
  }

  void AddNodeToDocument(Document& doc, Vector<DOMNodeId>& buffer) {
    Node* node = doc.createTextNode("New Text");
    auto* element = MakeGarbageCollected<Element>(html_names::kPTag, &doc);
    element->appendChild(node);
    Element* div_element = doc.getElementById("d1");
    div_element->appendChild(element);
    Compositor().BeginFrame();
    LayoutText* layout_text = ToLayoutText(node->GetLayoutObject());
    EXPECT_TRUE(layout_text->HasNodeId());
    buffer.push_front(layout_text->EnsureNodeId());
  }

  void InsertNodeContent(Document& doc,
                         const std::string& id,
                         const std::string& content,
                         unsigned offset) {
    To<Text>(doc.getElementById(id.c_str())->firstChild())
        ->insertData(offset, String(content.c_str()),
                     IGNORE_EXCEPTION_FOR_TESTING);
    Compositor().BeginFrame();
  }

  void DeleteNodeContent(Document& doc,
                         const std::string& id,
                         unsigned offset,
                         unsigned length) {
    To<Text>(doc.getElementById(id.c_str())->firstChild())
        ->deleteData(offset, length, IGNORE_EXCEPTION_FOR_TESTING);
    Compositor().BeginFrame();
  }

  void SetCapturedContent(const Vector<DOMNodeId>& captured_content) {
    GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .GetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->SetCapturedContentForTesting(captured_content);
  }

  Vector<std::string> main_frame_expected_text_;
  Vector<std::string> child_frame_expected_text_;
  Vector<DOMNodeId> main_frame_content_;
  Vector<DOMNodeId> child_frame_content_;
  WebContentCaptureClientTestHelper client_;
  WebContentCaptureClientTestHelper child_client_;
  Persistent<Document> child_document_;
};

const char* ContentCaptureSimTest::kEditableContent = "editable";

TEST_F(ContentCaptureSimTest, MultiFrame) {
  SetCapturedContent(ContentType::kAll);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_EQ(2u, ChildClient().Data().size());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
  EXPECT_THAT(ChildClient().AllText(),
              testing::UnorderedElementsAreArray(ChildFrameExpectedText()));
}

TEST_F(ContentCaptureSimTest, AddNodeToMultiFrame) {
  SetCapturedContent(ContentType::kMainFrame);
  // Stops after capturing content.
  RunContentCaptureTaskUntil(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  EXPECT_TRUE(Client().Data().empty());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());

  // Sends the first batch data.
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kProcessRetryTask);
  EXPECT_EQ(5u, Client().Data().size());
  EXPECT_TRUE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());

  // Sends the reset of data
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kProcessRetryTask);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  AddOneNodeToMainFrame();
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  // Though returns all main frame content, only new added node is unsent.
  EXPECT_EQ(1u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  AddOneNodeToChildFrame();
  SetCapturedContent(ContentType::kChildFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(3u, ChildClient().Data().size());
  EXPECT_THAT(ChildClient().AllText(),
              testing::UnorderedElementsAreArray(ChildFrameExpectedText()));
  EXPECT_TRUE(ChildClient().FirstData());
}

TEST_F(ContentCaptureSimTest, ChangeNode) {
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
  Vector<std::string> expected_text_update;
  std::string insert_text = "content ";

  // Changed content to 'content editable'.
  InsertMainFrameEditableContent(insert_text, 0);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  expected_text_update.push_back(insert_text + kEditableContent);
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));

  // Changing content multiple times before capturing.
  std::string insert_text1 = "i";
  // Changed content to 'content ieditable'.
  InsertMainFrameEditableContent(insert_text1, insert_text.size());
  std::string insert_text2 = "s ";
  // Changed content to 'content is editable'.
  InsertMainFrameEditableContent(insert_text2,
                                 insert_text.size() + insert_text1.size());

  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  expected_text_update.push_back(insert_text + insert_text1 + insert_text2 +
                                 kEditableContent);
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));
}

TEST_F(ContentCaptureSimTest, ChangeNodeBeforeCapture) {
  // Changed content to 'content editable' before capture.
  std::string insert_text = "content ";
  InsertMainFrameEditableContent(insert_text, 0);
  // Changing content multiple times before capturing.
  std::string insert_text1 = "i";
  // Changed content to 'content ieditable'.
  InsertMainFrameEditableContent(insert_text1, insert_text.size());
  std::string insert_text2 = "s ";
  // Changed content to 'content is editable'.
  InsertMainFrameEditableContent(insert_text2,
                                 insert_text.size() + insert_text1.size());

  // The changed content shall be captured as new content.
  ReplaceMainFrameExpectedText(
      kEditableContent,
      insert_text + insert_text1 + insert_text2 + kEditableContent);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_TRUE(ChildClient().UpdatedData().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
}

TEST_F(ContentCaptureSimTest, DeleteNodeContent) {
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  // Deleted 4 char, changed content to 'edit'.
  DeleteMainFrameEditableContent(4, 4);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  Vector<std::string> expected_text_update;
  expected_text_update.push_back("edit");
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));

  // Emptied content, the node shall be removed.
  DeleteMainFrameEditableContent(0, 4);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_TRUE(Client().UpdatedData().empty());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_EQ(1u, Client().RemovedData().size());
}

}  // namespace blink
