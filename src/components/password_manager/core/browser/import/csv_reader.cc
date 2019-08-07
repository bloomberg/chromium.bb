// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_reader.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"

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
          base::StringPiece ret(input->data(), eol_start);
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
  base::StringPiece ret = *input;
  *input = base::StringPiece();
  return ret;
}

// Created for a row (line) of comma-separated-values, iteratively returns
// individual fields.
class FieldParser {
 public:
  explicit FieldParser(base::StringPiece row);
  ~FieldParser();

  // Advances the parser over the next comma-separated field and writes its
  // contents into |field_contents| (comma separator excluded, enclosing
  // quotation marks excluded, if present). Returns true if there were no
  // errors. The input must not be empty (check with HasMoreFields() before
  // calling).
  // TODO(crbug.com/918530): Also unescape the field contents.
  bool NextField(base::StringPiece* field_contents);

  bool HasMoreFields() const {
    return state_ != State::kError && position_ <= row_.size();
  }

 private:
  enum class State {
    // The state just before a new field begins.
    kInit,
    // The state after parsing a syntax error.
    kError,
    // When inside a non-escaped block.
    kPlain,
    // When inside a quotation-mark-escaped block.
    kQuoted,
    // When after reading a block starting and ending with quotation marks. For
    // the following input, the state would be visited after reading characters
    // 4 and 7:
    // a,"b""c",d
    // 0123456789
    kAfter,
  };

  // Returns the next character to be read and updates |position_|.
  char ConsumeChar();

  // Updates |state_| based on the next character to be read, according to this
  // diagram (made with help of asciiflow.com):
  //
  //   ,
  //  +--+  +--------------------------+
  //  |  |  |                          |
  // +V--+--V+all but " or , +--------+|
  // |       +--------------->        ||
  // | kInit |               | kPlain ||
  // |       <---------------+        ||
  // ++------+      ,        +^------++|
  //  |                       |      | |
  // "|                       +------+ |
  //  |                    all but ,   |,
  //  |                                |
  //  |                                |
  //  |   +---------+    "     +-------++
  //  |   |         +---------->        |
  //  +---> kQuoted |          | kAfter |
  //      |         <----------+        |
  //      +---------+    "     +-----+--+
  //                                |
  //      +--------+                |
  //      |        |                |
  //      | kError <----------------+
  //      |        |   all but " or ,
  //      +--------+
  //
  // The state kError has no outgoing transitions and so UpdateState should not
  // be called when this state has been entered.
  void UpdateState();

  // State of the parser.
  State state_ = State::kInit;
  // The input.
  const base::StringPiece row_;
  // If |position_| is >=0 and < |row_.size()|, then it points at the character
  // to be read next from |row_|. If it is equal to |row_.size()|, then it means
  // a fake trailing "," will be read next. If it is |row_.size() + 1|, then
  // reading is done.
  size_t position_ = 0;

  // The number of successful past invocations of NextField().
  size_t fields_returned_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FieldParser);
};

FieldParser::FieldParser(base::StringPiece row) : row_(row) {}

FieldParser::~FieldParser() = default;

bool FieldParser::NextField(base::StringPiece* field_contents) {
  DCHECK(HasMoreFields());
  if (fields_returned_ >= CSVTable::kMaxColumns)
    return false;

  if (state_ != State::kInit) {
    state_ = State::kError;
    return false;
  }

  const size_t start = position_;
  do {
    UpdateState();
  } while (state_ != State::kInit && state_ != State::kError);

  if (state_ != State::kError) {
    DCHECK_GT(position_, start);  // There must have been at least the ','.
    *field_contents =
        base::StringPiece(row_.data() + start, position_ - start - 1);

    if (field_contents->starts_with("\"")) {
      DCHECK(field_contents->ends_with("\"")) << *field_contents;
      DCHECK_GE(field_contents->size(), 2u);
      field_contents->remove_prefix(1);
      field_contents->remove_suffix(1);
    }
    ++fields_returned_;
    return true;
  }
  return false;
}

