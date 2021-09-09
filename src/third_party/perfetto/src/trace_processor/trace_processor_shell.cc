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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>

#include <functional>
#include <iostream>
#include <vector>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/getopt.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/version.h"

#include "perfetto/trace_processor/read_trace.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/metrics/chrome/all_chrome_metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.descriptor.h"
#include "src/trace_processor/util/proto_to_json.h"
#include "src/trace_processor/util/status_macros.h"

#if PERFETTO_BUILDFLAG(PERFETTO_TP_HTTPD)
#include "src/trace_processor/rpc/httpd.h"
#endif
#include "src/profiling/deobfuscator.h"
#include "src/profiling/symbolizer/local_symbolizer.h"
#include "src/profiling/symbolizer/symbolize_database.h"
#include "src/profiling/symbolizer/symbolizer.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE)
#define PERFETTO_HAS_SIGNAL_H() 1
#else
#define PERFETTO_HAS_SIGNAL_H() 0
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_TP_LINENOISE)
#include <linenoise.h>
#include <pwd.h>
#include <sys/types.h>
#endif

#if PERFETTO_HAS_SIGNAL_H()
#include <signal.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <io.h>
#define ftruncate _chsize
#else
#include <dirent.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_TP_LINENOISE) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <unistd.h>  // For getuid() in GetConfigPath().
#endif

