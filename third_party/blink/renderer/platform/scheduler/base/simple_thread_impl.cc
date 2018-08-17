#include "third_party/blink/renderer/platform/scheduler/base/simple_thread_impl.h"

namespace base {
namespace sequence_manager {

SimpleThreadImpl::SimpleThreadImpl(ThreadCallback callback,
                                   TimeTicks initial_time)
    : SimpleThread("TestThread"),
      callback_(std::move(callback)),
      initial_time_(initial_time) {}

void SimpleThreadImpl::Run() {
  std::unique_ptr<ThreadData> thread_data =
      std::make_unique<ThreadData>(initial_time_);
  thread_data_ = thread_data.get();
  std::move(callback_).Run(thread_data_);
  thread_can_shutdown_.Wait();
}

SimpleThreadImpl::~SimpleThreadImpl() {
  thread_can_shutdown_.Signal();
  Join();
}

ThreadData* SimpleThreadImpl::thread_data() const {
  return thread_data_;
}

}  // namespace sequence_manager
}  // namespace base
