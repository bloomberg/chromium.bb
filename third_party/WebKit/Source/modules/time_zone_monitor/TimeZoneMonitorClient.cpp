// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/time_zone_monitor/TimeZoneMonitorClient.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "services/device/public/mojom/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Notify V8 that the date/time configuration of the system might have changed.
void NotifyTimezoneChangeToV8(v8::Isolate* isolate) {
  DCHECK(isolate);
  v8::Date::DateTimeConfigurationChangeNotification(isolate);
}

void NotifyTimezoneChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  NotifyTimezoneChangeToV8(ToIsolate(worker_thread->GlobalScope()));
}

}  // namespace

// static
void TimeZoneMonitorClient::Init() {
  DEFINE_STATIC_LOCAL(TimeZoneMonitorClient, instance, ());

  device::mojom::blink::TimeZoneMonitorPtr monitor;
  Platform::Current()->GetConnector()->BindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&monitor));
  device::mojom::blink::TimeZoneMonitorClientPtr client;
  instance.binding_.Bind(mojo::MakeRequest(&client));
  monitor->AddClient(std::move(client));
}

TimeZoneMonitorClient::TimeZoneMonitorClient() : binding_(this) {
  DCHECK(IsMainThread());
}

TimeZoneMonitorClient::~TimeZoneMonitorClient() = default;

void TimeZoneMonitorClient::OnTimeZoneChange(const String& time_zone_info) {
  DCHECK(IsMainThread());

  if (!time_zone_info.IsEmpty()) {
    DCHECK(time_zone_info.ContainsOnlyASCII());
    icu::TimeZone* zone = icu::TimeZone::createTimeZone(
        icu::UnicodeString(time_zone_info.Ascii().data(), -1, US_INV));
    icu::TimeZone::adoptDefault(zone);
    VLOG(1) << "ICU default timezone is set to " << time_zone_info;
  }

  NotifyTimezoneChangeToV8(V8PerIsolateData::MainThreadIsolate());

  HashSet<WorkerThread*>& threads = WorkerThread::WorkerThreads();
  HashSet<WorkerBackingThread*> posted;
  for (WorkerThread* thread : threads) {
    // Ensure every WorkerBackingThread(holding one platform thread) only get
    // the task posted once, because one WorkerBackingThread could be shared
    // among multiple WorkerThreads.
    if (posted.Contains(&thread->GetWorkerBackingThread()))
      continue;
    PostCrossThreadTask(*thread->GetTaskRunner(TaskType::kUnspecedTimer),
                        FROM_HERE,
                        CrossThreadBind(&NotifyTimezoneChangeOnWorkerThread,
                                        WTF::CrossThreadUnretained(thread)));
    posted.insert(&thread->GetWorkerBackingThread());
  }
}

}  // namespace blink
