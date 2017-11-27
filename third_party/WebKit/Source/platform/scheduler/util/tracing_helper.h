// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_TRACING_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_TRACING_HELPER_H_

#include <string>
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "platform/PlatformExport.h"

namespace blink {
namespace scheduler {

// DISCLAIMER
// Using these constants in TRACE_EVENTs is discouraged nor should you pass any
// non-literal string as a category, unless familiar with tracing internals.
PLATFORM_EXPORT extern const char kTracingCategoryNameDefault[];
PLATFORM_EXPORT extern const char kTracingCategoryNameInfo[];
PLATFORM_EXPORT extern const char kTracingCategoryNameDebug[];

namespace internal {

PLATFORM_EXPORT void ValidateTracingCategory(const char* category);

}  // namespace internal

PLATFORM_EXPORT void WarmupTracingCategories();

PLATFORM_EXPORT bool AreVerboseSnapshotsEnabled();

PLATFORM_EXPORT std::string PointerToString(const void* pointer);

// TRACE_EVENT macros define static variable to cache a pointer to the state
// of category. Hence, we need distinct version for each category in order to
// prevent unintended leak of state.

template <typename T, const char* category>
class TraceableState {
 public:
  using ConverterFuncPtr = const char* (*)(T);

  TraceableState(T initial_state,
                 const char* name,
                 const void* object,
                 ConverterFuncPtr converter)
      : name_(name),
        object_(object),
        converter_(converter),
        state_(initial_state),
        started_(false) {
    internal::ValidateTracingCategory(category);
    Trace();
  }

  ~TraceableState() {
    if (started_)
      TRACE_EVENT_ASYNC_END0(category, name_, object_);
  }

  TraceableState& operator =(const T& value) {
    Assign(value);
    return *this;
  }
  TraceableState& operator =(const TraceableState& another) {
    Assign(another.state_);
    return *this;
  }

  operator T() const {
    return state_;
  }

  void OnTraceLogEnabled() {
    Trace();
  }

 private:
  void Assign(T new_state) {
    if (state_ != new_state) {
      state_ = new_state;
      Trace();
    }
  }

  void Trace() {
    if (started_)
      TRACE_EVENT_ASYNC_END0(category, name_, object_);

    started_ = is_enabled();
    if (started_) {
      // Trace viewer logic relies on subslice starting at the exact same time
      // as the async event.
      base::TimeTicks now = base::TimeTicks::Now();
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(category, name_, object_, now);
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(category, name_, object_,
                                                  converter_(state_), now);
    }
  }

  bool is_enabled() const {
    bool result = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(category, &result);  // Cached.
    return result;
  }

  const char* const name_;  // Not owned.
  const void* const object_;  // Not owned.
  const ConverterFuncPtr converter_;

  T state_;
  // We have to track |started_| state to avoid race condition since SetState
  // might be called before OnTraceLogEnabled notification.
  bool started_;

  DISALLOW_COPY(TraceableState);
};

template <typename T, const char* category>
class TraceableCounter {
 public:
  using ConverterFuncPtr = double (*)(const T&);

  TraceableCounter(T initial_value,
                   const char* name,
                   const void* object,
                   ConverterFuncPtr converter)
      : name_(name),
        object_(object),
        converter_(converter),
        value_(initial_value) {
    internal::ValidateTracingCategory(category);
    Trace();
  }

  TraceableCounter& operator =(const T& value) {
    value_ = value;
    Trace();
    return *this;
  }
  TraceableCounter& operator =(const TraceableCounter& another) {
    value_ = another.value_;
    Trace();
    return *this;
  }

  const T& get() const {
    return value_;
  }
  operator T() const {
    return value_;
  }

  void Trace() {
    TRACE_COUNTER_ID1(category, name_, object_, converter_(value_));
  }

 private:
  const char* const name_;  // Not owned.
  const void* const object_;  // Not owned.
  const ConverterFuncPtr converter_;

  T value_;
  DISALLOW_COPY(TraceableCounter);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_TRACING_HELPER_H_
