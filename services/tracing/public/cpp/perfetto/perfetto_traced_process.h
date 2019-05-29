// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"

namespace tracing {

class PerfettoProducer;
class ProducerClient;
class SystemProducer;

// This represents global process level state that the Perfetto tracing system
// expects to exist. This includes a single base implementation of DataSources
// all implementors should use and the perfetto task runner that should be used
// when talking to the tracing system to prevent deadlocks.
//
// Implementations of new DataSources should:
// * Implement PerfettoTracedProcess::DataSourceBase.
// * Add a new data source name in perfetto_service.mojom.
// * Register the data source with Perfetto in ProducerHost::OnConnect.
// * Construct the new implementation when requested to
//   in PerfettoProducer::StartDataSource.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoTracedProcess final {
 public:
  class DataSourceBase {
   public:
    explicit DataSourceBase(const std::string& name);
    virtual ~DataSourceBase();

    void StartTracingWithID(
        uint64_t data_source_id,
        PerfettoProducer* producer_client,
        const perfetto::DataSourceConfig& data_source_config);

    virtual void StartTracing(
        PerfettoProducer* producer_client,
        const perfetto::DataSourceConfig& data_source_config) = 0;
    virtual void StopTracing(
        base::OnceClosure stop_complete_callback = base::OnceClosure()) = 0;

    // Flush the data source.
    virtual void Flush(base::RepeatingClosure flush_complete_callback) = 0;

    const std::string& name() const { return name_; }
    uint64_t data_source_id() const { return data_source_id_; }

   private:
    uint64_t data_source_id_ = 0;
    std::string name_;
  };

  // Returns the process-wide instance of the PerfettoTracedProcess.
  static PerfettoTracedProcess* Get();

  ProducerClient* producer_client();

  ~PerfettoTracedProcess();

  // Sets the ProducerClient and returns the old pointer. If tests want to
  // restore the state of the world they should store the pointer and call this
  // method again with it as the parameter.
  std::unique_ptr<ProducerClient> SetProducerClientForTesting(
      std::unique_ptr<ProducerClient> client);
  void ClearDataSourcesForTesting();
  static void DeleteSoonForTesting(std::unique_ptr<PerfettoTracedProcess>);

  // Returns the taskrunner used by any Perfetto service.
  static PerfettoTaskRunner* GetTaskRunner();

  // Add a new data source to the PerfettoTracedProcess; the caller retains
  // ownership and is responsible for making sure the data source outlives the
  // PerfettoTracedProcess.
  void AddDataSource(DataSourceBase*);
  const std::set<DataSourceBase*>& data_sources() { return data_sources_; }

  static void ResetTaskRunnerForTesting();

 protected:
  // protected for testing.
  PerfettoTracedProcess();

 private:
  friend class base::NoDestructor<PerfettoTracedProcess>;

  void AddDataSourceOnSequence(DataSourceBase* data_source);

  // The canonical set of DataSourceBases alive in this process. These will be
  // registered with the tracing service.
  std::set<DataSourceBase*> data_sources_;
  // A PerfettoProducer that connects to the chrome Perfetto service through
  // mojo.
  std::unique_ptr<ProducerClient> producer_client_;
  // A PerfettoProducer that connects to the system Perfetto service. If there
  // is no system Perfetto service this pointer will be valid, but all function
  // calls will be noops.
  std::unique_ptr<SystemProducer> system_producer_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);
  // NOTE: Weak pointers must be invalidated before all other member
  // variables.
  base::WeakPtrFactory<PerfettoTracedProcess> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PerfettoTracedProcess);
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
