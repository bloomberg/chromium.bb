// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TYPES_H_
#define WEBKIT_QUOTA_QUOTA_TYPES_H_
#pragma once

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/stl_util-inl.h"

namespace quota {

enum StorageType {
  kStorageTypeTemporary,
  kStorageTypePersistent,
  kStorageTypeUnknown,
};

enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = 9,
  kQuotaErrorAbort = 20,
  kQuotaStatusUnknown = -1,
};

// Common callback types that are used throughout in the quota module.
typedef Callback1<int64>::Type UsageCallback;
typedef Callback1<int64>::Type QuotaCallback;
typedef Callback2<const std::string& /* host */,
                  int64>::Type HostUsageCallback;
typedef Callback2<const std::string& /* host */,
                  int64>::Type HostQuotaCallback;

// Simple template wrapper for a callback queue.
template <typename CallbackType>
class CallbackQueueBase {
 public:
  typedef typename std::deque<CallbackType> Queue;
  typedef typename Queue::iterator iterator;

  virtual ~CallbackQueueBase() {
    STLDeleteContainerPointers(callbacks_.begin(), callbacks_.end());
  }

  // Returns true if the given |callback| is the first one added to the queue.
  bool Add(CallbackType callback) {
    callbacks_.push_back(callback);
    return (callbacks_.size() == 1);
  }

  bool HasCallbacks() const {
    return !callbacks_.empty();
  }

 protected:
  std::deque<CallbackType> callbacks_;
};

template <typename CallbackType1, typename A1>
class CallbackQueue1 : public CallbackQueueBase<CallbackType1> {
 public:
  typedef typename CallbackQueueBase<CallbackType1>::Queue Queue;
  // Runs the callbacks added to the queue and clears the queue.
  void Run(A1 arg) {
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      (*iter)->Run(arg);
      delete *iter;
    }
    this->callbacks_.clear();
  }
};

template <typename CallbackType2, typename A1, typename A2>
class CallbackQueue2 : public CallbackQueueBase<CallbackType2> {
 public:
  typedef typename CallbackQueueBase<CallbackType2>::Queue Queue;
  // Runs the callbacks added to the queue and clears the queue.
  void Run(A1 arg1, A2 arg2) {
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      (*iter)->Run(arg1, arg2);
      delete *iter;
    }
    this->callbacks_.clear();
  }
};

typedef CallbackQueue1<UsageCallback*, int64> UsageCallbackQueue;
typedef CallbackQueue1<QuotaCallback*, int64> QuotaCallbackQueue;

template <typename CallbackType2, typename KEY = std::string,
          typename ARG = int64>
class CallbackQueueMap {
 public:
  typedef CallbackQueue2<CallbackType2, const KEY&, ARG> Queue;
  typedef std::map<KEY, Queue> CallbackMap;
  typedef typename CallbackMap::iterator iterator;

  bool Add(const KEY& key, CallbackType2 callback) {
    return callback_map_[key].Add(callback);
  }

  bool HasCallbacks(const KEY& key) const {
    return (callback_map_.find(key) != callback_map_.end());
  }

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const KEY& key, ARG arg) {
    if (!HasCallbacks(key))
      return;
    Queue& queue = callback_map_[key];
    queue.Run(key, arg);
    callback_map_.erase(key);
  }

  iterator Begin() { return callback_map_.begin(); }
  iterator End() { return callback_map_.end(); }
  static void RunAt(iterator iter, ARG arg) {
    iter->second.Run(iter->first, arg);
  }

  void Clear() { callback_map_.clear(); }

 private:
  CallbackMap callback_map_;
};

typedef CallbackQueueMap<HostUsageCallback*> HostUsageCallbackMap;
typedef CallbackQueueMap<HostQuotaCallback*> HostQuotaCallbackMap;

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TYPES_H_
