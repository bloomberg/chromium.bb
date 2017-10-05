// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_

#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"
#include "url/gurl.h"

class ContextualSearchRankerLoggerImpl;
class DocumentWritePageLoadMetricsObserver;
class FromGWSPageLoadMetricsLogger;
class PluginInfoMessageFilter;
class ServiceWorkerPageLoadMetricsObserver;
class SubresourceFilterMetricsObserver;
class UkmPageLoadMetricsObserver;
class LocalNetworkRequestsPageLoadMetricsObserver;

namespace content {
class RenderWidgetHostLatencyTracker;
}  // namespace content

namespace password_manager {
class PasswordManagerMetricsRecorder;
}  // namespace password_manager

namespace previews {
class PreviewsUKMObserver;
}

namespace ukm {

class UkmEntryBuilder;
class UkmInterface;
class TestRecordingHelper;

namespace internal {
class UkmEntryBuilderBase;
}

// This feature controls whether UkmService should be created.
METRICS_EXPORT extern const base::Feature kUkmFeature;

// Interface for recording UKM
class METRICS_EXPORT UkmRecorder {
 public:
  UkmRecorder();
  virtual ~UkmRecorder();

  // Sets an instance of UkmRecorder to provided by Get().
  // TODO(holte): Migrate callers away from using Get, to using a context
  // specific getter, or a MojoUkmRecorder.
  static void Set(UkmRecorder* recorder);

  // Provides access to a previously constructed UkmRecorder instance. Only one
  // instance exists per process and must have been constructed prior to any
  // calls to this method.
  static UkmRecorder* Get();

  // Get the new source ID, which is unique for the duration of a browser
  // session.
  static SourceId GetNewSourceID();

  // Update the URL on the source keyed to the given source ID. If the source
  // does not exist, it will create a new UkmSource object.
  virtual void UpdateSourceURL(SourceId source_id, const GURL& url) = 0;

 private:
  friend ContextualSearchRankerLoggerImpl;
  friend PluginInfoMessageFilter;
  friend UkmPageLoadMetricsObserver;
  friend LocalNetworkRequestsPageLoadMetricsObserver;
  friend DocumentWritePageLoadMetricsObserver;
  friend FromGWSPageLoadMetricsLogger;
  friend ServiceWorkerPageLoadMetricsObserver;
  friend SubresourceFilterMetricsObserver;
  friend TestRecordingHelper;
  friend UkmInterface;
  friend content::RenderWidgetHostLatencyTracker;
  friend password_manager::PasswordManagerMetricsRecorder;
  friend previews::PreviewsUKMObserver;
  friend internal::UkmEntryBuilderBase;
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, AddEntryWithEmptyMetrics);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, EntryBuilderAndSerialization);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest,
                           LogsUploadedOnlyWhenHavingSourcesOrEntries);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, MetricsProviderTest);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, PersistAndPurge);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, WhitelistEntryTest);

  // Get a new UkmEntryBuilder object for the specified source ID and event,
  // which can get metrics added to.
  //
  // This API being private is intentional. Any client using UKM needs to
  // declare itself to be a friend of UkmService and go through code review
  // process.
  std::unique_ptr<UkmEntryBuilder> GetEntryBuilder(SourceId source_id,
                                                   const char* event_name);

 private:
  // Add an entry to the UkmEntry list.
  virtual void AddEntry(mojom::UkmEntryPtr entry) = 0;

  DISALLOW_COPY_AND_ASSIGN(UkmRecorder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_
