// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/picasa/pmp_table_reader.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "webkit/fileapi/media/picasa/pmp_column_reader.h"
#include "webkit/fileapi/media/picasa/pmp_constants.h"

namespace picasaimport {

namespace {

COMPILE_ASSERT(sizeof(double) == 8, double_must_be_8_bytes_long);

}  // namespace

PmpTableReader::PmpTableReader() : column_readers_(), max_row_count_(0) { }

PmpTableReader::~PmpTableReader() { }

bool PmpTableReader::Init(const std::string& table_name,
                          const base::FilePath& directory_path,
                          const std::vector<std::string>& columns) {
  DCHECK(!columns.empty());

  if (!column_readers_.empty())
    return false;

  if (!file_util::DirectoryExists(directory_path))
    return false;

  std::string table_prefix = table_name + "_";
  std::string indicator_file_name = table_prefix + "0";

  base::FilePath indicator_file = directory_path.Append(
      base::FilePath::FromUTF8Unsafe(indicator_file_name));

  // Look for the indicator_file file, indicating table existence.
  if (!file_util::PathExists(indicator_file) ||
      file_util::DirectoryExists(indicator_file)) {
    return false;
  }

  ScopedVector<PmpColumnReader> column_readers;
  uint32 max_row_count = 0;

  for (std::vector<std::string>::const_iterator it = columns.begin();
       it != columns.end(); ++it) {
    std::string filename = table_prefix + *it + "." + kPmpExtension;

    base::FilePath column_file_path = directory_path.Append(
        base::FilePath::FromUTF8Unsafe(filename));

    PmpColumnReader* column_reader = new PmpColumnReader();
    column_readers.push_back(column_reader);

    uint32 row_cnt;

    if (!column_reader->Init(column_file_path, &row_cnt))
      return false;

    max_row_count = std::max(max_row_count, row_cnt);
  }

  column_readers_ = column_readers.Pass();
  max_row_count_ = max_row_count;

  return true;
}

uint32 PmpTableReader::RowCount() const {
  return max_row_count_;
}

std::vector<const PmpColumnReader*> PmpTableReader::GetColumns() const {
  std::vector<const PmpColumnReader*> readers(
      column_readers_.begin(), column_readers_.end());
  return readers;
}

}  // namespace picasaimport
