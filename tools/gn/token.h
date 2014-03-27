// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TOKEN_H_
#define TOOLS_GN_TOKEN_H_

#include "base/strings/string_piece.h"
#include "tools/gn/location.h"

class Token {
 public:
  enum Type {
    INVALID,
    INTEGER,    // 123
    STRING,     // "blah"
    TRUE_TOKEN,  // Not "TRUE" to avoid collisions with #define in windows.h.
    FALSE_TOKEN,

    // Various operators.
    EQUAL,
    PLUS,
    MINUS,
    PLUS_EQUALS,
    MINUS_EQUALS,
    EQUAL_EQUAL,
    NOT_EQUAL,
    LESS_EQUAL,
    GREATER_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    BOOLEAN_AND,
    BOOLEAN_OR,
    BANG,
    DOT,

    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    LEFT_BRACE,
    RIGHT_BRACE,

    IF,
    ELSE,
    IDENTIFIER, // foo
    COMMA,  // ,
    COMMENT,    // #...\n

    UNCLASSIFIED_OPERATOR,  // TODO(scottmg): This shouldn't be necessary.

    NUM_TYPES
  };

  Token();
  Token(const Location& location, Type t, const base::StringPiece& v);

  Type type() const { return type_; }
  const base::StringPiece& value() const { return value_; }
  const Location& location() const { return location_; }
  LocationRange range() const {
    return LocationRange(location_,
                         Location(location_.file(), location_.line_number(),
                                  location_.char_offset() +
                                      static_cast<int>(value_.size())));
  }

  // Helper functions for comparing this token to something.
  bool IsIdentifierEqualTo(const char* v) const;
  bool IsStringEqualTo(const char* v) const;

 private:
  Type type_;
  base::StringPiece value_;
  Location location_;
};

#endif  // TOOLS_GN_TOKEN_H_