namespace perfetto {
namespace trace_processor {

namespace {
TraceProcessor* g_tp;

#if PERFETTO_BUILDFLAG(PERFETTO_TP_LINENOISE)

bool EnsureDir(const std::string& path) {
  return base::Mkdir(path) || errno == EEXIST;
}

bool EnsureFile(const std::string& path) {
  return base::OpenFile(path, O_RDONLY | O_CREAT, 0644).get() != -1;
}

std::string GetConfigPath() {
  const char* homedir = getenv("HOME");
#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE)
  if (homedir == nullptr)
    homedir = getpwuid(getuid())->pw_dir;
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  if (homedir == nullptr)
    homedir = getenv("USERPROFILE");
#endif
  if (homedir == nullptr)
    return "";
  return std::string(homedir) + "/.config";
}

std::string GetPerfettoPath() {
  std::string config = GetConfigPath();
  if (config.empty())
    return "";
  return config + "/perfetto";
}

std::string GetHistoryPath() {
  std::string perfetto = GetPerfettoPath();
  if (perfetto.empty())
    return "";
  return perfetto + "/.trace_processor_shell_history";
}

void SetupLineEditor() {
  linenoiseSetMultiLine(true);
  linenoiseHistorySetMaxLen(1000);

  bool success = !GetHistoryPath().empty();
  success = success && EnsureDir(GetConfigPath());
  success = success && EnsureDir(GetPerfettoPath());
  success = success && EnsureFile(GetHistoryPath());
  success = success && linenoiseHistoryLoad(GetHistoryPath().c_str()) != -1;
  if (!success) {
    PERFETTO_PLOG("Could not load history from %s", GetHistoryPath().c_str());
  }
}

struct LineDeleter {
  void operator()(char* p) const {
    linenoiseHistoryAdd(p);
    linenoiseHistorySave(GetHistoryPath().c_str());
    linenoiseFree(p);
  }
};

using ScopedLine = std::unique_ptr<char, LineDeleter>;

ScopedLine GetLine(const char* prompt) {
  errno = 0;
  auto line = ScopedLine(linenoise(prompt));
  // linenoise returns a nullptr both for CTRL-C and CTRL-D, however in the
  // former case it sets errno to EAGAIN.
  // If the user press CTRL-C return "" instead of nullptr. We don't want the
  // main loop to quit in that case as that is inconsistent with the behavior
  // "CTRL-C interrupts the current query" and frustrating when hitting that
  // a split second after the query is done.
  if (!line && errno == EAGAIN)
    return ScopedLine(strdup(""));
  return line;
}

#else

void SetupLineEditor() {}

using ScopedLine = std::unique_ptr<char>;

ScopedLine GetLine(const char* prompt) {
  printf("\r%80s\r%s", "", prompt);
  fflush(stdout);
  ScopedLine line(new char[1024]);
  if (!fgets(line.get(), 1024 - 1, stdin))
    return nullptr;
  if (strlen(line.get()) > 0)
    line.get()[strlen(line.get()) - 1] = 0;
  return line;
}

#endif  // PERFETTO_TP_LINENOISE

util::Status PrintStats() {
  auto it = g_tp->ExecuteQuery(
      "SELECT name, idx, source, value from stats "
      "where severity IN ('error', 'data_loss') and value > 0");

  bool first = true;
  for (uint32_t rows = 0; it.Next(); rows++) {
    if (first) {
      fprintf(stderr, "Error stats for this trace:\n");

      for (uint32_t i = 0; i < it.ColumnCount(); i++)
        fprintf(stderr, "%40s ", it.GetColumnName(i).c_str());
      fprintf(stderr, "\n");

      for (uint32_t i = 0; i < it.ColumnCount(); i++)
        fprintf(stderr, "%40s ", "----------------------------------------");
      fprintf(stderr, "\n");

      first = false;
    }

    for (uint32_t c = 0; c < it.ColumnCount(); c++) {
      auto value = it.Get(c);
      switch (value.type) {
        case SqlValue::Type::kNull:
          fprintf(stderr, "%-40.40s", "[NULL]");
          break;
        case SqlValue::Type::kDouble:
          fprintf(stderr, "%40f", value.double_value);
          break;
        case SqlValue::Type::kLong:
          fprintf(stderr, "%40" PRIi64, value.long_value);
          break;
        case SqlValue::Type::kString:
          fprintf(stderr, "%-40.40s", value.string_value);
          break;
        case SqlValue::Type::kBytes:
          printf("%-40.40s", "<raw bytes>");
          break;
      }
      fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");
  }

  util::Status status = it.Status();
  if (!status.ok()) {
    return util::ErrStatus("Error while iterating stats (%s)",
                           status.c_message());
  }
  return util::OkStatus();
}

util::Status ExportTraceToDatabase(const std::string& output_name) {
  PERFETTO_CHECK(output_name.find('\'') == std::string::npos);
  {
    base::ScopedFile fd(base::OpenFile(output_name, O_CREAT | O_RDWR, 0600));
    if (!fd)
      return util::ErrStatus("Failed to create file: %s", output_name.c_str());
    int res = ftruncate(fd.get(), 0);
    PERFETTO_CHECK(res == 0);
  }

  std::string attach_sql =
      "ATTACH DATABASE '" + output_name + "' AS perfetto_export";
  auto attach_it = g_tp->ExecuteQuery(attach_sql);
  bool attach_has_more = attach_it.Next();
  PERFETTO_DCHECK(!attach_has_more);

  util::Status status = attach_it.Status();
  if (!status.ok())
    return util::ErrStatus("SQLite error: %s", status.c_message());

  // Export real and virtual tables.
  auto tables_it = g_tp->ExecuteQuery(
      "SELECT name FROM perfetto_tables UNION "
      "SELECT name FROM sqlite_master WHERE type='table'");
  for (uint32_t rows = 0; tables_it.Next(); rows++) {
    std::string table_name = tables_it.Get(0).string_value;
    PERFETTO_CHECK(!base::Contains(table_name, '\''));
    std::string export_sql = "CREATE TABLE perfetto_export." + table_name +
                             " AS SELECT * FROM " + table_name;

    auto export_it = g_tp->ExecuteQuery(export_sql);
    bool export_has_more = export_it.Next();
    PERFETTO_DCHECK(!export_has_more);

    status = export_it.Status();
    if (!status.ok())
      return util::ErrStatus("SQLite error: %s", status.c_message());
  }
  status = tables_it.Status();
  if (!status.ok())
    return util::ErrStatus("SQLite error: %s", status.c_message());

  // Export views.
  auto views_it =
      g_tp->ExecuteQuery("SELECT sql FROM sqlite_master WHERE type='view'");
  for (uint32_t rows = 0; views_it.Next(); rows++) {
    std::string sql = views_it.Get(0).string_value;
    // View statements are of the form "CREATE VIEW name AS stmt". We need to
    // rewrite name to point to the exported db.
    const std::string kPrefix = "CREATE VIEW ";
    PERFETTO_CHECK(sql.find(kPrefix) == 0);
    sql = sql.substr(0, kPrefix.size()) + "perfetto_export." +
          sql.substr(kPrefix.size());

    auto export_it = g_tp->ExecuteQuery(sql);
    bool export_has_more = export_it.Next();
    PERFETTO_DCHECK(!export_has_more);

    status = export_it.Status();
    if (!status.ok())
      return util::ErrStatus("SQLite error: %s", status.c_message());
  }
  status = views_it.Status();
  if (!status.ok())
    return util::ErrStatus("SQLite error: %s", status.c_message());

  auto detach_it = g_tp->ExecuteQuery("DETACH DATABASE perfetto_export");
  bool detach_has_more = attach_it.Next();
  PERFETTO_DCHECK(!detach_has_more);
  status = detach_it.Status();
  return status.ok() ? util::OkStatus()
                     : util::ErrStatus("SQLite error: %s", status.c_message());
}

class ErrorPrinter : public google::protobuf::io::ErrorCollector {
  void AddError(int line, int col, const std::string& msg) override {
    PERFETTO_ELOG("%d:%d: %s", line, col, msg.c_str());
  }

  void AddWarning(int line, int col, const std::string& msg) override {
    PERFETTO_ILOG("%d:%d: %s", line, col, msg.c_str());
  }
};

// This function returns an indentifier for a metric suitable for use
// as an SQL table name (i.e. containing no forward or backward slashes).
std::string BaseName(std::string metric_path) {
  std::replace(metric_path.begin(), metric_path.end(), '\\', '/');
  auto slash_idx = metric_path.rfind('/');
  return slash_idx == std::string::npos ? metric_path
                                        : metric_path.substr(slash_idx + 1);
}

util::Status RegisterMetric(const std::string& register_metric) {
  std::string sql;
  base::ReadFile(register_metric, &sql);

  std::string path = "shell/" + BaseName(register_metric);

  return g_tp->RegisterMetric(path, sql);
}

util::Status ExtendMetricsProto(const std::string& extend_metrics_proto,
                                google::protobuf::DescriptorPool* pool) {
  google::protobuf::FileDescriptorSet desc_set;

  base::ScopedFile file(base::OpenFile(extend_metrics_proto, O_RDONLY));
  if (file.get() == -1) {
    return util::ErrStatus("Failed to open proto file %s",
                           extend_metrics_proto.c_str());
  }

  google::protobuf::io::FileInputStream stream(file.get());
  ErrorPrinter printer;
  google::protobuf::io::Tokenizer tokenizer(&stream, &printer);

  auto* file_desc = desc_set.add_file();
  google::protobuf::compiler::Parser parser;
  parser.Parse(&tokenizer, file_desc);

  file_desc->set_name(BaseName(extend_metrics_proto));
  pool->BuildFile(*file_desc);

  std::vector<uint8_t> metric_proto;
  metric_proto.resize(desc_set.ByteSizeLong());
  desc_set.SerializeToArray(metric_proto.data(),
                            static_cast<int>(metric_proto.size()));

  return g_tp->ExtendMetricsProto(metric_proto.data(), metric_proto.size());
}

enum OutputFormat {
  kBinaryProto,
  kTextProto,
  kJson,
  kNone,
};

util::Status RunMetrics(const std::vector<std::string>& metric_names,
                        OutputFormat format,
                        const google::protobuf::DescriptorPool& pool) {
  std::vector<uint8_t> metric_result;
  util::Status status = g_tp->ComputeMetric(metric_names, &metric_result);
  if (!status.ok()) {
    return util::ErrStatus("Error when computing metrics: %s",
                           status.c_message());
  }
  if (format == OutputFormat::kNone) {
    return util::OkStatus();
  }
  if (format == OutputFormat::kBinaryProto) {
    fwrite(metric_result.data(), sizeof(uint8_t), metric_result.size(), stdout);
    return util::OkStatus();
  }

  google::protobuf::DynamicMessageFactory factory(&pool);
  auto* descriptor = pool.FindMessageTypeByName("perfetto.protos.TraceMetrics");
  std::unique_ptr<google::protobuf::Message> metrics(
      factory.GetPrototype(descriptor)->New());
  metrics->ParseFromArray(metric_result.data(),
                          static_cast<int>(metric_result.size()));

  switch (format) {
    case OutputFormat::kTextProto: {
      std::string out;
      google::protobuf::TextFormat::PrintToString(*metrics, &out);
      fwrite(out.c_str(), sizeof(char), out.size(), stdout);
      break;
    }
    case OutputFormat::kJson: {
      // We need to instantiate field options from dynamic message factory
      // because otherwise it cannot parse our custom extensions.
      const google::protobuf::Message* field_options_prototype =
          factory.GetPrototype(
              pool.FindMessageTypeByName("google.protobuf.FieldOptions"));
      auto out = proto_to_json::MessageToJsonWithAnnotations(
          *metrics, field_options_prototype, 0);
      fwrite(out.c_str(), sizeof(char), out.size(), stdout);
      break;
    }
    case OutputFormat::kBinaryProto:
    case OutputFormat::kNone:
      PERFETTO_FATAL("Unsupported output format.");
  }
  return util::OkStatus();
}

void PrintQueryResultInteractively(Iterator* it,
                                   base::TimeNanos t_start,
                                   uint32_t column_width) {
  base::TimeNanos t_end = t_start;
  for (uint32_t rows = 0; it->Next(); rows++) {
    if (rows % 32 == 0) {
      if (rows > 0) {
        fprintf(stderr, "...\nType 'q' to stop, Enter for more records: ");
        fflush(stderr);
        char input[32];
        if (!fgets(input, sizeof(input) - 1, stdin))
          exit(0);
        if (input[0] == 'q')
          break;
      } else {
        t_end = base::GetWallTimeNs();
      }
      for (uint32_t i = 0; i < it->ColumnCount(); i++)
        printf("%-*.*s ", column_width, column_width,
               it->GetColumnName(i).c_str());
      printf("\n");

      std::string divider(column_width, '-');
      for (uint32_t i = 0; i < it->ColumnCount(); i++) {
        printf("%-*s ", column_width, divider.c_str());
      }
      printf("\n");
    }

    for (uint32_t c = 0; c < it->ColumnCount(); c++) {
      auto value = it->Get(c);
      switch (value.type) {
        case SqlValue::Type::kNull:
          printf("%-*s", column_width, "[NULL]");
          break;
        case SqlValue::Type::kDouble:
          printf("%*f", column_width, value.double_value);
          break;
        case SqlValue::Type::kLong:
          printf("%*" PRIi64, column_width, value.long_value);
          break;
        case SqlValue::Type::kString:
          printf("%-*.*s", column_width, column_width, value.string_value);
          break;
        case SqlValue::Type::kBytes:
          printf("%-*s", column_width, "<raw bytes>");
          break;
      }
      printf(" ");
    }
    printf("\n");
  }

  util::Status status = it->Status();
  if (!status.ok()) {
    PERFETTO_ELOG("SQLite error: %s", status.c_message());
  }
  printf("\nQuery executed in %.3f ms\n\n",
         static_cast<double>((t_end - t_start).count()) / 1E6);
}

util::Status PrintQueryResultAsCsv(Iterator* it, FILE* output) {
  for (uint32_t c = 0; c < it->ColumnCount(); c++) {
    if (c > 0)
      fprintf(output, ",");
    fprintf(output, "\"%s\"", it->GetColumnName(c).c_str());
  }
  fprintf(output, "\n");

  for (uint32_t rows = 0; it->Next(); rows++) {
    for (uint32_t c = 0; c < it->ColumnCount(); c++) {
      if (c > 0)
        fprintf(output, ",");

      auto value = it->Get(c);
      switch (value.type) {
        case SqlValue::Type::kNull:
          fprintf(output, "\"%s\"", "[NULL]");
          break;
        case SqlValue::Type::kDouble:
          fprintf(output, "%f", value.double_value);
          break;
        case SqlValue::Type::kLong:
          fprintf(output, "%" PRIi64, value.long_value);
          break;
        case SqlValue::Type::kString:
          fprintf(output, "\"%s\"", value.string_value);
          break;
        case SqlValue::Type::kBytes:
          fprintf(output, "\"%s\"", "<raw bytes>");
          break;
      }
    }
    fprintf(output, "\n");
  }
  return it->Status();
}

bool IsBlankLine(const std::string& buffer) {
  return buffer == "\n" || buffer == "\r\n";
}

bool IsCommentLine(const std::string& buffer) {
  return base::StartsWith(buffer, "--");
}

bool HasEndOfQueryDelimiter(const std::string& buffer) {
  return base::EndsWith(buffer, ";\n") || base::EndsWith(buffer, ";") ||
         base::EndsWith(buffer, ";\r\n");
}

util::Status LoadQueries(FILE* input, std::vector<std::string>* output) {
  char buffer[4096];
  while (!feof(input) && !ferror(input)) {
    std::string sql_query;
    while (fgets(buffer, sizeof(buffer), input)) {
      std::string line = base::TrimLeading(buffer);
      if (IsBlankLine(line))
        break;

      if (IsCommentLine(line))
        continue;

      sql_query.append(line);

      if (HasEndOfQueryDelimiter(line))
        break;
    }
    if (!sql_query.empty() && sql_query.back() == '\n')
      sql_query.resize(sql_query.size() - 1);

    // If we have a new line at the end of the file or an extra new line
    // somewhere in the file, we'll end up with an empty query which we should
    // just ignore.
    if (sql_query.empty())
      continue;

    output->push_back(sql_query);
  }
  if (ferror(input)) {
    return util::ErrStatus("Error reading query file");
  }
  return util::OkStatus();
}

util::Status RunQueriesWithoutOutput(const std::vector<std::string>& queries) {
  for (const auto& sql_query : queries) {
    PERFETTO_DLOG("Executing query: %s", sql_query.c_str());

    auto it = g_tp->ExecuteQuery(sql_query);
    RETURN_IF_ERROR(it.Status());
    if (it.Next()) {
      return util::ErrStatus("Unexpected result from a query.");
    }
  }
  return util::OkStatus();
}

util::Status RunQueriesAndPrintResult(const std::vector<std::string>& queries,
                                      FILE* output) {
  bool is_first_query = true;
  bool has_output = false;
  for (const auto& sql_query : queries) {
    // Add an extra newline separator between query results.
    if (!is_first_query)
      fprintf(output, "\n");
    is_first_query = false;

    PERFETTO_ILOG("Executing query: %s", sql_query.c_str());

    auto it = g_tp->ExecuteQuery(sql_query);
    RETURN_IF_ERROR(it.Status());
    if (it.ColumnCount() == 0) {
      bool it_has_more = it.Next();
      PERFETTO_DCHECK(!it_has_more);
      continue;
    }

    // If we have a single column with the name |suppress_query_output| that's
    // a hint to shell that it should not treat the query as having real
    // meaning.
    if (it.ColumnCount() == 1 &&
        it.GetColumnName(0) == "suppress_query_output") {
      // We should only see a single null value as this feature is usually used
      // as SELECT RUN_METRIC(<metric file>) as suppress_query_output and
      // RUN_METRIC returns a single null.
      bool has_next = it.Next();
      RETURN_IF_ERROR(it.Status());
      PERFETTO_DCHECK(has_next);
      PERFETTO_DCHECK(it.Get(0).is_null());

      has_next = it.Next();
      RETURN_IF_ERROR(it.Status());
      PERFETTO_DCHECK(!has_next);
      continue;
    }

    if (has_output) {
      return util::ErrStatus(
          "More than one query generated result rows. This is unsupported.");
    }
    has_output = true;
    RETURN_IF_ERROR(PrintQueryResultAsCsv(&it, output));
  }
  return util::OkStatus();
}

util::Status PrintPerfFile(const std::string& perf_file_path,
                           base::TimeNanos t_load,
                           base::TimeNanos t_run) {
  char buf[128];
  int count = snprintf(buf, sizeof(buf), "%" PRId64 ",%" PRId64,
                       static_cast<int64_t>(t_load.count()),
                       static_cast<int64_t>(t_run.count()));
  if (count < 0) {
    return util::ErrStatus("Failed to write perf data");
  }

  auto fd(base::OpenFile(perf_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666));
  if (!fd) {
    return util::ErrStatus("Failed to open perf file");
  }
  base::WriteAll(fd.get(), buf, static_cast<size_t>(count));
  return util::OkStatus();
}

struct CommandLineOptions {
  std::string perf_file_path;
  std::string query_file_path;
  std::string pre_metrics_path;
  std::string sqlite_file_path;
  std::string metric_names;
  std::string metric_output;
  std::string trace_file_path;
  std::string port_number;
  bool launch_shell = false;
  bool enable_httpd = false;
  bool wide = false;
  bool force_full_sort = false;
  std::string metatrace_path;
};

void PrintUsage(char** argv) {
  PERFETTO_ELOG(R"(
Interactive trace processor shell.
Usage: %s [OPTIONS] trace_file.pb

Options:
 -h, --help                           Prints this guide.
 -v, --version                        Prints the version of trace processor.
 -d, --debug                          Enable virtual table debugging.
 -W, --wide                           Prints interactive output with double
                                      column width.
 -p, --perf-file FILE                 Writes the time taken to ingest the trace
                                      and execute the queries to the given file.
                                      Only valid with -q or --run-metrics and
                                      the file will only be written if the
                                      execution is successful.
 -q, --query-file FILE                Read and execute an SQL query from a file.
                                      If used with --run-metrics, the query is
                                      executed after the selected metrics and
                                      the metrics output is suppressed.
 --pre-metrics FILE                   Read and execute an SQL query from a file.
                                      This query is executed before the selected
                                      metrics and can't output any results.
 -D, --httpd                          Enables the HTTP RPC server.
 --http-port PORT                     Specify what port to run HTTP RPC server.
 -i, --interactive                    Starts interactive mode even after a query
                                      file is specified with -q or
                                      --run-metrics.
 -e, --export FILE                    Export the contents of trace processor
                                      into an SQLite database after running any
                                      metrics or queries specified.
 --run-metrics x,y,z                  Runs a comma separated list of metrics and
                                      prints the result as a TraceMetrics proto
                                      to stdout. The specified can either be
                                      in-built metrics or SQL/proto files of
                                      extension metrics.
 --metrics-output=[binary|text|json]  Allows the output of --run-metrics to be
                                      specified in either proto binary, proto
                                      text format or JSON format (default: proto
                                      text).
 -m, --metatrace FILE                 Enables metatracing of trace processor
                                      writing the resulting trace into FILE.
 --full-sort                          Forces the trace processor into performing
                                      a full sort ignoring any windowing
                                      logic.)",
                argv[0]);
}

CommandLineOptions ParseCommandLineOptions(int argc, char** argv) {
  CommandLineOptions command_line_options;
  enum LongOption {
    OPT_RUN_METRICS = 1000,
    OPT_PRE_METRICS,
    OPT_METRICS_OUTPUT,
    OPT_FORCE_FULL_SORT,
    OPT_HTTP_PORT,
  };

  static const option long_options[] = {
      {"help", no_argument, nullptr, 'h'},
      {"version", no_argument, nullptr, 'v'},
      {"wide", no_argument, nullptr, 'W'},
      {"httpd", no_argument, nullptr, 'D'},
      {"interactive", no_argument, nullptr, 'i'},
      {"debug", no_argument, nullptr, 'd'},
      {"perf-file", required_argument, nullptr, 'p'},
      {"query-file", required_argument, nullptr, 'q'},
      {"export", required_argument, nullptr, 'e'},
      {"metatrace", required_argument, nullptr, 'm'},
      {"run-metrics", required_argument, nullptr, OPT_RUN_METRICS},
      {"pre-metrics", required_argument, nullptr, OPT_PRE_METRICS},
      {"metrics-output", required_argument, nullptr, OPT_METRICS_OUTPUT},
      {"full-sort", no_argument, nullptr, OPT_FORCE_FULL_SORT},
      {"http-port", required_argument, nullptr, OPT_HTTP_PORT},
      {nullptr, 0, nullptr, 0}};

  bool explicit_interactive = false;
  for (;;) {
    int option =
        getopt_long(argc, argv, "hvWiDdm:p:q:e:", long_options, nullptr);

    if (option == -1)
      break;  // EOF.

    if (option == 'v') {
      printf("%s\n", base::GetVersionString());
      exit(0);
    }

    if (option == 'i') {
      explicit_interactive = true;
      continue;
    }

    if (option == 'D') {
#if PERFETTO_BUILDFLAG(PERFETTO_TP_HTTPD)
      command_line_options.enable_httpd = true;
#else
      PERFETTO_FATAL("HTTP RPC module not supported in this build");
#endif
      continue;
    }

    if (option == 'W') {
      command_line_options.wide = true;
      continue;
    }

    if (option == 'd') {
      EnableSQLiteVtableDebugging();
      continue;
    }

    if (option == 'p') {
      command_line_options.perf_file_path = optarg;
      continue;
    }

    if (option == 'q') {
      command_line_options.query_file_path = optarg;
      continue;
    }

    if (option == 'e') {
      command_line_options.sqlite_file_path = optarg;
      continue;
    }

    if (option == 'm') {
      command_line_options.metatrace_path = optarg;
      continue;
    }

    if (option == OPT_PRE_METRICS) {
      command_line_options.pre_metrics_path = optarg;
      continue;
    }

    if (option == OPT_RUN_METRICS) {
      command_line_options.metric_names = optarg;
      continue;
    }

    if (option == OPT_METRICS_OUTPUT) {
      command_line_options.metric_output = optarg;
      continue;
    }

    if (option == OPT_FORCE_FULL_SORT) {
      command_line_options.force_full_sort = true;
      continue;
    }

    if (option == OPT_HTTP_PORT) {
      command_line_options.port_number = optarg;
      continue;
    }

    PrintUsage(argv);
    exit(option == 'h' ? 0 : 1);
  }

  command_line_options.launch_shell =
      explicit_interactive || (command_line_options.pre_metrics_path.empty() &&
                               command_line_options.metric_names.empty() &&
                               command_line_options.query_file_path.empty() &&
                               command_line_options.sqlite_file_path.empty());

  // Only allow non-interactive queries to emit perf data.
  if (!command_line_options.perf_file_path.empty() &&
      command_line_options.launch_shell) {
    PrintUsage(argv);
    exit(1);
  }

  // The only case where we allow omitting the trace file path is when running
  // in --http mode. In all other cases, the last argument must be the trace
  // file.
  if (optind == argc - 1 && argv[optind]) {
    command_line_options.trace_file_path = argv[optind];
  } else if (!command_line_options.enable_httpd) {
    PrintUsage(argv);
    exit(1);
  }
  return command_line_options;
}

void ExtendPoolWithBinaryDescriptor(google::protobuf::DescriptorPool& pool,
                                    const void* data,
                                    int size) {
  google::protobuf::FileDescriptorSet desc_set;
  desc_set.ParseFromArray(data, size);
  for (const auto& desc : desc_set.file()) {
    pool.BuildFile(desc);
  }
}

util::Status LoadTrace(const std::string& trace_file_path, double* size_mb) {
  util::Status read_status =
      ReadTrace(g_tp, trace_file_path.c_str(), [&size_mb](size_t parsed_size) {
        *size_mb = static_cast<double>(parsed_size) / 1E6;
        fprintf(stderr, "\rLoading trace: %.2f MB\r", *size_mb);
      });
  if (!read_status.ok()) {
    return util::ErrStatus("Could not read trace file (path: %s): %s",
                           trace_file_path.c_str(), read_status.c_message());
  }

  std::unique_ptr<profiling::Symbolizer> symbolizer =
      profiling::LocalSymbolizerOrDie(profiling::GetPerfettoBinaryPath(),
                                      getenv("PERFETTO_SYMBOLIZER_MODE"));

  if (symbolizer) {
    profiling::SymbolizeDatabase(
        g_tp, symbolizer.get(), [](const std::string& trace_proto) {
          std::unique_ptr<uint8_t[]> buf(new uint8_t[trace_proto.size()]);
          memcpy(buf.get(), trace_proto.data(), trace_proto.size());
          auto status = g_tp->Parse(std::move(buf), trace_proto.size());
          if (!status.ok()) {
            PERFETTO_DFATAL_OR_ELOG("Failed to parse: %s",
                                    status.message().c_str());
            return;
          }
        });
    g_tp->NotifyEndOfFile();
  }

  auto maybe_map = profiling::GetPerfettoProguardMapPath();
  if (!maybe_map.empty()) {
    profiling::ReadProguardMapsToDeobfuscationPackets(
        maybe_map, [](const std::string& trace_proto) {
          std::unique_ptr<uint8_t[]> buf(new uint8_t[trace_proto.size()]);
          memcpy(buf.get(), trace_proto.data(), trace_proto.size());
          auto status = g_tp->Parse(std::move(buf), trace_proto.size());
          if (!status.ok()) {
            PERFETTO_DFATAL_OR_ELOG("Failed to parse: %s",
                                    status.message().c_str());
            return;
          }
        });
  }
  return util::OkStatus();
}

util::Status RunQueries(const std::string& query_file_path,
                        bool expect_output) {
  std::vector<std::string> queries;
  base::ScopedFstream file(fopen(query_file_path.c_str(), "r"));
  if (!file) {
    return util::ErrStatus("Could not open query file (path: %s)",
                           query_file_path.c_str());
  }
  RETURN_IF_ERROR(LoadQueries(file.get(), &queries));

  util::Status status;
  if (expect_output) {
    status = RunQueriesAndPrintResult(queries, stdout);
  } else {
    status = RunQueriesWithoutOutput(queries);
  }
  if (!status.ok()) {
    return util::ErrStatus("Encountered error while running queries: %s",
                           status.c_message());
  }
  return util::OkStatus();
}

util::Status RunMetrics(const CommandLineOptions& options) {
  // Descriptor pool used for printing output as textproto.
  // Building on top of generated pool so default protos in
  // google.protobuf.descriptor.proto are available.
  google::protobuf::DescriptorPool pool(
      google::protobuf::DescriptorPool::generated_pool());
  ExtendPoolWithBinaryDescriptor(pool, kMetricsDescriptor.data(),
                                 kMetricsDescriptor.size());
  ExtendPoolWithBinaryDescriptor(pool, kAllChromeMetricsDescriptor.data(),
                                 kAllChromeMetricsDescriptor.size());

  std::vector<std::string> metrics;
  for (base::StringSplitter ss(options.metric_names, ','); ss.Next();) {
    metrics.emplace_back(ss.cur_token());
  }

  // For all metrics which are files, register them and extend the metrics
  // proto.
  for (size_t i = 0; i < metrics.size(); ++i) {
    const std::string& metric_or_path = metrics[i];

    // If there is no extension, we assume it is a builtin metric.
    auto ext_idx = metric_or_path.rfind('.');
    if (ext_idx == std::string::npos)
      continue;

    std::string no_ext_name = metric_or_path.substr(0, ext_idx);

    // The proto must be extended before registering the metric.
    util::Status status = ExtendMetricsProto(no_ext_name + ".proto", &pool);
    if (!status.ok()) {
      return util::ErrStatus("Unable to extend metrics proto %s: %s",
                             metric_or_path.c_str(), status.c_message());
    }

    status = RegisterMetric(no_ext_name + ".sql");
    if (!status.ok()) {
      return util::ErrStatus("Unable to register metric %s: %s",
                             metric_or_path.c_str(), status.c_message());
    }

    metrics[i] = BaseName(no_ext_name);
  }

  OutputFormat format;
  if (!options.query_file_path.empty()) {
    format = OutputFormat::kNone;
  } else if (options.metric_output == "binary") {
    format = OutputFormat::kBinaryProto;
  } else if (options.metric_output == "json") {
    format = OutputFormat::kJson;
  } else {
    format = OutputFormat::kTextProto;
  }
  return RunMetrics(std::move(metrics), format, pool);
}

void PrintShellUsage() {
  PERFETTO_ELOG(
      "Available commands:\n"
      ".quit, .q    Exit the shell.\n"
      ".help        This text.\n"
      ".dump FILE   Export the trace as a sqlite database.\n"
      ".read FILE   Executes the queries in the FILE.\n"
      ".reset       Destroys all tables/view created by the user.\n");
}

util::Status StartInteractiveShell(uint32_t column_width) {
  SetupLineEditor();

  for (;;) {
    ScopedLine line = GetLine("> ");
    if (!line)
      break;
    if (strcmp(line.get(), "") == 0) {
      printf("If you want to quit either type .q or press CTRL-D (EOF)\n");
      continue;
    }
    if (line.get()[0] == '.') {
      char command[32] = {};
      char arg[1024] = {};
      sscanf(line.get() + 1, "%31s %1023s", command, arg);
      if (strcmp(command, "quit") == 0 || strcmp(command, "q") == 0) {
        break;
      } else if (strcmp(command, "help") == 0) {
        PrintShellUsage();
      } else if (strcmp(command, "dump") == 0 && strlen(arg)) {
        if (!ExportTraceToDatabase(arg).ok())
          PERFETTO_ELOG("Database export failed");
      } else if (strcmp(command, "reset") == 0) {
        g_tp->RestoreInitialTables();
      } else if (strcmp(command, "read") == 0 && strlen(arg)) {
        util::Status status = RunQueries(arg, true);
        if (!status.ok()) {
          PERFETTO_ELOG("%s", status.c_message());
        }
      } else {
        PrintShellUsage();
      }
      continue;
    }

    base::TimeNanos t_start = base::GetWallTimeNs();
    auto it = g_tp->ExecuteQuery(line.get());
    PrintQueryResultInteractively(&it, t_start, column_width);
  }
  return util::OkStatus();
}

util::Status TraceProcessorMain(int argc, char** argv) {
  CommandLineOptions options = ParseCommandLineOptions(argc, argv);

  Config config;
  config.sorting_mode = options.force_full_sort
                            ? SortingMode::kForceFullSort
                            : SortingMode::kDefaultHeuristics;

  std::unique_ptr<TraceProcessor> tp = TraceProcessor::CreateInstance(config);
  g_tp = tp.get();

  // Enable metatracing as soon as possible.
  if (!options.metatrace_path.empty()) {
    tp->EnableMetatrace();
  }

  base::TimeNanos t_load{};
  if (!options.trace_file_path.empty()) {
    base::TimeNanos t_load_start = base::GetWallTimeNs();
    double size_mb = 0;
    RETURN_IF_ERROR(LoadTrace(options.trace_file_path, &size_mb));
    t_load = base::GetWallTimeNs() - t_load_start;

    double t_load_s = static_cast<double>(t_load.count()) / 1E9;
    PERFETTO_ILOG("Trace loaded: %.2f MB (%.1f MB/s)", size_mb,
                  size_mb / t_load_s);

    RETURN_IF_ERROR(PrintStats());
  }

#if PERFETTO_BUILDFLAG(PERFETTO_TP_HTTPD)
  if (options.enable_httpd) {
    RunHttpRPCServer(std::move(tp), options.port_number);
    PERFETTO_FATAL("Should never return");
  }
#endif

#if PERFETTO_HAS_SIGNAL_H()
  signal(SIGINT, [](int) { g_tp->InterruptQuery(); });
#endif

  base::TimeNanos t_query_start = base::GetWallTimeNs();
  if (!options.pre_metrics_path.empty()) {
    RETURN_IF_ERROR(RunQueries(options.pre_metrics_path, false));
  }

  if (!options.metric_names.empty()) {
    RETURN_IF_ERROR(RunMetrics(options));
  }

  if (!options.query_file_path.empty()) {
    RETURN_IF_ERROR(RunQueries(options.query_file_path, true));
  }
  base::TimeNanos t_query = base::GetWallTimeNs() - t_query_start;

  if (!options.sqlite_file_path.empty()) {
    RETURN_IF_ERROR(ExportTraceToDatabase(options.sqlite_file_path));
  }

  if (options.launch_shell) {
    RETURN_IF_ERROR(StartInteractiveShell(options.wide ? 40 : 20));
  } else if (!options.perf_file_path.empty()) {
    RETURN_IF_ERROR(PrintPerfFile(options.perf_file_path, t_load, t_query));
  }

  if (!options.metatrace_path.empty()) {
    std::vector<uint8_t> serialized;
    util::Status status = g_tp->DisableAndReadMetatrace(&serialized);
    if (!status.ok())
      return status;

    auto file =
        base::OpenFile(options.metatrace_path, O_CREAT | O_RDWR | O_TRUNC);
    if (!file)
      return util::ErrStatus("Unable to open metatrace file");

    ssize_t res = base::WriteAll(*file, serialized.data(), serialized.size());
    if (res < 0)
      return util::ErrStatus("Error while writing metatrace file");
  }

  return util::OkStatus();
}

}  // namespace

}  // namespace trace_processor
}  // namespace perfetto

int main(int argc, char** argv) {
  auto status = perfetto::trace_processor::TraceProcessorMain(argc, argv);
  if (!status.ok()) {
    PERFETTO_ELOG("%s", status.c_message());
    return 1;
  }
  return 0;
}