char FieldParser::ConsumeChar() {
  DCHECK_LE(position_, row_.size());
  // The default character to return once all from |row_| are consumed and
  // |position_| == |row_.size()|.
  char ret = ',';
  if (position_ < row_.size())
    ret = row_[position_];
  ++position_;
  return ret;
}

void FieldParser::UpdateState() {
  if (position_ > row_.size()) {
    // If in state |kInit| then the program attempts to read one field too many.
    DCHECK_NE(state_, State::kInit);
    // Otherwise a quotation mark was not matched before the end of input.
    state_ = State::kError;
    return;
  }

  char read = ConsumeChar();
  switch (state_) {
    case State::kInit:
      switch (read) {
        case ',':
          break;
        case '"':
          state_ = State::kQuoted;
          break;
        default:
          state_ = State::kPlain;
          break;
      }
      break;
    case State::kPlain:
      switch (read) {
        case ',':
          state_ = State::kInit;
          break;
        default:
          break;
      }
      break;
    case State::kQuoted:
      switch (read) {
        case '"':
          state_ = State::kAfter;
          break;
        default:
          break;
      }
      break;
    case State::kAfter:
      switch (read) {
        case ',':
          state_ = State::kInit;
          break;
        case '"':
          state_ = State::kQuoted;
          break;
        default:
          state_ = State::kError;
          break;
      }
      break;
    case State::kError:
      NOTREACHED();
      break;
  }
}

// Created for a string with potentially multiple rows of
// comma-separated-values, iteratively returns individual fields from row after
// row.
class CSVParser {
 public:
  explicit CSVParser(base::StringPiece csv);
  ~CSVParser();

  // Reads and unescapes values from the next row, and writes them to |fields|.
  // Consumes the end-of-line terminator. Returns false on syntax error. The
  // input must not be empty (check with HasMoreRows() before calling).
  bool ParseNextCSVRow(std::vector<std::string>* fields);

  bool HasMoreRows() const { return !remaining_csv_piece_.empty(); }

 private:
  base::StringPiece remaining_csv_piece_;

  DISALLOW_COPY_AND_ASSIGN(CSVParser);
};

CSVParser::CSVParser(base::StringPiece csv) : remaining_csv_piece_(csv) {}

CSVParser::~CSVParser() = default;

bool CSVParser::ParseNextCSVRow(std::vector<std::string>* fields) {
  fields->clear();

  DCHECK(HasMoreRows());
  FieldParser parser(ConsumeLine(&remaining_csv_piece_));
  base::StringPiece current_field;
  while (parser.HasMoreFields()) {
    if (!parser.NextField(&current_field))
      return false;
    // TODO(crbug.com/918530): Unescape the field contents in-place, as part of
    // NextField().
    std::string field_copy(current_field);
    base::ReplaceSubstringsAfterOffset(&field_copy, 0, "\"\"", "\"");
    fields->push_back(std::move(field_copy));
  }
  return true;
}

}  // namespace

CSVTable::CSVTable() = default;

CSVTable::~CSVTable() = default;

bool CSVTable::ReadCSV(base::StringPiece csv) {
  records_.clear();
  column_names_.clear();

  // Read header row.
  CSVParser parser(csv);
  if (!parser.HasMoreRows()) {
    // The empty CSV is a special case. It can be seen as having one row, with a
    // single field, which is an empty string.
    column_names_.emplace_back();
    return true;
  }
  if (!parser.ParseNextCSVRow(&column_names_))
    return false;

  // Reader data records rows.
  std::vector<std::string> fields;
  while (parser.HasMoreRows()) {
    if (!parser.ParseNextCSVRow(&fields))
      return false;
    // If there are more line-breaking characters in sequence, the row parser
    // will see an empty row in between each successive two of those. Discard
    // such results, because those are useless for importing passwords.
    if (fields.size() == 1 && fields[0].empty())
      continue;

    std::map<base::StringPiece, std::string> row_map;
    const size_t available_columns =
        std::min(column_names_.size(), fields.size());
    for (size_t i = 0; i < available_columns; ++i) {
      row_map[column_names_[i]] = std::move(fields[i]);
    }
    records_.push_back(std::move(row_map));
  }

  return true;
}

}  // namespace password_manager
