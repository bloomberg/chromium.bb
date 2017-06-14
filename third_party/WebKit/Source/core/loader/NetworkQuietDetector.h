// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkQuietDetector_h
#define NetworkQuietDetector_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Document;

// NetworkQuietDetector observes network request count everytime a load is
// finshed after DOMContentLoadedEventEnd is fired, and signals a network
// idleness signal to GRC when there are no more than 2 network connection
// active in 1 second.
class CORE_EXPORT NetworkQuietDetector
    : public GarbageCollectedFinalized<NetworkQuietDetector>,
      public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(NetworkQuietDetector);
  USING_GARBAGE_COLLECTED_MIXIN(NetworkQuietDetector);

 public:
  static NetworkQuietDetector& From(Document&);
  virtual ~NetworkQuietDetector() {}

  void CheckNetworkStable();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class NetworkQuietDetectorTest;

  // The page is quiet if there are no more than 2 active network requests for
  // this duration of time.
  static constexpr double kNetworkQuietWindowSeconds = 1.0;
  static constexpr int kNetworkQuietMaximumConnections = 2;

  explicit NetworkQuietDetector(Document&);
  int ActiveConnections();
  void SetNetworkQuietTimers(int active_connections);
  void NetworkQuietTimerFired(TimerBase*);

  bool network_quiet_reached_ = false;
  TaskRunnerTimer<NetworkQuietDetector> network_quiet_timer_;
};

}  // namespace blink

#endif
