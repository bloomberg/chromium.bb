// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/webrtc/api/metronome/metronome.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace blink {

namespace {

// Wraps a webrtc::Metronome::TickListener to ensure that OnTick is not called
// after it is removed from the WebRtcMetronomeAdapter, using the Deactive
// method.
class WebRtcMetronomeListenerWrapper
    : public base::RefCountedThreadSafe<WebRtcMetronomeListenerWrapper> {
 public:
  explicit WebRtcMetronomeListenerWrapper(
      webrtc::Metronome::TickListener* listener)
      : listener_(listener) {}

  void Deactivate() {
    base::AutoLock auto_lock(lock_);
    active_ = false;
  }

  webrtc::Metronome::TickListener* listener() { return listener_; }

  void OnTick() {
    base::AutoLock auto_lock(lock_);
    if (!active_)
      return;
    listener_->OnTick();
  }

 private:
  friend class base::RefCountedThreadSafe<WebRtcMetronomeListenerWrapper>;
  ~WebRtcMetronomeListenerWrapper() = default;

  webrtc::Metronome::TickListener* const listener_;

  base::Lock lock_;
  bool active_ GUARDED_BY(lock_) = true;
};

class WebRtcMetronomeAdapter : public webrtc::Metronome {
 public:
  explicit WebRtcMetronomeAdapter(
      scoped_refptr<MetronomeSource> metronome_source)
      : metronome_source_(std::move(metronome_source)) {
    DCHECK(metronome_source_);
  }

  ~WebRtcMetronomeAdapter() override { DCHECK(listeners_.empty()); }

  // Adds a tick listener to the metronome. Once this method has returned
  // OnTick will be invoked on each metronome tick. A listener may
  // only be added to the metronome once.
  void AddListener(TickListener* listener) override {
    DCHECK(listener);
    base::AutoLock auto_lock(lock_);
    auto [it, inserted] = listeners_.emplace(
        listener,
        base::MakeRefCounted<WebRtcMetronomeListenerWrapper>(listener));
    DCHECK(inserted);
    if (listeners_.size() == 1) {
      DCHECK(!tick_handle_);
      tick_handle_ = metronome_source_->AddListener(
          nullptr,
          base::BindRepeating(&WebRtcMetronomeAdapter::OnTick,
                              base::Unretained(this)),
          base::TimeTicks::Min());
    }
  }

  // Removes the tick listener from the metronome. Once this method has returned
  // OnTick will never be called again. This method must not be called from
  // within OnTick.
  void RemoveListener(TickListener* listener) override {
    DCHECK(listener);
    base::AutoLock auto_lock(lock_);
    auto it = listeners_.find(listener);
    if (it == listeners_.end()) {
      DLOG(WARNING) << __FUNCTION__ << " called with unregistered listener.";
      return;
    }
    it->second->Deactivate();
    listeners_.erase(it);
    if (listeners_.size() == 0) {
      metronome_source_->RemoveListener(std::move(tick_handle_));
    }
  }

  // Returns the current tick period of the metronome.
  webrtc::TimeDelta TickPeriod() const override {
    return webrtc::TimeDelta::Micros(
        metronome_source_->metronome_tick().InMicroseconds());
  }

 private:
  void OnTick() {
    base::AutoLock auto_lock(lock_);
    for (auto [listener, wrapper] : listeners_) {
      listener->OnTickTaskQueue()->PostTask(webrtc::ToQueuedTask(
          [wrapper = std::move(wrapper)] { wrapper->OnTick(); }));
    }
  }

  const scoped_refptr<MetronomeSource> metronome_source_;
  base::Lock lock_;
  base::flat_map<TickListener*, scoped_refptr<WebRtcMetronomeListenerWrapper>>
      listeners_ GUARDED_BY(lock_);
  scoped_refptr<MetronomeSource::ListenerHandle> tick_handle_ GUARDED_BY(lock_);
};

}  // namespace

