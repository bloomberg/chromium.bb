// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/queryable_data_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"

namespace {
constexpr char kBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/queryable_data_bindings.js");

// Builds a script snippet which, when executed, will inserts entries into the
// QueryableData JavaScript object. The scripts are inserted in the order
// provided by the iterator.
template <typename Iterator>
base::Optional<fuchsia::mem::Buffer> BuildScriptFromEntries(Iterator begin,
                                                            Iterator end) {
  // Prepare values for serialization as a JavaScript object literal.
  base::DictionaryValue output;
  for (auto it = begin; it != end; ++it) {
    const chromium::cast::QueryableDataEntry& current = *it;
    base::Optional<base::Value> value_parsed =
        base::JSONReader::Read(current.json_value);
    if (!value_parsed) {
      LOG(WARNING) << "Couldn't parse QueryableData item: " << current.key;
      continue;
    }

    output.SetKey(current.key, std::move(*value_parsed));
  }

  std::string output_json;
  if (!base::JSONWriter::Write(output, &output_json)) {
    LOG(ERROR)
        << "An unexpected error occurred while serializing QueryableData.";
    return base::nullopt;
  }

  // Check for the |cast| namespace in case we are injecting into a fresh Frame
  // which hasn't navigated to a page or received JS bindings.
  std::string initialize_values_js = base::StringPrintf(
      "if (window.cast)"
      "  cast.__platform__.__queryPlatformValueStore__.mergeValues(%s);",
      output_json.c_str());
  output_json.clear();
  output_json.shrink_to_fit();

  fuchsia::mem::Buffer initialize_values_js_buffer =
      cr_fuchsia::MemBufferFromString(initialize_values_js);
  initialize_values_js.clear();
  initialize_values_js.shrink_to_fit();

  return initialize_values_js_buffer;
}

}  // namespace

QueryableDataBindings::QueryableDataBindings(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<chromium::cast::QueryableData> service)
    : frame_(frame), service_(service.Bind()) {
  DCHECK(frame_);

  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  frame_->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(CastPlatformBindingsId::QUERYABLE_DATA), {"*"},
      cr_fuchsia::MemBufferFromFile(
          base::File(assets_path.AppendASCII(kBindingsPath),
                     base::File::FLAG_OPEN | base::File::FLAG_READ)),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "JavaScript injection error.";
      });

  service_->GetChangedEntries(
      fit::bind_member(this, &QueryableDataBindings::OnEntriesReceived));
}

QueryableDataBindings::~QueryableDataBindings() = default;

bool QueryableDataBindings::QueryableDataEntryLess::operator()(
    const chromium::cast::QueryableDataEntry& lhs,
    const chromium::cast::QueryableDataEntry& rhs) const {
  return lhs.key < rhs.key;
}

void QueryableDataBindings::OnEntriesReceived(
    std::vector<chromium::cast::QueryableDataEntry> new_entries) {
  // Push changes to the page immediately.
  base::Optional<fuchsia::mem::Buffer> update_script =
      BuildScriptFromEntries(new_entries.begin(), new_entries.end());

  if (!update_script)
    return;

  frame_->ExecuteJavaScriptNoResult(
      {"*"}, std::move(*update_script),
      [](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        DCHECK(result.is_response()) << "JavaScript injection error.";
      });

  // Update the cached values by merging in the new entries.
  cached_entries_.insert(new_entries.begin(), new_entries.end(),
                         base::KEEP_LAST_OF_DUPES);

  // Update the on-load script with the full list of values.
  base::Optional<fuchsia::mem::Buffer> on_load_script =
      BuildScriptFromEntries(cached_entries_.begin(), cached_entries_.end());
  if (!on_load_script)
    return;
  frame_->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(CastPlatformBindingsId::QUERYABLE_DATA_VALUES),
      {"*"}, std::move(*on_load_script),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "JavaScript injection error.";
      });

  // Request more changes from the FIDL service.
  service_->GetChangedEntries(
      fit::bind_member(this, &QueryableDataBindings::OnEntriesReceived));
}
