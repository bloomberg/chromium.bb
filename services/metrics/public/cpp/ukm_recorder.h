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

class DocumentWritePageLoadMetricsObserver;
class FromGWSPageLoadMetricsLogger;
class IOSChromePasswordManagerClient;
class LocalNetworkRequestsPageLoadMetricsObserver;
class MediaEngagementSession;
class PluginInfoHostImpl;
class ServiceWorkerPageLoadMetricsObserver;
class SubresourceFilterMetricsObserver;
class UkmPageLoadMetricsObserver;
class UseCounterPageLoadMetricsObserver;

namespace autofill {
class AutofillMetrics;
class FormStructure;
}  // namespace autofill

namespace assist_ranker {
class BasePredictor;
}

namespace blink {
class AutoplayUmaHelper;
class Document;
}

namespace cc {
class UkmManager;
}

namespace content {
class CrossSiteDocumentResourceHandler;
class WebContentsImpl;
class PluginServiceImpl;
class DownloadUkmHelper;
}  // namespace content

namespace password_manager {
class PasswordManagerMetricsRecorder;
}  // namespace password_manager

namespace payments {
class JourneyLogger;
}

namespace previews {
class PreviewsUKMObserver;
}

namespace metrics {
class UkmRecorderInterface;
}

namespace media {
class MediaMetricsProvider;
class VideoDecodePerfHistory;
class WatchTimeRecorder;
}  // namespace media

namespace translate {
class TranslateRankerImpl;
}

namespace ui {
class LatencyTracker;
}  // namespace ui

namespace ukm {

class DelegatingUkmRecorder;
class UkmEntryBuilder;
class TestRecordingHelper;

namespace internal {
class UkmEntryBuilderBase;
class SourceUrlRecorderWebContentsObserver;
}

// This feature controls whether UkmService should be created.
METRICS_EXPORT extern const base::Feature kUkmFeature;

// Interface for recording UKM
class METRICS_EXPORT UkmRecorder {
 public:
  UkmRecorder();
  virtual ~UkmRecorder();

  // Provides access to a global UkmRecorder instance for recording metrics.
  // This is typically passed to the Record() method of a entry object from
  // ukm_builders.h.
  // Use TestAutoSetUkmRecorder for capturing data written this way in tests.
  static UkmRecorder* Get();

  // Get the new source ID, which is unique for the duration of a browser
  // session.
  static SourceId GetNewSourceID();

 private:
  friend assist_ranker::BasePredictor;
  friend DelegatingUkmRecorder;
  friend DocumentWritePageLoadMetricsObserver;
  friend FromGWSPageLoadMetricsLogger;
  friend IOSChromePasswordManagerClient;
  friend LocalNetworkRequestsPageLoadMetricsObserver;
  friend MediaEngagementSession;
  friend PluginInfoHostImpl;
  friend ServiceWorkerPageLoadMetricsObserver;
  friend SubresourceFilterMetricsObserver;
  friend TestRecordingHelper;
  friend UkmPageLoadMetricsObserver;
  friend UseCounterPageLoadMetricsObserver;
  friend autofill::AutofillMetrics;
  friend autofill::FormStructure;
  friend blink::AutoplayUmaHelper;
  friend blink::Document;
  friend cc::UkmManager;
  friend content::CrossSiteDocumentResourceHandler;
  friend content::DownloadUkmHelper;
  friend content::PluginServiceImpl;
  friend content::WebContentsImpl;
  friend internal::SourceUrlRecorderWebContentsObserver;
  friend internal::UkmEntryBuilderBase;
  friend media::MediaMetricsProvider;
  friend media::VideoDecodePerfHistory;
  friend media::WatchTimeRecorder;
  friend metrics::UkmRecorderInterface;
  friend password_manager::PasswordManagerMetricsRecorder;
  friend payments::JourneyLogger;
  friend previews::PreviewsUKMObserver;
  friend translate::TranslateRankerImpl;
  friend ui::LatencyTracker;
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, AddEntryWithEmptyMetrics);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, EntryBuilderAndSerialization);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest,
                           LogsUploadedOnlyWhenHavingSourcesOrEntries);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, MetricsProviderTest);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, PersistAndPurge);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, WhitelistEntryTest);

  // Associates the SourceId with a URL. Most UKM recording code should prefer
  // to use a shared SourceId that is already associated with a URL, rather
  // than using this API directly. New uses of this API must be auditted to
  // maintain privacy constraints.
  virtual void UpdateSourceURL(SourceId source_id, const GURL& url) = 0;

  // Get a new UkmEntryBuilder object for the specified source ID and event,
  // which can get metrics added to.
  //
  // This API is deprecated, and new code should prefer using the API from
  // ukm_builders.h.
  std::unique_ptr<UkmEntryBuilder> GetEntryBuilder(SourceId source_id,
                                                   const char* event_name);

 private:
  // Add an entry to the UkmEntry list.
  virtual void AddEntry(mojom::UkmEntryPtr entry) = 0;

  DISALLOW_COPY_AND_ASSIGN(UkmRecorder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_