MetronomeSource::ListenerHandle::ListenerHandle(
    scoped_refptr<MetronomeSource> metronome_source,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time)
    : metronome_source_(std::move(metronome_source)),
      task_runner_(std::move(task_runner)),
      callback_(std::move(callback)),
      wakeup_time_(std::move(wakeup_time)) {}

MetronomeSource::ListenerHandle::~ListenerHandle() = default;

void MetronomeSource::ListenerHandle::SetWakeupTime(
    base::TimeTicks wakeup_time) {
  metronome_source_->metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MetronomeSource::ListenerHandle::SetWakeUpTimeOnMetronomeTaskRunner,
          this, wakeup_time));
}

void MetronomeSource::ListenerHandle::SetWakeUpTimeOnMetronomeTaskRunner(
    base::TimeTicks wakeup_time) {
  DCHECK(
      metronome_source_->metronome_task_runner_->RunsTasksInCurrentSequence());
  wakeup_time_ = wakeup_time;
  metronome_source_->EnsureNextTickIsScheduled(wakeup_time);
}

void MetronomeSource::ListenerHandle::OnMetronomeTickOnMetronomeTaskRunner(
    base::TimeTicks now) {
  DCHECK(
      metronome_source_->metronome_task_runner_->RunsTasksInCurrentSequence());
  if (wakeup_time_.is_max()) {
    // This listener is sleeping indefinitely.
    return;
  }
  if (now < wakeup_time_) {
    // It is not time for this listener to fire yet, but ensure that the next
    // tick is scheduled.
    metronome_source_->EnsureNextTickIsScheduled(wakeup_time_);
    return;
  }
  if (!wakeup_time_.is_min()) {
    // A wakeup time had been specified (set to anything other than "min").
    // Reset the wakeup time to "infinity", meaning SetWakeupTime() has to be
    // called again in order to wake up again.
    wakeup_time_ = base::TimeTicks::Max();
  }
  if (task_runner_ == nullptr) {
    // Run the task directly if |task_runner_| is null.
    MaybeRunCallback();
  } else {
    // Post to run on target |task_runner_|.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MetronomeSource::ListenerHandle::MaybeRunCallback,
                       this));
  }
}

void MetronomeSource::ListenerHandle::MaybeRunCallback() {
  DCHECK(task_runner_ == nullptr || task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(is_active_lock_);
  if (!is_active_)
    return;
  callback_.Run();
}

void MetronomeSource::ListenerHandle::Inactivate() {
  base::AutoLock auto_lock(is_active_lock_);
  is_active_ = false;
}

MetronomeSource::MetronomeSource(base::TimeTicks metronome_phase,
                                 base::TimeDelta metronome_tick)
    : metronome_task_runner_(
          // HIGHEST priority is used to reduce risk of jitter.
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::HIGHEST})),
      metronome_phase_(std::move(metronome_phase)),
      metronome_tick_(std::move(metronome_tick)) {
  base::TimeTicks now = base::TimeTicks::Now();
  prev_tick_ = GetTimeSnappedToNextMetronomeTick(now);
  if (prev_tick_ > now)
    prev_tick_ -= metronome_tick_;
}

MetronomeSource::~MetronomeSource() {
  DCHECK(listeners_.empty());
  DCHECK(!next_tick_handle_.IsValid());
}

base::TimeTicks MetronomeSource::GetTimeSnappedToNextMetronomeTick(
    base::TimeTicks time) const {
  return time.SnappedToNextTick(metronome_phase_, metronome_tick_);
}

scoped_refptr<MetronomeSource::ListenerHandle> MetronomeSource::AddListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time) {
  // Ref-counting keeps |this| alive until all listeners have been removed.
  scoped_refptr<ListenerHandle> listener_handle =
      base::MakeRefCounted<ListenerHandle>(this, std::move(task_runner),
                                           std::move(callback), wakeup_time);
  metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetronomeSource::AddListenerOnMetronomeTaskRunner,
                     scoped_refptr<MetronomeSource>(this), listener_handle));
  return listener_handle;
}

