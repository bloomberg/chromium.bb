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

#include "src/trace_processor/export_json.h"

#include "perfetto/base/temp_file.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <json/reader.h>
#include <json/value.h>

namespace perfetto {
namespace trace_processor {
namespace json {
namespace {

std::string ReadFile(FILE* input) {
  fseek(input, 0, SEEK_SET);
  const int kBufSize = 1000;
  char buffer[kBufSize];
  size_t ret = fread(buffer, sizeof(char), kBufSize, input);
  EXPECT_GT(ret, 0);
  return std::string(buffer, ret);
}

TEST(ExportJsonTest, EmptyStorage) {
  TraceStorage storage;

  base::TempFile temp_file = base::TempFile::Create();
  FILE* output = fopen(temp_file.path().c_str(), "w+");
  int code = ExportJson(&storage, output);

  EXPECT_EQ(code, kResultOk);

  Json::Reader reader;
  Json::Value result;

  EXPECT_TRUE(reader.parse(ReadFile(output), result));
  EXPECT_EQ(result["traceEvents"].size(), 0);
}

TEST(ExportJsonTest, StorageWithOneSlice) {
  const int64_t kTimestamp = 10000000;
  const int64_t kDuration = 10000;
  const int64_t kThreadID = 100;
  const char* kCategory = "cat";
  const char* kName = "name";

  TraceStorage storage;
  UniqueTid utid = storage.AddEmptyThread(kThreadID);
  StringId cat_id = storage.InternString(base::StringView(kCategory));
  StringId name_id = storage.InternString(base::StringView(kName));
  storage.mutable_nestable_slices()->AddSlice(
      kTimestamp, kDuration, utid, RefType::kRefUtid, cat_id, name_id, 0, 0, 0);

  base::TempFile temp_file = base::TempFile::Create();
  FILE* output = fopen(temp_file.path().c_str(), "w+");
  int code = ExportJson(&storage, output);

  EXPECT_EQ(code, kResultOk);

  Json::Reader reader;
  Json::Value result;
  EXPECT_TRUE(reader.parse(ReadFile(output), result));
  EXPECT_EQ(result["traceEvents"].size(), 1);

  Json::Value event = result["traceEvents"][0];
  EXPECT_EQ(event["ph"].asString(), "X");
  EXPECT_EQ(event["ts"].asInt64(), kTimestamp / 1000);
  EXPECT_EQ(event["dur"].asInt64(), kDuration / 1000);
  EXPECT_EQ(event["tid"].asUInt(), kThreadID);
  EXPECT_EQ(event["cat"].asString(), kCategory);
  EXPECT_EQ(event["name"].asString(), kName);
}

TEST(ExportJsonTest, StorageWithThreadName) {
  const int64_t kThreadID = 100;
  const char* kName = "thread";

  TraceStorage storage;
  UniqueTid utid = storage.AddEmptyThread(kThreadID);
  StringId name_id = storage.InternString(base::StringView(kName));
  storage.GetMutableThread(utid)->name_id = name_id;

  base::TempFile temp_file = base::TempFile::Create();
  FILE* output = fopen(temp_file.path().c_str(), "w+");
  int code = ExportJson(&storage, output);

  EXPECT_EQ(code, kResultOk);

  Json::Reader reader;
  Json::Value result;
  EXPECT_TRUE(reader.parse(ReadFile(output), result));
  EXPECT_EQ(result["traceEvents"].size(), 1);

  Json::Value event = result["traceEvents"][0];
  EXPECT_EQ(event["ph"].asString(), "M");
  EXPECT_EQ(event["tid"].asUInt(), kThreadID);
  EXPECT_EQ(event["name"].asString(), "thread_name");
  EXPECT_EQ(event["args"]["name"].asString(), kName);
}

TEST(ExportJsonTest, WrongRefType) {
  TraceStorage storage;
  storage.mutable_nestable_slices()->AddSlice(0, 0, 0, RefType::kRefCpuId, 0, 0,
                                              0, 0, 0);

  base::TempFile temp_file = base::TempFile::Create();
  FILE* output = fopen(temp_file.path().c_str(), "w+");
  int code = ExportJson(&storage, output);

  EXPECT_EQ(code, kResultWrongRefType);
}

TEST(ExportJsonTest, StorageWithMetadata) {
  const char* kDescription = "description";
  const char* kBenchmarkName = "benchmark name";
  const char* kStoryName = "story name";
  const char* kStoryTag1 = "tag1";
  const char* kStoryTag2 = "tag2";
  const int64_t kBenchmarkStart = 1000000;
  const int64_t kStoryStart = 2000000;
  const bool kHadFailures = true;

  TraceStorage storage;

  StringId desc_id = storage.InternString(base::StringView(kDescription));
  Variadic description = Variadic::String(desc_id);
  storage.SetMetadata(metadata::benchmark_description, description);

  StringId benchmark_name_id =
      storage.InternString(base::StringView(kBenchmarkName));
  Variadic benchmark_name = Variadic::String(benchmark_name_id);
  storage.SetMetadata(metadata::benchmark_name, benchmark_name);

  StringId story_name_id = storage.InternString(base::StringView(kStoryName));
  Variadic story_name = Variadic::String(story_name_id);
  storage.SetMetadata(metadata::benchmark_story_name, story_name);

  StringId tag1_id = storage.InternString(base::StringView(kStoryTag1));
  StringId tag2_id = storage.InternString(base::StringView(kStoryTag2));
  Variadic tag1 = Variadic::String(tag1_id);
  Variadic tag2 = Variadic::String(tag2_id);
  storage.AppendMetadata(metadata::benchmark_story_tags, tag1);
  storage.AppendMetadata(metadata::benchmark_story_tags, tag2);

  Variadic benchmark_start = Variadic::Integer(kBenchmarkStart);
  storage.SetMetadata(metadata::benchmark_start_time_us, benchmark_start);

  Variadic story_start = Variadic::Integer(kStoryStart);
  storage.SetMetadata(metadata::benchmark_story_run_time_us, story_start);

  Variadic had_failures = Variadic::Integer(kHadFailures);
  storage.SetMetadata(metadata::benchmark_had_failures, had_failures);

  base::TempFile temp_file = base::TempFile::Create();
  FILE* output = fopen(temp_file.path().c_str(), "w+");
  int code = ExportJson(&storage, output);

  EXPECT_EQ(code, kResultOk);

  Json::Reader reader;
  Json::Value result;

  EXPECT_TRUE(reader.parse(ReadFile(output), result));
  EXPECT_TRUE(result.isMember("metadata"));
  EXPECT_TRUE(result["metadata"].isMember("telemetry"));
  Json::Value telemetry_metadata = result["metadata"]["telemetry"];

  EXPECT_EQ(telemetry_metadata["benchmarkDescriptions"].size(), 1);
  EXPECT_EQ(telemetry_metadata["benchmarkDescriptions"][0].asString(),
            kDescription);

  EXPECT_EQ(telemetry_metadata["benchmarks"].size(), 1);
  EXPECT_EQ(telemetry_metadata["benchmarks"][0].asString(), kBenchmarkName);

  EXPECT_EQ(telemetry_metadata["stories"].size(), 1);
  EXPECT_EQ(telemetry_metadata["stories"][0].asString(), kStoryName);

  EXPECT_EQ(telemetry_metadata["storyTags"].size(), 2);
  EXPECT_EQ(telemetry_metadata["storyTags"][0].asString(), kStoryTag1);
  EXPECT_EQ(telemetry_metadata["storyTags"][1].asString(), kStoryTag2);

  EXPECT_EQ(telemetry_metadata["benchmarkStart"].asInt(),
            kBenchmarkStart / 1000.0);

  EXPECT_EQ(telemetry_metadata["traceStart"].asInt(), kStoryStart / 1000.0);

  EXPECT_EQ(telemetry_metadata["hadFailures"].size(), 1);
  EXPECT_EQ(telemetry_metadata["hadFailures"][0].asBool(), kHadFailures);
}

}  // namespace
}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto
