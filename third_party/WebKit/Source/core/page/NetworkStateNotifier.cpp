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

#include "core/page/NetworkStateNotifier.h"

#include "platform/CrossThreadFunctional.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"

namespace blink {

template <>
struct CrossThreadCopier<NetworkStateNotifier::NetworkState>
    : public CrossThreadCopierPassThrough<NetworkStateNotifier::NetworkState> {
  STATIC_ONLY(CrossThreadCopier);
};

NetworkStateNotifier& networkStateNotifier() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(NetworkStateNotifier, networkStateNotifier,
                                  new NetworkStateNotifier);
  return networkStateNotifier;
}

NetworkStateNotifier::ScopedNotifier::ScopedNotifier(
    NetworkStateNotifier& notifier)
    : m_notifier(notifier) {
  DCHECK(isMainThread());
  m_before =
      m_notifier.m_hasOverride ? m_notifier.m_override : m_notifier.m_state;
}

NetworkStateNotifier::ScopedNotifier::~ScopedNotifier() {
  DCHECK(isMainThread());
  const NetworkState& after =
      m_notifier.m_hasOverride ? m_notifier.m_override : m_notifier.m_state;
  if ((after.type != m_before.type ||
       after.maxBandwidthMbps != m_before.maxBandwidthMbps) &&
      m_before.connectionInitialized) {
    m_notifier.notifyObservers(m_notifier.m_connectionObservers,
                               ObserverType::CONNECTION_TYPE, after);
  }
  if (after.onLine != m_before.onLine && m_before.onLineInitialized) {
    m_notifier.notifyObservers(m_notifier.m_onLineStateObservers,
                               ObserverType::ONLINE_STATE, after);
  }
}

void NetworkStateNotifier::setOnLine(bool onLine) {
  DCHECK(isMainThread());
  ScopedNotifier notifier(*this);
  {
    MutexLocker locker(m_mutex);
    m_state.onLineInitialized = true;
    m_state.onLine = onLine;
  }
}

void NetworkStateNotifier::setWebConnection(WebConnectionType type,
                                            double maxBandwidthMbps) {
  DCHECK(isMainThread());
  ScopedNotifier notifier(*this);
  {
    MutexLocker locker(m_mutex);
    m_state.connectionInitialized = true;
    m_state.type = type;
    m_state.maxBandwidthMbps = maxBandwidthMbps;
  }
}

void NetworkStateNotifier::addConnectionObserver(
    NetworkStateObserver* observer,
    PassRefPtr<WebTaskRunner> taskRunner) {
  addObserver(m_connectionObservers, observer, std::move(taskRunner));
}

void NetworkStateNotifier::addOnLineObserver(
    NetworkStateObserver* observer,
    PassRefPtr<WebTaskRunner> taskRunner) {
  addObserver(m_onLineStateObservers, observer, std::move(taskRunner));
}

void NetworkStateNotifier::removeConnectionObserver(
    NetworkStateObserver* observer,
    PassRefPtr<WebTaskRunner> taskRunner) {
  removeObserver(m_connectionObservers, observer, std::move(taskRunner));
}

void NetworkStateNotifier::removeOnLineObserver(
    NetworkStateObserver* observer,
    PassRefPtr<WebTaskRunner> taskRunner) {
  removeObserver(m_onLineStateObservers, observer, std::move(taskRunner));
}

void NetworkStateNotifier::setOverride(bool onLine,
                                       WebConnectionType type,
                                       double maxBandwidthMbps) {
  DCHECK(isMainThread());
  ScopedNotifier notifier(*this);
  {
    MutexLocker locker(m_mutex);
    m_hasOverride = true;
    m_override.onLineInitialized = true;
    m_override.onLine = onLine;
    m_override.connectionInitialized = true;
    m_override.type = type;
    m_override.maxBandwidthMbps = maxBandwidthMbps;
  }
}

void NetworkStateNotifier::clearOverride() {
  DCHECK(isMainThread());
  ScopedNotifier notifier(*this);
  {
    MutexLocker locker(m_mutex);
    m_hasOverride = false;
  }
}

