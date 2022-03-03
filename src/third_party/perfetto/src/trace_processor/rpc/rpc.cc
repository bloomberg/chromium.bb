/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/rpc/rpc.h"

#include <string.h>

#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/version.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/protozero/proto_ring_buffer.h"
#include "src/trace_processor/rpc/query_result_serializer.h"
#include "src/trace_processor/tp_metatrace.h"

#include "protos/perfetto/trace_processor/trace_processor.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
// Writes a "Loading trace ..." update every N bytes.
constexpr size_t kProgressUpdateBytes = 50 * 1000 * 1000;
using TraceProcessorRpcStream = protos::pbzero::TraceProcessorRpcStream;
using RpcProto = protos::pbzero::TraceProcessorRpc;

// Most RPC messages are either very small or a query results.
// QueryResultSerializer splits rows into batches of approximately 128KB. Try
// avoid extra heap allocations for the nominal case.
constexpr auto kSliceSize =
    QueryResultSerializer::kDefaultBatchSplitThreshold + 4096;

// Holds a trace_processor::TraceProcessorRpc pbzero message. Avoids extra
// copies by doing direct scattered calls from the fragmented heap buffer onto
// the RpcResponseFunction (the receiver is expected to deal with arbitrary
// fragmentation anyways). It also takes care of prefixing each message with
// the proto preamble and varint size.
class Response {
 public:
  Response(int64_t seq, int method);
  Response(const Response&) = delete;
  Response& operator=(const Response&) = delete;
  RpcProto* operator->() { return msg_; }
  void Send(Rpc::RpcResponseFunction);

 private:
  RpcProto* msg_ = nullptr;

  // The reason why we use TraceProcessorRpcStream as root message is because
  // the RPC wire protocol expects each message to be prefixed with a proto
  // preamble and varint size. This happens to be the same serialization of a
  // repeated field (this is really the same trick we use between
  // Trace and TracePacket in trace.proto)
  protozero::HeapBuffered<TraceProcessorRpcStream> buf_;
};

Response::Response(int64_t seq, int method) : buf_(kSliceSize, kSliceSize) {
  msg_ = buf_->add_msg();
  msg_->set_seq(seq);
  msg_->set_response(static_cast<RpcProto::TraceProcessorMethod>(method));
}

void Response::Send(Rpc::RpcResponseFunction send_fn) {
  buf_->Finalize();
  for (const auto& slice : buf_.GetSlices()) {
    auto range = slice.GetUsedRange();
    send_fn(range.begin, static_cast<uint32_t>(range.size()));
  }
}

}  // namespace

Rpc::Rpc(std::unique_ptr<TraceProcessor> preloaded_instance)
    : trace_processor_(std::move(preloaded_instance)) {
  if (!trace_processor_)
    ResetTraceProcessor();
}

Rpc::Rpc() : Rpc(nullptr) {}
Rpc::~Rpc() = default;

void Rpc::ResetTraceProcessor() {
  trace_processor_ = TraceProcessor::CreateInstance(Config());
  bytes_parsed_ = bytes_last_progress_ = 0;
  t_parse_started_ = base::GetWallTimeNs().count();
  // Deliberately not resetting the RPC channel state (rxbuf_, {tx,rx}_seq_id_).
  // This is invoked from the same client to clear the current trace state
  // before loading a new one. The IPC channel is orthogonal to that and the
  // message numbering continues regardless of the reset.
}

void Rpc::OnRpcRequest(const void* data, size_t len) {
  rxbuf_.Append(data, len);
  for (;;) {
    auto msg = rxbuf_.ReadMessage();
    if (!msg.valid()) {
      if (msg.fatal_framing_error) {
        protozero::HeapBuffered<TraceProcessorRpcStream> err_msg;
        err_msg->add_msg()->set_fatal_error("RPC framing error");
        auto err = err_msg.SerializeAsArray();
        rpc_response_fn_(err.data(), static_cast<uint32_t>(err.size()));
        rpc_response_fn_(nullptr, 0);  // Disconnect.
      }
      break;
    }
    ParseRpcRequest(msg.start, msg.len);
  }
}

