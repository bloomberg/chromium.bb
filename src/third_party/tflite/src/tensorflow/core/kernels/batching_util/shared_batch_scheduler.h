/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_SHARED_BATCH_SCHEDULER_H_
#define TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_SHARED_BATCH_SCHEDULER_H_

#include <stddef.h>

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/clock.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "tensorflow/core/kernels/batching_util/batch_input_task.h"
#include "tensorflow/core/kernels/batching_util/batch_scheduler.h"
#include "tensorflow/core/kernels/batching_util/periodic_function.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/byte_order.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/thread_annotations.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/lib/connected_traceme.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/core/profiler/lib/traceme_encode.h"

namespace tensorflow {
namespace serving {
namespace internal {
template <typename TaskType>
class Queue;
}  // namespace internal
}  // namespace serving
}  // namespace tensorflow

namespace tensorflow {
namespace serving {

// A batch scheduler for server instances that service multiple request types
// (e.g. multiple machine-learned models, or multiple versions of a model served
// concurrently), or even multiple distinct tasks for a given request. The
// scheduler multiplexes batches of different kinds of tasks onto a fixed-size
// thread pool (each batch contains tasks of a single type), in a carefully
// controlled manner. A common configuration is to set the number of threads
// equal to the number of hardware accelerator units, in which case the
// scheduler takes care of multiplexing the task types onto the shared hardware,
// in a manner that is both fair and efficient.
//
// Semantically, SharedBatchScheduler behaves like having N instances of
// BasicBatchScheduler (see basic_batch_scheduler.h), one per task type. The
// difference is that under the covers there is a single shared thread pool,
// instead of N independent ones, with their sharing deliberately coordinated.
//
// SharedBatchScheduler does not implement the BatchScheduler API; rather, it
// presents an abstraction of "queues", where each queue corresponds to one type
// of task. Tasks submitted to a given queue are placed in their own batches,
// and cannot be mixed with other tasks. Queues can be added and deleted
// dynamically, to accommodate e.g. versions of a model being brought up and
// down over the lifetime of a server.
//
// The batch thread pool round-robins through the queues, running one batch
// from a queue and then moving to the next queue. Each queue behaves like a
// BasicBatchScheduler instance, in the sense that it has maximum batch size and
// timeout parameters, which govern when a batch is eligible to be processed.
//
// Each queue is independently configured with a maximum size (in terms of the
// maximum number of batches worth of enqueued tasks). For online serving, it is
// recommended that the queue sizes be configured such that the sum of the sizes
// of the active queues roughly equal the number of batch threads. (The idea is
// that if all threads become available at roughly the same time, there will be
// enough enqueued work for them to take on, but no more.)
//
// If queue sizes are configured in the manner suggested above, the maximum time
// a task can spend in a queue before being placed in a batch and assigned to a
// thread for processing, is the greater of:
//  - the maximum time to process one batch of tasks from any active queue
//  - the configured timeout parameter for the task's queue (which can be 0)
//
// For bulk processing jobs and throughput-oriented benchmarks, you may want to
// set the maximum queue size to a large value.
//
// TODO(b/26539183): Support queue servicing policies other than round-robin.
// E.g. let each queue specify a "share" (an int >= 1), so e.g. with queues A
// and B having shares 1 and 2 respectively, the servicing pattern is ABBABB...
//
//
// PERFORMANCE TUNING: See README.md.
//
template <typename TaskType>
class SharedBatchScheduler
    : public std::enable_shared_from_this<SharedBatchScheduler<TaskType>> {
 public:
  using BatchTaskHandleUniquePtr =
      std::unique_ptr<Batch<internal::BatchInputTaskHandle<TaskType>>>;
  using BatchTaskUniqueptr = std::unique_ptr<Batch<TaskType>>;
  using BatchUniquePtr =
      absl::variant<BatchTaskUniqueptr, BatchTaskHandleUniquePtr>;
  // TODO(b/25089730): Tune defaults based on best practices as they develop.
  struct Options {
    // The name to use for the pool of batch threads.
    string thread_pool_name = {"batch_threads"};

    // The number of threads to use to process batches.
    // Must be >= 1, and should be tuned carefully.
    int num_batch_threads = port::MaxParallelism();

    // The environment to use.
    // (Typically only overridden by test code.)
    Env* env = Env::Default();
  };
  // Ownership is shared between the caller of Create() and any queues created
  // via AddQueue().
  static Status Create(
      const Options& options,
      std::shared_ptr<SharedBatchScheduler<TaskType>>* scheduler);

  ~SharedBatchScheduler();

  // Adds a queue to which tasks may be submitted. The returned queue implements
  // the BatchScheduler API. Each queue has its own set of scheduling options,
  // and its own callback to process batches of tasks submitted to the queue.
  //
  // The returned queue's destructor blocks until all tasks submitted to it have
  // been processed.
  struct QueueOptions {
    // The size limit of an input batch to the queue.
    //
    // If `enable_large_batch_splitting` is True, 'input_batch_size_limit'
    // should be greater or equal than `max_execution_batch_size`; otherwise
    // `input_batch_size_limit` should be equal to `max_execution_batch_size`.
    size_t input_batch_size_limit = 1000;

    // If a task has been enqueued for this amount of time (in microseconds),
    // and a thread is available, the scheduler will immediately form a batch
    // from enqueued tasks and assign the batch to the thread for processing,
    // even if the batch's size is below 'input_batch_size_limit'.
    //
    // This parameter offers a way to bound queue latency, so that a task isn't
    // stuck in the queue indefinitely waiting for enough tasks to arrive to
    // make a full batch. (The latency bound is given in the class documentation
    // above.)
    //
    // The goal is to smooth out batch sizes under low request rates, and thus
    // avoid latency spikes.
    int64_t batch_timeout_micros = 0;

    // The maximum allowable number of enqueued (accepted by Schedule() but
    // not yet being processed on a batch thread) tasks in terms of batches.
    // If this limit is reached, Schedule() will return an UNAVAILABLE error.
    // See the class documentation above for guidelines on how to tune this
    // parameter.
    //
    // Must be positive, or else invalid argument error will be returned at
    // queue creation time.
    size_t max_enqueued_batches = 10;

    // If true, queue implementation would split one input batch task into
    // subtasks (as specified by `split_input_task_func` below) and fit subtasks
    // into different batches.
    //
    // For usage of `split_input_task_func`, please see its comment.
    bool enable_large_batch_splitting = false;

    // `input_task`: a unit of task to be split.
    // `first_output_task_size`: task size of first output.
    // `max_execution_batch_size`: Maximum size of each batch.
    // `output_tasks`: A list of output tasks after split.
    //
    // REQUIRED:
    // 1) All `output_tasks` should be non-empty tasks.
    // 2) Sizes of `output_tasks` add up to size of `input_task`.
    //
    // NOTE:
    // Instantiations of `TaskType` may vary, so it's up to caller to define
    // how (e.g., which members to access) to split input tasks.
    std::function<Status(std::unique_ptr<TaskType>* input_task,
                         int first_output_task_size, int input_batch_size_limit,
                         std::vector<std::unique_ptr<TaskType>>* output_tasks)>
        split_input_task_func;

    // If true, batch input tasks are split lazily after dequeue and not on the
    // critical path of enqueue operations.
    //
    // Must be false if `enable_large_batch_splitting` is false; elsewise errors
    // will be returned at queue creation time.
    //
    // TODO(b/194294263):
    // Make `enable_lazy_split` a template parameter of queue, and adapts
    // `batches_` and `task_handle_batches_` into one deque of
    // tensorflow::serving::Batch.
    bool enable_lazy_split = false;

    // The maximum size of each enqueued batch (i.e., in `batches_`).
    //
    // The scheduler may form batches of any size between 1 and this number
    // (inclusive). If there is a need to quantize the batch sizes, i.e. only
    // submit batches whose size is in a small set of allowed sizes, that can be
    // done by adding padding in the process-batch callback.
    size_t max_execution_batch_size = 1000;
  };
  Status AddQueue(const QueueOptions& options,
                  std::function<void(std::unique_ptr<Batch<TaskType>>)>
                      process_batch_callback,
                  std::unique_ptr<BatchScheduler<TaskType>>* queue);

 private:
  explicit SharedBatchScheduler(const Options& options);

  void GetNextWorkItem_Locked(internal::Queue<TaskType>** queue_for_batch_out,
                              BatchUniquePtr* batch_to_process_out)
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // The code executed in 'batch_threads_'. Obtains a batch to process from the
  // queue pointed to by 'next_queue_to_schedule_', and processes it. If that
  // queue declines to provide a batch to process, moves onto the next queue. If
  // no queues provide a batch to process, just sleeps briefly and exits.
  void ThreadLogic();

  // Called by `AddQueue`.
  Status AddQueueAfterRewritingOptions(
      const QueueOptions& options,
      std::function<void(std::unique_ptr<Batch<TaskType>>)>
          process_batch_callback,
      std::unique_ptr<BatchScheduler<TaskType>>* queue);

  static bool BatchExists(const BatchUniquePtr& batch_to_process);

  const Options options_;

  mutex mu_;

  // A list of queues. (We use std::list instead of std::vector to ensure that
  // iterators are not invalidated by adding/removing elements. It also offers
  // efficient removal of elements from the middle.)
  using QueueList = std::list<std::unique_ptr<internal::Queue<TaskType>>>;

  // All "active" queues, i.e. ones that either:
  //  - have not been removed, or
  //  - have been removed but are not yet empty.
  QueueList queues_ TF_GUARDED_BY(mu_);

  // An iterator over 'queues_', pointing to the queue from which the next
  // available batch thread should grab work.
  typename QueueList::iterator next_queue_to_schedule_ TF_GUARDED_BY(mu_);

  // Used by idle batch threads to wait for work to enter the system. Notified
  // whenever a batch becomes schedulable.
  condition_variable schedulable_batch_cv_;

  // Threads that process batches obtained from the queues.
  std::vector<std::unique_ptr<PeriodicFunction>> batch_threads_;

  TF_DISALLOW_COPY_AND_ASSIGN(SharedBatchScheduler);
};

//////////
// Implementation details follow. API users need not read.

namespace internal {

// A task queue for SharedBatchScheduler. Accepts tasks and accumulates them
// into batches, and dispenses those batches to be processed via a "pull"
// interface. The queue's behavior is governed by maximum batch size, timeout
// and maximum queue length parameters; see their documentation in
// SharedBatchScheduler.
//
// The queue is implemented as a deque of batches, with these invariants:
//  - The number of batches is between 1 and 'options_.max_enqueued_batches'.
//  - The back-most batch is open; the rest are closed.
//
// Submitted tasks are added to the open batch. If that batch doesn't have room
// but the queue isn't full, then that batch is closed and a new open batch is
// started.
//
// Batch pull requests are handled by dequeuing the front-most batch if it is
// closed. If the front-most batch is open (i.e. the queue contains only one
// batch) and has reached the timeout, it is immediately closed and returned;
// otherwise no batch is returned for the request.
template <typename TaskType>
class Queue {
 public:
  using ProcessBatchCallback =
      std::function<void(std::unique_ptr<Batch<TaskType>>)>;
  using SchedulableBatchCallback = std::function<void()>;
  using SplitInputTaskIntoSubtasksCallback = std::function<Status(
      std::unique_ptr<TaskType>* input_task, int open_batch_remaining_slot,
      int max_execution_batch_size,
      std::vector<std::unique_ptr<TaskType>>* output_tasks)>;
  Queue(const typename SharedBatchScheduler<TaskType>::QueueOptions& options,
        Env* env, ProcessBatchCallback process_batch_callback,
        SchedulableBatchCallback schedulable_batch_callback);

