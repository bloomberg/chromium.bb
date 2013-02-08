// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_throttle.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_job.h"

namespace net {

WebSocketThrottle::WebSocketThrottle() {
}

WebSocketThrottle::~WebSocketThrottle() {
  DCHECK(queue_.empty());
  DCHECK(addr_map_.empty());
}

// static
WebSocketThrottle* WebSocketThrottle::GetInstance() {
  return Singleton<WebSocketThrottle>::get();
}

void WebSocketThrottle::PutInQueue(WebSocketJob* job) {
  queue_.push_back(job);
  const AddressList& address_list = job->address_list();
  std::set<IPEndPoint> address_set;
  for (AddressList::const_iterator addr_iter = address_list.begin();
       addr_iter != address_list.end();
       ++addr_iter) {
    const IPEndPoint& address = *addr_iter;
    // If |address| is already processed, don't do it again.
    if (!address_set.insert(address).second)
      continue;

    ConnectingAddressMap::iterator iter = addr_map_.find(address);
    if (iter == addr_map_.end()) {
      ConnectingQueue* queue = new ConnectingQueue();
      queue->push_back(job);
      addr_map_[address] = queue;
    } else {
      iter->second->push_back(job);
      job->SetWaiting();
      DVLOG(1) << "Waiting on " << address.ToString();
    }
  }
}

void WebSocketThrottle::RemoveFromQueue(WebSocketJob* job) {
  ConnectingQueue::iterator queue_iter =
      std::find(queue_.begin(), queue_.end(), job);
  if (queue_iter == queue_.end())
    return;
  queue_.erase(queue_iter);
  const AddressList& address_list = job->address_list();
  std::set<IPEndPoint> address_set;
  for (AddressList::const_iterator addr_iter = address_list.begin();
       addr_iter != address_list.end();
       ++addr_iter) {
    const IPEndPoint& address = *addr_iter;
    // If |address| is already processed, don't do it again.
    if (!address_set.insert(address).second)
      continue;

    ConnectingAddressMap::iterator map_iter = addr_map_.find(address);
    DCHECK(map_iter != addr_map_.end());

    ConnectingQueue* queue = map_iter->second;
    // Job may not be front of queue if the socket is closed while waiting.
    ConnectingQueue::iterator address_queue_iter =
        std::find(queue->begin(), queue->end(), job);
    if (address_queue_iter != queue->end())
      queue->erase(address_queue_iter);
    if (queue->empty()) {
      delete queue;
      addr_map_.erase(map_iter);
    }
  }
}

void WebSocketThrottle::WakeupSocketIfNecessary() {
  for (ConnectingQueue::iterator iter = queue_.begin();
       iter != queue_.end();
       ++iter) {
    WebSocketJob* job = *iter;
    if (!job->IsWaiting())
      continue;

    bool should_wakeup = true;
    const AddressList& address_list = job->address_list();
    for (AddressList::const_iterator addr_iter = address_list.begin();
         addr_iter != address_list.end();
         ++addr_iter) {
      const IPEndPoint& address = *addr_iter;
      ConnectingAddressMap::iterator map_iter = addr_map_.find(address);
      DCHECK(map_iter != addr_map_.end());
      ConnectingQueue* queue = map_iter->second;
      if (job != queue->front()) {
        should_wakeup = false;
        break;
      }
    }
    if (should_wakeup)
      job->Wakeup();
  }
}

}  // namespace net
