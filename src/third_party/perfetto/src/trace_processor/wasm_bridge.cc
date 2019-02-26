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

#include <emscripten/emscripten.h>
#include <map>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/trace_processor/trace_processor.h"

#include "perfetto/trace_processor/raw_query.pb.h"
#include "perfetto/trace_processor/sched.pb.h"

namespace perfetto {
namespace trace_processor {

using RequestID = uint32_t;

// Reply(): replies to a RPC method invocation.
// Called asynchronously (i.e. in a separate task) by the C++ code inside the
// trace processor to return data for a RPC method call.
// The function is generic and thankfully we need just one for all methods
// because the output is always a protobuf buffer.
// Args:
//  RequestID: the ID passed by the embedder when invoking the RPC method (e.g.,
//             the first argument passed to sched_getSchedEvents()).
using ReplyFunction = void (*)(RequestID,
                               bool success,
                               const char* /*proto_reply_data*/,
                               uint32_t /*len*/);

namespace {
TraceProcessor* g_trace_processor;
ReplyFunction g_reply;
}  // namespace
// +---------------------------------------------------------------------------+
// | Exported functions called by the JS/TS running in the worker.             |
// +---------------------------------------------------------------------------+
extern "C" {

void EMSCRIPTEN_KEEPALIVE Initialize(ReplyFunction);
void Initialize(ReplyFunction reply_function) {
  PERFETTO_ILOG("Initializing WASM bridge");
  Config config;
  config.optimization_mode = OptimizationMode::kMaxBandwidth;
  g_trace_processor = TraceProcessor::CreateInstance(config).release();
  g_reply = reply_function;
}

void EMSCRIPTEN_KEEPALIVE trace_processor_parse(RequestID,
                                                const uint8_t*,
                                                uint32_t);
void trace_processor_parse(RequestID id, const uint8_t* data, size_t size) {
  // TODO(primiano): This copy is extremely unfortunate. Ideally there should be
  // a way to take the Blob coming from JS (either from FileReader or from th
  // fetch() stream) and move into WASM.
  // See https://github.com/WebAssembly/design/issues/1162.
  std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
  memcpy(buf.get(), data, size);
  g_trace_processor->Parse(std::move(buf), size);
  g_reply(id, true, "", 0);
}

// We keep the same signature as other methods even though we don't take input
// arguments for simplicity.
void EMSCRIPTEN_KEEPALIVE trace_processor_notifyEof(RequestID,
                                                    const uint8_t*,
                                                    uint32_t);
void trace_processor_notifyEof(RequestID id, const uint8_t*, uint32_t size) {
  PERFETTO_DCHECK(!size);
  g_trace_processor->NotifyEndOfFile();
  g_reply(id, true, "", 0);
}

void EMSCRIPTEN_KEEPALIVE trace_processor_rawQuery(RequestID,
                                                   const uint8_t*,
                                                   int);
void trace_processor_rawQuery(RequestID id,
                              const uint8_t* query_data,
                              int len) {
  protos::RawQueryArgs query;
  bool parsed = query.ParseFromArray(query_data, len);
  if (!parsed) {
    std::string err = "Failed to parse input request";
    g_reply(id, false, err.data(), err.size());
    return;
  }

  // When the C++ class implementing the service replies, serialize the protobuf
  // result and post it back to the worker script (|g_reply|).
  auto callback = [id](const protos::RawQueryResult& res) {
    std::string encoded;
    res.SerializeToString(&encoded);
    g_reply(id, true, encoded.data(), static_cast<uint32_t>(encoded.size()));
  };

  g_trace_processor->ExecuteQuery(query, callback);
}

}  // extern "C"

}  // namespace trace_processor
}  // namespace perfetto