  // Illegal to destruct unless the queue is empty.
  ~Queue();

  // Submits a task to the queue, with the same semantics as
  // BatchScheduler::Schedule().
  Status Schedule(std::unique_ptr<TaskType>* task);

  // Enqueue `task` as it is OR split it inline (eagerly) to form batches to be
  // processed by `Queue<TaskType>::ProcessBatch`
  Status ScheduleWithoutOrEagerSplit(std::unique_ptr<TaskType>* task);

  // Enqueue `task` along with the batch queue metadata.
  // Batches are formed by the time `ScheduleWithLazySplit` returns; and each
  // batch in the deque could evaluate to a batch to be processed after it's
  // dequeued (out of mutex-protected area).
  Status ScheduleWithLazySplit(std::unique_ptr<TaskType>* task);

  // Returns the number of enqueued tasks, with the same semantics as
  // BatchScheduler::NumEnqueuedTasks().
  size_t NumEnqueuedTasks() const;

  // Returns the queue capacity, with the same semantics as
  // BatchScheduler::SchedulingCapacity().
  size_t SchedulingCapacity() const;

  // Returns the maximum allowed size of tasks submitted to the queue.
  size_t max_task_size() const { return options_.input_batch_size_limit; }

  // Returns the maximum allowed size of tasks to be executed.
  // Returned value would be less than or equal to the maximum allowed input
  // size that's provided by caller of batch scheduler.
  size_t max_execution_batch_size() const { return max_execution_batch_size_; }

