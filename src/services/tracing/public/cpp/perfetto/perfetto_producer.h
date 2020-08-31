// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_

#include <memory>

#include "base/component_export.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/basic_types.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

// This class represents the perfetto producer endpoint which is used for
// producers to talk to the Perfetto service. It also provides methods to
// interact with the shared memory buffer by binding and creating TraceWriters.
//
// In addition to the PerfettoProducers' pure virtual methods, subclasses must
// implement the remaining methods of the ProducerEndpoint interface.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoProducer {
 public:
  PerfettoProducer(PerfettoTaskRunner* task_runner);

  virtual ~PerfettoProducer();

  // Setup the shared memory buffer and data sources for startup tracing.
  // Returns false on failure. Can be called on any thread.
  bool SetupStartupTracing(const base::trace_event::TraceConfig&,
                           bool privacy_filtering_enabled);

  // Schedules the startup tracing timeout if active.
  void OnThreadPoolAvailable();

  // See SharedMemoryArbiter::CreateStartupTraceWriter.
  std::unique_ptr<perfetto::TraceWriter> CreateStartupTraceWriter(
      uint16_t target_buffer_reservation_id);

  // See SharedMemoryArbiter::BindStartupTargetBuffer. Should be called on the
  // producer's task runner.
  virtual void BindStartupTargetBuffer(
      uint16_t target_buffer_reservation_id,
      perfetto::BufferID startup_target_buffer);

  // See SharedMemoryArbiter::AbortStartupTracingForReservation. Should be
  // called on the producer's task runner.
  virtual void AbortStartupTracingForReservation(
      uint16_t target_buffer_reservation_id);

  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs, and respond to flushes.
  //
  // Should only be called while a tracing session is active and a
  // SharedMemoryArbiter exists.
  //
  // Virtual for testing.
  virtual std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault);

  // Returns the SharedMemoryArbiter if available.
  // TODO(eseckler): Once startup tracing v2 is available in Chrome, this could
  // become GetSharedMemoryArbiter() instead.
  virtual perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() = 0;

  // Informs the PerfettoProducer a new Data Source was added. This instance
  // will also be found in |data_sources| having just be inserted before this
  // method is called by PerfettoTracedProcess. This enables the
  // PerfettoProducer to perform initialization on new data sources.
  virtual void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) = 0;

  // Returns true if this PerfettoProducer is currently tracing.
  virtual bool IsTracingActive() = 0;

  static void DeleteSoonForTesting(
      std::unique_ptr<PerfettoProducer> perfetto_producer);

  // In tests, PerfettoProducers may leak between tests, but their task sequence
  // may become invalid (e.g. TaskEnvironment is destroyed). This resets the
  // PerfettoProducer's sequence checker, so that it can later be rebound to a
  // new test's sequence. Note that this only resets the producer's sequence
  // checker - and there may be other sequence checkers elsewhere, e.g. in
  // PosixSystemProducer's socket (which would fail on disconnect if the system
  // producer's sequence is reset while it is connected).
  void ResetSequenceForTesting();

  void set_startup_tracing_timeout_for_testing(base::TimeDelta timeout) {
    startup_tracing_timeout_ = timeout;
  }

 protected:
  friend class MockProducerHost;

  // May be called on any thread.
  virtual bool SetupSharedMemoryForStartupTracing() = 0;

  // The PerfettoProducer subclass should call this once the startup tracing
  // session was taken over by the tracing service.
  // TODO(eseckler): Consider refactoring this into e.g. a delegate interface.
  void OnStartupTracingComplete();

  bool IsStartupTracingActive();

  // TODO(oysteine): Find a good compromise between performance and
  // data granularity (mainly relevant to running with small buffer sizes
  // when we use background tracing) on Android.
#if defined(OS_ANDROID)
  static constexpr size_t kSMBPageSizeBytes = 4 * 1024;
#else
  static constexpr size_t kSMBPageSizeBytes = 32 * 1024;
#endif

  // TODO(oysteine): Figure out a good buffer size.
  static constexpr size_t kSMBSizeBytes = 4 * 1024 * 1024;

  PerfettoTaskRunner* task_runner();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void MaybeScheduleStartupTracingTimeout();
  void OnStartupTracingTimeout();

  // If a startup tracing session is not taken over by the service after this
  // delay, the startup session will be aborted and its data lost. This is to
  // catch situations where e.g. a subprocess is spawned with startup tracing
  // flags, but the tracing session got disabled in the service while it was
  // initializing (in which case, the tracing service will not tell the
  // subprocess to start tracing after it connects).
  base::TimeDelta startup_tracing_timeout_ = base::TimeDelta::FromSeconds(60);

  PerfettoTaskRunner* const task_runner_;

  std::atomic<bool> startup_tracing_active_{false};

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PerfettoProducer> weak_ptr_factory_{this};
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
