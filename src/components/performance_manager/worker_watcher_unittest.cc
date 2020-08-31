// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Generates a new sequential int ID. Used for things that need a unique ID.
int GenerateNextId() {
  static int next_id = 0;
  return next_id++;
}

// Generates a unique URL for a fake worker node.
GURL GenerateWorkerUrl() {
  return GURL(base::StringPrintf("https://www.foo.org/worker_script_%d.js",
                                 GenerateNextId()));
}

// Helper function to check that |worker_node| and |client_frame_node| are
// correctly hooked up together.
bool IsWorkerClient(WorkerNodeImpl* worker_node,
                    FrameNodeImpl* client_frame_node) {
  return base::Contains(worker_node->client_frames(), client_frame_node) &&
         base::Contains(client_frame_node->child_worker_nodes(), worker_node);
}

// TestDedicatedWorkerService --------------------------------------------------

// A test TestDedicatedWorkerService that allows to simulate starting and
// stopping a dedicated worker.
class TestDedicatedWorkerService : public content::DedicatedWorkerService {
 public:
  TestDedicatedWorkerService();
  ~TestDedicatedWorkerService() override;

  // content::DedicatedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateDedicatedWorkers(Observer* observer) override;

  // Starts a new dedicated worker and returns its ID.
  content::DedicatedWorkerId StartDedicatedWorker(
      int worker_process_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  // Stops a running shared worker.
  void StopDedicatedWorker(content::DedicatedWorkerId dedicated_worker_id);

 private:
  base::ObserverList<Observer> observer_list_;

  // Generates IDs for new dedicated workers.
  content::DedicatedWorkerId::Generator dedicated_worker_id_generator_;

  // Maps each running worker to its client RenderFrameHost ID.
  base::flat_map<content::DedicatedWorkerId, content::GlobalFrameRoutingId>
      dedicated_worker_client_frame_;

  DISALLOW_COPY_AND_ASSIGN(TestDedicatedWorkerService);
};

TestDedicatedWorkerService::TestDedicatedWorkerService() = default;

TestDedicatedWorkerService::~TestDedicatedWorkerService() = default;

void TestDedicatedWorkerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TestDedicatedWorkerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TestDedicatedWorkerService::EnumerateDedicatedWorkers(Observer* observer) {
  // Not implemented.
  ADD_FAILURE();
}

content::DedicatedWorkerId TestDedicatedWorkerService::StartDedicatedWorker(
    int worker_process_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Create a new DedicatedWorkerId for the worker and add it to the map, along
  // with its client ID.
  content::DedicatedWorkerId dedicated_worker_id =
      dedicated_worker_id_generator_.GenerateNextId();

  bool inserted = dedicated_worker_client_frame_
                      .emplace(dedicated_worker_id, client_render_frame_host_id)
                      .second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerStarted(dedicated_worker_id, worker_process_id,
                             client_render_frame_host_id);
  }

  return dedicated_worker_id;
}

void TestDedicatedWorkerService::StopDedicatedWorker(
    content::DedicatedWorkerId dedicated_worker_id) {
  auto it = dedicated_worker_client_frame_.find(dedicated_worker_id);
  DCHECK(it != dedicated_worker_client_frame_.end());

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerTerminated(dedicated_worker_id, it->second);

  // Remove the worker ID from the map.
  dedicated_worker_client_frame_.erase(it);
}

// TestSharedWorkerService -----------------------------------------------------

// A test SharedWorkerService that allows to simulate a worker starting and
// stopping and adding clients to running workers.
class TestSharedWorkerService : public content::SharedWorkerService {
 public:
  TestSharedWorkerService();
  ~TestSharedWorkerService() override;

