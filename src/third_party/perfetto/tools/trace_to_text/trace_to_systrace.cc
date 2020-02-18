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

#include "tools/trace_to_text/trace_to_systrace.h"

#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/trace_processor/trace_processor.h"

// When running in Web Assembly, fflush() is a no-op and the stdio buffering
// sends progress updates to JS only when a write ends with \n.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WASM)
#define PROGRESS_CHAR "\n"
#else
#define PROGRESS_CHAR "\r"
#endif

namespace perfetto {
namespace trace_to_text {

namespace {

// Having an empty traceEvents object is necessary for trace viewer to
// load the json properly.
const char kTraceHeader[] = R"({
  "traceEvents": [],
)";

const char kTraceFooter[] = R"(\n",
  "controllerTraceDataKey": "systraceController"
})";

const char kProcessDumpHeader[] =
    ""
    "\"androidProcessDump\": "
    "\"PROCESS DUMP\\nUSER           PID  PPID     VSZ    RSS WCHAN  "
    "PC S NAME                        COMM                       \\n";

const char kThreadHeader[] = "USER           PID   TID CMD \\n";

const char kSystemTraceEvents[] =
    ""
    "  \"systemTraceEvents\": \"";

const char kFtraceHeader[] =
    "# tracer: nop\n"
    "#\n"
    "# entries-in-buffer/entries-written: 30624/30624   #P:4\n"
    "#\n"
    "#                                      _-----=> irqs-off\n"
    "#                                     / _----=> need-resched\n"
    "#                                    | / _---=> hardirq/softirq\n"
    "#                                    || / _--=> preempt-depth\n"
    "#                                    ||| /     delay\n"
    "#           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION\n"
    "#              | |        |      |   ||||       |         |\n";

const char kFtraceJsonHeader[] =
    "# tracer: nop\\n"
    "#\\n"
    "# entries-in-buffer/entries-written: 30624/30624   #P:4\\n"
    "#\\n"
    "#                                      _-----=> irqs-off\\n"
    "#                                     / _----=> need-resched\\n"
    "#                                    | / _---=> hardirq/softirq\\n"
    "#                                    || / _--=> preempt-depth\\n"
    "#                                    ||| /     delay\\n"
    "#           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION\\n"
    "#              | |        |      |   ||||       |         |\\n";

inline void FormatProcess(uint32_t pid,
                          uint32_t ppid,
                          const base::StringView& name,
                          base::StringWriter* writer) {
  writer->AppendLiteral("root             ");
  writer->AppendInt(pid);
  writer->AppendLiteral("     ");
  writer->AppendInt(ppid);
  writer->AppendLiteral("   00000   000 null 0000000000 S ");
  writer->AppendString(name);
  writer->AppendLiteral("         null");
}

inline void FormatThread(uint32_t tid,
                         uint32_t tgid,
                         const base::StringView& name,
                         base::StringWriter* writer) {
  writer->AppendLiteral("root         ");
  writer->AppendInt(tgid);
  writer->AppendChar(' ');
  writer->AppendInt(tid);
  writer->AppendChar(' ');
  if (name.empty()) {
    writer->AppendLiteral("<...>");
  } else {
    writer->AppendString(name);
  }
}

class QueryWriter {
 public:
  QueryWriter(trace_processor::TraceProcessor* tp, std::ostream* output)
      : tp_(tp),
        buffer_(base::PagedMemory::Allocate(kBufferSize)),
        global_writer_(static_cast<char*>(buffer_.Get()), kBufferSize),
        output_(output) {}

  template <typename Callback>
  bool RunQuery(const std::string& sql, Callback callback) {
    char buffer[2048];
    auto iterator = tp_->ExecuteQuery(sql);
    for (uint32_t rows = 0; iterator.Next(); rows++) {
      base::StringWriter line_writer(buffer, base::ArraySize(buffer));
      callback(&iterator, &line_writer);

      if (global_writer_.pos() + line_writer.pos() >= global_writer_.size()) {
        fprintf(stderr, "Writing row %" PRIu32 PROGRESS_CHAR, rows);
        auto str = global_writer_.GetStringView();
        output_->write(str.data(), static_cast<std::streamsize>(str.size()));
        global_writer_.reset();
      }
      global_writer_.AppendStringView(line_writer.GetStringView());
    }

    // Check if we have an error in the iterator and print if so.
    auto status = iterator.Status();
    if (!status.ok()) {
      PERFETTO_ELOG("Error while writing systrace %s", status.c_message());
      return false;
    }

    // Flush any dangling pieces in the global writer.
    auto str = global_writer_.GetStringView();
    output_->write(str.data(), static_cast<std::streamsize>(str.size()));
    global_writer_.reset();
    return true;
  }

 private:
  static constexpr uint32_t kBufferSize = 1024u * 1024u * 16u;

  trace_processor::TraceProcessor* tp_ = nullptr;
  base::PagedMemory buffer_;
  base::StringWriter global_writer_;
  std::ostream* output_ = nullptr;
};

}  // namespace

