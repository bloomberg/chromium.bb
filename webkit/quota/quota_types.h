// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TYPES_H_
#define WEBKIT_QUOTA_QUOTA_TYPES_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"

class GURL;

namespace quota {

enum StorageType {
  kStorageTypeTemporary,
  kStorageTypePersistent,
  kStorageTypeSyncable,
  kStorageTypeUnknown,
};

// The numbers should match with the error code defined in
// third_party/WebKit/Source/WebCore/dom/ExceptionCode.h.
enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = 9,          // NOT_SUPPORTED_ERR
  kQuotaErrorInvalidModification = 13,  // INVALID_MODIFICATION_ERR
  kQuotaErrorInvalidAccess = 15,        // INVALID_ACCESS_ERR
  kQuotaErrorAbort = 20,                // ABORT_ERR
  kQuotaStatusUnknown = -1,
};

struct UsageInfo;
typedef std::vector<UsageInfo> UsageInfoEntries;

// Common callback types that are used throughout in the quota module.
typedef base::Callback<void(StorageType status,
                            int64 usage,
                            int64 unlimited_usage)> GlobalUsageCallback;
typedef base::Callback<void(QuotaStatusCode status,
                            StorageType type,
                            int64 quota)> QuotaCallback;
// TODO(kinuko,nhiroki): HostUsageCallback could be probably replaced with
// simple Callback<void(int64)> callback by binding host and type with Bind.
typedef base::Callback<void(const std::string& host,
                            StorageType type,
                            int64 host_usage)> HostUsageCallback;
typedef base::Callback<void(QuotaStatusCode status,
                            const std::string& host,
                            StorageType type,
                            int64 quota)> HostQuotaCallback;
typedef base::Callback<void(QuotaStatusCode, int64)> AvailableSpaceCallback;
typedef base::Callback<void(QuotaStatusCode)> StatusCallback;
typedef base::Callback<void(const std::set<GURL>& origins,
                            StorageType type)> GetOriginsCallback;
typedef base::Callback<void(const UsageInfoEntries&)> GetUsageInfoCallback;

// Simple template wrapper for a callback queue.
template <typename CallbackType>
class CallbackQueueBase {
 public:
  typedef typename std::deque<CallbackType> Queue;
  typedef typename Queue::iterator iterator;

  virtual ~CallbackQueueBase() {}

  // Returns true if the given |callback| is the first one added to the queue.
  bool Add(const CallbackType& callback) {
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
    // Note: template-derived class needs 'this->' to access its base class.
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      iter->Run(arg);
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
      iter->Run(arg1, arg2);
    }
    this->callbacks_.clear();
  }
};

template <typename CallbackType3, typename A1, typename A2, typename A3>
class CallbackQueue3 : public CallbackQueueBase<CallbackType3> {
 public:
  typedef typename CallbackQueueBase<CallbackType3>::Queue Queue;
  // Runs the callbacks added to the queue and clears the queue.
  void Run(A1 arg1, A2 arg2, A3 arg3) {
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      iter->Run(arg1, arg2, arg3);
    }
    this->callbacks_.clear();
  }
};

typedef CallbackQueue3<GlobalUsageCallback,
                       StorageType, int64, int64> GlobalUsageCallbackQueue;

template <typename CallbackType, typename CallbackQueueType, typename KEY>
class CallbackQueueMapBase {
 public:
  typedef std::map<KEY, CallbackQueueType> CallbackMap;
  typedef typename CallbackMap::iterator iterator;

  bool Add(const KEY& key, const CallbackType& callback) {
    return callback_map_[key].Add(callback);
  }

  bool HasCallbacks(const KEY& key) const {
    return (callback_map_.find(key) != callback_map_.end());
  }

  bool HasAnyCallbacks() const {
    return !callback_map_.empty();
  }

  iterator Begin() { return callback_map_.begin(); }
  iterator End() { return callback_map_.end(); }

  void Clear() { callback_map_.clear(); }

 protected:
  CallbackMap callback_map_;
};

template <typename CallbackType1, typename KEY, typename ARG>
class CallbackQueueMap1
    : public CallbackQueueMapBase<CallbackType1,
                                  CallbackQueue1<CallbackType1, ARG>,
                                  KEY> {
 public:
  typedef typename CallbackQueueMapBase<
      CallbackType1,
      CallbackQueue1<CallbackType1, ARG>,
      KEY>::iterator iterator;
  typedef CallbackQueue1<CallbackType1, ARG> Queue;

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const KEY& key, ARG arg) {
    if (!this->HasCallbacks(key))
      return;
    Queue& queue = this->callback_map_[key];
    queue.Run(arg);
    this->callback_map_.erase(key);
  }
};

template <typename CallbackType2, typename KEY, typename ARG1, typename ARG2>
class CallbackQueueMap2
    : public CallbackQueueMapBase<CallbackType2,
                                  CallbackQueue2<CallbackType2, ARG1, ARG2>,
                                  KEY> {
 public:
  typedef typename CallbackQueueMapBase<
      CallbackType2,
      CallbackQueue2<CallbackType2, ARG1, ARG2>,
      KEY>::iterator iterator;
  typedef CallbackQueue2<CallbackType2, ARG1, ARG2> Queue;

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const KEY& key, ARG1 arg1, ARG2 arg2) {
    if (!this->HasCallbacks(key))
      return;
    Queue& queue = this->callback_map_[key];
    queue.Run(arg1, arg2);
    this->callback_map_.erase(key);
  }
};

template <typename CallbackType3, typename KEY,
          typename ARG1, typename ARG2, typename ARG3>
class CallbackQueueMap3
    : public CallbackQueueMapBase<CallbackType3,
                                  CallbackQueue3<CallbackType3,
                                                 ARG1, ARG2, ARG3>,
                                  KEY> {
 public:
  typedef typename CallbackQueueMapBase<
      CallbackType3,
      CallbackQueue3<CallbackType3, ARG1, ARG2, ARG3>,
      KEY>::iterator iterator;
  typedef CallbackQueue3<CallbackType3, ARG1, ARG2, ARG3> Queue;

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const KEY& key, ARG1 arg1, ARG2 arg2, ARG3 arg3) {
    if (!this->HasCallbacks(key))
      return;
    Queue& queue = this->callback_map_[key];
    queue.Run(arg1, arg2, arg3);
    this->callback_map_.erase(key);
  }
};

typedef CallbackQueueMap3<HostUsageCallback, std::string,
                          const std::string&,
                          StorageType, int64> HostUsageCallbackMap;

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TYPES_H_