  // content::SharedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateSharedWorkers(Observer* observer) override;
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const url::Origin& constructor_origin) override;

  // Starts a new shared worker and returns its ID.
  content::SharedWorkerId StartSharedWorker(int worker_process_id);

  // Stops a running shared worker.
  void StopSharedWorker(content::SharedWorkerId shared_worker_id);

  // Adds a new frame client to an existing worker.
  void AddFrameClientToWorker(
      content::SharedWorkerId shared_worker_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  // Removes an existing frame client from a worker.
  void RemoveFrameClientFromWorker(
      content::SharedWorkerId shared_worker_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

 private:
  base::ObserverList<Observer> observer_list_;

  // Generates IDs for new shared workers.
  content::SharedWorkerId::Generator shared_worker_id_generator_;

  // Contains the set of clients for each running workers.
  base::flat_map<content::SharedWorkerId,
                 base::flat_set<content::GlobalFrameRoutingId>>
      shared_worker_client_frames_;

  DISALLOW_COPY_AND_ASSIGN(TestSharedWorkerService);
};

TestSharedWorkerService::TestSharedWorkerService() = default;

TestSharedWorkerService::~TestSharedWorkerService() = default;

void TestSharedWorkerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TestSharedWorkerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TestSharedWorkerService::EnumerateSharedWorkers(Observer* observer) {
  // Not implemented.
  ADD_FAILURE();
}

bool TestSharedWorkerService::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  // Not implemented.
  ADD_FAILURE();
  return false;
}

content::SharedWorkerId TestSharedWorkerService::StartSharedWorker(
    int worker_process_id) {
  // Create a new DedicatedWorkerId for the worker and add it to the map.
  content::SharedWorkerId shared_worker_id =
      shared_worker_id_generator_.GenerateNextId();
  GURL worker_url = GenerateWorkerUrl();

  bool inserted =
      shared_worker_client_frames_.insert({shared_worker_id, {}}).second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerStarted(shared_worker_id, worker_process_id,
                             base::UnguessableToken::Create());
  }

  return shared_worker_id;
}

void TestSharedWorkerService::StopSharedWorker(
    content::SharedWorkerId shared_worker_id) {
  auto it = shared_worker_client_frames_.find(shared_worker_id);
  DCHECK(it != shared_worker_client_frames_.end());

  // A stopping worker should have no clients.
  DCHECK(it->second.empty());

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerTerminated(shared_worker_id);

  // Remove the worker ID from the map.
  shared_worker_client_frames_.erase(it);
}

void TestSharedWorkerService::AddFrameClientToWorker(
    content::SharedWorkerId shared_worker_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Add the frame to the set of clients for this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_id);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  bool inserted = client_frames.insert(client_render_frame_host_id).second;
  DCHECK(inserted);

  // Then notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientAdded(shared_worker_id, client_render_frame_host_id);
}

void TestSharedWorkerService::RemoveFrameClientFromWorker(
    content::SharedWorkerId shared_worker_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientRemoved(shared_worker_id, client_render_frame_host_id);

  // Then remove the frame from the set of clients of this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_id);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  size_t removed = client_frames.erase(client_render_frame_host_id);
  DCHECK_EQ(removed, 1u);
}

// TestServiceWorkerContext ----------------------------------------------------

// A test ServiceWorkerContext that allows to simulate a worker starting and
// stopping and adding clients to running workers.
//
// Extends content::FakeServiceWorkerContext to avoid reimplementing all the
// unused virtual functions.
class TestServiceWorkerContext : public content::FakeServiceWorkerContext {
 public:
  TestServiceWorkerContext();
  ~TestServiceWorkerContext() override;

  TestServiceWorkerContext(const TestServiceWorkerContext&) = delete;
  TestServiceWorkerContext& operator=(const TestServiceWorkerContext&) = delete;