  // Called by a thread that is ready to process a batch, to request one from
  // this queue. Either returns a batch that is ready to be processed, or
  // nullptr if the queue declines to schedule a batch at this time. If it
  // returns a batch, the batch is guaranteed to be closed.
  typename SharedBatchScheduler<TaskType>::BatchUniquePtr ScheduleBatch();

  // A variant of `ScheduleBatch`.
  // Batches are guaranteed to form at task enqueue time.
  std::unique_ptr<Batch<TaskType>> ScheduleBatchWithEagerSplit();

  // Processes a batch that has been returned earlier by ScheduleBatch().
  void ProcessBatch(std::unique_ptr<Batch<TaskType>> batch);

  // Determines whether the queue is empty, i.e. has no tasks waiting or being
  // processed.
  bool IsEmpty() const;

  // Marks the queue closed, and waits until it is empty.
  void CloseAndWaitUntilEmpty();

  bool closed() const TF_NO_THREAD_SAFETY_ANALYSIS { return closed_.load(); }

 private:
  // Computes the max_execution_batch_size of the queue based on queue options.
  static size_t GetMaxExecutionBatchSize(
      const typename SharedBatchScheduler<TaskType>::QueueOptions& options) {
    // If `enable_large_batch_splitting`, returns `max_execution_batch_size`
    // configured by user options directly; returns `input_batch_size_limit`
    // otherwise.
    //
    // Note `input_batch_size_limit` is used for backward compatibitliy ->
    // users may not specify `max_execution_batch_size` explicitly.
    if (options.enable_large_batch_splitting) {
      return options.max_execution_batch_size;
    } else {
      return options.input_batch_size_limit;
    }
  }

  // Same as IsEmpty(), but assumes the caller already holds a lock on 'mu_'.
  bool IsEmptyInternal() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Closes the open batch residing at the back of std::deque, and inserts a
  // fresh open batch behind it.
  void StartNewBatch() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Split `input task` into `output_tasks` according to 'task_sizes'.
  Status SplitInputBatchIntoSubtasks(
      std::unique_ptr<TaskType>* input_task,
      std::vector<std::unique_ptr<TaskType>>* output_tasks)
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Determines whether the open batch residing at the back of 'batches_' is
  // currently schedulable.
  bool IsOpenBatchSchedulable() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // A variant of `IsOpenBatchSchedulable`; used when batches are formed at
  // task enqueue time, and open batch is `batches_.back()`.
  bool IsOpenBatchSchedulableAfterEagerSplit() const
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Same as SchedulingCapacity(), but assumes the caller already holds a
  // lock on 'mu_'.
  size_t SchedulingCapacityInternal() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Returns true if queue doesn't have capacity for this task.
  //
  // `task` must outlive this method.
  bool BatchTaskExceedQueueCapacity(TaskType* task) const
      TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // The task size of the last batch in the queue.
  size_t tail_batch_task_size() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Returns the number of enqueued batches.
  int64 num_enqueued_batches() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const typename SharedBatchScheduler<TaskType>::QueueOptions options_;

  // The environment to use.
  Env* env_;

  // The maximum batch size to be executed by `Queue::ProcessBatch`.
  // See the comment of QueueOptions and helper function
  // `GetMaxExecutionBatchSize` for more details on what it means.
  const size_t max_execution_batch_size_;

  // A callback invoked to processes a batch of work units. Always invoked
  // from a batch thread.
  ProcessBatchCallback process_batch_callback_;

  // A callback invoked to notify the scheduler that a new batch has become
  // schedulable.
  SchedulableBatchCallback schedulable_batch_callback_;

  mutable mutex mu_;

  // Whether this queue can accept new tasks. This variable is monotonic: it
  // starts as false, and then at some point gets set to true and remains true
  // for the duration of this object's life.
  std::atomic<bool> closed_ TF_GUARDED_BY(mu_){false};