int TraceToSystrace(std::istream* input,
                    std::ostream* output,
                    uint64_t file_size_limit,
                    bool wrap_in_json) {
  trace_processor::Config config;
  std::unique_ptr<trace_processor::TraceProcessor> tp =
      trace_processor::TraceProcessor::CreateInstance(config);

  // 1MB chunk size seems the best tradeoff on a MacBook Pro 2013 - i7 2.8 GHz.
  constexpr size_t kChunkSize = 1024 * 1024;

// Printing the status update on stderr can be a perf bottleneck. On WASM print
// status updates more frequently because it can be slower to parse each chunk.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WASM)
  constexpr int kStderrRate = 1;
#else
  constexpr int kStderrRate = 128;
#endif
  uint64_t file_size = 0;

  for (int i = 0; file_size < file_size_limit; i++) {
    if (i % kStderrRate == 0) {
      fprintf(stderr, "Loading trace %.2f MB" PROGRESS_CHAR, file_size / 1.0e6);
      fflush(stderr);
    }

    std::unique_ptr<uint8_t[]> buf(new uint8_t[kChunkSize]);
    input->read(reinterpret_cast<char*>(buf.get()), kChunkSize);
    if (input->bad()) {
      PERFETTO_ELOG("Failed when reading trace");
      return 1;
    }

    auto rsize = input->gcount();
    if (rsize <= 0)
      break;
    file_size += static_cast<uint64_t>(rsize);
    tp->Parse(std::move(buf), static_cast<size_t>(rsize));
  }
  tp->NotifyEndOfFile();

  fprintf(stderr, "Loaded trace" PROGRESS_CHAR);
  fflush(stderr);

  using Iterator = trace_processor::TraceProcessor::Iterator;

  QueryWriter q_writer(tp.get(), output);
  if (wrap_in_json) {
    *output << kTraceHeader;

    *output << kProcessDumpHeader;

    // Write out all the processes in the trace.
    // TODO(lalitm): change this query to actually use ppid when it is exposed
    // by the process table.
    static const char kPSql[] = "select pid, 0 as ppid, name from process";
    auto p_callback = [](Iterator* it, base::StringWriter* writer) {
      uint32_t pid = static_cast<uint32_t>(it->Get(0 /* col */).long_value);
      uint32_t ppid = static_cast<uint32_t>(it->Get(1 /* col */).long_value);
      const auto& name_col = it->Get(2 /* col */);
      auto name_view = name_col.type == trace_processor::SqlValue::kString
                           ? base::StringView(name_col.string_value)
                           : base::StringView();
      FormatProcess(pid, ppid, name_view, writer);
    };
    if (!q_writer.RunQuery(kPSql, p_callback))
      return 1;

    *output << kThreadHeader;

    // Write out all the threads in the trace.
    static const char kTSql[] =
        "select tid, COALESCE(upid, 0), thread.name "
        "from thread left join process using (upid)";
    auto t_callback = [](Iterator* it, base::StringWriter* writer) {
      uint32_t tid = static_cast<uint32_t>(it->Get(0 /* col */).long_value);
      uint32_t tgid = static_cast<uint32_t>(it->Get(1 /* col */).long_value);
      const auto& name_col = it->Get(2 /* col */);
      auto name_view = name_col.type == trace_processor::SqlValue::kString
                           ? base::StringView(name_col.string_value)
                           : base::StringView();
      FormatThread(tid, tgid, name_view, writer);
    };
    if (!q_writer.RunQuery(kTSql, t_callback))
      return 1;

    *output << "\",";
    *output << kSystemTraceEvents;
    *output << kFtraceJsonHeader;
  } else {
    *output << "TRACE:\n";
    *output << kFtraceHeader;
  }

  fprintf(stderr, "Converting trace events" PROGRESS_CHAR);
  fflush(stderr);

  static const char kRawSql[] = "select to_ftrace(id) from raw";
  auto raw_callback = [wrap_in_json](Iterator* it, base::StringWriter* writer) {
    const char* line = it->Get(0 /* col */).string_value;
    if (wrap_in_json) {
      for (uint32_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        switch (c) {
          case '\n':
            writer->AppendLiteral("\\n");
            break;
          case '\f':
            writer->AppendLiteral("\\f");
            break;
          case '\b':
            writer->AppendLiteral("\\b");
            break;
          case '\r':
            writer->AppendLiteral("\\r");
            break;
          case '\t':
            writer->AppendLiteral("\\t");
            break;
          case '\\':
            writer->AppendLiteral("\\\\");
            break;
          case '"':
            writer->AppendLiteral("\\\"");
            break;
          default:
            writer->AppendChar(c);
            break;
        }
      }
      writer->AppendChar('\\');
      writer->AppendChar('n');
    } else {
      writer->AppendString(line);
      writer->AppendChar('\n');
    }
  };
  if (!q_writer.RunQuery(kRawSql, raw_callback))
    return 1;

  if (wrap_in_json)
    *output << kTraceFooter;

  return 0;
}

}  // namespace trace_to_text
}  // namespace perfetto