  // content::FakeServiceWorkerContext:
  void AddObserver(content::ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(content::ServiceWorkerContextObserver* observer) override;

  // Starts a new service worker and returns its version ID.
  int64_t StartServiceWorker(int worker_process_id);

  // Stops a service shared worker.
  void StopServiceWorker(int64_t version_id);

  // Adds a new frame client to an existing worker.
  void AddFrameClientToWorker(
      int64_t version_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  // Removes an existing frame client from a worker.
  void RemoveFrameClientFromWorker(
      int64_t version_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

 private:
  base::ObserverList<content::ServiceWorkerContextObserver>::Unchecked
      observer_list_;

  // The ID that the next service worker will be assigned.
  int64_t next_service_worker_instance_id_ = 0;

  // Contains the set of clients for each running workers.
  base::flat_map<int64_t /*version_id*/,
                 base::flat_set<content::GlobalFrameRoutingId>>
      service_worker_client_frames_;
};

TestServiceWorkerContext::TestServiceWorkerContext() = default;

TestServiceWorkerContext::~TestServiceWorkerContext() = default;

void TestServiceWorkerContext::AddObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TestServiceWorkerContext::RemoveObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int64_t TestServiceWorkerContext::StartServiceWorker(int worker_process_id) {
  // Create a new version ID and add it to the map.
  GURL worker_url = GenerateWorkerUrl();
  int64_t version_id = next_service_worker_instance_id_++;

  bool inserted = service_worker_client_frames_.insert({version_id, {}}).second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnVersionStartedRunning(version_id,
                                     {worker_url, GURL(), worker_process_id});
  }

  return version_id;
}

void TestServiceWorkerContext::StopServiceWorker(int64_t version_id) {
  auto it = service_worker_client_frames_.find(version_id);
  DCHECK(it != service_worker_client_frames_.end());

  // A stopping worker should have no clients.
  DCHECK(it->second.empty());

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);

  // Remove the worker instance from the map.
  service_worker_client_frames_.erase(it);
}

void TestServiceWorkerContext::AddFrameClientToWorker(
    int64_t version_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Add the frame to the set of clients for this worker.
  auto it = service_worker_client_frames_.find(version_id);
  DCHECK(it != service_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  bool inserted = client_frames.insert(client_render_frame_host_id).second;
  DCHECK(inserted);

  // TODO(pmonette): Notify observers when the ServiceWorkerContextObserver
  //                 interface supports it.
}

void TestServiceWorkerContext::RemoveFrameClientFromWorker(
    int64_t version_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // TODO(pmonette): Notify observers when the ServiceWorkerContextObserver
  //                 interface supports it.

  // Then remove the frame from the set of clients of this worker.
  auto it = service_worker_client_frames_.find(version_id);
  DCHECK(it != service_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  size_t removed = client_frames.erase(client_render_frame_host_id);
  DCHECK_EQ(removed, 1u);
}

// TestProcessNodeSource -------------------------------------------------------

// A test ProcessNodeSource that allows creating process nodes on demand to
// "host" frames and workers.
class TestProcessNodeSource : public ProcessNodeSource {
 public:
  TestProcessNodeSource();
  ~TestProcessNodeSource() override;

  // ProcessNodeSource:
  ProcessNodeImpl* GetProcessNode(int render_process_id) override;

  // Creates a process node and returns its generated render process ID.
  int CreateProcessNode();

 private:
  // Maps render process IDs with their associated process node.
  base::flat_map<int, std::unique_ptr<ProcessNodeImpl>> process_node_map_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessNodeSource);
};

TestProcessNodeSource::TestProcessNodeSource() = default;

TestProcessNodeSource::~TestProcessNodeSource() {
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.reserve(process_node_map_.size());
  for (auto& kv : process_node_map_) {
    std::unique_ptr<ProcessNodeImpl> process_node = std::move(kv.second);
    nodes.push_back(std::move(process_node));
  }
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
  process_node_map_.clear();
}

ProcessNodeImpl* TestProcessNodeSource::GetProcessNode(int render_process_id) {
  auto it = process_node_map_.find(render_process_id);
  DCHECK(it != process_node_map_.end());
  return it->second.get();
}

int TestProcessNodeSource::CreateProcessNode() {
  // Generate a render process ID for this process node.
  int render_process_id = GenerateNextId();

  // Create the process node and insert it into the map.
  auto process_node = PerformanceManagerImpl::CreateProcessNode(
      content::PROCESS_TYPE_RENDERER, RenderProcessHostProxy());
  bool inserted =
      process_node_map_.insert({render_process_id, std::move(process_node)})
          .second;
  DCHECK(inserted);

  return render_process_id;
}

// TestFrameNodeSource ---------------------------------------------------------

class TestFrameNodeSource : public FrameNodeSource {
 public:
  TestFrameNodeSource();
  ~TestFrameNodeSource() override;

  // FrameNodeSource:
  FrameNodeImpl* GetFrameNode(
      content::GlobalFrameRoutingId render_frame_host_id) override;
  void SubscribeToFrameNode(content::GlobalFrameRoutingId render_frame_host_id,
                            OnbeforeFrameNodeRemovedCallback
                                on_before_frame_node_removed_callback) override;
  void UnsubscribeFromFrameNode(
      content::GlobalFrameRoutingId render_frame_host_id) override;

  // Creates a frame node and returns its generated render frame host id.
  content::GlobalFrameRoutingId CreateFrameNode(int render_process_id,
                                                ProcessNodeImpl* process_node);

  // Deletes an existing frame node and notify subscribers.
  void DeleteFrameNode(content::GlobalFrameRoutingId render_frame_host_id);

 private:
  // Helper function that invokes the OnBeforeFrameNodeRemovedCallback
  // associated with |frame_node| and removes it from the map.
  void InvokeAndRemoveCallback(FrameNodeImpl* frame_node);

  // The page node that hosts all frames.
  std::unique_ptr<PageNodeImpl> page_node_;

  // Maps each frame's render frame host id with their associated frame node.
  base::flat_map<content::GlobalFrameRoutingId, std::unique_ptr<FrameNodeImpl>>
      frame_node_map_;

  // Maps each observed frame node to their callback.
  base::flat_map<FrameNodeImpl*, OnbeforeFrameNodeRemovedCallback>
      frame_node_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameNodeSource);
};

TestFrameNodeSource::TestFrameNodeSource()
    : page_node_(
          PerformanceManagerImpl::CreatePageNode(WebContentsProxy(),
                                                 "page_node_context_id",
                                                 GURL(),
                                                 false,
                                                 false,
                                                 base::TimeTicks::Now())) {}

TestFrameNodeSource::~TestFrameNodeSource() {
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.push_back(std::move(page_node_));
  nodes.reserve(frame_node_map_.size());
  for (auto& kv : frame_node_map_)
    nodes.push_back(std::move(kv.second));
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
  frame_node_map_.clear();
}

FrameNodeImpl* TestFrameNodeSource::GetFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  return it != frame_node_map_.end() ? it->second.get() : nullptr;
}

