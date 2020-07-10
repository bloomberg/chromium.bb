// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <set>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_iterator.h"

namespace password_manager {

namespace {

// Given a CSV column |name|, returns the matching CSVPassword::Label or nothing
// if the column name is not recognised.
base::Optional<CSVPassword::Label> NameToLabel(base::StringPiece name) {
  using Label = CSVPassword::Label;
  // Recognised column names for origin URL, usernames and passwords.
  static const base::NoDestructor<base::flat_map<base::StringPiece, Label>>
      kLabelMap({
          {"url", Label::kOrigin},
          {"website", Label::kOrigin},
          {"origin", Label::kOrigin},
          {"hostname", Label::kOrigin},

          {"username", Label::kUsername},
          {"user", Label::kUsername},
          {"login", Label::kUsername},
          {"account", Label::kUsername},

          {"password", Label::kPassword},
      });
  auto it = kLabelMap->find(base::ToLowerASCII(name));
  return it != kLabelMap->end() ? base::make_optional(it->second)
                                : base::nullopt;
}

}  // namespace

CSVPasswordSequence::CSVPasswordSequence(std::string csv)
    : csv_(std::move(csv)) {
  // Sanity check.
  if (csv_.empty()) {
    result_ = CSVPassword::Status::kSyntaxError;
    return;
  }
  data_rows_ = csv_;

  // Construct ColumnMap.
  base::StringPiece first = ConsumeCSVLine(&data_rows_);
  size_t col_index = 0;
  for (CSVFieldParser parser(first); parser.HasMoreFields(); ++col_index) {
    base::StringPiece name;
    if (!parser.NextField(&name)) {
      result_ = CSVPassword::Status::kSyntaxError;
      return;
    }
    base::Optional<CSVPassword::Label> label = NameToLabel(name);
    if (label) {
      map_[col_index] = *label;
    }
  }

  // Check that each of the three labels is assigned to exactly one column.
  if (map_.size() != CSVPassword::kLabelCount) {
    result_ = CSVPassword::Status::kSemanticError;
    return;
  }
  base::flat_set<CSVPassword::Label> all_labels;
  for (const auto& kv : map_) {
    if (!all_labels.insert(kv.second).second) {
      // More columns share the same label.
      result_ = CSVPassword::Status::kSemanticError;
      return;
    }
  }
}

CSVPasswordSequence::CSVPasswordSequence(CSVPasswordSequence&&) = default;
CSVPasswordSequence& CSVPasswordSequence::operator=(CSVPasswordSequence&&) =
    default;

CSVPasswordSequence::~CSVPasswordSequence() = default;

CSVPasswordIterator CSVPasswordSequence::begin() const {
  if (result_ != CSVPassword::Status::kOK)
    return end();
  return CSVPasswordIterator(map_, data_rows_);
}

CSVPasswordIterator CSVPasswordSequence::end() const {
  return CSVPasswordIterator(map_, base::StringPiece());
}

}  // namespace password_manager