  // The enqueued batches.
  // Each element corresponds to a task to be dequeued and processed by
  // `Queue<TaskType>::ProcessBatch`.
  //
  // Used iff `QueueOptions.enable_lazy_split` is false.
  std::deque<std::unique_ptr<Batch<TaskType>>> batches_ TF_GUARDED_BY(mu_);

  // The enqueued batches.
  //
  // Each element corresponds to the `task` enqueued in `Queue::Schedule`; the
  // element could be split and processed in batches at dequeue time.
  //
  // Used iff `QueueOptions.enable_lazy_split` is true.
  std::deque<std::unique_ptr<Batch<BatchInputTaskHandle<TaskType>>>>
      task_handle_batches_ TF_GUARDED_BY(mu_);

  // The counter of the TraceMe context ids.
  uint64 traceme_context_id_counter_ TF_GUARDED_BY(mu_) = 0;

  // The time at which the first task was added to the open (back-most) batch
  // in 'batches_'. Valid iff that batch contains at least one task.
  uint64 open_batch_start_time_micros_ TF_GUARDED_BY(mu_);

  // Whether this queue contains a batch that is eligible to be scheduled.
  // Used to keep track of when to call 'schedulable_batch_callback_'.
  bool schedulable_batch_ TF_GUARDED_BY(mu_) = false;

  // The number of batches currently being processed by batch threads.
  // Incremented in ScheduleBatch() and decremented in ProcessBatch().
  int num_batches_being_processed_ TF_GUARDED_BY(mu_) = 0;

  // Used by CloseAndWaitUntilEmpty() to wait until the queue is empty, for
  // the case in which the queue is not empty when CloseAndWaitUntilEmpty()
  // starts. When ProcessBatch() dequeues the last batch and makes the queue
  // empty, if 'empty_notification_' is non-null it calls
  // 'empty_notification_->Notify()'.
  Notification* empty_notification_ TF_GUARDED_BY(mu_) = nullptr;

  TF_DISALLOW_COPY_AND_ASSIGN(Queue);
};

// A RAII-style object that points to a Queue and implements
// the BatchScheduler API. To be handed out to clients who call AddQueue().
template <typename TaskType>
class QueueHandle : public BatchScheduler<TaskType> {
 public:
  QueueHandle(std::shared_ptr<SharedBatchScheduler<TaskType>> scheduler,
              Queue<TaskType>* queue);
  ~QueueHandle() override;

  Status Schedule(std::unique_ptr<TaskType>* task) override;
  size_t NumEnqueuedTasks() const override;
  size_t SchedulingCapacity() const override;

  size_t max_task_size() const override { return queue_->max_task_size(); }

 private:
  // The scheduler that owns 'queue_'.
  std::shared_ptr<SharedBatchScheduler<TaskType>> scheduler_;

  // The queue this handle wraps. Owned by 'scheduler_', which keeps it alive at
  // least until this class's destructor closes it.
  Queue<TaskType>* queue_;