void TestFrameNodeSource::SubscribeToFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id,
    OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host_id);
  DCHECK(frame_node);

  bool inserted =
      frame_node_callbacks_
          .insert(std::make_pair(
              frame_node, std::move(on_before_frame_node_removed_callback)))
          .second;
  DCHECK(inserted);
}

void TestFrameNodeSource::UnsubscribeFromFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host_id);
  DCHECK(frame_node);

  size_t removed = frame_node_callbacks_.erase(frame_node);
  DCHECK_EQ(removed, 1u);
}

content::GlobalFrameRoutingId TestFrameNodeSource::CreateFrameNode(
    int render_process_id,
    ProcessNodeImpl* process_node) {
  int frame_id = GenerateNextId();
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     frame_id);
  auto frame_node = PerformanceManagerImpl::CreateFrameNode(
      process_node, page_node_.get(), nullptr, 0, frame_id,
      base::UnguessableToken::Null(), 0, 0);

  bool inserted =
      frame_node_map_.insert({render_frame_host_id, std::move(frame_node)})
          .second;
  DCHECK(inserted);

  return render_frame_host_id;
}

void TestFrameNodeSource::DeleteFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  DCHECK(it != frame_node_map_.end());

  FrameNodeImpl* frame_node = it->second.get();

  // Notify the subscriber then delete the node.
  InvokeAndRemoveCallback(frame_node);
  PerformanceManagerImpl::DeleteNode(std::move(it->second));

  frame_node_map_.erase(it);
}

