// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_TASK_RUNNER_BOUND_OBSERVER_LIST_H_
#define WEBKIT_FILEAPI_TASK_RUNNER_BOUND_OBSERVER_LIST_H_

#include <map>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"

namespace fileapi {

// A wrapper for dispatching method.
template <class T, class Method, class Params>
void NotifyWrapper(T obj, Method m, const Params& p) {
  DispatchToMethod(base::internal::UnwrapTraits<T>::Unwrap(obj), m, p);
}

// An observer list helper to notify on a given task runner.
// Observer pointers (stored as ObserverStoreType) must be kept alive
// until this list dispatches all the notifications.
//
// Unlike regular ObserverList or ObserverListThreadSafe internal observer
// list is immutable (though not declared const) and cannot be modified after
// constructed.
//
// It is ok to specify scoped_refptr<Observer> as ObserverStoreType to
// explicitly keep references if necessary.
template <class Observer, class ObserverStoreType = Observer*>
class TaskRunnerBoundObserverList {
 public:
  // A constructor parameter class.
  class Source {
   public:
    typedef scoped_refptr<base::SequencedTaskRunner> TaskRunnerPtr;
    typedef std::map<ObserverStoreType, TaskRunnerPtr> ObserversListMap;

    // Add |observer| to the list parameter. The |observer| will be notified on
    // the |runner_to_notify| runner.  It is valid to give NULL as
    // |runner_to_notify| (in such case notifications are dispatched on
    // the current runner).
    void AddObserver(Observer* observer,
                     base::SequencedTaskRunner* runner_to_notify) {
      observers_.insert(std::make_pair(observer, runner_to_notify));
    }

    const ObserversListMap& observers() const { return observers_; }

   private:
    ObserversListMap observers_;
  };

  // Creates an empty list.
  TaskRunnerBoundObserverList<Observer, ObserverStoreType>() {}

  // Creates a new list with given |observers_param|.
  explicit TaskRunnerBoundObserverList<Observer, ObserverStoreType>(
      const Source& source)
      : observers_(source.observers()) {}

  virtual ~TaskRunnerBoundObserverList<Observer, ObserverStoreType>() {}

  // Notify on the task runner that is given to AddObserver.
  // If we're already on the runner this just dispatches the method.
  template <class Method, class Params>
  void Notify(Method method, const Params& params) const {
    COMPILE_ASSERT(
        (base::internal::ParamsUseScopedRefptrCorrectly<Params>::value),
        badunboundmethodparams);
    for (typename ObserversListMap::const_iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      if (!it->second.get() || it->second->RunsTasksOnCurrentThread()) {
        DispatchToMethod(UnwrapTraits::Unwrap(it->first), method, params);
        continue;
      }
      it->second->PostTask(
          FROM_HERE,
          base::Bind(&NotifyWrapper<ObserverStoreType, Method, Params>,
                     it->first, method, params));
    }
  }

 private:
  typedef base::internal::UnwrapTraits<ObserverStoreType> UnwrapTraits;
  typedef scoped_refptr<base::SequencedTaskRunner> TaskRunnerPtr;
  typedef std::map<ObserverStoreType, TaskRunnerPtr> ObserversListMap;

  ObserversListMap observers_;
};

class FileAccessObserver;
class FileChangeObserver;
class FileUpdateObserver;

typedef TaskRunnerBoundObserverList<FileAccessObserver> AccessObserverList;
typedef TaskRunnerBoundObserverList<FileChangeObserver> ChangeObserverList;
typedef TaskRunnerBoundObserverList<FileUpdateObserver> UpdateObserverList;

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_TASK_RUNNER_BOUND_OBSERVER_LIST_H_