  TF_DISALLOW_COPY_AND_ASSIGN(QueueHandle);
};

}  // namespace internal

template <typename TaskType>
Status SharedBatchScheduler<TaskType>::Create(
    const Options& options,
    std::shared_ptr<SharedBatchScheduler<TaskType>>* scheduler) {
  if (options.num_batch_threads < 1) {
    return errors::InvalidArgument("num_batch_threads must be positive; was ",
                                   options.num_batch_threads);
  }
  scheduler->reset(new SharedBatchScheduler<TaskType>(options));
  return Status::OK();
}

template <typename TaskType>
SharedBatchScheduler<TaskType>::~SharedBatchScheduler() {
  // Wait until the batch threads finish clearing out and deleting the closed
  // queues.
  for (;;) {
    {
      mutex_lock l(mu_);
      if (queues_.empty()) {
        break;
      }
    }
    const int64_t kSleepTimeMicros = 100;
    options_.env->SleepForMicroseconds(kSleepTimeMicros);
  }
  // Delete the batch threads before allowing state the threads may access (e.g.
  // 'mu_') to be deleted.
  batch_threads_.clear();
}

template <typename TaskType>
Status SharedBatchScheduler<TaskType>::AddQueue(
    const QueueOptions& options,
    std::function<void(std::unique_ptr<Batch<TaskType>>)>
        process_batch_callback,
    std::unique_ptr<BatchScheduler<TaskType>>* queue) {
  QueueOptions rewrite_options = options;
  if ((!rewrite_options.enable_large_batch_splitting) &&
      rewrite_options.max_enqueued_batches == 0) {
    // Many existing models (with very low QPS) rely on this option to be >0.
    // Rewrite and set this to one and retain old behavior to allow such models
    // to continue to work.
    //
    // Note, technically an invalid-argument error should be returned, but
    // that may break such models.
    rewrite_options.max_enqueued_batches = 1;
  }
  return AddQueueAfterRewritingOptions(rewrite_options, process_batch_callback,
                                       queue);
}

template <typename TaskType>
Status SharedBatchScheduler<TaskType>::AddQueueAfterRewritingOptions(
    const QueueOptions& options,
    std::function<void(std::unique_ptr<Batch<TaskType>>)>
        process_batch_callback,
    std::unique_ptr<BatchScheduler<TaskType>>* queue) {
  if (options.input_batch_size_limit == 0) {
    return errors::InvalidArgument(
        "input_batch_size_limit must be positive; was ",
        options.input_batch_size_limit);
  }
  if (options.batch_timeout_micros < 0) {
    return errors::InvalidArgument(
        "batch_timeout_micros must be non-negative; was ",
        options.batch_timeout_micros);
  }
  if (options.max_enqueued_batches == 0) {
    return errors::InvalidArgument(
        "max_enqueued_batches must be positive; was ",
        options.max_enqueued_batches);
  }

  if (options.enable_large_batch_splitting &&
      options.split_input_task_func == nullptr) {
    return errors::InvalidArgument(
        "split_input_task_func must be specified when split_input_task is "
        "true: ",
        options.enable_large_batch_splitting);
  }

  if (options.enable_lazy_split && (!options.enable_large_batch_splitting)) {
    return errors::InvalidArgument(
        "enable_lazy_split should be enabled only if "
        "enable_large_batch_splitting is enabled.");
  }

  if (options.enable_large_batch_splitting &&
      (options.input_batch_size_limit < options.max_execution_batch_size)) {
    return errors::InvalidArgument(
        "When enable_large_batch_splitting is true, input_batch_size_limit "
        "must be "
        "greater than or equal to max_execution_batch_size.",
        options.enable_large_batch_splitting, options.input_batch_size_limit,
        options.max_execution_batch_size);
  }

  auto schedulable_batch_callback = [this] {
    mutex_lock l(mu_);
    schedulable_batch_cv_.notify_one();
  };
  auto internal_queue =
      std::unique_ptr<internal::Queue<TaskType>>(new internal::Queue<TaskType>(
          options, options_.env, process_batch_callback,
          schedulable_batch_callback));
  auto handle = std::unique_ptr<BatchScheduler<TaskType>>(
      new internal::QueueHandle<TaskType>(this->shared_from_this(),
                                          internal_queue.get()));
  {
    mutex_lock l(mu_);
    queues_.push_back(std::move(internal_queue));
    if (next_queue_to_schedule_ == queues_.end()) {
      next_queue_to_schedule_ = queues_.begin();
    }
  }
  *queue = std::move(handle);
  return Status::OK();
}

template <typename TaskType>
SharedBatchScheduler<TaskType>::SharedBatchScheduler(const Options& options)
    : options_(options), next_queue_to_schedule_(queues_.end()) {
  // Kick off the batch threads.
  PeriodicFunction::Options periodic_fn_options;
  periodic_fn_options.thread_name_prefix =
      strings::StrCat(options.thread_pool_name, "_");
  for (int i = 0; i < options.num_batch_threads; ++i) {
    std::unique_ptr<PeriodicFunction> thread(new PeriodicFunction(
        [this] { this->ThreadLogic(); },
        0 /* function invocation interval time */, periodic_fn_options));
    batch_threads_.push_back(std::move(thread));
  }
}

template <typename TaskType>
bool SharedBatchScheduler<TaskType>::BatchExists(
    const BatchUniquePtr& batch_to_process) {
  if (absl::holds_alternative<BatchTaskUniqueptr>(batch_to_process)) {
    return absl::get<BatchTaskUniqueptr>(batch_to_process) == nullptr;
  }
  return absl::get<BatchTaskHandleUniquePtr>(batch_to_process) == nullptr;
}

template <typename TaskType>
void SharedBatchScheduler<TaskType>::GetNextWorkItem_Locked(
    internal::Queue<TaskType>** queue_for_batch_out,
    BatchUniquePtr* batch_to_process_out) {
  BatchUniquePtr batch_to_process;
  internal::Queue<TaskType>* queue_for_batch = nullptr;
  const int num_queues = queues_.size();
  for (int num_queues_tried = 0;
       (BatchExists(batch_to_process)) && num_queues_tried < num_queues;
       ++num_queues_tried) {
    DCHECK(next_queue_to_schedule_ != queues_.end());

    // If a closed queue responds to ScheduleBatch() with nullptr, the queue
    // will never yield any further batches so we can drop it. To avoid a
    // race, we take a snapshot of the queue's closedness state *before*
    // calling ScheduleBatch().
    const bool queue_closed = (*next_queue_to_schedule_)->closed();

    // Ask '*next_queue_to_schedule_' if it wants us to process a batch.
    batch_to_process = (*next_queue_to_schedule_)->ScheduleBatch();

    if (!BatchExists(batch_to_process)) {
      queue_for_batch = next_queue_to_schedule_->get();
    }

    // Advance 'next_queue_to_schedule_'.
    if (queue_closed && (*next_queue_to_schedule_)->IsEmpty() &&
        (BatchExists(batch_to_process))) {
      // We've encountered a closed queue with no work to do. Drop it.
      DCHECK_NE(queue_for_batch, next_queue_to_schedule_->get());
      next_queue_to_schedule_ = queues_.erase(next_queue_to_schedule_);
    } else {
      ++next_queue_to_schedule_;
    }
    if (next_queue_to_schedule_ == queues_.end() && !queues_.empty()) {
      // We've hit the end. Wrap to the first queue.
      next_queue_to_schedule_ = queues_.begin();
    }
  }
  *queue_for_batch_out = queue_for_batch;
  *batch_to_process_out = std::move(batch_to_process);
}

template <typename TaskType>
void SharedBatchScheduler<TaskType>::ThreadLogic() {
  // A batch to process next (or nullptr if no work to do).
  BatchUniquePtr batch_to_process;
  // The queue with which 'batch_to_process' is associated.
  internal::Queue<TaskType>* queue_for_batch = nullptr;
  {
    mutex_lock l(mu_);
    while (true) {
      GetNextWorkItem_Locked(&queue_for_batch, &batch_to_process);
      if (!BatchExists(batch_to_process)) {
        break;
      }
      if (queues_.empty()) return;
      // We couldn't find any work to do. Wait until a new batch becomes
      // schedulable, or some time has elapsed, before checking again.
      const int64_t kTimeoutMillis =
          1;  // The smallest accepted granule of time.
      WaitForMilliseconds(&l, &schedulable_batch_cv_, kTimeoutMillis);
    }
  }

  std::unique_ptr<Batch<TaskType>> batch_to_schedule;
  if (absl::holds_alternative<BatchTaskHandleUniquePtr>(batch_to_process)) {
    // The corresponding `queue_for_batch` must be created with
    // `enable_lazy_split=true`.
    BatchTaskHandleUniquePtr ptr =
        std::move(absl::get<BatchTaskHandleUniquePtr>(batch_to_process));
    batch_to_schedule = std::make_unique<Batch<TaskType>>();
    std::vector<std::unique_ptr<internal::BatchInputTaskHandle<TaskType>>>
        task_handles = ptr->RemoveAllTasks();

    // TODO(b/194294263):
    // Handle the batch-kernel callback properly when lazy split returns
    // error.
    for (int i = 0; i < task_handles.size(); i++) {
      batch_to_schedule->AddTask(std::move(task_handles[i]->GetSplitTask()));
    }
    batch_to_schedule->Close();

  } else {
    // The corresponding `queue_for_batch` must be created with
    // `enable_lazy_split=false`.
    batch_to_schedule =
        std::move(absl::get<BatchTaskUniqueptr>(batch_to_process));
  }

  queue_for_batch->ProcessBatch(std::move(batch_to_schedule));
}

namespace internal {

template <typename TaskType>
Queue<TaskType>::Queue(
    const typename SharedBatchScheduler<TaskType>::QueueOptions& options,
    Env* env, ProcessBatchCallback process_batch_callback,
    SchedulableBatchCallback schedulable_batch_callback)
    : options_(options),
      env_(env),
      max_execution_batch_size_(GetMaxExecutionBatchSize(options_)),
      process_batch_callback_(process_batch_callback),
      schedulable_batch_callback_(schedulable_batch_callback) {
  // Set the higher 32 bits of traceme_context_id_counter_ to be the creation
  // time of the queue. This prevents the batches in different queues to have
  // the same traceme_context_id_counter_.
  traceme_context_id_counter_ = absl::GetCurrentTimeNanos() << 32;
  // Create an initial, open batch.
  if (options_.enable_lazy_split) {
    task_handle_batches_.emplace_back(
        new Batch<BatchInputTaskHandle<TaskType>>);
  } else {
    batches_.emplace_back(new Batch<TaskType>);
  }
}

template <typename TaskType>
Queue<TaskType>::~Queue() {
  mutex_lock l(mu_);
  DCHECK(IsEmptyInternal());

  // Close the (empty) open batch, so its destructor doesn't block.
  if (options_.enable_lazy_split) {
    task_handle_batches_.back()->Close();
  } else {
    batches_.back()->Close();
  }
}

template <typename TaskType>
Status Queue<TaskType>::Schedule(std::unique_ptr<TaskType>* task) {
  if ((*task)->size() > options_.input_batch_size_limit) {
    return errors::InvalidArgument("Task size ", (*task)->size(),
                                   " is larger than maximum input batch size ",
                                   options_.input_batch_size_limit);
  }
  if (options_.enable_lazy_split) {
    return ScheduleWithLazySplit(std::move(task));
  }
  return ScheduleWithoutOrEagerSplit(std::move(task));
}

template <typename TaskType>
Status Queue<TaskType>::ScheduleWithLazySplit(std::unique_ptr<TaskType>* task) {
  profiler::TraceMe trace_me([task] {
    return profiler::TraceMeEncode(
        "ScheduleWithLazySplit",
        {{"batching_input_task_size", (*task)->size()}});
  });
  // The max size to be enqueued.
  const int max_execution_batch_size = options_.max_execution_batch_size;

  bool notify_of_schedulable_batch = false;
  {
    mutex_lock l(mu_);

    DCHECK(!closed_);

    if (BatchTaskExceedQueueCapacity((*task).get())) {
      return errors::Unavailable(
          "The batch scheduling queue to which this task was submitted is "
          "full");
    }
    const int64 open_batch_capacity =
        max_execution_batch_size - this->tail_batch_task_size();

    auto input_batch = std::make_shared<BatchInputTask<TaskType>>(
        std::move(*task), open_batch_capacity, max_execution_batch_size,
        options_.split_input_task_func);
    std::vector<std::unique_ptr<BatchInputTaskHandle<TaskType>>> task_handles;

    input_batch->ToTaskHandles(&task_handles);

    for (int i = 0; i < task_handles.size(); ++i) {
      if (task_handle_batches_.back()->size() + task_handles[i]->size() >
          options_.max_execution_batch_size) {
        StartNewBatch();
      }
      if (task_handle_batches_.back()->empty()) {
        open_batch_start_time_micros_ = env_->NowMicros();
      }
      profiler::TraceMeProducer trace_me(
          [&task_handles, i] {
            return profiler::TraceMeEncode("ScheduleOutputTask",
                                           {{"size", task_handles[i]->size()}});
          },
          profiler::ContextType::kSharedBatchScheduler,
          task_handle_batches_.back()->traceme_context_id());

      task_handle_batches_.back()->AddTask(std::move(task_handles[i]));
    }

    if (!schedulable_batch_) {
      if (batches_.size() > 1 || IsOpenBatchSchedulable()) {
        schedulable_batch_ = true;
        notify_of_schedulable_batch = true;
      }
    }
  }
  // TODO(b/194294263):
  // Add unit tests to verify that `schedulable_batch_callback_` could be
  // triggered when batches are scheduled.
  if (notify_of_schedulable_batch) {
    schedulable_batch_callback_();
  }

  return Status::OK();
}

// TODO(b/194294263):
// Merge `ScheduleWithoutOrEagerSplit` and `ScheduleWithLazySplit` into
// `Schedule`.
template <typename TaskType>
Status Queue<TaskType>::ScheduleWithoutOrEagerSplit(
    std::unique_ptr<TaskType>* task) {
  const bool large_batch_splitting = options_.enable_large_batch_splitting;
  profiler::TraceMe trace_me([task, large_batch_splitting] {
    return profiler::TraceMeEncode(
        large_batch_splitting ? "ScheduleWithEagerSplit"
                              : "ScheduleWithoutSplit",
        {{"batching_input_task_size", (*task)->size()}});
  });

  bool notify_of_schedulable_batch = false;
  {
    mutex_lock l(mu_);

    DCHECK(!closed_);

    // TODO(b/161857471):
    // Add test coverage when when concurrent incoming batches arrives and
    // use up all queue capacity.
    if (BatchTaskExceedQueueCapacity((*task).get())) {
      return errors::Unavailable(
          "The batch scheduling queue to which this task was submitted is "
          "full");
    }

    const int64_t open_batch_remaining_slot =
        max_execution_batch_size() - batches_.back()->size();

    const int64_t input_task_size = (*task)->size();

    std::vector<std::unique_ptr<TaskType>> output_tasks;

    if (input_task_size <= open_batch_remaining_slot ||
        !large_batch_splitting) {
      // This is the fast path when input doesn't need to be split.
      output_tasks.push_back(std::move(*task));
    } else {
      TF_RETURN_IF_ERROR(SplitInputBatchIntoSubtasks(task, &output_tasks));
    }

    for (int i = 0; i < output_tasks.size(); ++i) {
      if (batches_.back()->size() + output_tasks[i]->size() >
          max_execution_batch_size()) {
        StartNewBatch();
      }
      if (batches_.back()->empty()) {
        open_batch_start_time_micros_ = env_->NowMicros();
      }
      profiler::TraceMeProducer trace_me(
          [&output_tasks, i] {
            return profiler::TraceMeEncode("ScheduleOutputTask",
                                           {{"size", output_tasks[i]->size()}});
          },
          profiler::ContextType::kSharedBatchScheduler,
          batches_.back()->traceme_context_id());
      batches_.back()->AddTask(std::move(output_tasks[i]));
    }

    if (!schedulable_batch_) {
      if (batches_.size() > 1 || IsOpenBatchSchedulable()) {
        schedulable_batch_ = true;
        notify_of_schedulable_batch = true;
      }
    }
  }

  if (notify_of_schedulable_batch) {
    schedulable_batch_callback_();
  }

  return Status::OK();
}

template <typename TaskType>
size_t Queue<TaskType>::NumEnqueuedTasks() const {
  size_t num_enqueued_tasks = 0;
  mutex_lock l(mu_);
  if (options_.enable_lazy_split) {
    for (const auto& batch : task_handle_batches_) {
      num_enqueued_tasks += batch->num_tasks();
    }
    return num_enqueued_tasks;
  }

  for (const auto& batch : batches_) {
    num_enqueued_tasks += batch->num_tasks();
  }
  return num_enqueued_tasks;
}

template <typename TaskType>
size_t Queue<TaskType>::SchedulingCapacity() const {
  mutex_lock l(mu_);
  return SchedulingCapacityInternal();
}

template <typename TaskType>
size_t Queue<TaskType>::SchedulingCapacityInternal() const {
  const int64 num_new_batches_schedulable =
      static_cast<int64>(options_.max_enqueued_batches) -
      this->num_enqueued_batches();
  const int64 execution_batch_size_limit = max_execution_batch_size();
  const int64 open_batch_capacity =
      execution_batch_size_limit - this->tail_batch_task_size();
  // Note the returned value is guaranteed to be not negative, since
  // enqueue operation could only happen if queue has enough capacity.
  return (num_new_batches_schedulable * execution_batch_size_limit) +
         open_batch_capacity;
}

template <typename TaskType>
bool Queue<TaskType>::BatchTaskExceedQueueCapacity(TaskType* task) const {
  // Queue creation requires that `enable_large_batch_splitting` is true
  // when `enable_lazy_split` is true, so this covers both eager split and
  // lazy split.
  if (options_.enable_large_batch_splitting) {
    return task->size() > SchedulingCapacityInternal();
  }

  // NOTE, the capacity checking below is loose and is retained
  // for backward compatibility that was broken due to the merge of no-split
  // and eager split.
  // There are existing clients/models that rely on the loose check
  // and can get errors after the merge. Retaining the old behavior
  // allows such models to continue to work.
  //
  // We need to revisit/remove this check after we fix model configs.
  bool batch_task_exceed_queue_capacity = false;
  if (batches_.back()->size() + task->size() >
      options_.input_batch_size_limit) {
    if (batches_.size() >= options_.max_enqueued_batches) {
      batch_task_exceed_queue_capacity = true;
    }
  }
  return batch_task_exceed_queue_capacity;
}

template <typename TaskType>
std::unique_ptr<Batch<TaskType>>
Queue<TaskType>::ScheduleBatchWithEagerSplit() {
  // The batch to schedule, which we may populate below. (If left as nullptr,
  // that means we are electing not to schedule a batch at this time.)
  std::unique_ptr<Batch<TaskType>> batch_to_schedule;

  {
    mutex_lock l(mu_);

    // Consider closing the open batch at this time, to schedule it.
    if (batches_.size() == 1 && IsOpenBatchSchedulable()) {
      StartNewBatch();
    }

    if (batches_.size() >= 2) {
      // There is at least one closed batch that is ready to be scheduled.
      ++num_batches_being_processed_;
      batch_to_schedule = std::move(batches_.front());
      batches_.pop_front();
    } else {
      schedulable_batch_ = false;
    }
  }

  return batch_to_schedule;
}

template <typename TaskType>
typename SharedBatchScheduler<TaskType>::BatchUniquePtr
Queue<TaskType>::ScheduleBatch() {
  if (!options_.enable_lazy_split) {
    return ScheduleBatchWithEagerSplit();
  }
  // The batch to schedule, which we may populate below. (If left as nullptr,
  // that means we are electing not to schedule a batch at this time.)
  std::unique_ptr<Batch<BatchInputTaskHandle<TaskType>>>
      task_handles_to_schedule;

  {
    mutex_lock l(mu_);

    // Consider closing the open batch at this time, to schedule it.
    if (task_handle_batches_.size() == 1 && IsOpenBatchSchedulable()) {
      StartNewBatch();
    }

    if (task_handle_batches_.size() >= 2) {
      // There is at least one closed batch that is ready to be scheduled.
      ++num_batches_being_processed_;
      task_handles_to_schedule = std::move(task_handle_batches_.front());
      task_handle_batches_.pop_front();
    } else {
      schedulable_batch_ = false;
    }
  }

  return std::move(task_handles_to_schedule);
}

template <typename TaskType>
void Queue<TaskType>::ProcessBatch(std::unique_ptr<Batch<TaskType>> batch) {
  profiler::TraceMeConsumer trace_me(
      [&] {
        return profiler::TraceMeEncode(
            "ProcessBatch", {{"batch_size_before_padding", batch->size()},
                             {"_r", 2} /*root_event*/});
      },
      profiler::ContextType::kSharedBatchScheduler,
      batch->traceme_context_id());
  process_batch_callback_(std::move(batch));

  {
    mutex_lock l(mu_);
    --num_batches_being_processed_;
    if (empty_notification_ != nullptr && IsEmptyInternal()) {
      empty_notification_->Notify();
    }
  }
}

template <typename TaskType>
bool Queue<TaskType>::IsEmpty() const {
  mutex_lock l(mu_);
  return IsEmptyInternal();
}

template <typename TaskType>
void Queue<TaskType>::CloseAndWaitUntilEmpty() {
  Notification empty;
  {
    mutex_lock l(mu_);
    closed_ = true;
    if (IsEmptyInternal()) {
      empty.Notify();
    } else {
      // Arrange for ProcessBatch() to notify when the queue becomes empty.
      empty_notification_ = &empty;
    }
  }
  empty.WaitForNotification();
}

template <typename TaskType>
bool Queue<TaskType>::IsEmptyInternal() const {
  if (options_.enable_lazy_split) {
    return num_batches_being_processed_ == 0 &&
           task_handle_batches_.size() == 1 &&
           task_handle_batches_.back()->empty();
  }
  return num_batches_being_processed_ == 0 && batches_.size() == 1 &&
         batches_.back()->empty();
}

template <typename TaskType>
void Queue<TaskType>::StartNewBatch() {
  if (options_.enable_lazy_split) {
    task_handle_batches_.back()->Close();
    task_handle_batches_.emplace_back(new Batch<BatchInputTaskHandle<TaskType>>(
        ++traceme_context_id_counter_));
    return;
  }
  batches_.back()->Close();
  batches_.emplace_back(new Batch<TaskType>(++traceme_context_id_counter_));
}

template <typename TaskType>
Status Queue<TaskType>::SplitInputBatchIntoSubtasks(
    std::unique_ptr<TaskType>* input_task,
    std::vector<std::unique_ptr<TaskType>>* output_tasks) {
  const int open_batch_remaining_slot =
      max_execution_batch_size() - this->tail_batch_task_size();
  return options_.split_input_task_func(
      std::move(input_task), open_batch_remaining_slot,
      max_execution_batch_size(), std::move(output_tasks));
}

template <typename TaskType>
bool Queue<TaskType>::IsOpenBatchSchedulableAfterEagerSplit() const {
  Batch<TaskType>* open_batch = batches_.back().get();
  if (open_batch->empty()) {
    return false;
  }
  return closed_ || open_batch->size() >= max_execution_batch_size() ||
         env_->NowMicros() >=
             open_batch_start_time_micros_ + options_.batch_timeout_micros;
}

template <typename TaskType>
bool Queue<TaskType>::IsOpenBatchSchedulable() const {
  if (!options_.enable_lazy_split) {
    return IsOpenBatchSchedulableAfterEagerSplit();
  }
  Batch<BatchInputTaskHandle<TaskType>>* open_batch =
      task_handle_batches_.back().get();
  if (open_batch->empty()) {
    return false;
  }
  return closed_ || open_batch->size() >= max_execution_batch_size() ||
         env_->NowMicros() >=
             open_batch_start_time_micros_ + options_.batch_timeout_micros;
}

template <typename TaskType>
size_t Queue<TaskType>::tail_batch_task_size() const {
  if (options_.enable_lazy_split) {
    return task_handle_batches_.back()->size();
  }

  return batches_.back()->size();
}

template <typename TaskType>
int64 Queue<TaskType>::num_enqueued_batches() const {
  if (options_.enable_lazy_split) {
    return task_handle_batches_.size();
  }
  return batches_.size();
}

template <typename TaskType>
QueueHandle<TaskType>::QueueHandle(
    std::shared_ptr<SharedBatchScheduler<TaskType>> scheduler,
    Queue<TaskType>* queue)
    : scheduler_(scheduler), queue_(queue) {}

template <typename TaskType>
QueueHandle<TaskType>::~QueueHandle() {
  queue_->CloseAndWaitUntilEmpty();
}

template <typename TaskType>
Status QueueHandle<TaskType>::Schedule(std::unique_ptr<TaskType>* task) {
  return queue_->Schedule(task);
}

template <typename TaskType>
size_t QueueHandle<TaskType>::NumEnqueuedTasks() const {
  return queue_->NumEnqueuedTasks();
}

template <typename TaskType>
size_t QueueHandle<TaskType>::SchedulingCapacity() const {
  return queue_->SchedulingCapacity();
}

}  // namespace internal

}  // namespace serving
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_SHARED_BATCH_SCHEDULER_H_
