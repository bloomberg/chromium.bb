/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_CORE_TFRT_RUNTIME_WORK_QUEUE_INTERFACE_H_
#define TENSORFLOW_CORE_TFRT_RUNTIME_WORK_QUEUE_INTERFACE_H_

#include "tensorflow/core/platform/context.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/threadpool_interface.h"
#include "tensorflow/core/profiler/lib/connected_traceme.h"
#include "tensorflow/core/profiler/lib/traceme_encode.h"
#include "tfrt/host_context/concurrent_work_queue.h"  // from @tf_runtime
#include "tfrt/support/error_util.h"  // from @tf_runtime

namespace tensorflow {
namespace tfrt_stub {

// This is an intermediate interface in tensorflow for injecting thread pool
// implementation into TFRT. We can add savedmodel/tensorflow specific
// methods (eg. create an intra op thread pool) without changing TFRT core.
class WorkQueueInterface : public tfrt::ConcurrentWorkQueue {
 public:
  ~WorkQueueInterface() override = 0;

  // Returns per-request work queue if possible. A nullptr should be returned if
  // the implementation does not implement the per-request work queue.
  //
  // TODO(b/198671794): Remove per-request concepts from the work queue
  // interface so that the interface is more composable. Per-request logic
  // should be handled separately.
  ABSL_DEPRECATED("Create the instance directly instead.")
  virtual StatusOr<std::unique_ptr<WorkQueueInterface>> InitializeRequest(
      tfrt::RequestContextBuilder* request_context_builder,
      thread::ThreadPoolInterface** intra_op_threadpool) const {
    *intra_op_threadpool = nullptr;
    return {nullptr};
  }
};

inline WorkQueueInterface::~WorkQueueInterface() = default;

// Creates a WorkQueueInterface from a ConcurrentWorkQueue. The returned
// WorkQueueInterface simply delegates all its public methods to the specified
// ConcurrentWorkQueue.
std::unique_ptr<WorkQueueInterface> WrapDefaultWorkQueue(
    std::unique_ptr<tfrt::ConcurrentWorkQueue> work_queue);

// Creates a WorkQueueInterface from a ConcurrentWorkQueue. The returned
// WorkQueueInterface simply delegates all its public methods to the specified
// ConcurrentWorkQueue. The `intra_thread_pool` is stored and will be passed out
// when `InitializeRequest()` is called.
std::unique_ptr<WorkQueueInterface> WrapDefaultWorkQueue(
    std::unique_ptr<tfrt::ConcurrentWorkQueue> work_queue,
    thread::ThreadPoolInterface* intra_thread_pool);

// A helper function that wraps tasks with traceme events.
template <typename Callable>
tfrt::TaskFunction WrapWork(int64_t id, absl::string_view name,
                            Callable&& work) {
  tensorflow::Context context(tensorflow::ContextKind::kThread);
  return tfrt::TaskFunction([id, name = std::string(name),
                             context = std::move(context),
                             work = std::forward<Callable>(work)]() mutable {
    // From TraceMeProducer in the function that launches graph execution, eg.
    // SavedModelImpl::Run().
    tensorflow::profiler::TraceMeConsumer activity(
        [&]() {
          return tensorflow::profiler::TraceMeEncode(name, {{"id", id}});
        },
        tensorflow::profiler::ContextType::kTfrtExecutor, id,
        tensorflow::profiler::TraceMeLevel::kInfo);
    tensorflow::WithContext wc(context);
    std::forward<Callable>(work)();
  });
}

}  // namespace tfrt_stub
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_TFRT_RUNTIME_WORK_QUEUE_INTERFACE_H_
