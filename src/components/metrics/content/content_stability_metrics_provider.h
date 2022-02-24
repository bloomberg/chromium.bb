// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefService;

namespace metrics {

class ExtensionsHelper;

// ContentStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class ContentStabilityMetricsProvider
    : public MetricsProvider,
      public content::BrowserChildProcessObserver,
#if BUILDFLAG(IS_ANDROID)
      public crash_reporter::CrashMetricsReporter::Observer,
#endif
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  // |extensions_helper| is used to determine if a process corresponds to an
  // extension and is optional. If an ExtensionsHelper is not supplied it is
  // assumed the process does not correspond to an extension.
  ContentStabilityMetricsProvider(
      PrefService* local_state,
      std::unique_ptr<ExtensionsHelper> extensions_helper);
  ContentStabilityMetricsProvider(const ContentStabilityMetricsProvider&) =
      delete;
  ContentStabilityMetricsProvider& operator=(
      const ContentStabilityMetricsProvider&) = delete;
  ~ContentStabilityMetricsProvider() override;

  // MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

  static void SetupWebContentsObserver(content::WebContents* web_contents);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           BrowserChildProcessObserverGpu);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           BrowserChildProcessObserverUtility);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           NotificationObserver);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           ExtensionsNotificationObserver);

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessLaunchFailed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  void DidStartLoading();
  void OnRendererUnresponsive();

#if BUILDFLAG(IS_ANDROID)
  // crash_reporter::CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override;

  base::ScopedObservation<crash_reporter::CrashMetricsReporter,
                          crash_reporter::CrashMetricsReporter::Observer>
      scoped_observation_{this};
#endif  // BUILDFLAG(IS_ANDROID)

  StabilityMetricsHelper helper_;

  std::unique_ptr<ExtensionsHelper> extensions_helper_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      scoped_observations_{this};

  class WebContentsObserverImpl
      : public content::WebContentsObserver,
        public content::WebContentsUserData<WebContentsObserverImpl> {
   public:
    explicit WebContentsObserverImpl(content::WebContents* web_contents);

    void DidStartLoading() override;
    void OnRendererUnresponsive(content::RenderProcessHost* host) override;

    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_
