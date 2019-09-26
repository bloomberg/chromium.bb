// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SERIAL_DELETE_PTR_H_
#define UTIL_SERIAL_DELETE_PTR_H_

#include <memory>

#include "platform/api/task_runner.h"

namespace openscreen {

template <typename Type, typename DeleterType>
class SerialDelete {
 public:
  SerialDelete(platform::TaskRunner* task_runner)
      : task_runner_(task_runner), deleter_() {
    OSP_DCHECK(task_runner);
  }

  template <typename DT>
  SerialDelete(platform::TaskRunner* task_runner, DT&& deleter)
      : task_runner_(task_runner), deleter_(std::forward<DT>(deleter)) {
    OSP_DCHECK(task_runner);
  }

  void operator()(Type* pointer) const {
    // Deletion of the object depends on the task being run by the task runner.
    task_runner_->PostTask([pointer, deleter = deleter_] { deleter(pointer); });
  }

 private:
  platform::TaskRunner* task_runner_;
  DeleterType deleter_;
};

template <typename Type, typename DeleterType = std::default_delete<Type>>
class SerialDeletePtr
    : public std::unique_ptr<Type, SerialDelete<Type, DeleterType>> {
 public:
  SerialDeletePtr(platform::TaskRunner* task_runner) noexcept
      : std::unique_ptr<Type, SerialDelete<Type, DeleterType>>(
            nullptr,
            SerialDelete<Type, DeleterType>(task_runner)) {
    OSP_DCHECK(task_runner);
  }

  SerialDeletePtr(platform::TaskRunner* task_runner, std::nullptr_t) noexcept
      : std::unique_ptr<Type, SerialDelete<Type, DeleterType>>(
            nullptr,
            SerialDelete<Type, DeleterType>(task_runner)) {
    OSP_DCHECK(task_runner);
  }

  SerialDeletePtr(platform::TaskRunner* task_runner, Type* pointer) noexcept
      : std::unique_ptr<Type, SerialDelete<Type, DeleterType>>(
            pointer,
            SerialDelete<Type, DeleterType>(task_runner)) {
    OSP_DCHECK(task_runner);
  }

  SerialDeletePtr(
      platform::TaskRunner* task_runner,
      Type* pointer,
      typename std::conditional<std::is_reference<DeleterType>::value,
                                DeleterType,
                                const DeleterType&>::type deleter) noexcept
      : std::unique_ptr<Type, SerialDelete<Type, DeleterType>>(
            pointer,
            SerialDelete<Type, DeleterType>(task_runner, deleter)) {
    OSP_DCHECK(task_runner);
  }

  SerialDeletePtr(
      platform::TaskRunner* task_runner,
      Type* pointer,
      typename std::remove_reference<DeleterType>::type&& deleter) noexcept
      : std::unique_ptr<Type, SerialDelete<Type, DeleterType>>(
            pointer,
            SerialDelete<Type, DeleterType>(task_runner, std::move(deleter))) {
    OSP_DCHECK(task_runner);
  }
};

template <typename Type, typename... Args>
SerialDeletePtr<Type> MakeSerialDelete(platform::TaskRunner* task_runner,
                                       Args&&... args) {
  return SerialDeletePtr<Type>(task_runner,
                               new Type(std::forward<Args>(args)...));
}

}  // namespace openscreen

#endif  // UTIL_SERIAL_DELETE_PTR_H_