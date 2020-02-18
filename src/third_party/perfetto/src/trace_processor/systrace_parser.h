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

#ifndef SRC_TRACE_PROCESSOR_SYSTRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_SYSTRACE_PARSER_H_

#include <ostream>

#include "src/trace_processor/trace_processor_context.h"

#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

namespace systrace_utils {

// Visible for unittesting.
enum class SystraceParseResult { kFailure = 0, kUnsupported, kSuccess };

// Visible for unittesting.
struct SystraceTracePoint {
  SystraceTracePoint() {}

  static SystraceTracePoint B(uint32_t tgid, base::StringView name) {
    return SystraceTracePoint('B', tgid, std::move(name), 0);
  }

  static SystraceTracePoint E(uint32_t tgid) {
    return SystraceTracePoint('E', tgid, {}, 0);
  }

  static SystraceTracePoint C(uint32_t tgid,
                              base::StringView name,
                              double value) {
    return SystraceTracePoint('C', tgid, std::move(name), value);
  }

  static SystraceTracePoint S(uint32_t tgid,
                              base::StringView name,
                              double value) {
    return SystraceTracePoint('S', tgid, std::move(name), value);
  }

  static SystraceTracePoint F(uint32_t tgid,
                              base::StringView name,
                              double value) {
    return SystraceTracePoint('F', tgid, std::move(name), value);
  }

  SystraceTracePoint(char p, uint32_t tg, base::StringView n, double v)
      : phase(p), tgid(tg), name(std::move(n)), value(v) {}

  // Phase can be one of B, E, C, S, F.
  char phase = '\0';

  uint32_t tgid = 0;

  // For phase = 'B' and phase = 'C' only.
  base::StringView name;

  // For phase = 'C' only.
  double value = 0;

  // Visible for unittesting.
  friend std::ostream& operator<<(std::ostream& os,
                                  const SystraceTracePoint& point) {
    return os << "SystraceTracePoint{'" << point.phase << "', " << point.tgid
              << ", \"" << point.name.ToStdString() << "\", " << point.value
              << "}";
  }
};

// We have to handle trace_marker events of a few different types:
// 1. some random text
// 2. B|1636|pokeUserActivity
// 3. E|1636
// 4. C|1636|wq:monitor|0
// 5. S|1636|frame_capture|123
// 6. F|1636|frame_capture|456
// Visible for unittesting.
inline SystraceParseResult ParseSystraceTracePoint(base::StringView str,
                                                   SystraceTracePoint* out) {
  const char* s = str.data();
  size_t len = str.size();
  *out = {};

  if (len < 2)
    return SystraceParseResult::kFailure;

  // If str matches '[BEC]\|[0-9]+[\|\n]?' set tgid_length to the length of
  // the number. Otherwise return kFailure.
  if (s[1] != '|' && s[1] != '\n')
    return SystraceParseResult::kFailure;

  char ph = s[0];
  if (ph != 'B' && ph != 'E' && ph != 'C' && ph != 'S' && ph != 'F')
    return SystraceParseResult::kFailure;

  size_t tgid_length = 0;
  for (size_t i = 2; i < len; i++) {
    if (s[i] == '|' || s[i] == '\n') {
      break;
    }
    if (s[i] < '0' || s[i] > '9')
      return SystraceParseResult::kFailure;
    tgid_length++;
  }

  if (tgid_length == 0) {
    out->tgid = 0;
  } else {
    std::string tgid_str(s + 2, tgid_length);
    out->tgid = static_cast<uint32_t>(std::stoi(tgid_str.c_str()));
  }

  out->phase = ph;
  switch (ph) {
    case 'B': {
      size_t name_index = 2 + tgid_length + 1;
      out->name = base::StringView(
          s + name_index, len - name_index - (s[len - 1] == '\n' ? 1 : 0));
      if (out->name.size() == 0)
        return SystraceParseResult::kFailure;
      return SystraceParseResult::kSuccess;
    }
    case 'E': {
      return SystraceParseResult::kSuccess;
    }
    case 'S':
    case 'F':
    case 'C': {
      size_t name_index = 2 + tgid_length + 1;
      base::Optional<size_t> name_length;
      for (size_t i = name_index; i < len; i++) {
        if (s[i] == '|') {
          name_length = i - name_index;
          break;
        }
      }
      if (!name_length.has_value())
        return SystraceParseResult::kFailure;
      out->name = base::StringView(s + name_index, name_length.value());

      size_t value_index = name_index + name_length.value() + 1;
      size_t value_len = len - value_index;
      if (value_len == 0)
        return SystraceParseResult::kFailure;
      std::string value_str(s + value_index, value_len);
      out->value = std::stod(value_str.c_str());
      return SystraceParseResult::kSuccess;
    }
    default:
      return SystraceParseResult::kFailure;
  }
}

// Visible for unittesting.
inline bool operator==(const SystraceTracePoint& x,
                       const SystraceTracePoint& y) {
  return std::tie(x.phase, x.tgid, x.name, x.value) ==
         std::tie(y.phase, y.tgid, y.name, y.value);
}

}  // namespace systrace_utils

class SystraceParser {
 public:
  explicit SystraceParser(TraceProcessorContext*);

  void ParsePrintEvent(int64_t ts, uint32_t pid, base::StringView event);

  void ParseZeroEvent(int64_t ts,
                      uint32_t pid,
                      int32_t flag,
                      base::StringView name,
                      uint32_t tgid,
                      int64_t value);

 private:
  void ParseSystracePoint(int64_t ts,
                          uint32_t pid,
                          systrace_utils::SystraceTracePoint event);

  TraceProcessorContext* const context_;
  const StringId lmk_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SYSTRACE_PARSER_H_