void TestFrameNodeSource::InvokeAndRemoveCallback(FrameNodeImpl* frame_node) {
  auto it = frame_node_callbacks_.find(frame_node);
  DCHECK(it != frame_node_callbacks_.end());

  std::move(it->second).Run(frame_node);

  frame_node_callbacks_.erase(it);
}

}  // namespace

class WorkerWatcherTest : public testing::Test {
 public:
  WorkerWatcherTest();
  ~WorkerWatcherTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Wraps a |graph_callback| and ensures the task completes before returning.
  void CallOnGraphAndWait(
      PerformanceManagerImpl::GraphImplCallback graph_callback);

  // Retrieves an existing worker node.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      content::DedicatedWorkerId dedicated_worker_id);
  WorkerNodeImpl* GetSharedWorkerNode(content::SharedWorkerId shared_worker_id);
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id);

  TestDedicatedWorkerService* dedicated_worker_service() {
    return &dedicated_worker_service_;
  }

  TestSharedWorkerService* shared_worker_service() {
    return &shared_worker_service_;
  }

  TestServiceWorkerContext* service_worker_context() {
    return &service_worker_context_;
  }

  TestProcessNodeSource* process_node_source() {
    return process_node_source_.get();
  }

  TestFrameNodeSource* frame_node_source() { return frame_node_source_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  TestDedicatedWorkerService dedicated_worker_service_;
  TestSharedWorkerService shared_worker_service_;
  TestServiceWorkerContext service_worker_context_;

  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  std::unique_ptr<TestProcessNodeSource> process_node_source_;
  std::unique_ptr<TestFrameNodeSource> frame_node_source_;

  // The WorkerWatcher that's being tested.
  std::unique_ptr<WorkerWatcher> worker_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WorkerWatcherTest);
};

WorkerWatcherTest::WorkerWatcherTest() = default;

WorkerWatcherTest::~WorkerWatcherTest() = default;

void WorkerWatcherTest::SetUp() {
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());

  process_node_source_ = std::make_unique<TestProcessNodeSource>();
  frame_node_source_ = std::make_unique<TestFrameNodeSource>();

  worker_watcher_ = std::make_unique<WorkerWatcher>(
      "browser_context_id", &dedicated_worker_service_, &shared_worker_service_,
      &service_worker_context_, process_node_source_.get(),
      frame_node_source_.get());
}

void WorkerWatcherTest::TearDown() {
  // Clean up the performance manager correctly.
  worker_watcher_->TearDown();
  worker_watcher_ = nullptr;

  // Delete the TestFrameNodeSource and the TestProcessNodeSource in
  // that order since they own graph nodes.
  frame_node_source_ = nullptr;
  process_node_source_ = nullptr;
  PerformanceManagerImpl::Destroy(std::move(performance_manager_));
}

void WorkerWatcherTest::CallOnGraphAndWait(
    PerformanceManagerImpl::GraphImplCallback graph_callback) {
  base::RunLoop run_loop;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindLambdaForTesting(
          [graph_callback = std::move(graph_callback),
           quit_closure = run_loop.QuitClosure()](GraphImpl* graph) mutable {
            std::move(graph_callback).Run(graph);
            quit_closure.Run();
          }));
  run_loop.Run();
}

