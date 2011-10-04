// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TYPES_H_
#define WEBKIT_QUOTA_QUOTA_TYPES_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/stl_util.h"

class GURL;

namespace quota {

enum StorageType {
  kStorageTypeTemporary,
  kStorageTypePersistent,
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
typedef Callback2<StorageType, int64>::Type UsageCallback;
typedef Callback3<StorageType, int64, int64>::Type GlobalUsageCallback;
typedef Callback3<QuotaStatusCode,
                  StorageType,
                  int64>::Type QuotaCallback;
typedef Callback3<const std::string& /* host */,
                  StorageType,
                  int64>::Type HostUsageCallback;
typedef Callback4<QuotaStatusCode,
                  const std::string& /* host */,
                  StorageType,
                  int64>::Type HostQuotaCallback;
typedef Callback2<QuotaStatusCode,
                  int64>::Type AvailableSpaceCallback;
typedef Callback1<QuotaStatusCode>::Type StatusCallback;
typedef Callback2<const std::set<GURL>&, StorageType>::Type GetOriginsCallback;
typedef Callback1<const UsageInfoEntries&>::Type GetUsageInfoCallback;

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
    // Note: template-derived class needs 'this->' to access its base class.
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

template <typename CallbackType3, typename A1, typename A2, typename A3>
class CallbackQueue3 : public CallbackQueueBase<CallbackType3> {
 public:
  typedef typename CallbackQueueBase<CallbackType3>::Queue Queue;
  // Runs the callbacks added to the queue and clears the queue.
  void Run(A1 arg1, A2 arg2, A3 arg3) {
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      (*iter)->Run(arg1, arg2, arg3);
      delete *iter;
    }
    this->callbacks_.clear();
  }
};

template <typename CallbackType4,
          typename A1, typename A2, typename A3, typename A4>
class CallbackQueue4 : public CallbackQueueBase<CallbackType4> {
 public:
  typedef typename CallbackQueueBase<CallbackType4>::Queue Queue;
  // Runs the callbacks added to the queue and clears the queue.
  void Run(A1 arg1, A2 arg2, A3 arg3, A4 arg4) {
    for (typename Queue::iterator iter = this->callbacks_.begin();
         iter != this->callbacks_.end(); ++iter) {
      (*iter)->Run(arg1, arg2, arg3, arg4);
      delete *iter;
    }
    this->callbacks_.clear();
  }
};

typedef CallbackQueue2<UsageCallback*,
                       StorageType, int64> UsageCallbackQueue;
typedef CallbackQueue3<GlobalUsageCallback*,
                       StorageType, int64, int64> GlobalUsageCallbackQueue;
typedef CallbackQueue3<QuotaCallback*,
                       QuotaStatusCode,
                       StorageType, int64> QuotaCallbackQueue;

template <typename CallbackType, typename CallbackQueueType, typename KEY>
class CallbackQueueMapBase {
 public:
  typedef std::map<KEY, CallbackQueueType> CallbackMap;
  typedef typename CallbackMap::iterator iterator;

  bool Add(const KEY& key, CallbackType callback) {
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

template <typename CallbackType4, typename KEY,
          typename ARG1, typename ARG2, typename ARG3, typename ARG4>
class CallbackQueueMap4
    : public CallbackQueueMapBase<CallbackType4,
                                  CallbackQueue4<CallbackType4,
                                                 ARG1, ARG2, ARG3, ARG4>,
                                  KEY> {
 public:
  typedef typename CallbackQueueMapBase<
      CallbackType4,
      CallbackQueue4<CallbackType4, ARG1, ARG2, ARG3, ARG4>,
      KEY>::iterator iterator;
  typedef CallbackQueue4<CallbackType4, ARG1, ARG2, ARG3, ARG4> Queue;

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const KEY& key, ARG1 arg1, ARG2 arg2, ARG3 arg3, ARG4 arg4) {
    if (!this->HasCallbacks(key))
      return;
    Queue& queue = this->callback_map_[key];
    queue.Run(arg1, arg2, arg3, arg4);
    this->callback_map_.erase(key);
  }
};

typedef CallbackQueueMap1<UsageCallback*, GURL, int64> OriginUsageCallbackMap;
typedef CallbackQueueMap3<HostUsageCallback*, std::string,
                          const std::string&,
                          StorageType, int64> HostUsageCallbackMap;
typedef CallbackQueueMap4<HostQuotaCallback*, std::string,
                          QuotaStatusCode,
                          const std::string&,
                          StorageType, int64> HostQuotaCallbackMap;

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TYPES_H_
