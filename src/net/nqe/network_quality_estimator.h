// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_ESTIMATOR_H_
#define NET_NQE_NETWORK_QUALITY_ESTIMATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/event_creator.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_observation.h"
#include "net/nqe/network_quality_observation_source.h"
#include "net/nqe/network_quality_provider.h"
#include "net/nqe/network_quality_store.h"
#include "net/nqe/observation_buffer.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"
#include "net/nqe/socket_watcher_factory.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class NetLog;

namespace nqe {
namespace internal {
class ThroughputAnalyzer;
}
}

class URLRequest;

// NetworkQualityEstimator provides network quality estimates (quality of the
// full paths to all origins that have been connected to).
// The estimates are based on the observed organic traffic.
// A NetworkQualityEstimator instance is attached to URLRequestContexts and
// observes the traffic of URLRequests spawned from the URLRequestContexts.
// A single instance of NQE can be attached to multiple URLRequestContexts,
// thereby increasing the single NQE instance's accuracy by providing more
// observed traffic characteristics.
class NET_EXPORT NetworkQualityEstimator
    : public NetworkChangeNotifier::ConnectionTypeObserver,
      public NetworkQualityProvider {
 public:
  // Observes measurements of round trip time.
  class NET_EXPORT_PRIVATE RTTObserver {
   public:
    // Will be called when a new RTT observation is available. The round trip
    // time is specified in milliseconds. The time when the observation was
    // taken and the source of the observation are provided.
    virtual void OnRTTObservation(int32_t rtt_ms,
                                  const base::TimeTicks& timestamp,
                                  NetworkQualityObservationSource source) = 0;

   protected:
    RTTObserver() {}
    virtual ~RTTObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(RTTObserver);
  };

  // Observes measurements of throughput.
  class NET_EXPORT_PRIVATE ThroughputObserver {
   public:
    // Will be called when a new throughput observation is available.
    // Throughput is specified in kilobits per second.
    virtual void OnThroughputObservation(
        int32_t throughput_kbps,
        const base::TimeTicks& timestamp,
        NetworkQualityObservationSource source) = 0;

   protected:
    ThroughputObserver() {}
    virtual ~ThroughputObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ThroughputObserver);
  };

  // Creates a new NetworkQualityEstimator.
  // |params| contains the
  // configuration parameters relevant to network quality estimator. The caller
  // must guarantee that |net_log| outlives |this|.
  NetworkQualityEstimator(
      std::unique_ptr<NetworkQualityEstimatorParams> params,
      NetLog* net_log);

  ~NetworkQualityEstimator() override;

  // Returns the effective type of the current connection based on only the
  // samples observed after |start_time|. This should only be used for
  // recording the metrics. Virtualized for testing.
  virtual EffectiveConnectionType GetRecentEffectiveConnectionType(
      const base::TimeTicks& start_time) const;

  // NetworkQualityProvider implementation:
  // Must be called on the IO thread.
  EffectiveConnectionType GetEffectiveConnectionType() const override;
  void AddEffectiveConnectionTypeObserver(
      EffectiveConnectionTypeObserver* observer) override;
  void RemoveEffectiveConnectionTypeObserver(
      EffectiveConnectionTypeObserver* observer) override;
  base::Optional<base::TimeDelta> GetHttpRTT() const override;
  base::Optional<base::TimeDelta> GetTransportRTT() const override;
  base::Optional<int32_t> GetDownstreamThroughputKbps() const override;
  base::Optional<int32_t> GetBandwidthDelayProductKbits() const override;

  // Adds |observer| to the list of RTT and throughput estimate observers.
  // The observer must register and unregister itself on the same thread.
  // |observer| would be notified on the thread on which it registered.
  // |observer| would be notified of the current values in the next message
  // pump.
  void AddRTTAndThroughputEstimatesObserver(
      RTTAndThroughputEstimatesObserver* observer);

  // Removes |observer| from the list of RTT and throughput estimate
  // observers.
  void RemoveRTTAndThroughputEstimatesObserver(
      RTTAndThroughputEstimatesObserver* observer);

  // Notifies NetworkQualityEstimator that the response header of |request| has
  // been received.
  void NotifyHeadersReceived(const URLRequest& request);

  // Notifies NetworkQualityEstimator that unfiltered bytes have been read for
  // |request|.
  void NotifyBytesRead(const URLRequest& request);

  // Notifies NetworkQualityEstimator that the headers of |request| are about to
  // be sent.
  void NotifyStartTransaction(const URLRequest& request);

  // Notifies NetworkQualityEstimator that the response body of |request| has
  // been received.
  void NotifyRequestCompleted(const URLRequest& request, int net_error);

  // Notifies NetworkQualityEstimator that |request| will be destroyed.
  void NotifyURLRequestDestroyed(const URLRequest& request);

  // Adds |rtt_observer| to the list of round trip time observers. Must be
  // called on the IO thread.
  void AddRTTObserver(RTTObserver* rtt_observer);

  // Removes |rtt_observer| from the list of round trip time observers if it
  // is on the list of observers. Must be called on the IO thread.
  void RemoveRTTObserver(RTTObserver* rtt_observer);

  // Adds |throughput_observer| to the list of throughput observers. Must be
  // called on the IO thread.
  void AddThroughputObserver(ThroughputObserver* throughput_observer);

  // Removes |throughput_observer| from the list of throughput observers if it
  // is on the list of observers. Must be called on the IO thread.
  void RemoveThroughputObserver(ThroughputObserver* throughput_observer);

  SocketPerformanceWatcherFactory* GetSocketPerformanceWatcherFactory();

  // |use_localhost_requests| should only be true when testing against local
  // HTTP server and allows the requests to local host to be used for network
  // quality estimation.
  void SetUseLocalHostRequestsForTesting(bool use_localhost_requests);

  // |use_small_responses| should only be true when testing.
  // Allows the responses smaller than |kMinTransferSizeInBits| to be used for
  // network quality estimation.
  void SetUseSmallResponsesForTesting(bool use_small_responses);

  // If |disable_offline_check| is set to true, then the device offline check is
  // disabled when computing the effective connection type or when writing the
  // prefs.
  void DisableOfflineCheckForTesting(bool disable_offline_check);

  // Reports |effective_connection_type| to all
  // EffectiveConnectionTypeObservers.
  void ReportEffectiveConnectionTypeForTesting(
      EffectiveConnectionType effective_connection_type);

  // Reports the RTTs and throughput to all RTTAndThroughputEstimatesObservers.
  void ReportRTTsAndThroughputForTesting(base::TimeDelta http_rtt,
                                         base::TimeDelta transport_rtt,
                                         int32_t downstream_throughput_kbps);

  // Adds and removes |observer| from the list of cache observers.
  void AddNetworkQualitiesCacheObserver(
      nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver*
          observer);
  void RemoveNetworkQualitiesCacheObserver(
      nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver*
          observer);

  // Called when the persistent prefs have been read. |read_prefs| contains the
  // parsed prefs as a map between NetworkIDs and CachedNetworkQualities.
  void OnPrefsRead(
      const std::map<nqe::internal::NetworkID,
                     nqe::internal::CachedNetworkQuality> read_prefs);

  const NetworkQualityEstimatorParams* params() { return params_.get(); }