WorkerNodeImpl* WorkerWatcherTest::GetDedicatedWorkerNode(
    content::DedicatedWorkerId dedicated_worker_id) {
  return worker_watcher_->GetDedicatedWorkerNode(dedicated_worker_id);
}

WorkerNodeImpl* WorkerWatcherTest::GetSharedWorkerNode(
    content::SharedWorkerId shared_worker_id) {
  return worker_watcher_->GetSharedWorkerNode(shared_worker_id);
}

WorkerNodeImpl* WorkerWatcherTest::GetServiceWorkerNode(int64_t version_id) {
  return worker_watcher_->GetServiceWorkerNode(version_id);
}

// This test creates one dedicated worker.
TEST_F(WorkerWatcherTest, SimpleDedicatedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  content::DedicatedWorkerId dedicated_worker_id =
      dedicated_worker_service()->StartDedicatedWorker(render_process_id,
                                                       render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetDedicatedWorkerNode(dedicated_worker_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(),
                  WorkerNode::WorkerType::kDedicated);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  dedicated_worker_service()->StopDedicatedWorker(dedicated_worker_id);
}

// This test creates one shared worker with one client frame.
TEST_F(WorkerWatcherTest, SimpleSharedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  content::SharedWorkerId shared_worker_id =
      shared_worker_service()->StartSharedWorker(render_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id,
                                                  render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetSharedWorkerNode(shared_worker_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id,
                                                       render_frame_host_id);
  shared_worker_service()->StopSharedWorker(shared_worker_id);
}

// This test creates one service worker with one client frame.
TEST_F(WorkerWatcherTest, SimpleServiceWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  int64_t service_worker_version_id =
      service_worker_context()->StartServiceWorker(render_process_id);

  // Connect the frame to the worker.
  service_worker_context()->AddFrameClientToWorker(service_worker_version_id,
                                                   render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);
        // TODO(pmonette): Change the following to EXPECT_TRUE when the
        //                 service worker node gets hooked up correctly.
        EXPECT_FALSE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  service_worker_context()->RemoveFrameClientFromWorker(
      service_worker_version_id, render_frame_host_id);
  service_worker_context()->StopServiceWorker(service_worker_version_id);
}

TEST_F(WorkerWatcherTest, SharedWorkerCrossProcessClient) {
  // Create the frame node.
  int frame_process_id = process_node_source()->CreateProcessNode();
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          frame_process_id,
          process_node_source()->GetProcessNode(frame_process_id));

  // Create the worker in a different process.
  int worker_process_id = process_node_source()->CreateProcessNode();
  content::SharedWorkerId shared_worker_id =
      shared_worker_service()->StartSharedWorker(worker_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id,
                                                  render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_process_node =
           process_node_source()->GetProcessNode(worker_process_id),
       worker_node = GetSharedWorkerNode(shared_worker_id),
       client_process_node =
           process_node_source()->GetProcessNode(frame_process_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), worker_process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id,
                                                       render_frame_host_id);
  shared_worker_service()->StopSharedWorker(shared_worker_id);
}

TEST_F(WorkerWatcherTest, OneSharedWorkerTwoClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  content::SharedWorkerId shared_worker_id =
      shared_worker_service()->StartSharedWorker(render_process_id);

  // Create 2 client frame nodes and connect them to the worker.
  content::GlobalFrameRoutingId render_frame_host_id_1 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id,
                                                  render_frame_host_id_1);

  content::GlobalFrameRoutingId render_frame_host_id_2 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id,
                                                  render_frame_host_id_2);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node = GetSharedWorkerNode(shared_worker_id),
       client_frame_node_1 =
           frame_node_source()->GetFrameNode(render_frame_host_id_1),
       client_frame_node_2 = frame_node_source()->GetFrameNode(
           render_frame_host_id_2)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);

        // Check frame 1.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_1));

        // Check frame 2.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_2));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id,
                                                       render_frame_host_id_1);
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id,
                                                       render_frame_host_id_2);
  shared_worker_service()->StopSharedWorker(shared_worker_id);
}