void NetworkStateNotifier::notifyObservers(ObserverListMap& map,
                                           ObserverType type,
                                           const NetworkState& state) {
  DCHECK(isMainThread());
  MutexLocker locker(m_mutex);
  for (const auto& entry : map) {
    RefPtr<WebTaskRunner> taskRunner = entry.key;
    taskRunner->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&NetworkStateNotifier::notifyObserversOnTaskRunner,
                        crossThreadUnretained(this),
                        crossThreadUnretained(&map), type, taskRunner, state));
  }
}

void NetworkStateNotifier::notifyObserversOnTaskRunner(
    ObserverListMap* map,
    ObserverType type,
    PassRefPtr<WebTaskRunner> passTaskRunner,
    const NetworkState& state) {
  RefPtr<WebTaskRunner> taskRunner = passTaskRunner;
  ObserverList* observerList = lockAndFindObserverList(*map, taskRunner);

  // The context could have been removed before the notification task got to
  // run.
  if (!observerList)
    return;

  DCHECK(taskRunner->runsTasksOnCurrentThread());

  observerList->iterating = true;

  for (size_t i = 0; i < observerList->observers.size(); ++i) {
    // Observers removed during iteration are zeroed out, skip them.
    if (!observerList->observers[i])
      continue;
    switch (type) {
      case ObserverType::ONLINE_STATE:
        observerList->observers[i]->onLineStateChange(state.onLine);
        continue;
      case ObserverType::CONNECTION_TYPE:
        observerList->observers[i]->connectionChange(state.type,
                                                     state.maxBandwidthMbps);
        continue;
    }
    NOTREACHED();
  }

  observerList->iterating = false;

  if (!observerList->zeroedObservers.isEmpty())
    collectZeroedObservers(*map, observerList, taskRunner);
}

void NetworkStateNotifier::addObserver(ObserverListMap& map,
                                       NetworkStateObserver* observer,
                                       PassRefPtr<WebTaskRunner> taskRunner) {
  DCHECK(taskRunner->runsTasksOnCurrentThread());
  DCHECK(observer);

  MutexLocker locker(m_mutex);
  ObserverListMap::AddResult result =
      map.insert(std::move(taskRunner), nullptr);
  if (result.isNewEntry)
    result.storedValue->value = WTF::makeUnique<ObserverList>();

  DCHECK(result.storedValue->value->observers.find(observer) == kNotFound);
  result.storedValue->value->observers.push_back(observer);
}

void NetworkStateNotifier::removeObserver(
    ObserverListMap& map,
    NetworkStateObserver* observer,
    PassRefPtr<WebTaskRunner> passTaskRunner) {
  RefPtr<WebTaskRunner> taskRunner = passTaskRunner;
  DCHECK(taskRunner->runsTasksOnCurrentThread());
  DCHECK(observer);

  ObserverList* observerList = lockAndFindObserverList(map, taskRunner);
  if (!observerList)
    return;

  Vector<NetworkStateObserver*>& observers = observerList->observers;
  size_t index = observers.find(observer);
  if (index != kNotFound) {
    observers[index] = 0;
    observerList->zeroedObservers.push_back(index);
  }

  if (!observerList->iterating && !observerList->zeroedObservers.isEmpty())
    collectZeroedObservers(map, observerList, taskRunner);
}

NetworkStateNotifier::ObserverList*
NetworkStateNotifier::lockAndFindObserverList(
    ObserverListMap& map,
    PassRefPtr<WebTaskRunner> taskRunner) {
  MutexLocker locker(m_mutex);
  ObserverListMap::iterator it = map.find(taskRunner);
  return it == map.end() ? nullptr : it->value.get();
}

void NetworkStateNotifier::collectZeroedObservers(
    ObserverListMap& map,
    ObserverList* list,
    PassRefPtr<WebTaskRunner> taskRunner) {
  DCHECK(taskRunner->runsTasksOnCurrentThread());
  DCHECK(!list->iterating);

  // If any observers were removed during the iteration they will have
  // 0 values, clean them up.
  for (size_t i = 0; i < list->zeroedObservers.size(); ++i)
    list->observers.remove(list->zeroedObservers[i]);

  list->zeroedObservers.clear();

  if (list->observers.isEmpty()) {
    MutexLocker locker(m_mutex);
    map.erase(taskRunner);  // deletes list
  }
}

}  // namespace blink
