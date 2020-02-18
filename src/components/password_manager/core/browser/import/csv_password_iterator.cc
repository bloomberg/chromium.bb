// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_iterator.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/template_util.h"

namespace password_manager {

namespace {

// Returns all the characters from the start of |input| until the first '\n',
// "\r\n" (exclusive) or the end of |input|. Cuts the returned part (inclusive
// the line breaks) from |input|. Skips blocks of matching quotes. Examples:
// old input -> returned value, new input
// "ab\ncd" -> "ab", "cd"
// "\r\n" -> "", ""
// "abcd" -> "abcd", ""
// "\r" -> "\r", ""
// "a\"\n\"b" -> "a\"\n\"b", ""
base::StringPiece ConsumeLine(base::StringPiece* input) {
  DCHECK(input);
  DCHECK(!input->empty());

  bool inside_quotes = false;
  bool last_char_was_CR = false;
  for (size_t current = 0; current < input->size(); ++current) {
    char c = (*input)[current];
    switch (c) {
      case '\n':
        if (!inside_quotes) {
          const size_t eol_start = last_char_was_CR ? current - 1 : current;
          base::StringPiece ret = input->substr(0, eol_start);
          *input = input->substr(current + 1);
          return ret;
        }
        break;
      case '"':
        inside_quotes = !inside_quotes;
        break;
      default:
        break;
    }
    last_char_was_CR = (c == '\r');
  }

  // The whole |*input| is one line.
  return std::exchange(*input, base::StringPiece());
}

// Takes the |rest| of the CSV lines, returns the first one and stores the
// remaining ones back in |rest|.
base::StringPiece ExtractFirstRow(base::StringPiece* rest) {
  DCHECK(rest);
  if (!rest->empty())
    return ConsumeLine(rest);
  return base::StringPiece();
}

}  // namespace

CSVPasswordIterator::CSVPasswordIterator() = default;

CSVPasswordIterator::CSVPasswordIterator(const CSVPassword::ColumnMap& map,
                                         base::StringPiece csv)
    : map_(&map),
      csv_rest_(csv),
      csv_row_(ExtractFirstRow(&csv_rest_)),
      password_(base::in_place, map, csv_row_) {}

CSVPasswordIterator::CSVPasswordIterator(const CSVPasswordIterator& other) {
  *this = other;
}

CSVPasswordIterator& CSVPasswordIterator::operator=(
    const CSVPasswordIterator& other) {
  map_ = other.map_;
  csv_rest_ = other.csv_rest_;
  csv_row_ = other.csv_row_;
  if (map_)
    password_.emplace(*map_, csv_row_);
  else
    password_.reset();
  return *this;
}

CSVPasswordIterator::~CSVPasswordIterator() = default;

CSVPasswordIterator& CSVPasswordIterator::operator++() {
  DCHECK(map_);
  csv_row_ = ExtractFirstRow(&csv_rest_);
  password_.emplace(*map_, csv_row_);
  return *this;
}

CSVPasswordIterator CSVPasswordIterator::operator++(int) {
  CSVPasswordIterator old = *this;
  ++*this;
  return old;
}

bool CSVPasswordIterator::operator==(const CSVPasswordIterator& other) const {
  // There is no need to compare |password_|, because it is determined by |map_|
  // and |csv_row_|.
  return
      // Checking StringPiece::data() equality instead of just StringPiece has
      // two reasons: (1) flagging the case when, e.g., two identical lines in
      // one CSV blob would otherwise cause the corresponding iterators look the
      // same, and (2) efficiency. StringPiece::size() is not checked on the
      // assumption that always the whole row is contained in |csv_row_|.
      csv_row_.data() == other.csv_row_.data() &&
      // The column map should reference the same map if the iterators come from
      // the same sequence, and iterators from different sequences are not
      // considered equal. Therefore the maps' addresses are checked instead of
      // their contents.
      map_ == other.map_;
}

}  // namespace password_manager