TEST_F(WorkerWatcherTest, OneClientTwoSharedWorkers) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the 2 workers and connect them to the frame.
  content::SharedWorkerId shared_worker_id_1 =
      shared_worker_service()->StartSharedWorker(render_process_id);
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id_1,
                                                  render_frame_host_id);

  content::SharedWorkerId shared_worker_id_2 =
      shared_worker_service()->StartSharedWorker(render_process_id);
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id_2,
                                                  render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node_1 = GetSharedWorkerNode(shared_worker_id_1),
       worker_node_2 = GetSharedWorkerNode(shared_worker_id_2),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        // Check worker 1.
        EXPECT_TRUE(graph->NodeInGraph(worker_node_1));
        EXPECT_EQ(worker_node_1->worker_type(),
                  WorkerNode::WorkerType::kShared);
        EXPECT_TRUE(IsWorkerClient(worker_node_1, client_frame_node));

        // Check worker 2.
        EXPECT_TRUE(graph->NodeInGraph(worker_node_2));
        EXPECT_EQ(worker_node_2->worker_type(),
                  WorkerNode::WorkerType::kShared);
        EXPECT_TRUE(IsWorkerClient(worker_node_2, client_frame_node));
      }));

  // Disconnect and clean up the workers.
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id_1,
                                                       render_frame_host_id);
  shared_worker_service()->StopSharedWorker(shared_worker_id_1);

  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id_2,
                                                       render_frame_host_id);
  shared_worker_service()->StopSharedWorker(shared_worker_id_2);
}

TEST_F(WorkerWatcherTest, FrameDestroyed) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create a worker of each type.
  content::DedicatedWorkerId dedicated_worker_id =
      dedicated_worker_service()->StartDedicatedWorker(render_process_id,
                                                       render_frame_host_id);
  content::SharedWorkerId shared_worker_id =
      shared_worker_service()->StartSharedWorker(render_process_id);
  int64_t service_worker_version_id =
      service_worker_context()->StartServiceWorker(render_process_id);

  // Connect the frame to the shared worker.
  shared_worker_service()->AddFrameClientToWorker(shared_worker_id,
                                                  render_frame_host_id);
  service_worker_context()->AddFrameClientToWorker(service_worker_version_id,
                                                   render_frame_host_id);

  // Check that everything is wired up correctly.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_id),
       shared_worker_node = GetSharedWorkerNode(shared_worker_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_TRUE(IsWorkerClient(dedicated_worker_node, client_frame_node));
        EXPECT_TRUE(IsWorkerClient(shared_worker_node, client_frame_node));
        // TODO(pmonette): Change the following to EXPECT_TRUE when the
        //                 service worker node gets hooked up correctly.
        EXPECT_FALSE(IsWorkerClient(service_worker_node, client_frame_node));
      }));

  frame_node_source()->DeleteFrameNode(render_frame_host_id);

  // Check that the worker is no longer connected to the deleted frame.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_id),
       shared_worker_node = GetSharedWorkerNode(shared_worker_id),
       service_worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_TRUE(dedicated_worker_node->client_frames().empty());
        EXPECT_TRUE(shared_worker_node->client_frames().empty());
        EXPECT_TRUE(service_worker_node->client_frames().empty());
      }));

  // The watcher is still expecting a worker removed notification.
  service_worker_context()->RemoveFrameClientFromWorker(
      service_worker_version_id, render_frame_host_id);
  service_worker_context()->StopServiceWorker(service_worker_version_id);
  shared_worker_service()->RemoveFrameClientFromWorker(shared_worker_id,
                                                       render_frame_host_id);
  shared_worker_service()->StopSharedWorker(shared_worker_id);
  dedicated_worker_service()->StopDedicatedWorker(dedicated_worker_id);
}

}  // namespace performance_manager
