// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CONTENT_DATA_USE_MEASUREMENT_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CONTENT_DATA_USE_MEASUREMENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/metrics/data_use_tracker.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

class GURL;

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace data_use_measurement {

class DataUseAscriber;
class URLRequestClassifier;

// Records the data use of user traffic and various services in UMA histograms.
// The UMA is broken down by network technology used (Wi-Fi vs cellular). On
// Android, the UMA is further broken down by whether the application was in the
// background or foreground during the request.
// TODO(amohammadkhan): Complete the layered architecture.
// http://crbug.com/527460
class DataUseMeasurement {
 public:
  // Returns true if the |request| is initiated by user traffic.
  static bool IsUserRequest(const net::URLRequest& request);

  // Returns the content-type saved in the request userdata when the response
  // headers were received.
  static DataUseUserData::DataUseContentType GetContentTypeForRequest(
      const net::URLRequest& request);

  DataUseMeasurement(
      std::unique_ptr<URLRequestClassifier> url_request_classifier,
      const metrics::UpdateUsagePrefCallbackType& metrics_data_use_forwarder,
      DataUseAscriber* ascriber);
  ~DataUseMeasurement();

  // Called before a request is sent.
  void OnBeforeURLRequest(net::URLRequest* request);

  // Called right after a redirect response code was received for |request|.
  void OnBeforeRedirect(const net::URLRequest& request,
                        const GURL& new_location);

  // Called when response headers are received for |request|.
  void OnHeadersReceived(net::URLRequest* request,
                         const net::HttpResponseHeaders* response_headers);

  // Called when data is received or sent on the network, respectively.
  void OnNetworkBytesReceived(const net::URLRequest& request,
                              int64_t bytes_received);
  void OnNetworkBytesSent(const net::URLRequest& request, int64_t bytes_sent);

  // Indicates that |request| has been completed or failed.
  void OnCompleted(const net::URLRequest& request, bool started);

#if defined(OS_ANDROID)
  // This function should just be used for testing purposes. A change in
  // application state can be simulated by calling this function.
  void OnApplicationStateChangeForTesting(
      base::android::ApplicationState application_state);
#endif

 private:
  friend class DataUseMeasurementTest;
  FRIEND_TEST_ALL_PREFIXES(DataUseMeasurementTest,
                           TimeOfBackgroundDownstreamBytes);

  // Specifies that data is received or sent, respectively.
  enum TrafficDirection { DOWNSTREAM, UPSTREAM };

  // Returns the current application state (Foreground or Background). It always
  // returns Foreground if Chrome is not running on Android.
  DataUseUserData::AppState CurrentAppState() const;

  // Makes the full name of the histogram. It is made from |prefix| and suffix
  // which is made based on network and application status. suffix is a string
  // representing whether the data use was on the send ("Upstream") or receive
  // ("Downstream") path, and whether the app was in the "Foreground" or
  // "Background".
  std::string GetHistogramNameWithConnectionType(
      const char* prefix,
      TrafficDirection dir,
      DataUseUserData::AppState app_state) const;

  // Makes the full name of the histogram. It is made from |prefix| and suffix
  // which is made based on network and application status. suffix is a string
  // representing whether the data use was on the send ("Upstream") or receive
  // ("Downstream") path, whether the app was in the "Foreground" or
  // "Background", and whether a "Cellular" or "WiFi" network was use. For
  // example, "Prefix.Upstream.Foreground.Cellular" is a possible output.
  // |app_state| indicates the app state which can be foreground, background, or
  // unknown.
  std::string GetHistogramName(const char* prefix,
                               TrafficDirection dir,
                               DataUseUserData::AppState app_state,
                               bool is_connection_cellular) const;

#if defined(OS_ANDROID)
  // Called whenever the application transitions from foreground to background
  // and vice versa.
  void OnApplicationStateChange(
      base::android::ApplicationState application_state);

  // Records the count of bytes received and sent by Chrome on the network as
  // reported by the operating system.
  void MaybeRecordNetworkBytesOS();
#endif

  // Records the data use of the |request|, thus |request| must be non-null.
  // |dir| is the direction (which is upstream or downstream) and |bytes| is the
  // number of bytes in the direction.
  void ReportDataUseUMA(const net::URLRequest& request,
                        TrafficDirection dir,
                        int64_t bytes);

  // Updates the data use of the |request|, thus |request| must be non-null.
  void UpdateDataUsePrefs(const net::URLRequest& request) const;

  // Reports the message size of the service requests.
  void ReportServicesMessageSizeUMA(const net::URLRequest& request);

  // Records data use histograms of services. It gets the size of exchanged
  // message, its direction (which is upstream or downstream) and reports to the
  // histogram DataUse.Services.{Dimensions} with, services as the buckets.
  // |app_state| indicates the app state which can be foreground, or background.
  void ReportDataUsageServices(int32_t traffic_annotation_hash,
                               TrafficDirection dir,
                               DataUseUserData::AppState app_state,
                               int64_t message_size) const;

  // Records data use histograms split on TrafficDirection, AppState and
  // TabState.
  void RecordTabStateHistogram(TrafficDirection dir,
                               DataUseUserData::AppState app_state,
                               bool is_tab_visible,
                               int64_t bytes) const;

  // Records data use histograms split on page tranition.
  void RecordPageTransitionUMA(const net::URLRequest& request) const;

  // Records data use histograms of user traffic and services traffic split on
  // content type, AppState and TabState.
  void RecordContentTypeHistogram(
      DataUseUserData::DataUseContentType content_type,
      bool is_user_traffic,
      DataUseUserData::AppState app_state,
      bool is_tab_visible,
      int64_t bytes);

  // Classifier for identifying if an URL request is user initiated.
  std::unique_ptr<URLRequestClassifier> url_request_classifier_;

  // Callback for updating metrics about data use of user traffic and services.
  // TODO(rajendrant): Change this to not report data use service name. Instead
  // pass a bool to distinguish if the data use is for metrics services
  // (UMA, UKM).
  metrics::UpdateUsagePrefCallbackType metrics_data_use_forwarder_;

  // DataUseAscriber used to get the attributes of data use.
  DataUseAscriber* ascriber_;

#if defined(OS_ANDROID)
  // Application listener store the last known state of the application in this
  // field.
  base::android::ApplicationState app_state_;

  // ApplicationStatusListener used to monitor whether the application is in the
  // foreground or in the background. It is owned by DataUseMeasurement.
  std::unique_ptr<base::android::ApplicationStatusListener> app_listener_;

  // Number of bytes received and sent by Chromium as reported by the operating
  // system when it was last queried for traffic statistics. Set to 0 if the
  // operating system was never queried.
  int64_t rx_bytes_os_;
  int64_t tx_bytes_os_;

  // Number of bytes received and sent by Chromium as reported by the network
  // delegate since the operating system was last queried for traffic
  // statistics.
  int64_t bytes_transferred_since_last_traffic_stats_query_;

  // The time at which Chromium app state changed to background. Can be null if
  // app is not in background.
  base::TimeTicks last_app_background_time_;

  // True if app is in background and first network read has not yet happened.
  bool no_reads_since_background_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurement);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CONTENT_DATA_USE_MEASUREMENT_H_