#if defined(OS_CHROMEOS)
  // Enables getting the network id asynchronously when
  // GatherEstimatesForNextConnectionType(). This should always be called in
  // production, because getting the network id involves a blocking call to
  // recv() in AddressTrackerLinux, and the IO thread should never be blocked.
  // TODO(https://crbug.com/821607): Remove after the bug is resolved.
  void EnableGetNetworkIdAsynchronously();
#endif  // defined(OS_CHROMEOS)

  // Forces the effective connection type to be recomputed as |type|. Once
  // called, effective connection type would always be computed as |type|.
  // Calling this also notifies all the observers of the effective connection
  // type as |type|.
  void SimulateNetworkQualityChangeForTesting(
      net::EffectiveConnectionType type);

  typedef nqe::internal::Observation Observation;
  typedef nqe::internal::ObservationBuffer ObservationBuffer;

 protected:
  // NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // Returns true if median RTT across all samples that belong to
  // |observation_category| is available and sets |rtt| to the median of RTT
  // observations since |start_time|. Virtualized for testing. |rtt| should not
  // be null. If |observations_count| is not null, then it is set to the number
  // of RTT observations that were used for computing the RTT estimate.
  virtual bool GetRecentRTT(
      nqe::internal::ObservationCategory observation_category,
      const base::TimeTicks& start_time,
      base::TimeDelta* rtt,
      size_t* observations_count) const WARN_UNUSED_RESULT;

  // Returns true if median downstream throughput is available and sets |kbps|
  // to the median of downstream throughput (in kilobits per second)
  // observations since |start_time|. Virtualized for testing. |kbps|
  // should not be null. Virtualized for testing.
  // TODO(tbansal): Change it to return throughput as int32.
  virtual bool GetRecentDownlinkThroughputKbps(
      const base::TimeTicks& start_time,
      int32_t* kbps) const WARN_UNUSED_RESULT;

  // Returns the list of intervals at which the accuracy of network quality
  // prediction should be recorded. Virtualized for testing.
  virtual const std::vector<base::TimeDelta>& GetAccuracyRecordingIntervals()
      const;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Returns the effective type of the current connection based on only the
  // observations received after |start_time|. |http_rtt|, |transport_rtt| and
  // |downstream_throughput_kbps| must be non-null. |http_rtt|, |transport_rtt|
  // and |downstream_throughput_kbps| are set to the expected HTTP RTT,
  // transport RTT and downstream throughput (in kilobits per second) based on
  // observations taken since |start_time|. Virtualized for testing.
  // If |transport_rtt_observation_count| is not null, then it is set to the
  // number of transport RTT observations that are available when computing the
  // effective connection type.
  virtual EffectiveConnectionType
  GetRecentEffectiveConnectionTypeAndNetworkQuality(
      const base::TimeTicks& start_time,
      base::TimeDelta* http_rtt,
      base::TimeDelta* transport_rtt,
      base::TimeDelta* end_to_end_rtt,
      int32_t* downstream_throughput_kbps,
      size_t* transport_rtt_observation_count,
      size_t* end_to_end_rtt_observation_count) const;

  // Notifies |this| of a new transport layer RTT. Called by socket watchers.
  // Protected for testing.
  void OnUpdatedTransportRTTAvailable(
      SocketPerformanceWatcherFactory::Protocol protocol,
      const base::TimeDelta& rtt,
      const base::Optional<nqe::internal::IPHash>& host);

  // Returns an estimate of network quality at the specified |percentile|.
  // Only the observations later than |start_time| are taken into account.
  // |percentile| must be between 0 and 100 (both inclusive) with higher
  // percentiles indicating less performant networks. For example, if
  // |percentile| is 90, then the network is expected to be faster than the
  // returned estimate with 0.9 probability. Similarly, network is expected to
  // be slower than the returned estimate with 0.1 probability.
  // Virtualized for testing.
  // |observation_category| is the category of observations which should be used
  // for computing the RTT estimate.
  // If |observations_count| is not null, then it is set to the number of RTT
  // observations that were available when computing the RTT estimate.
  virtual base::TimeDelta GetRTTEstimateInternal(
      base::TimeTicks start_time,
      nqe::internal::ObservationCategory observation_category,
      int percentile,
      size_t* observations_count) const;
  int32_t GetDownlinkThroughputKbpsEstimateInternal(
      const base::TimeTicks& start_time,
      int percentile) const;

  // Notifies the observers of RTT or throughput estimates computation.
  virtual void NotifyObserversOfRTTOrThroughputComputed() const;

  // Notifies |observer| of the current RTT and throughput if |observer| is
  // still registered as an observer.
  virtual void NotifyRTTAndThroughputEstimatesObserverIfPresent(
      RTTAndThroughputEstimatesObserver* observer) const;

  // Adds |observation| to the buffer of RTT observations, and notifies RTT
  // observers of |observation|. May also trigger recomputation of effective
  // connection type.
  void AddAndNotifyObserversOfRTT(const Observation& observation);

  // Adds |observation| to the buffer of throughput observations, and notifies
  // throughput observers of |observation|. May also trigger recomputation of
  // effective connection type.
  void AddAndNotifyObserversOfThroughput(const Observation& observation);

  // Returns true if the request with observed HTTP of |observed_http_rtt| is
  // expected to be a hanging request. The decision is made by comparing
  // |observed_http_rtt| with the expected HTTP and transport RTT.
  bool IsHangingRequest(base::TimeDelta observed_http_rtt) const;

  base::Optional<int32_t> ComputeIncreaseInTransportRTTForTests() {
    return ComputeIncreaseInTransportRTT();
  }

  // Returns the current network signal strength by querying the platform APIs.
  // Set to INT32_MIN when the value is unavailable. Otherwise, must be between
  // 0 and 4 (both inclusive). This may take into account many different radio
  // technology inputs. 0 represents very poor signal strength while 4
  // represents a very strong signal strength. The range is capped between 0 and
  // 4 to ensure that a change in the value indicates a non-negligible change in
  // the signal quality.
  virtual int32_t GetCurrentSignalStrength() const;

  // Forces computation of effective connection type, and notifies observers
  // if there is a change in its value.
  void ComputeEffectiveConnectionType();

  // Observer list for RTT or throughput estimates. Protected for testing.
  base::ObserverList<RTTAndThroughputEstimatesObserver>::Unchecked
      rtt_and_throughput_estimates_observer_list_;

  // Observer list for changes in effective connection type.
  base::ObserverList<EffectiveConnectionTypeObserver>::Unchecked
      effective_connection_type_observer_list_;

  // Params to configure the network quality estimator.
  const std::unique_ptr<NetworkQualityEstimatorParams> params_;

  // Number of end to end RTT samples available when the ECT was last computed.
  size_t end_to_end_rtt_observation_count_at_last_ect_computation_;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           AdaptiveRecomputationEffectiveConnectionType);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, StoreObservations);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestAddObservation);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           DefaultObservationsOverridden);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, ComputedPercentiles);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestGetMetricsSince);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           UnknownEffectiveConnectionType);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TypicalNetworkQualities);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           OnPrefsReadWithReadingDisabled);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           ForceEffectiveConnectionTypeThroughFieldTrial);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestBDPComputation);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TestComputeIncreaseInTransportRTTFullHostsOverlap);
  FRIEND_TEST_ALL_PREFIXES(
      NetworkQualityEstimatorTest,
      TestComputeIncreaseInTransportRTTPartialHostsOverlap);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           ObservationDiscardedIfCachedEstimateAvailable);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TestRttThroughputObservers);

  // Defines how a metric (e.g, transport RTT) should be used when computing
  // the effective connection type.
  enum class MetricUsage {
    // The metric should not be used when computing the effective connection
    // type.
    DO_NOT_USE = 0,
    // If the metric is available, then it should be used when computing the
    // effective connection type.
    USE_IF_AVAILABLE,
    // The metric is required when computing the effective connection type.
    // If the value of the metric is unavailable, effective connection type
    // should be set to |EFFECTIVE_CONNECTION_TYPE_UNKNOWN|.
    MUST_BE_USED,
  };

  // Returns the RTT value to be used when the valid RTT is unavailable. Readers
  // should discard RTT if it is set to the value returned by |InvalidRTT()|.
  static const base::TimeDelta InvalidRTT();

  // Records UMA on whether the NetworkID was available or not. Called right
  // after a network change event.
  void RecordNetworkIDAvailability() const;

  // Records UMA on main frame requests.
  void RecordMetricsOnMainFrameRequest() const;

  // Records a downstream throughput observation to the observation buffer if
  // a valid observation is available. |downstream_kbps| is the downstream
  // throughput in kilobits per second.
  void OnNewThroughputObservationAvailable(int32_t downstream_kbps);

  // Adds the default median RTT and downstream throughput estimate for the
  // current connection type to the observation buffer.
  void AddDefaultEstimates();

  // Returns the current network ID checking by calling the platform APIs.
  // Virtualized for testing.
  virtual nqe::internal::NetworkID GetCurrentNetworkID() const;

  // Returns true only if the |request| can be used for RTT estimation.
  bool RequestProvidesRTTObservation(const URLRequest& request) const;

  // Returns true if ECT should be recomputed.
  bool ShouldComputeEffectiveConnectionType() const;

  // Calls ShouldComputeEffectiveConnectionType() to determine if ECT needs to
  // be computed. If so, it recomputes effective connection type.
  void MaybeComputeEffectiveConnectionType();

  // Notifies observers of a change in effective connection type.
  void NotifyObserversOfEffectiveConnectionTypeChanged();

  // Notifies |observer| of the current effective connection type if |observer|
  // is still registered as an observer.
  void NotifyEffectiveConnectionTypeObserverIfPresent(
      EffectiveConnectionTypeObserver* observer) const;

  // Records NQE accuracy metrics. |measuring_duration| should belong to the
  // vector returned by AccuracyRecordingIntervals().
  // RecordAccuracyAfterMainFrame should be called |measuring_duration| after a
  // main frame request is observed.
  void RecordAccuracyAfterMainFrame(base::TimeDelta measuring_duration) const;

  // Obtains the current cellular signal strength value and updates
  // |min_signal_strength_since_connection_change_| and
  // |max_signal_strength_since_connection_change_|.
  void UpdateSignalStrength();

  // Returns the effective type of the current connection based on only the
  // samples observed after |start_time|. May use HTTP RTT, transport RTT and
  // downstream throughput to compute the effective connection type based on
  // |http_rtt_metric|, |transport_rtt_metric| and
  // |downstream_throughput_kbps_metric|, respectively. |http_rtt|,
  // |transport_rtt| and |downstream_throughput_kbps| must be non-null.
  // |http_rtt|, |transport_rtt| and |downstream_throughput_kbps| are
  // set to the expected HTTP RTT, transport RTT and downstream throughput (in
  // kilobits per second) based on observations taken since |start_time|.
  // If |transport_rtt_observation_count| is not null, then it is set to the
  // number of transport RTT observations that were available when computing the
  // effective connection type.
  EffectiveConnectionType GetRecentEffectiveConnectionTypeUsingMetrics(
      const base::TimeTicks& start_time,
      MetricUsage http_rtt_metric,
      MetricUsage transport_rtt_metric,
      MetricUsage downstream_throughput_kbps_metric,
      base::TimeDelta* http_rtt,
      base::TimeDelta* transport_rtt,
      base::TimeDelta* end_to_end_rtt,
      int32_t* downstream_throughput_kbps,
      size_t* transport_rtt_observation_count,
      size_t* end_to_end_rtt_observation_count) const;

  // Returns true if the cached network quality estimate was successfully read.
  bool ReadCachedNetworkQualityEstimate();

  // Computes the bandwidth delay product in kilobits. The computed value is
  // stored in |bandwidth_delay_product_kbits_| and can be accessed using
  // |GetBandwidthDelayProductKbits|.
  void ComputeBandwidthDelayProduct();

  // Computes the current increase in transport RTT in milliseconds over the
  // baseline transport RTT due to congestion. This value can be interpreted as
  // the additional delay caused due to an increase in queue length in the last
  // mile. The baseline is computed using the transport RTT observations in the
  // past 60 seconds. The current RTT is computed using the observations in the
  // past 5 seconds. Returns an empty optional when there was no recent data.
  base::Optional<int32_t> ComputeIncreaseInTransportRTT() const;

  // Periodically updates |increase_in_transport_rtt_| by posting delayed tasks.
  void IncreaseInTransportRTTUpdater();

  // Gathers metrics for the next connection type. Called when there is a change
  // in the connection type.
  void GatherEstimatesForNextConnectionType();

  // Invoked to continue GatherEstimatesForNextConnectionType work after getting
  // network id. If |get_network_id_asynchronously_| is set, the network id is
  // fetched on a worker thread. Otherwise, GatherEstimatesForNextConnectionType
  // calls this directly. This is a workaround for https://crbug.com/821607
  // where net::GetWifiSSID() call gets stuck.
  void ContinueGatherEstimatesForNextConnectionType(
      const nqe::internal::NetworkID& network_id);

  // Updates the value of |cached_estimate_applied_| if |observation| is
  // computed from a cached estimate. |buffer| is the observation buffer to
  // which the cached estimate is being added to.
  void MaybeUpdateCachedEstimateApplied(const Observation& observation,
                                        ObservationBuffer* buffer);

  // Returns true if |observation| should be added to the observation buffer.
  bool ShouldAddObservation(const Observation& observation) const;

  // Returns true if the socket watcher can run the callback to notify the RTT
  // observations.
  bool ShouldSocketWatcherNotifyRTT(base::TimeTicks now);

  // Determines if the requests to local host can be used in estimating the
  // network quality. Set to true only for tests.
  bool use_localhost_requests_;

  // When set to true, the device offline check is disabled when computing the
  // effective connection type or when writing the prefs. Set to true only for
  // testing.
  bool disable_offline_check_;

  // Tick clock used by the network quality estimator.
  const base::TickClock* tick_clock_;

  // Intervals after the main frame request arrives at which accuracy of network
  // quality prediction is recorded.
  std::vector<base::TimeDelta> accuracy_recording_intervals_;

  // Time when last connection change was observed.
  base::TimeTicks last_connection_change_;

  // ID of the current network.
  nqe::internal::NetworkID current_network_id_;

  // Buffer that holds throughput observations from the HTTP layer (in kilobits
  // per second) sorted by timestamp.
  ObservationBuffer http_downstream_throughput_kbps_observations_;

  // Buffer that holds RTT observations with different observation categories.
  // The entries in |rtt_ms_observations_| are in the same order as the
  // entries in the nqe::internal:ObservationCategory enum.  Size of
  // |rtt_ms_observations_| is nqe::internal::OBSERVATION_CATEGORY_COUNT.
  // Each observation buffer in |rtt_ms_observations_| stores RTT observations
  // in milliseconds. Within a buffer, the observations are sorted by timestamp.
  std::vector<ObservationBuffer> rtt_ms_observations_;

  // Time when the transaction for the last main frame request was started.
  base::TimeTicks last_main_frame_request_;

  // Estimated network quality when the transaction for the last main frame
  // request was started.
  nqe::internal::NetworkQuality estimated_quality_at_last_main_frame_;
  EffectiveConnectionType effective_connection_type_at_last_main_frame_;

  // Observer lists for round trip times and throughput measurements.
  base::ObserverList<RTTObserver>::Unchecked rtt_observer_list_;
  base::ObserverList<ThroughputObserver>::Unchecked throughput_observer_list_;

  std::unique_ptr<nqe::internal::SocketWatcherFactory> watcher_factory_;

  // Takes throughput measurements, and passes them back to |this| through the
  // provided callback. |this| stores the throughput observations in
  // |downstream_throughput_kbps_observations_|, which are later used for
  // estimating the throughput.
  std::unique_ptr<nqe::internal::ThroughputAnalyzer> throughput_analyzer_;

  // Minimum duration between two consecutive computations of effective
  // connection type. Set to non-zero value as a performance optimization.
  const base::TimeDelta effective_connection_type_recomputation_interval_;

  // Time when the effective connection type was last computed.
  base::TimeTicks last_effective_connection_type_computation_;

  // Number of RTT and bandwidth samples available when effective connection
  // type was last recomputed.
  size_t rtt_observations_size_at_last_ect_computation_;
  size_t throughput_observations_size_at_last_ect_computation_;

  // Number of transport RTT samples available when the ECT was last computed.
  size_t transport_rtt_observation_count_last_ect_computation_;

  // Number of RTT observations received since the effective connection type was
  // last computed.
  size_t new_rtt_observations_since_last_ect_computation_;

  // Number of throughput observations received since the effective connection
  // type was last computed.
  size_t new_throughput_observations_since_last_ect_computation_;

  // Current estimate of the network quality.
  nqe::internal::NetworkQuality network_quality_;
  base::Optional<base::TimeDelta> end_to_end_rtt_;

  // Current estimate of the bandwidth delay product (BDP) in kilobits.
  base::Optional<int32_t> bandwidth_delay_product_kbits_;

  // Current estimate of the increase in the transport RTT due to congestion.
  base::Optional<int32_t> increase_in_transport_rtt_;

  // This is true if there is a task posted for |IncreaseInTransportRTTUpdater|.
  bool increase_in_transport_rtt_updater_posted_;

  // Current effective connection type. It is updated on connection change
  // events. It is also updated every time there is network traffic (provided
  // the last computation was more than
  // |effective_connection_type_recomputation_interval_| ago).
  EffectiveConnectionType effective_connection_type_;

  // Minimum and maximum signal strength level observed since last connection
  // change. Updated on connection change and main frame requests.
  base::Optional<int32_t> min_signal_strength_since_connection_change_;
  base::Optional<int32_t> max_signal_strength_since_connection_change_;

  // Stores the qualities of different networks.
  std::unique_ptr<nqe::internal::NetworkQualityStore> network_quality_store_;

  // True if a cached RTT or throughput estimate was available and the
  // corresponding observation has been added on the current network.
  bool cached_estimate_applied_;

  base::ThreadChecker thread_checker_;

  NetLogWithSource net_log_;

  // Manages the writing of events to the net log.
  nqe::internal::EventCreator event_creator_;

  // Time when the last RTT observation from a socket watcher was received.
  base::TimeTicks last_socket_watcher_rtt_notification_;

#if defined(OS_CHROMEOS)
  // Whether the network id should be obtained on a worker thread.
  bool get_network_id_asynchronously_ = false;
#endif

  base::WeakPtrFactory<NetworkQualityEstimator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimator);
};

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_ESTIMATOR_H_