// [data, len] here is a tokenized TraceProcessorRpc proto message, without the
// size header.
void Rpc::ParseRpcRequest(const uint8_t* data, size_t len) {
  RpcProto::Decoder req(data, len);

  // We allow restarting the sequence from 0. This happens when refreshing the
  // browser while using the external trace_processor_shell --httpd.
  if (req.seq() != 0 && rx_seq_id_ != 0 && req.seq() != rx_seq_id_ + 1) {
    char err_str[255];
    // "(ERR:rpc_seq)" is intercepted by error_dialog.ts in the UI.
    sprintf(err_str,
            "RPC request out of order. Expected %" PRId64 ", got %" PRId64
            " (ERR:rpc_seq)",
            rx_seq_id_ + 1, req.seq());
    PERFETTO_ELOG("%s", err_str);
    protozero::HeapBuffered<TraceProcessorRpcStream> err_msg;
    err_msg->add_msg()->set_fatal_error(err_str);
    auto err = err_msg.SerializeAsArray();
    rpc_response_fn_(err.data(), static_cast<uint32_t>(err.size()));
    rpc_response_fn_(nullptr, 0);  // Disconnect.
    return;
  }
  rx_seq_id_ = req.seq();

  // The static cast is to prevent that the compiler breaks future proofness.
  const int req_type = static_cast<int>(req.request());
  static const char kErrFieldNotSet[] = "RPC error: request field not set";
  switch (req_type) {
    case RpcProto::TPM_APPEND_TRACE_DATA: {
      Response resp(tx_seq_id_++, req_type);
      auto* result = resp->set_append_result();
      if (!req.has_append_trace_data()) {
        result->set_error(kErrFieldNotSet);
      } else {
        protozero::ConstBytes byte_range = req.append_trace_data();
        util::Status res = Parse(byte_range.data, byte_range.size);
        if (!res.ok()) {
          result->set_error(res.message());
        }
      }
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_FINALIZE_TRACE_DATA: {
      Response resp(tx_seq_id_++, req_type);
      NotifyEndOfFile();
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_QUERY_STREAMING: {
      if (!req.has_query_args()) {
        Response resp(tx_seq_id_++, req_type);
        auto* result = resp->set_query_result();
        result->set_error(kErrFieldNotSet);
        resp.Send(rpc_response_fn_);
      } else {
        protozero::ConstBytes args = req.query_args();
        auto it = QueryInternal(args.data, args.size);
        QueryResultSerializer serializer(std::move(it));
        for (bool has_more = true; has_more;) {
          Response resp(tx_seq_id_++, req_type);
          has_more = serializer.Serialize(resp->set_query_result());
          resp.Send(rpc_response_fn_);
        }
      }
      break;
    }
    case RpcProto::TPM_QUERY_RAW_DEPRECATED: {
      Response resp(tx_seq_id_++, req_type);
      auto* result = resp->set_raw_query_result();
      if (!req.has_raw_query_args()) {
        result->set_error(kErrFieldNotSet);
      } else {
        protozero::ConstBytes args = req.raw_query_args();
        RawQueryInternal(args.data, args.size, result);
      }
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_COMPUTE_METRIC: {
      Response resp(tx_seq_id_++, req_type);
      auto* result = resp->set_metric_result();
      if (!req.has_compute_metric_args()) {
        result->set_error(kErrFieldNotSet);
      } else {
        protozero::ConstBytes args = req.compute_metric_args();
        ComputeMetricInternal(args.data, args.size, result);
      }
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_GET_METRIC_DESCRIPTORS: {
      Response resp(tx_seq_id_++, req_type);
      auto descriptor_set = trace_processor_->GetMetricDescriptors();
      auto* result = resp->set_metric_descriptors();
      result->AppendRawProtoBytes(descriptor_set.data(), descriptor_set.size());
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_RESTORE_INITIAL_TABLES: {
      trace_processor_->RestoreInitialTables();
      Response resp(tx_seq_id_++, req_type);
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_ENABLE_METATRACE: {
      trace_processor_->EnableMetatrace();
      Response resp(tx_seq_id_++, req_type);
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_DISABLE_AND_READ_METATRACE: {
      Response resp(tx_seq_id_++, req_type);
      DisableAndReadMetatraceInternal(resp->set_metatrace());
      resp.Send(rpc_response_fn_);
      break;
    }
    case RpcProto::TPM_GET_STATUS: {
      Response resp(tx_seq_id_++, req_type);
      std::vector<uint8_t> status = GetStatus();
      resp->set_status()->AppendRawProtoBytes(status.data(), status.size());
      resp.Send(rpc_response_fn_);
      break;
    }
    default: {
      // This can legitimately happen if the client is newer. We reply with a
      // generic "unkown request" response, so the client can do feature
      // detection
      PERFETTO_DLOG("[RPC] Uknown request type (%d), size=%zu", req_type, len);
      Response resp(tx_seq_id_++, req_type);
      resp->set_invalid_request(
          static_cast<RpcProto::TraceProcessorMethod>(req_type));
      resp.Send(rpc_response_fn_);
      break;
    }
  }  // switch(req_type)
}

util::Status Rpc::Parse(const uint8_t* data, size_t len) {
  if (eof_) {
    // Reset the trace processor state if another trace has been previously
    // loaded.
    ResetTraceProcessor();
  }

  eof_ = false;
  bytes_parsed_ += len;
  MaybePrintProgress();

  if (len == 0)
    return util::OkStatus();

  // TraceProcessor needs take ownership of the memory chunk.
  std::unique_ptr<uint8_t[]> data_copy(new uint8_t[len]);
  memcpy(data_copy.get(), data, len);
  return trace_processor_->Parse(std::move(data_copy), len);
}

void Rpc::NotifyEndOfFile() {
  trace_processor_->NotifyEndOfFile();
  eof_ = true;
  MaybePrintProgress();
}

void Rpc::MaybePrintProgress() {
  if (eof_ || bytes_parsed_ - bytes_last_progress_ > kProgressUpdateBytes) {
    bytes_last_progress_ = bytes_parsed_;
    auto t_load_s =
        static_cast<double>(base::GetWallTimeNs().count() - t_parse_started_) /
        1e9;
    fprintf(stderr, "\rLoading trace %.2f MB (%.1f MB/s)%s",
            static_cast<double>(bytes_parsed_) / 1e6,
            static_cast<double>(bytes_parsed_) / 1e6 / t_load_s,
            (eof_ ? "\n" : ""));
    fflush(stderr);
  }
}

void Rpc::Query(const uint8_t* args,
                size_t len,
                QueryResultBatchCallback result_callback) {
  auto it = QueryInternal(args, len);
  QueryResultSerializer serializer(std::move(it));

  std::vector<uint8_t> res;
  for (bool has_more = true; has_more;) {
    has_more = serializer.Serialize(&res);
    result_callback(res.data(), res.size(), has_more);
    res.clear();
  }
}

Iterator Rpc::QueryInternal(const uint8_t* args, size_t len) {
  protos::pbzero::RawQueryArgs::Decoder query(args, len);
  std::string sql = query.sql_query().ToStdString();
  PERFETTO_DLOG("[RPC] Query < %s", sql.c_str());
  PERFETTO_TP_TRACE("RPC_QUERY",
                    [&](metatrace::Record* r) { r->AddArg("SQL", sql); });

  return trace_processor_->ExecuteQuery(sql.c_str());
}

std::vector<uint8_t> Rpc::RawQuery(const uint8_t* args, size_t len) {
  protozero::HeapBuffered<protos::pbzero::RawQueryResult> result;
  RawQueryInternal(args, len, result.get());
  return result.SerializeAsArray();
}

void Rpc::RawQueryInternal(const uint8_t* args,
                           size_t len,
                           protos::pbzero::RawQueryResult* result) {
  using ColumnValues = protos::pbzero::RawQueryResult::ColumnValues;
  using ColumnDesc = protos::pbzero::RawQueryResult::ColumnDesc;

  protos::pbzero::RawQueryArgs::Decoder query(args, len);
  std::string sql = query.sql_query().ToStdString();
  PERFETTO_DLOG("[RPC] RawQuery < %s", sql.c_str());
  PERFETTO_TP_TRACE("RPC_RAW_QUERY",
                    [&](metatrace::Record* r) { r->AddArg("SQL", sql); });

  auto it = trace_processor_->ExecuteQuery(sql.c_str());

  // This vector contains a standalone protozero message per column. The problem
  // it's solving is the following: (i) sqlite iterators are row-based; (ii) the
  // RawQueryResult proto is column-based (that was a poor design choice we
  // should revisit at some point); (iii) the protozero API doesn't allow to
  // begin a new nested message before the previous one is completed.
  // In order to avoid the interleaved-writing, we write each column in a
  // dedicated heap buffer and then we merge all the column data at the end,
  // after having iterated all rows.
  std::vector<protozero::HeapBuffered<ColumnValues>> cols(it.ColumnCount());

  // This constexpr is to avoid ODR-use of protozero constants which are only
  // declared but not defined. Putting directly UNKONWN in the vector ctor
  // causes a linker error in the WASM toolchain.
  static constexpr auto kUnknown = ColumnDesc::UNKNOWN;
  std::vector<ColumnDesc::Type> col_types(it.ColumnCount(), kUnknown);
  uint32_t rows = 0;

  for (; it.Next(); ++rows) {
    for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
      auto& col = cols[col_idx];
      auto& col_type = col_types[col_idx];

      using SqlValue = trace_processor::SqlValue;
      auto cell = it.Get(col_idx);
      if (col_type == ColumnDesc::UNKNOWN) {
        switch (cell.type) {
          case SqlValue::Type::kLong:
            col_type = ColumnDesc::LONG;
            break;
          case SqlValue::Type::kString:
            col_type = ColumnDesc::STRING;
            break;
          case SqlValue::Type::kDouble:
            col_type = ColumnDesc::DOUBLE;
            break;
          case SqlValue::Type::kBytes:
            col_type = ColumnDesc::STRING;
            break;
          case SqlValue::Type::kNull:
            break;
        }
      }

      // If either the column type is null or we still don't know the type,
      // just add null values to all the columns.
      if (cell.type == SqlValue::Type::kNull ||
          col_type == ColumnDesc::UNKNOWN) {
        col->add_long_values(0);
        col->add_string_values("[NULL]");
        col->add_double_values(0);
        col->add_is_nulls(true);
        continue;
      }

      // Cast the sqlite value to the type of the column.
      switch (col_type) {
        case ColumnDesc::LONG:
          PERFETTO_CHECK(cell.type == SqlValue::Type::kLong ||
                         cell.type == SqlValue::Type::kDouble);
          if (cell.type == SqlValue::Type::kLong) {
            col->add_long_values(cell.long_value);
          } else /* if (cell.type == SqlValue::Type::kDouble) */ {
            col->add_long_values(static_cast<int64_t>(cell.double_value));
          }
          col->add_is_nulls(false);
          break;
        case ColumnDesc::STRING: {
          if (cell.type == SqlValue::Type::kBytes) {
            col->add_string_values("<bytes>");
          } else {
            PERFETTO_CHECK(cell.type == SqlValue::Type::kString);
            col->add_string_values(cell.string_value);
          }
          col->add_is_nulls(false);
          break;
        }
        case ColumnDesc::DOUBLE:
          PERFETTO_CHECK(cell.type == SqlValue::Type::kLong ||
                         cell.type == SqlValue::Type::kDouble);
          if (cell.type == SqlValue::Type::kLong) {
            col->add_double_values(static_cast<double>(cell.long_value));
          } else /* if (cell.type == SqlValue::Type::kDouble) */ {
            col->add_double_values(cell.double_value);
          }
          col->add_is_nulls(false);
          break;
        case ColumnDesc::UNKNOWN:
          PERFETTO_FATAL("Handled in if statement above.");
      }
    }  // for(col)
  }    // for(row)

  // Write the column descriptors.
  for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
    auto* descriptor = result->add_column_descriptors();
    std::string col_name = it.GetColumnName(col_idx);
    descriptor->set_name(col_name.data(), col_name.size());
    descriptor->set_type(col_types[col_idx]);
  }

  // Merge the column values.
  for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
    std::vector<uint8_t> col_data = cols[col_idx].SerializeAsArray();
    result->AppendBytes(protos::pbzero::RawQueryResult::kColumnsFieldNumber,
                        col_data.data(), col_data.size());
  }

  util::Status status = it.Status();
  result->set_num_records(rows);
  if (!status.ok())
    result->set_error(status.c_message());
  PERFETTO_DLOG("[RPC] RawQuery > %d rows (err: %d)", rows, !status.ok());
}

void Rpc::RestoreInitialTables() {
  trace_processor_->RestoreInitialTables();
}

std::vector<uint8_t> Rpc::ComputeMetric(const uint8_t* args, size_t len) {
  protozero::HeapBuffered<protos::pbzero::ComputeMetricResult> result;
  ComputeMetricInternal(args, len, result.get());
  return result.SerializeAsArray();
}

void Rpc::ComputeMetricInternal(const uint8_t* data,
                                size_t len,
                                protos::pbzero::ComputeMetricResult* result) {
  protos::pbzero::ComputeMetricArgs::Decoder args(data, len);
  std::vector<std::string> metric_names;
  for (auto it = args.metric_names(); it; ++it) {
    metric_names.emplace_back(it->as_std_string());
  }

  PERFETTO_TP_TRACE("RPC_COMPUTE_METRIC", [&](metatrace::Record* r) {
    for (const auto& metric : metric_names) {
      r->AddArg("Metric", metric);
      r->AddArg("Format", std::to_string(args.format()));
    }
  });

  PERFETTO_DLOG("[RPC] ComputeMetrics(%zu, %s), format=%d", metric_names.size(),
                metric_names.empty() ? "" : metric_names.front().c_str(),
                args.format());
  switch (args.format()) {
    case protos::pbzero::ComputeMetricArgs::BINARY_PROTOBUF: {
      std::vector<uint8_t> metrics_proto;
      util::Status status =
          trace_processor_->ComputeMetric(metric_names, &metrics_proto);
      if (status.ok()) {
        result->set_metrics(metrics_proto.data(), metrics_proto.size());
      } else {
        result->set_error(status.message());
      }
      break;
    }
    case protos::pbzero::ComputeMetricArgs::TEXTPROTO: {
      std::string metrics_string;
      util::Status status = trace_processor_->ComputeMetricText(
          metric_names, TraceProcessor::MetricResultFormat::kProtoText,
          &metrics_string);
      if (status.ok()) {
        result->set_metrics_as_prototext(metrics_string);
      } else {
        result->set_error(status.message());
      }
      break;
    }
  }
}

void Rpc::EnableMetatrace() {
  trace_processor_->EnableMetatrace();
}

std::vector<uint8_t> Rpc::DisableAndReadMetatrace() {
  protozero::HeapBuffered<protos::pbzero::DisableAndReadMetatraceResult> result;
  DisableAndReadMetatraceInternal(result.get());
  return result.SerializeAsArray();
}

void Rpc::DisableAndReadMetatraceInternal(
    protos::pbzero::DisableAndReadMetatraceResult* result) {
  std::vector<uint8_t> trace_proto;
  util::Status status = trace_processor_->DisableAndReadMetatrace(&trace_proto);
  if (status.ok()) {
    result->set_metatrace(trace_proto.data(), trace_proto.size());
  } else {
    result->set_error(status.message());
  }
}

std::vector<uint8_t> Rpc::GetStatus() {
  protozero::HeapBuffered<protos::pbzero::StatusResult> status;
  status->set_loaded_trace_name(trace_processor_->GetCurrentTraceName());
  status->set_human_readable_version(base::GetVersionString());
  status->set_api_version(protos::pbzero::TRACE_PROCESSOR_CURRENT_API_VERSION);
  return status.SerializeAsArray();
}

}  // namespace trace_processor
}  // namespace perfetto
