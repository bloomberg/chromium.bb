// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/text_log_upload_list.h"

#include <algorithm>
#include <sstream>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

constexpr size_t kUploadTimeIndex = 0;
constexpr size_t kCaptureTimeIndex = 3;

constexpr char kJsonLogKeyUploadId[] = "upload_id";
constexpr char kJsonLogKeyUploadTime[] = "upload_time";
constexpr char kJsonLogKeyLocalId[] = "local_id";
constexpr char kJsonLogKeyCaptureTime[] = "capture_time";
constexpr char kJsonLogKeyState[] = "state";
constexpr char kJsonLogKeySource[] = "source";

std::vector<std::string> SplitIntoLines(const std::string& file_contents) {
  return base::SplitString(file_contents, base::kWhitespaceASCII,
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> SplitIntoComponents(const std::string& line) {
  return base::SplitString(line, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

bool CheckCsvUploadListOutOfRange(const std::string& line,
                                  const base::Time& begin,
                                  const base::Time& end) {
  std::vector<std::string> components = SplitIntoComponents(line);
  double seconds_since_epoch;
  if (components.size() > kUploadTimeIndex &&
      !components[kUploadTimeIndex].empty() &&
      base::StringToDouble(components[kUploadTimeIndex],
                           &seconds_since_epoch)) {
    base::Time upload_time = base::Time::FromDoubleT(seconds_since_epoch);
    if (begin <= upload_time && upload_time <= end)
      return false;
  }

  if (components.size() > kCaptureTimeIndex &&
      !components[kCaptureTimeIndex].empty() &&
      base::StringToDouble(components[kCaptureTimeIndex],
                           &seconds_since_epoch)) {
    base::Time capture_time = base::Time::FromDoubleT(seconds_since_epoch);
    if (begin <= capture_time && capture_time <= end)
      return false;
  }

  return true;
}

bool CheckFieldOutOfRange(const std::string* time_string,
                          const base::Time& begin,
                          const base::Time& end) {
  if (time_string) {
    double upload_time_double = 0.0;
    if (base::StringToDouble(*time_string, &upload_time_double)) {
      base::Time upload_time = base::Time::FromDoubleT(upload_time_double);
      if (begin <= upload_time && upload_time <= end)
        return false;
    }
  }
  return true;
}

bool CheckJsonUploadListOutOfRange(const base::Value& dict,
                                   const base::Time& begin,
                                   const base::Time& end) {
  const std::string* upload_time_string =
      dict.FindStringKey(kJsonLogKeyUploadTime);

  const std::string* capture_time_string =
      dict.FindStringKey(kJsonLogKeyCaptureTime);

  return CheckFieldOutOfRange(upload_time_string, begin, end) &&
         CheckFieldOutOfRange(capture_time_string, begin, end);
}

// Tries to parse one upload log line based on CSV format, then converts it to
// a UploadInfo entry. If the conversion succeeds, it returns a valid UploadInfo
// instance. Otherwise, it returns nullptr.
std::unique_ptr<TextLogUploadList::UploadInfo> TryParseCsvLogEntry(
    const std::string& log_line) {
  std::vector<std::string> components = SplitIntoComponents(log_line);
  // Skip any blank (or corrupted) lines.
  if (components.size() < 2 || components.size() > 5)
    return nullptr;
  base::Time upload_time;
  double seconds_since_epoch;
  if (!components[kUploadTimeIndex].empty()) {
    if (!base::StringToDouble(components[kUploadTimeIndex],
                              &seconds_since_epoch))
      return nullptr;
    upload_time = base::Time::FromDoubleT(seconds_since_epoch);
  }
  auto info = std::make_unique<TextLogUploadList::UploadInfo>(components[1],
                                                              upload_time);

  // Add local ID if present.
  if (components.size() > 2)
    info->local_id = components[2];

  // Add capture time if present.
  if (components.size() > kCaptureTimeIndex &&
      !components[kCaptureTimeIndex].empty() &&
      base::StringToDouble(components[kCaptureTimeIndex],
                           &seconds_since_epoch)) {
    info->capture_time = base::Time::FromDoubleT(seconds_since_epoch);
  }

  int state;
  if (components.size() > 4 && !components[4].empty() &&
      base::StringToInt(components[4], &state)) {
    info->state = static_cast<TextLogUploadList::UploadInfo::State>(state);
  }

  return info;
}

// Tries to parse one upload log dictionary based on line-based JSON format (no
// internal additional newline is permitted), then converts it to a UploadInfo
// entry. If the conversion succeeds, it returns a valid UploadInfo instance.
// Otherwise, it returns nullptr.
std::unique_ptr<TextLogUploadList::UploadInfo> TryParseJsonLogEntry(
    const base::Value& dict) {
  // Parse upload_id.
  const base::Value* upload_id_value = dict.FindKey(kJsonLogKeyUploadId);
  std::string upload_id;
  if (upload_id_value && !upload_id_value->GetAsString(&upload_id))
    return nullptr;

  // Parse upload_time.
  const std::string* upload_time_string =
      dict.FindStringKey(kJsonLogKeyUploadTime);
  double upload_time_double = 0.0;
  if (upload_time_string &&
      !base::StringToDouble(*upload_time_string, &upload_time_double))
    return nullptr;

  auto info = std::make_unique<TextLogUploadList::UploadInfo>(
      upload_id, base::Time::FromDoubleT(upload_time_double));

  // Parse local_id.
  const std::string* local_id = dict.FindStringKey(kJsonLogKeyLocalId);
  if (local_id)
    info->local_id = *local_id;

  // Parse capture_time.
  const std::string* capture_time_string =
      dict.FindStringKey(kJsonLogKeyCaptureTime);
  double capture_time_double = 0.0;
  if (capture_time_string &&
      base::StringToDouble(*capture_time_string, &capture_time_double))
    info->capture_time = base::Time::FromDoubleT(capture_time_double);

  // Parse state.
  base::Optional<int> state = dict.FindIntKey(kJsonLogKeyState);
  if (state.has_value())
    info->state =
        static_cast<TextLogUploadList::UploadInfo::State>(state.value());

  // Parse source.
  const std::string* source = dict.FindStringKey(kJsonLogKeySource);
  if (source)
    info->source = *source;

  return info;
}

}  // namespace

TextLogUploadList::TextLogUploadList(const base::FilePath& upload_log_path)
    : upload_log_path_(upload_log_path) {}

TextLogUploadList::~TextLogUploadList() = default;

std::vector<UploadList::UploadInfo> TextLogUploadList::LoadUploadList() {
  std::vector<UploadInfo> uploads;

  if (base::PathExists(upload_log_path_)) {
    std::string contents;
    base::ReadFileToString(upload_log_path_, &contents);
    ParseLogEntries(SplitIntoLines(contents), &uploads);
  }

  return uploads;
}

void TextLogUploadList::ClearUploadList(const base::Time& begin,
                                        const base::Time& end) {
  if (!base::PathExists(upload_log_path_))
    return;

  std::string contents;
  base::ReadFileToString(upload_log_path_, &contents);
  std::vector<std::string> log_entries = SplitIntoLines(contents);

  std::ostringstream new_contents_stream;
  for (const std::string& line : log_entries) {
    base::Optional<base::Value> json = base::JSONReader::Read(line);
    bool should_copy = false;

    if (json.has_value())
      should_copy = CheckJsonUploadListOutOfRange(json.value(), begin, end);
    else
      should_copy = CheckCsvUploadListOutOfRange(line, begin, end);

    if (should_copy)
      new_contents_stream << line << std::endl;
  }

  std::string new_contents = new_contents_stream.str();
  if (new_contents.size() == 0) {
    base::DeleteFile(upload_log_path_, /*recursive*/ false);
  } else {
    base::WriteFile(upload_log_path_, new_contents.c_str(),
                    new_contents.size());
  }
}

void TextLogUploadList::ParseLogEntries(
    const std::vector<std::string>& log_entries,
    std::vector<UploadInfo>* uploads) {
  std::vector<std::string>::const_reverse_iterator i;
  for (i = log_entries.rbegin(); i != log_entries.rend(); ++i) {
    const std::string& line = *i;
    std::unique_ptr<UploadInfo> info = nullptr;
    base::Optional<base::Value> json = base::JSONReader::Read(line);

    if (json.has_value())
      info = TryParseJsonLogEntry(json.value());
    else
      info = TryParseCsvLogEntry(line);

    if (info)
      uploads->push_back(*info);
  }
}