void MetronomeSource::RemoveListener(
    scoped_refptr<ListenerHandle> listener_handle) {
  listener_handle->Inactivate();
  metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetronomeSource::RemoveListenerOnMetronomeTaskRunner,
                     scoped_refptr<MetronomeSource>(this), listener_handle));
}

void MetronomeSource::AddListenerOnMetronomeTaskRunner(
    scoped_refptr<ListenerHandle> listener_handle) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  listeners_.insert(listener_handle);
  EnsureNextTickIsScheduled(listener_handle->wakeup_time_);
}

void MetronomeSource::RemoveListenerOnMetronomeTaskRunner(
    scoped_refptr<ListenerHandle> listener_handle) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  listeners_.erase(listener_handle);
  // To avoid additional complexity we do not reschedule the next tick, but we
  // do cancel the next tick if there are no more listeners.
  if (listeners_.empty()) {
    next_tick_ = base::TimeTicks::Min();
    next_tick_handle_.CancelTask();
  }
}

void MetronomeSource::EnsureNextTickIsScheduled(base::TimeTicks wakeup_time) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  if (wakeup_time.is_max()) {
    return;
  }
  if (wakeup_time <= prev_tick_) {
    // Do not reschedule a tick that already fired, such as when adding a
    // listener on a tick.
    wakeup_time = prev_tick_ + metronome_tick_;
  }
  base::TimeTicks wakeup_tick = GetTimeSnappedToNextMetronomeTick(wakeup_time);
  if (!next_tick_.is_min() && wakeup_tick >= next_tick_) {
    //  We already have the next tick scheduled.
    return;
  }
  // If we already have a tick scheduled but too far in the future, cancel it.
  next_tick_handle_.CancelTask();
  next_tick_ = wakeup_tick;
  next_tick_handle_ = metronome_task_runner_->PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&MetronomeSource::OnMetronomeTick,
                     // Unretained is safe because tasks are cancelled prior to
                     // destruction.
                     base::Unretained(this), next_tick_),
      next_tick_, base::subtle::DelayPolicy::kPrecise);
}

void MetronomeSource::OnMetronomeTick(base::TimeTicks now_tick) {
  TRACE_EVENT_INSTANT0("webrtc", "MetronomeSource::OnMetronomeTick",
                       TRACE_EVENT_SCOPE_PROCESS);
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  // We no longer have a tick scheduled.
  prev_tick_ = now_tick;
  next_tick_ = base::TimeTicks::Min();
  bool schedule_next_tick = false;
  base::TimeTicks now = base::TimeTicks::Now();
  // On some platforms (Android), base::TimeTicks::Now() may in some cases lag
  // behind by ~1 ms due to clock caching. To ensure listeners with the same
  // wake up time as |now_tick| runs, ensure |now| is at least |now_tick|.
  if (now < now_tick) {
    now = now_tick;
  }
  for (auto& listener : listeners_) {
    listener->OnMetronomeTickOnMetronomeTaskRunner(now);
    schedule_next_tick |= listener->wakeup_time_.is_min();
  }
  if (schedule_next_tick) {
    // The next tick is `now_tick + metronome_tick_`, but if late wakeup happens
    // due to load we could in extreme cases miss ticks. To avoid posting
    // immediate "catch-up" tasks, make it possible to skip metronome ticks.
    constexpr double kTickThreshold = 0.5;
    EnsureNextTickIsScheduled(base::TimeTicks::Now() +
                              metronome_tick_ * kTickThreshold);
  }
}

std::unique_ptr<webrtc::Metronome> MetronomeSource::CreateWebRtcMetronome() {
  return std::make_unique<WebRtcMetronomeAdapter>(base::WrapRefCounted(this));
}

bool MetronomeSource::HasListenersForTesting() {
  base::WaitableEvent event;
  bool has_listeners = false;
  metronome_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](MetronomeSource* thiz, bool* has_listeners,
                        base::WaitableEvent* event) {
                       *has_listeners = !thiz->listeners_.empty();
                       event->Signal();
                     },
                     base::Unretained(this), base::Unretained(&has_listeners),
                     base::Unretained(&event)));
  event.Wait();
  return has_listeners;
}

}  // namespace blink
