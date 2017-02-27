// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/time_zone_monitor/TimeZoneMonitorClient.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/ServiceConnector.h"
#include "public/platform/Platform.h"
#include "services/device/public/interfaces/constants.mojom-blink.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Notify V8 that the date/time configuration of the system might have changed.
void NotifyTimezoneChangeToV8(v8::Isolate* isolate) {
  DCHECK(isolate);
  v8::Date::DateTimeConfigurationChangeNotification(isolate);
}

void NotifyTimezoneChangeOnWorkerThread(WorkerThread* workerThread) {
  DCHECK(workerThread->isCurrentThread());
  NotifyTimezoneChangeToV8(toIsolate(workerThread->globalScope()));
}

}  // namespace

// static
void TimeZoneMonitorClient::Init() {
  DEFINE_STATIC_LOCAL(TimeZoneMonitorClient, instance, ());

  device::mojom::blink::TimeZoneMonitorPtr monitor;
  ServiceConnector::instance().connectToInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&monitor));
  monitor->AddClient(instance.m_binding.CreateInterfacePtrAndBind());
}

TimeZoneMonitorClient::TimeZoneMonitorClient() : m_binding(this) {
  DCHECK(isMainThread());
}

TimeZoneMonitorClient::~TimeZoneMonitorClient() {}

void TimeZoneMonitorClient::OnTimeZoneChange(const String& timeZoneInfo) {
  DCHECK(isMainThread());

  if (!timeZoneInfo.isEmpty()) {
    icu::TimeZone* zone = icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(timeZoneInfo.utf8().data()));
    icu::TimeZone::adoptDefault(zone);
    VLOG(1) << "ICU default timezone is set to " << timeZoneInfo;
  }

  NotifyTimezoneChangeToV8(V8PerIsolateData::mainThreadIsolate());

  HashSet<WorkerThread*>& threads = WorkerThread::workerThreads();
  HashSet<WorkerBackingThread*> posted;
  for (WorkerThread* thread : threads) {
    // Ensure every WorkerBackingThread(holding one platform thread) only get
    // the task posted once, because one WorkerBackingThread could be shared
    // among multiple WorkerThreads.
    if (posted.contains(&thread->workerBackingThread()))
      continue;
    thread->postTask(BLINK_FROM_HERE,
                     crossThreadBind(&NotifyTimezoneChangeOnWorkerThread,
                                     WTF::crossThreadUnretained(thread)));
    posted.insert(&thread->workerBackingThread());
  }
}

}  // namespace blink
