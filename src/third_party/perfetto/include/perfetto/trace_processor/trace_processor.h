/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_H_
#define INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"

namespace perfetto {

namespace trace_processor {

// Coordinates the loading of traces from an arbitrary source and allows
// execution of SQL queries on the events in these traces.
class TraceProcessor {
 public:
  class IteratorImpl;

  // Iterator returning SQL rows satisfied by a query.
  class Iterator {
   public:
    Iterator(std::unique_ptr<IteratorImpl> iterator);
    ~Iterator();

    Iterator(Iterator&) noexcept = delete;
    Iterator& operator=(Iterator&) = delete;

    Iterator(Iterator&&) noexcept;
    Iterator& operator=(Iterator&&);

    // Forwards the iterator to the next result row and returns a boolean of
    // whether there is a next row. If this method returns false,
    // |GetLastError()| should be called to check if there was an error. If
    // there was no error, this means the EOF was reached.
    bool Next();

    // Returns the value associated with the column |col|. Any call to
    // |Get()| must be preceeded by a call to |Next()| returning
    // kHasNext. |col| must be less than the number returned by |ColumnCount()|.
    SqlValue Get(uint32_t col);

    // Returns the name of the column at index |col|. Can be called even before
    // calling |Next()|.
    std::string GetColumName(uint32_t col);

    // Returns the number of columns in this iterator's query. Can be called
    // even before calling |Next()|.
    uint32_t ColumnCount();

    // Returns the status of the iterator.
    util::Status Status();

   private:
    std::unique_ptr<IteratorImpl> iterator_;
  };

  // Creates a new instance of TraceProcessor.
  static std::unique_ptr<TraceProcessor> CreateInstance(const Config&);

  virtual ~TraceProcessor();

  // The entry point to push trace data into the processor. The trace format
  // will be automatically discovered on the first push call. It is possible
  // to make queries between two pushes.
  // Returns the Ok status if parsing has been succeeding so far, and Error
  // status if some unrecoverable error happened. If this happens, the
  // TraceProcessor will ignore the following Parse() requests, drop data on the
  // floor and return errors forever.
  virtual util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) = 0;

  // When parsing a bounded file (as opposite to streaming from a device) this
  // function should be called when the last chunk of the file has been passed
  // into Parse(). This allows to flush the events queued in the ordering stage,
  // without having to wait for their time window to expire.
  virtual void NotifyEndOfFile() = 0;

  // Executes a SQLite query on the loaded portion of the trace. The returned
  // iterator can be used to load rows from the result.
  virtual Iterator ExecuteQuery(const std::string& sql,
                                int64_t time_queued = 0) = 0;

  // Registers a metric at the given path which will run the specified SQL.
  virtual util::Status RegisterMetric(const std::string& path,
                                      const std::string& sql) = 0;

  // Reads the FileDescriptorSet proto message given by |data| and |size| and
  // adds any extensions to the metrics proto to allow them to be available as
  // proto builder functions when computing metrics.
  virtual util::Status ExtendMetricsProto(const uint8_t* data, size_t size) = 0;

  // Computes the given metrics on the loded portion of the trace. If
  // successful, the output argument |metrics_proto| will be filled with the
  // proto-encoded bytes for the message TraceMetrics in
  // perfetto/metrics/metrics.proto.
  virtual util::Status ComputeMetric(
      const std::vector<std::string>& metric_names,
      std::vector<uint8_t>* metrics_proto) = 0;

  // Interrupts the current query. Typically used by Ctrl-C handler.
  virtual void InterruptQuery() = 0;
};

// When set, logs SQLite actions on the console.
void EnableSQLiteVtableDebugging();

}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_H_
