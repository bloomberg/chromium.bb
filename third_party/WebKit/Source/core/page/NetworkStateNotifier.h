/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NetworkStateNotifier_h
#define NetworkStateNotifier_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/CrossThreadCopier.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebConnectionType.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NetworkStateNotifier {
  WTF_MAKE_NONCOPYABLE(NetworkStateNotifier);
  USING_FAST_MALLOC(NetworkStateNotifier);

 public:
  struct NetworkState {
    static const int kInvalidMaxBandwidth = -1;
    bool onLineInitialized = false;
    bool onLine = true;
    bool connectionInitialized = false;
    WebConnectionType type = WebConnectionTypeOther;
    double maxBandwidthMbps = kInvalidMaxBandwidth;
  };

  class NetworkStateObserver {
   public:
    // Will be called on the task runner that is passed in add*Observer.
    virtual void connectionChange(WebConnectionType, double maxBandwidthMbps) {}
    virtual void onLineStateChange(bool onLine) {}
  };

  NetworkStateNotifier() : m_hasOverride(false) {}

  // Can be called on any thread.
  bool onLine() const {
    MutexLocker locker(m_mutex);
    const NetworkState& state = m_hasOverride ? m_override : m_state;
    DCHECK(state.onLineInitialized);
    return state.onLine;
  }

  void setOnLine(bool);

  // Can be called on any thread.
  WebConnectionType connectionType() const {
    MutexLocker locker(m_mutex);
    const NetworkState& state = m_hasOverride ? m_override : m_state;
    DCHECK(state.connectionInitialized);
    return state.type;
  }

  // Can be called on any thread.
  bool isCellularConnectionType() const {
    switch (connectionType()) {
      case WebConnectionTypeCellular2G:
      case WebConnectionTypeCellular3G:
      case WebConnectionTypeCellular4G:
        return true;
      case WebConnectionTypeBluetooth:
      case WebConnectionTypeEthernet:
      case WebConnectionTypeWifi:
      case WebConnectionTypeWimax:
      case WebConnectionTypeOther:
      case WebConnectionTypeNone:
      case WebConnectionTypeUnknown:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
  }

  // Can be called on any thread.
  double maxBandwidth() const {
    MutexLocker locker(m_mutex);
    const NetworkState& state = m_hasOverride ? m_override : m_state;
    DCHECK(state.connectionInitialized);
    return state.maxBandwidthMbps;
  }

  void setWebConnection(WebConnectionType, double maxBandwidthMbps);

  // When called, successive setWebConnectionType/setOnLine calls are stored,
  // and supplied overridden values are used instead until clearOverride() is
  // called.  This is used for layout tests (see crbug.com/377736) and inspector
  // emulation.
  //
  // Since this class is a singleton, tests must clear override when completed
  // to avoid indeterminate state across the test harness.
  void setOverride(bool onLine, WebConnectionType, double maxBandwidthMbps);
  void clearOverride();

  // Must be called on the given task runner. An added observer must be removed
  // before the observer or its execution context goes away. It's possible for
  // an observer to be called twice for the same event if it is first removed
  // and then added during notification.
  void addConnectionObserver(NetworkStateObserver*, PassRefPtr<WebTaskRunner>);
  void addOnLineObserver(NetworkStateObserver*, PassRefPtr<WebTaskRunner>);
  void removeConnectionObserver(NetworkStateObserver*,
                                PassRefPtr<WebTaskRunner>);
  void removeOnLineObserver(NetworkStateObserver*, PassRefPtr<WebTaskRunner>);

 private:
  struct ObserverList {
    ObserverList() : iterating(false) {}
    bool iterating;
    Vector<NetworkStateObserver*> observers;
    Vector<size_t> zeroedObservers;  // Indices in observers that are 0.
  };

  // This helper scope issues required notifications when mutating the state if
  // something has changed.  It's only possible to mutate the state on the main
  // thread.  Note that ScopedNotifier must be destroyed when not holding a lock
  // so that onLine notifications can be dispatched without a deadlock.
  class ScopedNotifier {
   public:
    explicit ScopedNotifier(NetworkStateNotifier&);
    ~ScopedNotifier();

   private:
    NetworkStateNotifier& m_notifier;
    NetworkState m_before;
  };

  enum class ObserverType {
    ONLINE_STATE,
    CONNECTION_TYPE,
  };

  // The ObserverListMap is cross-thread accessed, adding/removing Observers
  // running on a task runner.
  using ObserverListMap =
      HashMap<RefPtr<WebTaskRunner>, std::unique_ptr<ObserverList>>;

  void notifyObservers(ObserverListMap&, ObserverType, const NetworkState&);
  void notifyObserversOnTaskRunner(ObserverListMap*,
                                   ObserverType,
                                   PassRefPtr<WebTaskRunner>,
                                   const NetworkState&);

  void addObserver(ObserverListMap&,
                   NetworkStateObserver*,
                   PassRefPtr<WebTaskRunner>);
  void removeObserver(ObserverListMap&,
                      NetworkStateObserver*,
                      PassRefPtr<WebTaskRunner>);

  ObserverList* lockAndFindObserverList(ObserverListMap&,
                                        PassRefPtr<WebTaskRunner>);

  // Removed observers are nulled out in the list in case the list is being
  // iterated over. Once done iterating, call this to clean up nulled
  // observers.
  void collectZeroedObservers(ObserverListMap&,
                              ObserverList*,
                              PassRefPtr<WebTaskRunner>);

  mutable Mutex m_mutex;
  NetworkState m_state;
  bool m_hasOverride;
  NetworkState m_override;

  ObserverListMap m_connectionObservers;
  ObserverListMap m_onLineStateObservers;
};

CORE_EXPORT NetworkStateNotifier& networkStateNotifier();

}  // namespace blink

#endif  // NetworkStateNotifier_h
